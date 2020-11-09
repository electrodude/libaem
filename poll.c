#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "poll.h"

struct aem_poll_event *aem_poll_event_init(struct aem_poll_event *evt)
{
	aem_assert(evt);
	evt->on_event = NULL;
	evt->i = -1;

	evt->fd = -1;
	evt->events = 0;

	return evt;
}

void aem_poll_init(struct aem_poll *p)
{
	aem_assert(p);
	p->n = 0;
	p->maxn = 8;
	p->fds  = malloc(p->maxn*sizeof(*p->fds ));
	p->evts = malloc(p->maxn*sizeof(*p->evts));
}

void aem_poll_dtor(struct aem_poll *p)
{
	aem_assert(p);
	p->maxn = 0;
	if (p->fds)
		free(p->fds);
	if (p->evts)
		free(p->evts);
}

static void aem_poll_resize(struct aem_poll *p, size_t maxn)
{
	aem_assert(p);
	aem_logf_ctx(AEM_LOG_DEBUG, "%p: resize from %zd to %zd\n", p, p->maxn, maxn);
	p->maxn = maxn;
	p->fds  = realloc(p->fds , p->maxn*sizeof(*p->fds ));
	p->evts = realloc(p->evts, p->maxn*sizeof(*p->evts));
	// TODO: Indicate failure if realloc fails.
	aem_assert(p->fds);
	aem_assert(p->evts);
}

static void aem_poll_assign(struct aem_poll *p, struct aem_poll_event *evt)
{
	aem_assert(p);
	aem_assert(evt);
	aem_logf_ctx(AEM_LOG_DEBUG, "evt %p[%zd] = %p: fd %d\n", p, evt->i, evt, evt->fd);

	aem_assert(evt->i >= 0 && (size_t)evt->i < p->n);

	size_t i = evt->i;
	p->fds[i] = (struct pollfd){.fd = evt->fd, .events = evt->events, .revents = 0};
	p->evts[i] = evt;
}

ssize_t aem_poll_add(struct aem_poll *p, struct aem_poll_event *evt)
{
	aem_assert(p);
	aem_assert(evt);
	if (evt->fd < 0) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid fd: %d\n", evt->fd);
		return -1;
	}

	// Increase array size if necessary.
	if (p->n >= p->maxn) {
		// Double the array size, but don't let it be smaller than 8.
		size_t maxn = p->n*2;
		aem_poll_resize(p, maxn >= 8 ? maxn : 8);
	}
	aem_assert(p->maxn);

	aem_assert(evt->i == -1);
	size_t i = p->n++;
	evt->i = i;

	aem_poll_assign(p, evt);

	aem_logf_ctx(AEM_LOG_DEBUG, "evt %p[%zd] = %p: fd %d\n", p, evt->i, evt, evt->fd);

	return evt->i;
}

int aem_poll_del(struct aem_poll *p, struct aem_poll_event *evt)
{
	aem_assert(p);
	aem_assert(evt);
	aem_logf_ctx(AEM_LOG_DEBUG, "evt %p[%zd] = %p: fd %d\n", p, evt->i, evt, evt->fd);
	if (evt->i == -1)
		return -1;

	aem_assert(evt->i >= 0);
	size_t i = evt->i;
	aem_assert(i < p->n);
	aem_assert(p->evts[i] == evt);

	// Invalidate this event
	evt->i = -1;

	// Remove event from list
	if (i != p->n-1) { // Unless this already was the last one,
		// Move last event into deleted event's position.
		struct aem_poll_event *last = p->evts[p->n - 1];
		aem_assert(last->i >= 0 && (size_t)last->i == p->n-1);
		last->i = i;
		aem_poll_assign(p, last);
	}

	p->n--;

	if (p->n && p->n*8 <= p->maxn) {
		aem_poll_resize(p, p->n*2);
	}

	return 0;
}

void aem_poll_mod(struct aem_poll *p, struct aem_poll_event *evt)
{
	aem_assert(evt);

	// Must already be registered
	if (evt->i < 0)
		return;

	aem_assert(p);

	struct pollfd *pollfd = aem_poll_get_pollfd(p, evt);
	pollfd->fd = evt->fd;
	pollfd->events = evt->events;
}

struct pollfd *aem_poll_get_pollfd(struct aem_poll *p, struct aem_poll_event *evt)
{
	if (!p)
		return NULL;
	if (!evt)
		return NULL;
	if (evt->i == -1)
		return NULL;
	aem_assert(p);
	aem_assert(evt);
	aem_assert(evt->i != -1);
	size_t i = evt->i;
	aem_assert(i < p->n);
	aem_assert(evt == p->evts[i]);

	struct pollfd *pollfd = &p->fds[i];

	return pollfd;
}


int aem_poll_poll(struct aem_poll *p)
{
	aem_assert(p);
	// Ensure all registrations are consistent
	for (size_t i = 0; i < p->n; i++) {
		struct pollfd *pollfd = &p->fds[i];
		struct aem_poll_event *evt = p->evts[i];
		aem_assert(evt);
		aem_assert(evt->i >= 0 && (size_t)evt->i == i);
		// If it's wrong, just fix it instead of complaining
		//pollfd->fd = evt->fd;
		//pollfd->events = evt->events;
		aem_assert(pollfd->fd == evt->fd);
		aem_assert(pollfd->events == evt->events);
	}

	aem_logf_ctx(AEM_LOG_DEBUG, "%p: poll %zd events\n", p, p->n);

	int timeout = -1;
	int rc = poll(p->fds, p->n, timeout);

	if (rc < 0) {
		// error
		switch (errno) {
			case EINTR:
				return rc;

			default: {
				int myerrno = errno;
				aem_logf_ctx(AEM_LOG_FATAL, "poll failed: %s\n", strerror(errno));
				errno = myerrno;
				return rc;
			}
		}
	}

	if (!rc)
		aem_logf_ctx(AEM_LOG_DEBUG, "timeout after %d\n", timeout);

	aem_logf_ctx(AEM_LOG_DEBUG, "%d pending events\n", rc);

	int progress = 0;
	do {
		for (size_t i = 0; i < p->n; i++) {
			struct pollfd *pollfd = &p->fds[i];
			struct aem_poll_event *evt = p->evts[i];
			aem_assert(pollfd->fd == evt->fd);
			short revents = evt->revents = pollfd->revents;

			if (!revents)
				continue;

			progress = 1;

			// Decrement this for each event we process: it should
			// be zero by the time we're done.
			rc--;

			if (revents & POLLNVAL)
				aem_logf_ctx(AEM_LOG_BUG, "POLLNVAL on fd %d for poll %p, evt %zd\n", pollfd->fd, p, i);

			aem_logf_ctx(AEM_LOG_DEBUG, "%p[%zd]: fd %d revents %x\n", p, i, pollfd->fd, revents);

			if (evt->on_event)
				evt->on_event(p, evt);

			if ((revents & POLLHUP) && evt->i != -1) {
				// TODO: Is ignoring or deregistering chronically ignored events trying too hard?
				aem_poll_del(p, evt);
				aem_logf_ctx(AEM_LOG_BUG, "We unregistered fd %d for you due to POLLHUP because your buggy code forgot to do it itself.  Memory around (struct aem_poll_event*)%p was likely leaked.\n", evt->fd, evt);
			}

			if (evt->revents)
				aem_logf_ctx(AEM_LOG_BUG, "unhandled revents %x on event %zd (fd %d)\n", evt->revents, i, evt->fd);

			// Ensure we don't get stuck in an infinite loop when
			// some idiot writes an event handler that, in a single
			// call, both swaps itself with the next event in the
			// list *and* leaves events unhandled.  By clearing
			// this, we ensure that the above `if (revents)
			// continue;` statement won't let us re-process this
			// event until the next poll.
			evt->revents = 0;

			// Now we have the wonderful task of determining whether the
			// event we just processed is no longer at the current position
			// in the event list, in which case we have to adjust various
			// things.

			// Check that the event's registration is still
			// consistent, unless it's no longer registered.
			if (evt->i != -1)
				aem_assert(p->evts[evt->i] == evt);

			// If this slot no longer contains the event we just
			// proccessed, try this slot again.  This won't cause a
			// problem if this was the last event because p->n
			// would also have been decremented.
			if (evt->i == -1 || (size_t)evt->i != i)
				i--;
		}
		// If unprocessed events still remain, hopefully it's because the event list was shuffled around underneath us.  Try again until all events are processed (or until we somehow stop making progress, which would imply a bug).
	} while (rc > 0 && progress);

	if (rc)
		aem_logf_ctx(AEM_LOG_BUG, "%d events unprocessed!\n", rc);

	return rc;
}

/*
struct aem_poll_event *aem_poll_next(struct aem_poll *p)
{
	aem_assert(p);
	if (!p->curr) {
		aem_logf_ctx(AEM_LOG_DEBUG, "%p: poll %zd events\n", p, p->n);
	again:;
		int rc = poll(p->fds, p->n, -1);

		if (rc < 0) {
			int myerrno = errno;
			switch (myerrno) {
				case EINTR:
					goto again;

				default:
					aem_logf_ctx(AEM_LOG_FATAL, "poll failed: %s\n", strerror(myerrno));
					errno = myerrno;
					return NULL;
			}
		}

		if (rc <= 0)
			return NULL;

		p->curr = p->n;
	} else {
		size_t i = p->curr;

		struct pollfd *pollfd = &p->fds[i];
		struct aem_poll_event *evt = p->evts[i];

		pollfd->fd = evt->fd;
		pollfd->events = evt->events;
	}

	while (p->curr) {
		size_t i = --p->curr;

		struct pollfd *pollfd = &p->fds[i];
		struct aem_poll_event *evt = p->evts[i];

		short revents = pollfd->revents;

		// TODO: is ignoring or deregistering chronically ignored events trying too hard?

		if (revents & POLLNVAL) {
			aem_logf_ctx(AEM_LOG_BUG, "POLLNVAL fd %d\n", pollfd->fd);
			if (pollfd->fd != evt->fd) {
				aem_logf_ctx(AEM_LOG_BUG, "inconsistent: pollfd->fd = %d, evt->fd = %d\n", pollfd->fd, evt->fd);
				aem_logf_ctx(AEM_LOG_BUG, "always re-register an event via aem_poll_add(p, evt) after changing its fd\n");
			} else {
				aem_logf_ctx(AEM_LOG_BUG, "always deregister closed fds via aem_poll_del(p, evt)\n");
				// aem_poll_del(p, evt);
			}
		}

		if (revents) {
			if (evt->on_event) {
				aem_logf_ctx(AEM_LOG_DEBUG, "%p: i %zd fd %d revents %x internal\n", p, i, pollfd->fd, revents);
				evt->revents = revents;
				evt->on_event(p, evt);
				if (evt->revents)
					aem_logf_ctx(AEM_LOG_BUG, "unhandled revents %x\n", evt->revents);
				pollfd->fd = evt->fd;
				pollfd->events = evt->events;
			} else {
				aem_logf_ctx(AEM_LOG_DEBUG, "%p: i %zd fd %d revents %x external\n", p, i, pollfd->fd, revents);
				return evt;
			}
		}
	}

	return NULL;
}
*/
