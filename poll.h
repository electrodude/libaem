#ifndef AEM_POLL_H
#define AEM_POLL_H

#include <unistd.h>
#include <poll.h>

#include <aem/log.h>

#ifdef AEM_CONFIG_UNIX
#ifdef POLLRDHUP
#define AEM_POLL_EVENT_EOF POLLRDHUP
#else
#define AEM_POLL_NO_POLLRDHUP
//#define AEM_POLL_EVENT_EOF ???
#endif

#define AEM_POLL_EVENT_RD POLLIN
#define AEM_POLL_EVENT_WR POLLOUT
#endif

struct aem_poll;
struct aem_poll_event;

struct aem_poll_event {
	/* This callback is called whenever this event is triggered.  It must
	 * deal with all events in `evt->revents` whenever it is called, which
	 * it should check using `aem_poll_event_check`, or equivalent code,
	 * resulting in `evt->revents` being completely cleared by the time it
	 * returns.
	 *
	 * If the POLLHUP event is active, the callback must deregister the
	 * event.  The event loop can't tell the difference between the
	 * callback forgetting to deregister the event and the callback validly
	 * closing and opening a new fd with the same number and reusing the
	 * same event for it - this is a defect.
	 *
	 * Make this event struct a member of your own parent struct with
	 * context, and use `aem_container_of` to find the parent object.
	 *
	 * This callback *must not* free the memory containing `evt` while
	 * `aem_poll_poll(p)` is active.  Use <aem/pmcrcu.h> or a real RCU
	 * implementation to ensure object freeing is postponed to occur
	 * outside of `aem_poll_poll`.
	 */
	void (*on_event)(struct aem_poll *p, struct aem_poll_event *evt);
	ssize_t i;

	int fd;
	short events;
	short revents;
};

#define AEM_POLL_EVENT_EMPTY ((struct aem_poll_event){.i = -1});

struct aem_poll_event *aem_poll_event_init(struct aem_poll_event *evt);

static inline int aem_poll_event_check(struct aem_poll_event *evt, short event)
{
	aem_assert(evt);
	short event_mask = evt->revents & event;
	evt->revents &= ~event;

	return event_mask;
}

struct aem_poll {
	size_t n;
	size_t maxn;
	struct pollfd *fds;
	struct aem_poll_event **evts;

	int poll_rc;
};

void aem_poll_init(struct aem_poll *p);
void aem_poll_dtor(struct aem_poll *p);

ssize_t aem_poll_add(struct aem_poll *p, struct aem_poll_event *evt);
int aem_poll_del(struct aem_poll *p, struct aem_poll_event *evt);
void aem_poll_mod(struct aem_poll *p, struct aem_poll_event *evt);

struct pollfd *aem_poll_get_pollfd(struct aem_poll *p, struct aem_poll_event *evt);

// Debug functions
struct aem_stringbuf;
void aem_poll_print_event_bits(struct aem_stringbuf *out, short revents);
void aem_poll_event_dump(struct aem_stringbuf *out, const struct aem_poll_event *evt);

// Call poll(2)
int aem_poll_wait(struct aem_poll *p);
// Process events found by previous aem_poll_wait
int aem_poll_process(struct aem_poll *p);
// Call aem_poll_wait and then aem_poll_process, returning the result of the latter
int aem_poll_poll(struct aem_poll *p);

// Send an artificial HUP to each event handler
void aem_poll_hup_all(struct aem_poll *p);

#endif /* AEM_POLL_H */
