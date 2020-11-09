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
	 * Make this event struct a member of your own parent struct with
	 * context, and use `aem_container_of` to find the parent object.
	 *
	 * This callback *must not* free the memory containing `evt` while
	 * `aem_poll_poll(p)` is active.  Use <aem/pmcrcu.h> or a real RCU
	 * implementation to free objects outside of `aem_poll_poll`.
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
};

void aem_poll_init(struct aem_poll *p);
void aem_poll_dtor(struct aem_poll *p);

ssize_t aem_poll_add(struct aem_poll *p, struct aem_poll_event *evt);
int aem_poll_del(struct aem_poll *p, struct aem_poll_event *evt);
void aem_poll_mod(struct aem_poll *p, struct aem_poll_event *evt);

struct pollfd *aem_poll_get_pollfd(struct aem_poll *p, struct aem_poll_event *evt);

int aem_poll_poll(struct aem_poll *p);
//struct aem_poll_event *aem_poll_next(struct aem_poll *p);

#endif /* AEM_POLL_H */
