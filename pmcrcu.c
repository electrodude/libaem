//#define AEM_PMCRCU_DLFCN

#ifdef AEM_PMCRCU_DLFCN
# define _GNU_SOURCE
# include <dlfcn.h>
#endif

#define AEM_INTERNAL
#include <aem/linked_list.h>
#include <aem/log.h>

#include "pmcrcu.h"

static struct aem_pmcrcu_rcu_head *aem_pmcrcu_head = NULL;

void aem_pmcrcu_call_rcu(struct aem_pmcrcu_rcu_head *head,
	      void (*func)(struct aem_pmcrcu_rcu_head *head))
{
	aem_assert(head);
	aem_assert(func);

	head->func = func;

#ifdef AEM_DEBUG
	// Make sure this rcu_head isn't already enqueued.
	AEM_LL_FOR_RANGE(curr, aem_pmcrcu_head, NULL, next) {
		aem_assert(curr != head);
	}
#endif

	head->next = aem_pmcrcu_head;
	aem_pmcrcu_head = head;
}

static int aem_pmcrcu_process_one_callback(void)
{
	struct aem_pmcrcu_rcu_head *head = aem_pmcrcu_head;
	if (!head)
		return -1;

	aem_assert(head != head->next);
	aem_pmcrcu_head = head->next;

#ifdef AEM_PMCRCU_DLFCN
	{
		Dl_info info;
		if (dladdr(head->func, &info)) {
			//aem_logf_ctx(AEM_LOG_DEBUG, "rcu callback: obj %p, method %s from %s", head, info.dli_sname, info.dli_fname);
			aem_logf_ctx(AEM_LOG_DEBUG, "rcu callback: %s(%p)", info.dli_sname, head);
		} else {
			aem_logf_ctx(AEM_LOG_DEBUG, "rcu callback: ((void(*)(struct rcu_head*))%p)(%p)", head->func, head);
		}
	}
#endif

	aem_assert(head->func);
	head->func(head);

	return 0;
}

void aem_pmcrcu_rcu_barrier(void)
{
	while (aem_pmcrcu_head) {
		aem_pmcrcu_process_one_callback();
	}
}
