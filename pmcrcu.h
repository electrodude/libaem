#ifndef AEM_PMCRCU_H
#define AEM_PMCRCU_H

/// Poor man's call_rcu
// Compatible with liburcu's call_rcu, but *NOT SUITABLE* for use with
// multi-threaded programs.

// TODO: If we detect a real RCU implementation, either make compilation fail or ignore the rest of this file.

struct aem_pmcrcu_rcu_head {
	struct aem_pmcrcu_rcu_head *next;
	void (*func)(struct aem_pmcrcu_rcu_head *head);
};

void aem_pmcrcu_call_rcu(struct aem_pmcrcu_rcu_head *head,
                         void (*func)(struct aem_pmcrcu_rcu_head *head));

void aem_pmcrcu_rcu_barrier(void);


#define rcu_head aem_pmcrcu_rcu_head
#define call_rcu aem_pmcrcu_call_rcu
#define rcu_barrier aem_pmcrcu_rcu_barrier
#define synchronize_rcu aem_pmcrcu_rcu_barrier

#endif /* AEM_PMCRCU_H */
