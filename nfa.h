#ifndef AEM_NFA_H
#define AEM_NFA_H

#include <stdint.h>

#include <aem/enum.h>
#include <aem/stringslice.h>

// TODO: Make these three always enabled for aem_nfa_run, and make
// a second, faster (, caching?) version of aem_nfa_run that doesn't do them.
#define AEM_NFA_CAPTURES 1
#define AEM_NFA_TRACING 1

// Required for captures or tracing to do anything meaningful.
#define AEM_NFA_THREAD_STATE 1


/// NFA definition
#define AEM_NFA_OP_DEFINE(def) \
	def(AEM_NFA_RANGE, range) \
	def(AEM_NFA_CLASS, class) \
	def(AEM_NFA_CAPTURE, capture) \
	def(AEM_NFA_MATCH, match) \
	def(AEM_NFA_JMP, jmp) \
	def(AEM_NFA_FORK, fork)

AEM_ENUM_DECLARE(aem_nfa_op, AEM_NFA_OP)
const char *aem_nfa_op_name(enum aem_nfa_op op);

#define AEM_NFA_CCLASS_DEFINE(def) \
	/* ctype classes */ \
	def(AEM_NFA_CCLASS_ALNUM, alnum) \
	def(AEM_NFA_CCLASS_ALPHA, alpha) \
	def(AEM_NFA_CCLASS_BLANK, blank) \
	def(AEM_NFA_CCLASS_CNTRL, cntrl) \
	def(AEM_NFA_CCLASS_DIGIT, digit) \
	def(AEM_NFA_CCLASS_GRAPH, graph) \
	def(AEM_NFA_CCLASS_LOWER, lower) \
	def(AEM_NFA_CCLASS_PRINT, print) \
	def(AEM_NFA_CCLASS_PUNCT, punct) \
	def(AEM_NFA_CCLASS_SPACE, space) \
	def(AEM_NFA_CCLASS_UPPER, upper) \
	def(AEM_NFA_CCLASS_XDIGIT, xdigit) \
	/* custom */ \
	def(AEM_NFA_CCLASS_ANY, any) \
	def(AEM_NFA_CCLASS_LINE, line) /* for '^', '$', and '.' */

AEM_ENUM_DECLARE(aem_nfa_cclass, AEM_NFA_CCLASS)
const char *aem_nfa_cclass_name(enum aem_nfa_cclass cclass);
int aem_nfa_cclass_match(int neg, enum aem_nfa_cclass cclass, int c);

typedef uint32_t aem_nfa_insn;

typedef uint32_t aem_nfa_bitfield;

enum aem_nfa_thr_state {
	AEM_NFA_THR_LIVE,
	AEM_NFA_THR_DEAD,
	AEM_NFA_THR_MATCHED,
};

struct aem_nfa_trace_info {
	struct aem_stringslice where;
	int match;
};
struct aem_nfa {
	aem_nfa_insn *pgm;
	size_t n_insns;
	size_t alloc_insns;

	size_t n_captures;
	struct aem_nfa_trace_info *trace_dbg;

	aem_nfa_bitfield *thr_init;
	size_t alloc_bitfields;

	int n_matches;
};

#define AEM_NFA_EMPTY ((struct aem_nfa){0})
static inline struct aem_nfa *aem_nfa_init(struct aem_nfa *nfa)
{
	*nfa = AEM_NFA_EMPTY;
	return nfa;
}
void aem_nfa_dtor(struct aem_nfa *nfa);
struct aem_nfa *aem_nfa_dup(struct aem_nfa *dst, const struct aem_nfa *src);


/// NFA construction
// You probably don't want to call these if you aren't regex.c
size_t aem_nfa_put_insn(struct aem_nfa *nfa, size_t i, aem_nfa_insn insn);
size_t aem_nfa_append_insn(struct aem_nfa *nfa, aem_nfa_insn insn);
void aem_nfa_set_dbg(struct aem_nfa *nfa, size_t i, struct aem_stringslice dbg, int match);
aem_nfa_insn aem_nfa_insn_range(uint32_t lo, uint32_t hi);
aem_nfa_insn aem_nfa_insn_char(uint32_t c);
aem_nfa_insn aem_nfa_insn_class(unsigned int neg, unsigned int frontier, enum aem_nfa_cclass cclass);
aem_nfa_insn aem_nfa_insn_capture(unsigned int end, size_t n);
aem_nfa_insn aem_nfa_insn_match(int match);
aem_nfa_insn aem_nfa_insn_jmp(size_t pc);
aem_nfa_insn aem_nfa_insn_fork(size_t pc);

void aem_nfa_optimize(struct aem_nfa *nfa);


/// NFA inspection
void aem_nfa_disas(struct aem_stringbuf *out, const struct aem_nfa *nfa, const uint32_t *marks);


/// NFA engine
// Returns -1 if no match, match ID >= 0 if match, or < -1 on error.
struct aem_nfa_match {
	struct aem_stringslice *captures;
	aem_nfa_bitfield *visited;
	int match;

	size_t n_insns;
	size_t n_captures;
};
void aem_nfa_match_dtor(struct aem_nfa_match *match);
int aem_nfa_run(const struct aem_nfa *nfa, struct aem_stringslice *in, struct aem_nfa_match *match_p);

#endif /* AEM_NFA_H */
