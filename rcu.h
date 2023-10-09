#ifndef AEM_RCU_H
#define AEM_RCU_H

// Use liburcu if requested
#ifdef AEM_HAVE_URCU
# define AEM_FOUND_RCU_IMPL
# include <urcu.h>
#endif

// If we couldn't find a real RCU implementation, use our dumb single-threaded version.
#ifndef AEM_FOUND_RCU_IMPL
# define AEM_FOUND_RCU_IMPL

# include <aem/pmcrcu.h>

# define rcu_head aem_pmcrcu_rcu_head
# define call_rcu aem_pmcrcu_call_rcu
# define rcu_barrier aem_pmcrcu_rcu_barrier
# define synchronize_rcu aem_pmcrcu_rcu_barrier

static inline void aem_pmcrcu_dummy(void) {}

# define rcu_init() aem_pmcrcu_dummy()
# define rcu_register_thread() aem_pmcrcu_dummy()
# define rcu_unregister_thread() aem_pmcrcu_dummy()

#endif

#endif /* AEM_RCU_H */
