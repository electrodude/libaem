#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define AEM_INTERNAL
#include <aem/stringbuf.h>

#include "poll.h"

struct aem_poll_event *aem_poll_event_init(struct aem_poll_event *evt)
{
	aem_assert(evt);
	evt->on_event = NULL;
	evt->i = -1;

	evt->fd = -1;
	evt->events = 0;
	evt->revents = 0;

	return evt;
}

static void aem_poll_event_verify(struct aem_poll *p, size_t i)
{
	struct pollfd *pollfd = &p->fds[i];
	struct aem_poll_event *evt = p->evts[i];
	aem_assert(evt);
	aem_assert(evt->i >= 0);
	aem_assert((size_t)evt->i == i);
	aem_assert(evt->fd >= 0);
	aem_assert(pollfd->fd == evt->fd);
	aem_assert(pollfd->events == evt->events);
}

void aem_poll_init(struct aem_poll *p)
{
	aem_assert(p);
	p->n = 0;
	p->maxn = 8;
	p->fds  = malloc(p->maxn*sizeof(*p->fds ));
	p->evts = malloc(p->maxn*sizeof(*p->evts));
	p->poll_rc = 0;
}

void aem_poll_dtor(struct aem_poll *p)
{
	aem_assert(p);

	//aem_poll_hup_all(p);

	p->maxn = 0;
	if (p->fds)
		free(p->fds);
	if (p->evts)
		free(p->evts);
}

static void aem_poll_resize(struct aem_poll *p)
{
	aem_assert(p);
	// Try to have twice as many elements as necessary, but never fewer than 8.
	size_t maxn = p->n*2;
	if (maxn < 8)
		maxn = 8;
	if (maxn == p->maxn)
		return;
	aem_logf_ctx(AEM_LOG_DEBUG, "%p: resize from %zd to %zd (%zd used)", p, p->maxn, maxn, p->n);
	aem_assert(p->n+1 <= maxn); // Make sure we have room for at least one more.
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
	aem_logf_ctx(AEM_LOG_DEBUG, "evt %p[%zd] := %p (fd %d)", p, evt->i, evt, evt->fd);

	aem_assert(evt->i >= 0);
	aem_assert((size_t)evt->i < p->n);

	size_t i = evt->i;
	p->fds[i] = (struct pollfd){.fd = evt->fd, .events = evt->events, .revents = 0};
	p->evts[i] = evt;
}

ssize_t aem_poll_add(struct aem_poll *p, struct aem_poll_event *evt)
{
	aem_assert(p);
	aem_assert(evt);
	if (evt->fd < 0) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid fd: %d", evt->fd);
		return -1;
	}

	// Increase array size if necessary.
	if (p->n >= p->maxn) {
		aem_poll_resize(p);
	}
	aem_assert(p->maxn);

	aem_assert(evt->i == -1);
	size_t i = p->n++;
	evt->i = i;

	aem_poll_assign(p, evt);

	aem_logf_ctx(AEM_LOG_DEBUG, "evt %p[%zd] = %p: fd %d", p, evt->i, evt, evt->fd);

	return evt->i;
}

int aem_poll_del(struct aem_poll *p, struct aem_poll_event *evt)
{
	aem_assert(p);
	aem_assert(evt);
	aem_logf_ctx(AEM_LOG_DEBUG, "evt %p[%zd] = %p: fd %d", p, evt->i, evt, evt->fd);
	if (evt->i == -1) {
		aem_logf_ctx(AEM_LOG_BUG, "Event %p (fd %d) not registered", evt, evt->fd);
		return -1;
	}

	aem_assert(evt->i >= 0);
	size_t i = evt->i;
	aem_assert(i < p->n);
	aem_assert(p->evts[i] == evt);

	// Mark this event as invalid.
	evt->i = -1;

	// Remove event from list.
	if (i != p->n-1) { // Unless this already was the last one,
		// Move last event into deleted event's position.
		struct aem_poll_event *last = p->evts[p->n - 1];
		aem_assert(last);
		aem_assert(last->i >= 0);
		aem_assert((size_t)last->i == p->n-1);
		last->i = i;
		aem_poll_assign(p, last);
	}

	p->n--;

	if (p->n*8 <= p->maxn) {
		aem_poll_resize(p);
	}

	return 0;
}

void aem_poll_mod(struct aem_poll *p, struct aem_poll_event *evt)
{
	aem_assert(p);
	aem_assert(evt);

	// Only makes sense if event is already registered.
	if (evt->i < 0)
		return;

	size_t i = evt->i;
	aem_assert(i < p->n);
	aem_assert(p->evts[i] == evt);

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

#ifdef AEM_DEBUG
static void aem_poll_verify(struct aem_poll *p)
{
	aem_assert(p);

	// Ensure all registrations are consistent
	for (size_t i = 0; i < p->n; i++) {
		struct aem_poll_event *evt = p->evts[i];
		// If it's wrong, just fix it instead of complaining
		//struct pollfd *pollfd = &p->fds[i];
		//pollfd->fd = evt->fd;
		//pollfd->events = evt->events;
		aem_poll_event_verify(p, i);
		if (!evt->events) {
			if (aem_log_header(&aem_log_buf, AEM_LOG_WARN)) {
				aem_stringbuf_printf(&aem_log_buf, "Empty event %zd: ", i);
				aem_poll_event_dump(&aem_log_buf, evt);
				aem_stringbuf_puts(&aem_log_buf, "\n");
				aem_log_str(&aem_log_buf);
			}
		}
	}
}
#endif

void aem_poll_print_event_bits(struct aem_stringbuf *out, short revents)
{
	int first = 1;

#define APPEND_REVENTS_STR(name) if (revents & name) { if (!first) aem_stringbuf_puts(out, " | "); first = 0; aem_stringbuf_puts(out, #name); revents &= ~name; }
	APPEND_REVENTS_STR(POLLIN)
	APPEND_REVENTS_STR(POLLPRI)
	APPEND_REVENTS_STR(POLLOUT)
#ifdef POLLRDHUP
	APPEND_REVENTS_STR(POLLRDHUP)
#endif
	APPEND_REVENTS_STR(POLLERR)
	APPEND_REVENTS_STR(POLLHUP)
	APPEND_REVENTS_STR(POLLNVAL)
#undef APPEND_REVENTS_STR

	if (revents) { if (!first) aem_stringbuf_puts(out, " | "); first = 0; aem_stringbuf_printf(out, "%#x", revents); }
	if (first) {
		aem_stringbuf_puts(out, "0");
	}
}

void aem_poll_event_dump(struct aem_stringbuf *out, const struct aem_poll_event *evt)
{
	aem_stringbuf_printf(out, "fd %d: events = ", evt->fd);
	aem_poll_print_event_bits(out, evt->events);
	aem_stringbuf_puts(out, ", revents = ");
	aem_poll_print_event_bits(out, evt->revents);
}


int aem_poll_wait(struct aem_poll *p)
{
	aem_assert(p);

#ifdef AEM_DEBUG
	aem_poll_verify(p);
#endif

	aem_logf_ctx(AEM_LOG_DEBUG, "%p: poll %zd events", p, p->n);

	int timeout = -1;
	int rc = poll(p->fds, p->n, timeout);

	p->poll_rc = rc;

	if (rc < 0) {
		// error
		switch (errno) {
			case EAGAIN:
			case EINTR:
				return rc;

			default: {
				int myerrno = errno;
				aem_logf_ctx(AEM_LOG_FATAL, "poll failed: %s", strerror(errno));
				errno = myerrno;
				return rc;
			}
		}
	}

	if (!rc)
		aem_logf_ctx(AEM_LOG_DEBUG, "timeout after %d", timeout);

	aem_logf_ctx(AEM_LOG_DEBUG, "%d pending events", rc);

	return rc;
}

int aem_poll_process(struct aem_poll *p)
{
	aem_assert(p);

	int rc = p->poll_rc;

	if (rc < 0)
		return rc;

	int rc_orig;
	do {
		rc_orig = rc;
		for (size_t i = 0; i < p->n; i++) {
			struct pollfd *pollfd = &p->fds[i];
			struct aem_poll_event *evt = p->evts[i];
			aem_poll_event_verify(p, i);
			short revents = evt->revents = pollfd->revents;

			if (!revents)
				continue;

			// Decrement this for each event we process: it should
			// be zero by the time we're done.
			rc--;

			if (revents & POLLNVAL)
				aem_logf_ctx(AEM_LOG_BUG, "POLLNVAL on fd %d for poll %p, evt %zd", pollfd->fd, p, i);

			if (aem_log_header(&aem_log_buf, AEM_LOG_DEBUG)) {
				aem_stringbuf_printf(&aem_log_buf, "%p[%zd]: ", p, i);
				aem_poll_event_dump(&aem_log_buf, evt);
				aem_stringbuf_puts(&aem_log_buf, "\n");
				aem_log_str(&aem_log_buf);
			}

			if (evt->on_event)
				evt->on_event(p, evt);

			// Ensure event isn't still registered after a POLLHUP
			if ((revents & POLLHUP) && evt->i != -1) {
				// TODO: Is ignoring or deregistering chronically ignored events trying too hard?
				// TODO: This can have false positives if e.g. on POLLHUP, the callback deregisters its event, closes the fd, opens a new fd with the same number, and reregisters the event and it happens to get the same index.
				aem_poll_del(p, evt);
				aem_logf_ctx(AEM_LOG_BUG, "We deregistered fd %d for you due to POLLHUP because your buggy code forgot to do it itself.  The object containing (struct aem_poll_event*)%p was likely leaked.", evt->fd, evt);
			}

			if (evt->revents) {
				if (aem_log_header(&aem_log_buf, AEM_LOG_BUG)) {
					aem_stringbuf_printf(&aem_log_buf, "Unhandled revents on event %zd: ", i);
					aem_poll_event_dump(&aem_log_buf, evt);
					aem_stringbuf_puts(&aem_log_buf, "\n");
					aem_log_str(&aem_log_buf);
				}
			}


			// Only do this stuff if the event is still registered.
			if (evt->i != -1) {
				// Get it again in case evt->i changes or p->fds is moved by realloc().
				pollfd = &p->fds[evt->i];

				// Ensure we don't get stuck in an infinite loop when
				// some idiot writes an event handler that, in a single
				// call, both swaps itself with the next event in the
				// list *and* leaves events unhandled.  By clearing
				// this, we ensure that the above `if (revents)
				// continue;` statement won't let us re-process this
				// event until the next poll.
				pollfd->revents = 0;

				// Ensure event is still consistent
				aem_assert(p->evts[evt->i] == evt);
				aem_poll_event_verify(p, evt->i);
			}

			// If this slot no longer contains the event we just
			// proccessed, try this slot again.  This won't cause a
			// problem if this was the last event because p->n
			// would also have been decremented.
			// It can't hurt to process the same event twice,
			// assuming evt->revents was cleared the first time
			// through.
			if (evt->i == -1 || (size_t)evt->i != i)
				i--;
		}
		// If unprocessed events still remain, hopefully it's because
		// the event list was shuffled around underneath us.  Try again
		// until all events are processed (or until we somehow stop
		// making progress, which would imply a bug).
		if (rc == rc_orig) {
			// TODO: What if an event deregisters some other event with pending revents?
			aem_logf_ctx(AEM_LOG_BUG, "Made no progress; lost %d event%s", rc, rc != 1 ? "s" : "");
			break;
		}
	} while (rc > 0);

	return rc;
}

int aem_poll_poll(struct aem_poll *p)
{
	aem_poll_wait(p);

	return aem_poll_process(p);
}

void aem_poll_hup_all(struct aem_poll *p)
{
	aem_assert(p);

#ifdef AEM_DEBUG
	aem_poll_verify(p);
#endif

	aem_logf_ctx(AEM_LOG_DEBUG, "%p: HUP all", p);

	// Pretend poll(2) returned POLLHUP on every fd
	for (size_t i = 0; i < p->n; i++) {
		struct pollfd *pollfd = &p->fds[i];
		pollfd->revents |= POLLHUP;
	}

	p->poll_rc = p->n;
	aem_poll_process(p);
}
