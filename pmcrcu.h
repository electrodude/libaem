#ifndef AEM_PMCRCU_H
#define AEM_PMCRCU_H

#if !defined(AEM_FOUND_RCU_IMPL) && !defined(AEM_INTERNAL)
# warning Do not include this header directly; include <aem/rcu.h> instead.
#endif

/// Poor man's call_rcu
// Compatible with liburcu's call_rcu, but *NOT SUITABLE* for use with
// multi-threaded programs.

struct aem_pmcrcu_rcu_head {
	struct aem_pmcrcu_rcu_head *next;
	void (*func)(struct aem_pmcrcu_rcu_head *head);
};

void aem_pmcrcu_call_rcu(struct aem_pmcrcu_rcu_head *head,
                         void (*func)(struct aem_pmcrcu_rcu_head *head));

void aem_pmcrcu_rcu_barrier(void);

#endif /* AEM_PMCRCU_H */
