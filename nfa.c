#include <alloca.h>
#include <ctype.h>
#include <errno.h>

#define AEM_INTERNAL
#include <aem/ansi-term.h>
#include <aem/log.h>
#include <aem/memory.h>
#include <aem/nfa-util.h>
#include <aem/stack.h>
#include <aem/stringbuf.h>
// for AEM_NFA_CAPTURES
#include <aem/translate.h>

#include "nfa.h"

/// Helpers
static void bitfield_set(aem_nfa_bitfield *bf, size_t i)
{
	aem_assert(bf);
	aem_nfa_bitfield mask = 1 << (i & 0x1f);
	bf[i >> 5] |= mask;
}
static void bitfield_clear(aem_nfa_bitfield *bf, size_t i)
{
	aem_assert(bf);
	aem_nfa_bitfield mask = 1 << (i & 0x1f);
	bf[i >> 5] &= ~mask;
}
static int bitfield_test(const aem_nfa_bitfield *bf, size_t i)
{
	aem_assert(bf);
	aem_nfa_bitfield mask = 1 << (i & 0x1f);
	return (bf[i >> 5] & mask) > 0;
}


/// NFA helpers
AEM_ENUM_DEFINE(aem_nfa_op, AEM_NFA_OP)
AEM_ENUM_DEFINE(aem_nfa_cclass, AEM_NFA_CCLASS)

int aem_nfa_cclass_match(int neg, enum aem_nfa_cclass cclass, int c)
{
	int match = 0;
	if (0 <= c && c < 0x80) {
		// Only used for 7-bit ASCII.
		// Anything outside of this range doesn't match.
		switch (cclass) {
			// ctype classes
			case AEM_NFA_CCLASS_ALNUM : match = isalnum (c); break;
			case AEM_NFA_CCLASS_ALPHA : match = isalpha (c); break;
			case AEM_NFA_CCLASS_BLANK : match = isblank (c); break;
			case AEM_NFA_CCLASS_CNTRL : match = iscntrl (c); break;
			case AEM_NFA_CCLASS_DIGIT : match = isdigit (c); break;
			case AEM_NFA_CCLASS_GRAPH : match = isgraph (c); break;
			case AEM_NFA_CCLASS_LOWER : match = islower (c); break;
			case AEM_NFA_CCLASS_PRINT : match = isprint (c); break;
			case AEM_NFA_CCLASS_PUNCT : match = ispunct (c); break;
			case AEM_NFA_CCLASS_SPACE : match = isspace (c); break;
			case AEM_NFA_CCLASS_UPPER : match = isupper (c); break;
			case AEM_NFA_CCLASS_XDIGIT: match = isxdigit(c); break;
			// custom
			case AEM_NFA_CCLASS_ANY   : match = 1                    ; break;
			case AEM_NFA_CCLASS_LINE  : match = c >= ' ' || c == '\t'; break;
			default                   : match = 0                    ; break;
		}
	}

	return neg ? !match : match;
}


/// NFA definition
void aem_nfa_dtor(struct aem_nfa *nfa)
{
	if (!nfa)
		return;

	free(nfa->pgm);
	free(nfa->thr_init);
	free(nfa->trace_dbg);
}

// TODO: test
struct aem_nfa *aem_nfa_dup(struct aem_nfa *dst, const struct aem_nfa *src)
{
	aem_assert(dst);

	aem_nfa_init(dst);

	if (!src)
		return dst;

	dst->n_insns = src->n_insns;
	dst->alloc_insns = src->alloc_insns;
	aem_assert(!AEM_ARRAY_RESIZE(dst->pgm, dst->alloc_insns));
	for (size_t i = 0; i < dst->alloc_insns; i++) {
		dst->pgm[i] = src->pgm[i];
	}

	dst->n_captures = src->n_captures;

	dst->alloc_bitfields = src->alloc_bitfields;
	aem_assert(!AEM_ARRAY_RESIZE(dst->thr_init, dst->alloc_bitfields));
	for (size_t i = 0; i < dst->alloc_bitfields; i++) {
		dst->thr_init[i] = src->thr_init[i];
	}

	aem_assert(!AEM_ARRAY_RESIZE(dst->trace_dbg, dst->alloc_insns));
	for (size_t i = 0; i < dst->alloc_insns; i++) {
		dst->trace_dbg[i] = src->trace_dbg[i];
	}

	return dst;
}

size_t aem_nfa_put_insn(struct aem_nfa *nfa, size_t i, aem_nfa_insn insn)
{
	aem_assert(nfa);

	if (i+1 >= nfa->n_insns) {
		nfa->n_insns = i+1;
		size_t alloc_insns = nfa->alloc_insns;
		int rc = AEM_ARRAY_GROW(nfa->pgm, nfa->n_insns, nfa->alloc_insns);
		aem_assert(rc >= 0);
		if (rc)
			aem_assert(!AEM_ARRAY_RESIZE(nfa->trace_dbg, nfa->alloc_insns));
		for (size_t i = alloc_insns; i < nfa->alloc_insns; i++) {
			nfa->pgm[i] = aem_nfa_insn_match(-1);
			nfa->trace_dbg[i] = (struct aem_nfa_trace_info){.where = AEM_STRINGSLICE_EMPTY, .match = -1};
		}

		// Resize bitfields
		size_t list_32 = (nfa->n_insns + 31) >> 5;
		if (list_32 > nfa->alloc_bitfields) {
			size_t alloc_new = nfa->alloc_bitfields*2;
			if (alloc_new < list_32)
				alloc_new = list_32+1;
			aem_assert(!AEM_ARRAY_RESIZE(nfa->thr_init, alloc_new));
			for (size_t i = nfa->alloc_bitfields; i < alloc_new; i++) {
				nfa->thr_init[i] = 0;
			}
			nfa->alloc_bitfields = alloc_new;
		}
	}

	nfa->pgm[i] = insn;
	//nfa->trace_dbg[i] = (struct aem_nfa_trace_info){.where = AEM_STRINGSLICE_EMPTY, .match = -1};

	return i;
}
size_t aem_nfa_append_insn(struct aem_nfa *nfa, aem_nfa_insn insn)
{
	aem_assert(nfa);

	return aem_nfa_put_insn(nfa, nfa->n_insns, insn);
}
void aem_nfa_set_dbg(struct aem_nfa *nfa, size_t i, struct aem_stringslice where, int match)
{
	aem_assert(nfa);

	if (i >= nfa->n_insns) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid insn: %zx/%zx", i, nfa->n_insns);
		return;
	}

	nfa->trace_dbg[i] = (struct aem_nfa_trace_info){.where = where, .match = match};
}

#define AEM_NFA_OP_LEN 3
AEM_STATIC_ASSERT(AEM_NFA_OP_MAX <= (1 << AEM_NFA_OP_LEN), "AEM_NFA_OP_LEN not big enough!");
static aem_nfa_insn aem_nfa_mk_insn(enum aem_nfa_op op, aem_nfa_insn arg)
{
	if (op >> AEM_NFA_OP_LEN) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid op: %x", op);
	}
	return (arg << AEM_NFA_OP_LEN) | op;
}

aem_nfa_insn aem_nfa_insn_range(uint32_t lo, uint32_t hi)
{
	if (hi < lo) {
		aem_logf_ctx(AEM_LOG_BUG, "Nonsensical range: hi %#02x < lo %#02x ", hi, lo);
	}
	if (lo >> 8) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid lo: %#02x", lo);
		lo = 0xff;
	}
	if (hi >> 8) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid hi: %#02x", hi);
		hi = 0xff;
	}
	return aem_nfa_mk_insn(AEM_NFA_RANGE, ((aem_nfa_insn)hi << 8) | (aem_nfa_insn)lo);
}
aem_nfa_insn aem_nfa_insn_char(uint32_t c)
{
	return aem_nfa_insn_range(c, c);
}

aem_nfa_insn aem_nfa_insn_class(unsigned int neg, unsigned int frontier, enum aem_nfa_cclass cclass)
{
	if (neg >> 1) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid neg: %02x", neg);
	}
	if (frontier >> 1) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid frontier: %02x", frontier);
	}
	return aem_nfa_mk_insn(AEM_NFA_CLASS, (cclass << 2) | (frontier << 1) | neg);
}

aem_nfa_insn aem_nfa_insn_capture(unsigned int end, size_t n)
{
	/*
	aem_assert(nfa);
	if (n >= nfa->n_captures) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid capture: %zd/%zd", n, nfa->n_captures);
		return -1;
	}
	*/
	if (end >> 1) {
		aem_logf_ctx(AEM_LOG_BUG, "Invalid end: %x", end);
	}
	return aem_nfa_mk_insn(AEM_NFA_CAPTURE, (n << 1) | end);
}

aem_nfa_insn aem_nfa_insn_match(int match)
{
	return aem_nfa_mk_insn(AEM_NFA_MATCH, match);
}

aem_nfa_insn aem_nfa_insn_jmp(size_t pc)
{
	return aem_nfa_mk_insn(AEM_NFA_JMP, pc);
}

aem_nfa_insn aem_nfa_insn_fork(size_t pc)
{
	return aem_nfa_mk_insn(AEM_NFA_FORK, pc);
}

static void aem_nfa_mark_reachable(const struct aem_nfa *nfa, aem_nfa_bitfield *reachable, size_t pc)
{
	while (pc < nfa->n_insns && !bitfield_test(reachable, pc)) {
		bitfield_set(reachable, pc);

		// Decode instruction
		aem_nfa_insn insn = nfa->pgm[pc++];
		enum aem_nfa_op op = insn & ((1 << AEM_NFA_OP_LEN) - 1);
		insn >>= AEM_NFA_OP_LEN;

		switch (op) {
		case AEM_NFA_JMP:
			pc = insn;
			break;
		case AEM_NFA_FORK:
			aem_nfa_mark_reachable(nfa, reachable, insn);
			break;
		case AEM_NFA_MATCH:
			return;
		default:
			// do nothing
			break;
		}
	}
}
void aem_nfa_optimize(struct aem_nfa *nfa)
{
	aem_assert(nfa);

	size_t list_32 = (nfa->n_insns + 31) >> 5;

#if 1
	/// Thread chains of JMPs straight to the end
	for (size_t pc = 0; pc < nfa->n_insns; pc++) {
		// Decode instruction
		aem_nfa_insn insn = nfa->pgm[pc];
		enum aem_nfa_op op = insn & ((1 << AEM_NFA_OP_LEN) - 1);
		insn >>= AEM_NFA_OP_LEN;

		switch (op) {
		case AEM_NFA_JMP:
		case AEM_NFA_FORK: {
			size_t dst = insn;
			int loop = 0; // got stuck in a loop
			for (size_t i = 0; i < nfa->n_insns || ((loop = 1), 0); i++) {
				size_t pc_next = dst;
				if (pc_next >= nfa->n_insns) {
					aem_logf_ctx(AEM_LOG_BUG, "Invalid pc: %zx/%zx", pc_next, nfa->n_insns);
					break;
				}
				aem_nfa_insn insn2 = nfa->pgm[pc_next];
				enum aem_nfa_op op2 = insn2 & ((1 << AEM_NFA_OP_LEN) - 1);
				insn2 >>= AEM_NFA_OP_LEN;
				if (op2 == AEM_NFA_JMP) {
					dst = insn2;
				} else {
					break;
				}
			}
			if (loop) {
				aem_logf_ctx(AEM_LOG_BUG, "loop of JMPs @ %zx", pc);
				break;
			}
			if (dst != insn) {
				aem_logf_ctx(AEM_LOG_DEBUG, "thread %zx %s %zx -> %zx", pc, aem_nfa_op_name(op), insn, dst);
				aem_nfa_put_insn(nfa, pc, aem_nfa_mk_insn(op, dst));
			}
			break;
		}
		default:
			// do nothing
			break;
		}
	}
#endif

	/// TODO: Replace ranges with character classes when possible

#if 0
	/// TODO: Merge common prefixes
	for (size_t pc1 = 0; pc1 < nfa->n_insns; pc1++) {
		// Skip non-initial instructions
		if (!bitfield_test(nfa->thr_init, pc1))
			continue;

		aem_nfa_insn insn1 = nfa->pgm[pc1];
		enum aem_nfa_op op1 = insn1 & ((1 << AEM_NFA_OP_LEN) - 1);
		insn1 >>= AEM_NFA_OP_LEN;

		for (size_t pc2 = pc1 + 1; pc2 < nfa->n_insns; pc2++) {
			if (!bitfield_test(nfa->thr_init, pc2))
				continue;

			aem_nfa_insn insn2 = nfa->pgm[pc2];
			enum aem_nfa_op op2 = insn2 & ((1 << AEM_NFA_OP_LEN) - 1);
			insn2 >>= AEM_NFA_OP_LEN;

			if (op1 == op2 && insn1 == insn2) {
				aem_logf_ctx(AEM_LOG_NOTICE, "Potential optimization: pcs %zx and %zx", pc1, pc2);
			}
		}
	}
#endif

#if 1
	/// Split initial forks
	for (size_t pc = 0; pc < nfa->n_insns; pc++) {
		// Skip non-initial instructions
		if (!bitfield_test(nfa->thr_init, pc))
			continue;

		// Decode instruction
		aem_nfa_insn insn = nfa->pgm[pc];
		enum aem_nfa_op op = insn & ((1 << AEM_NFA_OP_LEN) - 1);
		insn >>= AEM_NFA_OP_LEN;

		switch (op) {
		case AEM_NFA_FORK: {
			size_t pc_next = insn;
			if (pc_next >= nfa->n_insns) {
				aem_logf_ctx(AEM_LOG_BUG, "Invalid pc: %zx/%zx", pc_next, nfa->n_insns);
				break;
			}
			aem_logf_ctx(AEM_LOG_DEBUG, "split initial %zx fork %zx", pc, pc_next);
			bitfield_clear(nfa->thr_init, pc);
			bitfield_set(nfa->thr_init, pc+1);
			bitfield_set(nfa->thr_init, pc_next);
			break;
		}
		default:
			// do nothing
			break;
		}
	}
#endif

#if 1
	/// Find unreachable instructions
	aem_nfa_bitfield *reachable = alloca(list_32 * sizeof(*reachable));
	for (size_t i = 0; i < list_32; i++) {
		reachable[i] = 0;
	}
	// Mark all initial children and their children, recursively
	for (size_t pc = 0; pc < nfa->n_insns; pc++) {
		// Skip non-initial instructions
		if (!bitfield_test(nfa->thr_init, pc))
			continue;

		aem_nfa_mark_reachable(nfa, reachable, pc);
	}
	// Complain about unreachable instructions
	for (size_t pc = 0; pc < nfa->n_insns; pc++) {
		// Skip reachable instructions
		if (bitfield_test(reachable, pc))
			continue;

		// Decode instruction
		aem_nfa_insn insn = nfa->pgm[pc];
		enum aem_nfa_op op = insn & ((1 << AEM_NFA_OP_LEN) - 1);
		insn >>= AEM_NFA_OP_LEN;

		aem_logf_ctx(AEM_LOG_DEBUG, "unreachable: %zx %s %zx", pc, aem_nfa_op_name(op), insn);
		//aem_nfa_put_insn(nfa, pc, (1 << AEM_NFA_OP_LEN) - 1);
		struct aem_nfa_trace_info *dbg = &nfa->trace_dbg[pc];
		aem_nfa_set_dbg(nfa, pc, aem_stringslice_new_cstr("unreachable"), dbg->match);
	}
#endif
}


/// NFA inspection
void aem_nfa_disas(struct aem_stringbuf *out, const struct aem_nfa *nfa, const aem_nfa_bitfield *marks)
{
	aem_assert(out);
	aem_assert(nfa);

	unsigned int pc_width = 0;
	for (size_t n = nfa->n_insns; n; n /= 16)
		pc_width++;

	unsigned int match_width = 0;
	for (int n = nfa->n_matches; n > 0; n /= 10)
		match_width++;

#if 0
	char *jump_count = alloca(nfa->n_insns * sizeof(*jump_count));
	//wrong char (*jump_str)[16] = alloca(nfa->n_insns * sizeof(*jump_str));
	for (size_t pc = 0; pc < nfa->n_insns; pc++) {
		jump_count[pc] = 0;
		//for (size_t i = 0; i < 16; i++)
		//	jump_str[pc][i] = ' ';
	}
	*/

	size_t jump_count_max = 0;
	for (size_t pc = 0; pc < nfa->n_insns; pc++) {
		// Decode instruction
		aem_nfa_insn insn = nfa->pgm[pc];
		enum aem_nfa_op op = insn & ((1 << AEM_NFA_OP_LEN) - 1);
		insn >>= AEM_NFA_OP_LEN;

		switch (op) {
		case AEM_NFA_JMP:
		case AEM_NFA_FORK:
		{
			size_t pc_next = insn;
			for (size_t pc2 = pc; pc != pc_next; pc += (pc_next > pc ? 1 : -1)) {
				jump_count[pc2]++;
			}
			jump_count[pc_next]++;
			break;
		}
		default:
			// do nothing
			break;
		}
	}
#endif

	for (size_t pc = 0; pc < nfa->n_insns; pc++) {
		// Decode instruction
		aem_nfa_insn insn = nfa->pgm[pc];
		enum aem_nfa_op op = insn & ((1 << AEM_NFA_OP_LEN) - 1);
		insn >>= AEM_NFA_OP_LEN;

		// Record start of line
		size_t line_start = out->n;

		// Check mark
		const char *mark = marks && bitfield_test(marks, pc) ? ">" : " ";

		aem_stringbuf_printf(out, "%s %0*zx: ", mark, pc_width, pc);
		size_t op_start = out->n;
		const char *op_name = aem_nfa_op_name(op);
		if (op_name) {
			aem_stringbuf_puts(out, AEM_SGR("96"));
		} else {
			op_name = "invalid";
			aem_stringbuf_puts(out, AEM_SGR("91"));
		}
		aem_stringbuf_puts(out, op_name);
		aem_stringbuf_puts(out, AEM_SGR("0"));

		// Pad to widest instruction
		aem_ansi_pad(out, op_start, 8);

		switch (op) {
		case AEM_NFA_RANGE: {
			uint8_t lo =  insn       & 0xff;
			uint8_t hi = (insn >> 8) & 0xff;
			aem_nfa_desc_range(out, lo, hi);
			break;
		}
		case AEM_NFA_CLASS: {
			int neg = insn & 0x1;
			int frontier = insn & 0x2;
			if (frontier || neg) {
				aem_stringbuf_puts(out, AEM_SGR("95"));
				if (frontier)
					aem_stringbuf_puts(out, ">");
				if (neg)
					aem_stringbuf_puts(out, "!");
				aem_stringbuf_puts(out, AEM_SGR("0"));
			}
			enum aem_nfa_cclass cclass = insn >> 2;
			const char *name = aem_nfa_cclass_name(cclass);
			if (name) {
				aem_stringbuf_puts(out, name);
			} else {
				aem_stringbuf_printf(out, AEM_SGR("91") "<%#x>" AEM_SGR("0"), cclass);
			}
			break;
		}

		case AEM_NFA_CAPTURE: {
			int end = insn & 0x1;
			insn >>= 1;
			aem_stringbuf_printf(out, "%s %zx ", end ? "end" : "start", insn);
			break;
		}

		case AEM_NFA_MATCH:
			aem_stringbuf_printf(out, "%zx", insn);
			break;

		case AEM_NFA_JMP:
		case AEM_NFA_FORK:
		{
			size_t pc_next = insn;
			aem_stringbuf_printf(out, "%zx", pc_next);
			break;
		}

		default:
			aem_stringbuf_printf(out, "op %x %zx", op, insn);
			break;
		}

		// Get tracing information
		struct aem_nfa_trace_info *dbg = &nfa->trace_dbg[pc];

		aem_ansi_pad(out, line_start, 40);
		aem_stringbuf_printf(out, "%*d", match_width, dbg->match);
		if (aem_stringslice_ok(dbg->where)) {
			aem_stringbuf_puts(out, "  ");
			aem_stringbuf_putss(out, dbg->where);
		}

		aem_stringbuf_puts(out, AEM_SGR("0") "\n");
	}
}


/// NFA engine

// Shared state of one call to aem_nfa_run
struct aem_nfa_run {
	struct aem_stack curr;
	struct aem_stack next;
	struct aem_stringslice in_curr;
	struct aem_stringslice longest_match;
	const char *p_curr;
	const struct aem_nfa *nfa;
	aem_nfa_bitfield *map_curr; // Needs to be run on this character, or already was
	aem_nfa_bitfield *map_next; // Needs to be run on next character

	// We store copies of these here in case another OS thread expands the
	// NFA program while we're running.  But this isn't sufficient - what
	// if nfa->pgm gets realloc()'d?  We'd need RCU or something scary.
	// If this sort of thread-safety is useless or too complicated, we
	// should probably remove this.
	size_t n_insns;
	size_t n_captures;

	int c;
	int c_prev;
};

struct aem_nfa_thread {
	size_t pc;
	struct aem_nfa_match match;
};
static struct aem_nfa_thread *aem_nfa_thread_init(struct aem_nfa_thread *thr, const struct aem_nfa_run *run, size_t pc)
{
	aem_assert(thr);
	aem_assert(run);

	thr->pc = pc;
#if AEM_NFA_CAPTURES
	thr->match.captures = malloc(run->n_captures * sizeof(*thr->match.captures));
	aem_assert(thr->match.captures);
	// Clear all captures
	for (size_t i = 0; i < run->n_captures; i++) {
		thr->match.captures[i] = AEM_STRINGSLICE_EMPTY;
	}
#else
	thr->match.captures = NULL;
#endif
#if AEM_NFA_TRACING
	size_t list_32 = (run->n_insns + 31) >> 5;
	thr->match.visited = malloc(list_32 * sizeof(*thr->match.visited));
	aem_assert(thr->match.visited);
	for (size_t i = 0; i < list_32; i++) {
		thr->match.visited[i] = 0;
	}
#else
	thr->match.visited = NULL;
#endif
	thr->match.match = -1;

	return thr;
}
static void aem_nfa_thread_dtor(struct aem_nfa_thread *thr)
{
	if (!thr)
		return;

	aem_nfa_match_dtor(&thr->match);
}
static struct aem_nfa_thread *aem_nfa_thread_new(const struct aem_nfa_run *run, size_t pc)
{
	struct aem_nfa_thread *thr = malloc(sizeof(*thr));
	if (!thr) {
		aem_logf_ctx(AEM_LOG_ERROR, "malloc() failed: %s", strerror(errno));
		return NULL;
	}

	return aem_nfa_thread_init(thr, run, pc);
}
static void aem_nfa_thread_free(struct aem_nfa_thread *thr)
{
	if (!thr)
		return;

	aem_nfa_thread_dtor(thr);

	free(thr);
}

static void aem_nfa_thread_add(struct aem_nfa_run *run, int next, struct aem_nfa_thread *thr)
{
	aem_assert(run);
	aem_assert(thr);

	size_t pc = thr->pc;

	aem_assert(pc < run->n_insns);

	aem_nfa_bitfield *map = next ? run->map_next : run->map_curr;
	struct aem_stack *stk = next ? &run->next : &run->curr;

	// If some other thread already got to this PC first, drop this one in favor of the first.
	if (bitfield_test(map, pc)) {
		//aem_logf_ctx(AEM_LOG_DEBUG3, "dup thread @ %zx", pc);
		aem_nfa_thread_free(thr);
		return;
	}

	// Set bitmap
	bitfield_set(map, pc);

	// Add thread to queue
	aem_stack_push(stk, thr);
}

static inline int aem_nfa_thread_step(struct aem_nfa_run *run, struct aem_nfa_thread *thr, int c)
{
	aem_assert(run);
	aem_assert(thr);
	const struct aem_nfa *nfa = run->nfa;
	aem_assert(nfa);

#if AEM_NFA_TRACING
	size_t list_32 = (run->n_insns + 31) >> 5;
#endif

	for (;;) {
		if (thr->pc >= run->n_insns) {
			aem_logf_ctx(AEM_LOG_BUG, "Invalid pc: %zx/%zx", thr->pc, run->n_insns);
			return -2;
		}
		// Ignore new threads on instructions that were already active this character.
		if (bitfield_test(run->map_curr, thr->pc)) {
			// Thread is a duplicate; remove
			goto dead;
		}
		bitfield_set(run->map_curr, thr->pc);

#if AEM_NFA_TRACING
		size_t pc_curr = thr->pc;
#endif
		aem_nfa_insn insn = nfa->pgm[thr->pc++];
		enum aem_nfa_op op = insn & ((1 << AEM_NFA_OP_LEN) - 1);
		insn >>= AEM_NFA_OP_LEN;
		switch (op) {
		case AEM_NFA_RANGE: {
			// TODO: Allow chained range instructions, for e.g. [0-9A-Za-z]
			uint8_t lo =  insn       & 0xff;
			uint8_t hi = (insn >> 8) & 0xff;
			AEM_LOG_MULTI(out, AEM_LOG_DEBUG3) {
				aem_stringbuf_puts(out, "range ");
				aem_nfa_desc_range(out, lo, hi);
			}

			// No more input => dead
			if (c < 0)
				goto dead;

			if (!(lo <= c && c <= hi))
				goto dead;
#if AEM_NFA_TRACING
			bitfield_set(thr->match.visited, pc_curr);
#endif
			goto pass;
		}
		case AEM_NFA_CLASS: {
			int neg = insn & 0x1;
			int frontier = insn & 0x2;
			enum aem_nfa_cclass cclass = insn >> 2;
			aem_logf_ctx(AEM_LOG_DEBUG3, "class %s%s%s", frontier ? ">" : "", neg ? "!" : "", aem_nfa_cclass_name(cclass));

			int match = aem_nfa_cclass_match(neg, cclass, c);
			// Frontier: previous character must have not matched
			if (frontier && match)
				match = !aem_nfa_cclass_match(neg, cclass, run->c_prev);

			if (!match)
				goto dead;

#if AEM_NFA_TRACING
			bitfield_set(thr->match.visited, pc_curr);
#endif

			// Frontiers don't consume anything
			if (frontier)
				break;

			goto pass;
		}

		case AEM_NFA_CAPTURE: {
#if AEM_NFA_CAPTURES
			int end = insn & 0x1;
			insn >>= 1;
			if (insn >= run->n_captures) {
				aem_logf_ctx(AEM_LOG_BUG, "Invalid capture: %zx/%zx", insn, run->n_captures);
				return -2;
			}
			aem_logf_ctx(AEM_LOG_DEBUG3, "capture %s %zx", end ? "end" : "start", insn);
			struct aem_stringslice *capture = &thr->match.captures[insn];
			if (end)
				capture->end = run->p_curr;
			else
				capture->start = run->p_curr;
#endif
			break;
		}

		case AEM_NFA_MATCH: {
			aem_logf_ctx(AEM_LOG_DEBUG3, "match %x", insn);
			// Do NOT mark this instruction as visited.

			run->longest_match.end = run->in_curr.start;

			// Return argument of latest match
			return thr->match.match = insn;
		}

		case AEM_NFA_JMP: {
			size_t pc_next = insn;
			aem_logf_ctx(AEM_LOG_DEBUG3, "jmp %x", pc_next);
			if (pc_next >= run->n_insns) {
				aem_logf_ctx(AEM_LOG_BUG, "Invalid pc: %zx/%zx", pc_next, run->n_insns);
				return -2;
			}
			thr->pc = pc_next;
			break;
		}

		case AEM_NFA_FORK: {
			size_t pc_next = insn;
			aem_logf_ctx(AEM_LOG_DEBUG3, "fork %x", pc_next);
			if (pc_next >= run->n_insns) {
				aem_logf_ctx(AEM_LOG_BUG, "Invalid pc: %zx/%zx", pc_next, run->n_insns);
				return -2;
			}
			struct aem_nfa_thread *child = aem_nfa_thread_new(run, pc_next);
			aem_assert(child);
#if AEM_NFA_CAPTURES
			for (size_t i = 0; i < run->n_captures; i++) {
				child->match.captures[i] = thr->match.captures[i];
			}
#endif
#if AEM_NFA_TRACING
			for (size_t i = 0; i < list_32; i++) {
				child->match.visited[i] = thr->match.visited[i];
			}
			bitfield_set(child->match.visited, pc_curr);
#endif
			aem_nfa_thread_add(run, 0, child);
			break;
		}

		default:
			aem_logf_ctx(AEM_LOG_BUG, "Invalid op: %x", op);
			return -2;
		}

#if AEM_NFA_TRACING
		bitfield_set(thr->match.visited, pc_curr);
#endif
	}

	aem_unreachable();


pass:
	aem_nfa_thread_add(run, 1, thr);
	return -1;


dead:
	aem_nfa_thread_free(thr);
	return -1;
}
static int aem_nfa_step(struct aem_nfa_run *run, struct aem_nfa_thread **thr_matched_p, int c)
{
	aem_assert(run);
	aem_assert(thr_matched_p);

	int rc = -1;

	// For each thread
	//aem_logf_ctx(AEM_LOG_DEBUG3, "%zd live threads", run->curr.n);
	AEM_STACK_FOREACH(i, &run->curr) {
		struct aem_nfa_thread *thr = run->curr.s[i];
		aem_assert(thr);
		run->curr.s[i] = NULL;

		//aem_logf_ctx(AEM_LOG_DEBUG3, "thread %zd/%zd @ %zx", i, run->curr.n, thr->pc);

		// Stop blocking this instruction, since we've gotten
		// to the queued thread that's been sitting on it.
		bitfield_clear(run->map_curr, thr->pc);

		int rc2 = aem_nfa_thread_step(run, thr, c);

		// Fatal error
		if (rc2 <= -2)
			return rc2;

		if (rc2 >= 0) {
			if (*thr_matched_p)
				aem_nfa_thread_free(*thr_matched_p);

			// Replace previous candidate
			rc = rc2;
			*thr_matched_p = thr;
			aem_assert(rc == (*thr_matched_p)->match.match);
		}
	}

	return rc;
}

void aem_nfa_show_trace(const struct aem_nfa *nfa, const struct aem_nfa_thread *thr)
{
	aem_assert(nfa);
	aem_assert(thr);
	AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
		aem_stringbuf_puts(out, "Thread match trace:\n");

		// Debug information for pc-1, the MATCH instruction, should
		// contain the complete regex.
		size_t match_pc = thr->pc - 1;
		aem_nfa_insn insn = nfa->pgm[match_pc];
		const struct aem_nfa_trace_info *regex = &nfa->trace_dbg[match_pc];

		enum aem_nfa_op op = insn & ((1 << AEM_NFA_OP_LEN) - 1);
		if (op != AEM_NFA_MATCH) {
			aem_stringbuf_printf(out, "(didn't match; showing disassembly instead)\n");
			aem_nfa_disas(out, nfa, thr->match.visited);
			continue;
		}

		if (regex->match < 0) {
			aem_stringbuf_printf(out, "(not attached to any regex)");
			continue;
		}

		aem_stringbuf_putss(out, regex->where);
		aem_stringbuf_puts(out, "\n");

		struct aem_stringslice bounds = regex->where;
		size_t base = aem_log_buf.n;
		// TODO: Thread safety on nfa->n_insns
		for (size_t i = 0; i < nfa->n_insns; i++) {
			if (!bitfield_test(thr->match.visited, i))
				continue;

			const struct aem_nfa_trace_info *part = &nfa->trace_dbg[i];

			if (part->match != regex->match)
				continue;

			/*
			if (part->where.start < bounds.start && bounds.start - part->where.start < 100) {
				bounds.start = part->where.start;
				i = 0; // start over;
				continue;
			}
			*/

			struct aem_stringslice overlap = part->where;

			if (overlap.start < bounds.start) {
				overlap.start = bounds.start;
			}

			if (overlap.end > bounds.end)
				overlap.end = bounds.end;

			if (!aem_stringslice_ok(overlap))
				continue;

			if (overlap.end < overlap.start) {
				aem_stringbuf_puts(out, "\n(negative overlap)");
				continue;
			}

			for (const char *p = overlap.start; p != overlap.end; p++) {
				size_t j = base + (p - bounds.start);
				aem_assert(j < 1000000);
				aem_stringbuf_assign(out, base + (p - bounds.start), ' ', '^');
			}
		}
	}
}

void aem_nfa_match_dtor(struct aem_nfa_match *match)
{
	if (!match)
		return;

	if (match->captures)
		free(match->captures);
	if (match->visited)
		free(match->visited);

	match->captures = NULL;
	match->visited = NULL;
}

int aem_nfa_run(const struct aem_nfa *nfa, struct aem_stringslice *in, struct aem_nfa_match *match_p)
{
	aem_assert(nfa);
	aem_assert(in);

	// TODO: If nfa->n_insns and nfa->n_captures are read atomically (or perhaps just in the right order), no locking should be required between aem_nfa_run and extending the nfa.
	struct aem_nfa_run run = {0};
	run.in_curr = *in;
	run.longest_match = aem_stringslice_new_len(run.in_curr.start, 0);
	run.nfa = nfa;
	run.n_insns = nfa->n_insns;
#if AEM_NFA_CAPTURES
	run.n_captures = nfa->n_captures;
#endif
	aem_stack_init(&run.curr);
	aem_stack_init(&run.next);
	struct aem_nfa_thread *thr_matched = NULL;
	run.c_prev = -1;

	// Initialize thread list: curr and next
	size_t list_32 = (run.n_insns + 31) >> 5;
	run.map_curr = alloca(list_32 * sizeof(*run.map_curr));
	run.map_next = alloca(list_32 * sizeof(*run.map_next));
	//aem_logf_ctx(AEM_LOG_DEBUG3, "%zd %zd", run.n_insns, list_32);
	for (size_t i = 0; i < list_32; i++) {
		run.map_curr[i] = 0;
		run.map_next[i] = 0;
	}

	for (size_t pc = 0; pc < run.n_insns; pc++) {
		if (!bitfield_test(nfa->thr_init, pc))
			continue;

		aem_logf_ctx(AEM_LOG_DEBUG3, "init thread @ %zx", pc);

		struct aem_nfa_thread *thr = aem_nfa_thread_new(&run, pc);
		aem_assert(thr);
		aem_nfa_thread_add(&run, 1, thr);
	}
	aem_logf_ctx(AEM_LOG_DEBUG3, "%zd init threads", run.next.n);

	int rc = -1;

	for (;;) {
		// Move next => curr, clear next, break if no live threads
		aem_nfa_bitfield *map_tmp = run.map_curr;
		run.map_curr = run.map_next;
		run.map_next = map_tmp;
		int live = 0;
		for (size_t i = 0; i < list_32; i++) {
			run.map_next[i] = 0;
			live |= run.map_curr[i];
		}

		{
			struct aem_stack stk_tmp = run.curr;
			run.curr = run.next;
			run.next = stk_tmp;
			aem_stack_reset(&run.next);
		}

		if (!live) {
			aem_assert(!run.curr.n);
			break;
		}

		aem_assert(run.curr.n);

		run.p_curr = run.in_curr.start;
		int c = aem_stringslice_getc(&run.in_curr);

		AEM_LOG_MULTI(out, AEM_LOG_DEBUG3) {
			aem_stringbuf_puts(out, "char ");
			aem_nfa_desc_char(out, c);
		}
		int rc2 = aem_nfa_step(&run, &thr_matched, c);
		if (rc2 <= -2) {
			rc = rc2;
			goto out;
		} else if (rc2 >= 0) {
			aem_assert(thr_matched);
			rc = rc2;
			aem_assert(thr_matched->match.match == rc);
		}

		// Halt on EOF
		if (c < 0)
			break;

		run.c_prev = c;
	}

out:
	aem_logf_ctx(AEM_LOG_DEBUG3, "done");
	if (match_p) {
		*match_p = (struct aem_nfa_match){ .match = rc };
	}

	if (thr_matched) {
		if (match_p) {
			// Extract info
			*match_p = thr_matched->match;
			thr_matched->match = (struct aem_nfa_match){0};
		} else {
#if AEM_NFA_CAPTURES
			AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
				aem_stringbuf_puts(out, "Captures:");
				for (size_t i = 0; i < run.n_captures; i++) {
					aem_stringbuf_printf(out, " %zd: \"", i);
					aem_string_escape(out, thr_matched->match.captures[i]);
					aem_stringbuf_puts(out, "\"");
				}
			}
#endif
			aem_nfa_show_trace(run.nfa, thr_matched);
		}
		aem_nfa_thread_free(thr_matched);
	}
	aem_assert((thr_matched != NULL) == (rc >= 0));

	in->start = run.longest_match.end;
	// In case any stubborn threads insist on matching EOFs instead of
	// dying, clean them up here.  This should never happen.
	while (run.curr.n) {
		struct aem_nfa_thread *thr = aem_stack_pop(&run.curr);
		aem_nfa_thread_free(thr);
	}
	while (run.next.n) {
		struct aem_nfa_thread *thr = aem_stack_pop(&run.next);
		aem_nfa_thread_free(thr);
	}
	aem_stack_dtor(&run.curr);
	aem_stack_dtor(&run.next);
	return rc;
}
