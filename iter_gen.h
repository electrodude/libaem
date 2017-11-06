#ifndef AEM_ITER_GEN_H
#define AEM_ITER_GEN_H

/* Recursive data structure iteration helper
 *
 * Make any recursive structs you wish to iterate over contain an instance of
 * iter_gen.  For each recursive data structure you have, you should have a
 * global master instance of iter_gen.  Before iterating over a data structure,
 * call iter_gen_master_reset(&your_master).  Each time you come across a node
 * while iterating over your data structure, call
 * iter_gen_id(&your_obj->iter, &your_master) on it.  If it has not yet been
 * visited since the last call to iter_gen_master_reset(&your_master), it will
 * return an identifier number >= 0 unique since the last call to
 * iter_gen_master_reset.
 *
 * Be careful that you only ever use one master struct iter_gen for any given
 * data structure.  Otherwise, you will miss elements.
 *
 * Two separate iterations (e.g. in different threads) must not be performed
 * over the same data structure at the same time; otherwise, elements will be
 * missed, and your program might even get crash in infinite recursion or
 * otherwise.
 */

struct aem_iter_gen
{
	int gen;
	int id;

	void *ptr;
};

#define iter_gen aem_iter_gen

static inline void iter_gen_master_reset(struct aem_iter_gen *master)
{
	master->gen++;
	master->id = 0;
}

static inline int iter_gen_id(struct aem_iter_gen *obj, struct aem_iter_gen *master)
{
	if (obj->gen == master->gen) return -1;

	obj->gen = master->gen;

	obj->id = master->id++;

	return obj->id;
}

#endif /* AEM_ITER_GEN_H */
