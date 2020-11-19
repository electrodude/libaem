#ifndef AEM_ITER_GEN_H
#define AEM_ITER_GEN_H

/* Recursive data structure iteration helper
 *
 * Make any recursive structs you wish to iterate over contain an instance of
 * `struct aem_iter_gen`, and initialize it with
 * `aem_iter_gen_init(&your_obj->iter)`.  For each recursive data structure you
 * have, you should have a global master instance of `struct iter_gen`,
 * initialized with `aem_iter_gen_init-master(&your_master)`.  Before iterating
 * over a data structure, call `aem_iter_gen_reset_master(&your_master)`.  Each
 * time you come across a node while iterating over your data structure, call
 * `aem_iter_gen_id(&your_obj->iter, &your_master)` on it.  If it has not yet
 * been visited since the last call to
 * `aem_iter_gen_reset_master(&your_master)`, it will return an identifier
 * number >= 0 unique since the last call to `aem_iter_gen_master_reset`.
 *
 * Be careful that you only ever use one master `struct aem_iter_gen` for any
 * given data structure.  Otherwise, you may miss or repeat elements.
 *
 * Two separate iterations (e.g. in different threads) must not be performed
 * over the same data structure at the same time; otherwise, elements will be
 * missed, and your program might even get crash in infinite recursion or
 * otherwise.
 */

struct aem_iter_gen {
	int gen;
	int id;
};

static inline void aem_iter_gen_init_master(struct aem_iter_gen *master)
{
	master->gen = 0;
	master->id = 0;
}

static inline void aem_iter_gen_init(struct aem_iter_gen *obj, struct aem_iter_gen *master)
{
	obj->gen = master->gen;
	obj->id = master->id++;
}

static inline void aem_iter_gen_reset_master(struct aem_iter_gen *master)
{
	master->gen++;
	master->id = 0;
}

//#define aem_iter_gen_master_reset aem_iter_gen_reset_master

static inline int aem_iter_gen_hit(struct aem_iter_gen *obj, struct aem_iter_gen *master)
{
	return obj->gen == master->gen;
}

static inline int aem_iter_gen_id(struct aem_iter_gen *obj, struct aem_iter_gen *master)
{
	if (obj->gen == master->gen)
		return -1;

	obj->gen = master->gen;

	obj->id = master->id++;

	return obj->id;
}

#endif /* AEM_ITER_GEN_H */
