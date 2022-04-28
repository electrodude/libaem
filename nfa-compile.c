#include <errno.h>
#include <limits.h>

#define AEM_INTERNAL
#include <aem/ansi-term.h>
#include <aem/log.h>
#include <aem/nfa-util.h>
#include <aem/translate.h>
#include <aem/utf8.h>

#include "nfa-compile.h"

/// Regex parser AST structore
struct aem_nfa_node *aem_nfa_node_new(enum aem_nfa_node_type type)
{
	struct aem_nfa_node *node = malloc(sizeof(*node));
	if (!node) {
		aem_logf_ctx(AEM_LOG_ERROR, "malloc() failed: %s", strerror(errno));
		return NULL;
	}

	node->type = type;
	node->text = AEM_STRINGSLICE_EMPTY;
	aem_stack_init(&node->children);

	return node;
}
void aem_nfa_node_free(struct aem_nfa_node *node)
{
	if (!node)
		return;

	while (node->children.n) {
		struct aem_nfa_node *child = aem_stack_pop(&node->children);
		aem_nfa_node_free(child);
	}
	aem_stack_dtor(&node->children);

	free(node);
}
void aem_nfa_node_push(struct aem_nfa_node *node, struct aem_nfa_node *child)
{
	aem_assert(node);

	aem_stack_push(&node->children, child);
}
void aem_nfa_node_sexpr(struct aem_stringbuf *out, const struct aem_nfa_node *node)
{
	aem_assert(out);

	if (!node) {
		aem_stringbuf_puts(out, "()");
		return;
	}

	int do_parens = node->children.n > 0;
	int want_space = 1;
	switch (node->type) {
	case AEM_NFA_NODE_REPEAT:
		do_parens = 1;
		break;
	default:
		break;
	}

	if (do_parens)
		aem_stringbuf_printf(out, AEM_SGR("1;96") "(" AEM_SGR("0"));

	switch (node->type) {
	case AEM_NFA_NODE_RANGE: {
		const struct aem_nfa_node_range range = node->args.range;

		aem_nfa_desc_range(out, range.min, range.max);
		break;
	}
	case AEM_NFA_NODE_ATOM:
		aem_stringbuf_putc(out, '\'');
		aem_stringbuf_putss(out, node->text);
		aem_stringbuf_putc(out, '\'');
		break;
	case AEM_NFA_NODE_CLASS:
		aem_stringbuf_putc(out, '\'');
		aem_stringbuf_putss(out, node->text);
		aem_stringbuf_putc(out, '\'');
		break;
	case AEM_NFA_NODE_REPEAT: {
		/*
		aem_stringbuf_puts(out, "repeat ");
		aem_stringbuf_putc(out, '\'');
		aem_stringbuf_putss(out, node->text);
		aem_stringbuf_puts(out, "\' ");
		*/
		aem_stringbuf_puts(out, "{");
		const struct aem_nfa_node_repeat repeat = node->args.repeat;
		if (repeat.min)
			aem_stringbuf_printf(out, "%d", repeat.min);
		aem_stringbuf_puts(out, ",");
		if (repeat.max != UINT_MAX)
			aem_stringbuf_printf(out, "%d", repeat.max);
		aem_stringbuf_puts(out, "}");
		break;
	}
	case AEM_NFA_NODE_CAPTURE:
		aem_stringbuf_puts(out, "capture");
		const struct aem_nfa_node_capture capture = node->args.capture;
		aem_stringbuf_printf(out, " %d", capture.capture);
		break;
	case AEM_NFA_NODE_BRANCH:
		want_space = 0;
		break;
	case AEM_NFA_NODE_ALTERNATION:
		aem_stringbuf_putss(out, node->text);
		break;
	default:
		aem_stringbuf_puts(out, "<invalid>");
		break;
	}

	AEM_STACK_FOREACH(i, &node->children) {
		struct aem_nfa_node *child = node->children.s[i];
		if (want_space)
			aem_stringbuf_putc(out, ' ');
		aem_nfa_node_sexpr(out, child);
		want_space = 1;
	}

	if (do_parens)
		aem_stringbuf_printf(out, AEM_SGR("1;96") ")" AEM_SGR("0"));
}


/// AST construction
// Flags
enum aem_regex_flags aem_regex_flags_parse(struct aem_stringslice *in, int sandbox)
{
	aem_assert(in);
	enum aem_regex_flags flags = 0;
	for (;;) {
#define X(name, flag, safe, value) \
		if (aem_stringslice_match(in, flag) && (safe || !sandbox)) { \
			flags |= name; \
			continue; \
		}
		AEM_REGEX_FLAGS_DEFINE(X)
#undef X
		break;
	}
	return flags;
}
enum aem_regex_flags aem_regex_flags_adj(struct aem_stringslice *in, enum aem_regex_flags flags, int sandbox)
{
	aem_assert(in);
	flags |= aem_regex_flags_parse(in, sandbox);
	if (aem_stringslice_match(in, "-"))
		flags &= ~aem_regex_flags_parse(in, sandbox);
	return flags;
}
void aem_regex_flags_describe(struct aem_stringbuf *out, enum aem_regex_flags flags, int sandbox)
{
	aem_assert(out);
#define X(name, flag, safe, value) \
	if ((flags & name) && (safe || !sandbox)) \
		aem_stringbuf_puts(out, flag);
	AEM_REGEX_FLAGS_DEFINE(X)
#undef X
	size_t checkpoint = out->n;
	aem_stringbuf_puts(out, "-");
#define X(name, flag, safe, value) \
	if (!(flags & name) && (safe || !sandbox)) \
		aem_stringbuf_puts(out, flag);
	AEM_REGEX_FLAGS_DEFINE(X)
#undef X
	// Remove "-" if no negative flags were appended.
	if (out->n == checkpoint+1)
		out->n = checkpoint;
}


/// AST compilation
static void re_set_debug(struct aem_nfa_compile_ctx *ctx, size_t i, struct aem_stringslice dbg)
{
	aem_assert(ctx);

	if (!(ctx->flags & AEM_REGEX_FLAG_DEBUG))
		dbg = AEM_STRINGSLICE_EMPTY;

	aem_nfa_set_dbg(ctx->nfa, i, dbg, ctx->match);
}

static size_t aem_nfa_node_compile(struct aem_nfa_compile_ctx *ctx, struct aem_nfa_node *node);
static size_t aem_nfa_node_gen_alternation(struct aem_nfa_compile_ctx *ctx, const struct aem_nfa_node *node)
{
	aem_assert(ctx);
	struct aem_nfa *nfa = ctx->nfa;
	aem_assert(nfa);
	aem_assert(node);

	size_t entry = nfa->n_insns;

	/*
	 * fork post_0
	 * alt 0
	 * end_-1:
	 * jmp end_1
	 * post_0: // patch curr fork
	 *
	 * fork post_1
	 * alt 1
	 * end_1: // patch prev jmp
	 * jmp end_2
	 * post_1: // patch curr fork
	 *
	 * alt 2
	 * end_2: // patch prev jmp
	 */
	size_t jmp_prev = AEM_NFA_PARSE_ERROR;
	AEM_STACK_FOREACH(i, &node->children) {
		struct aem_nfa_node *child = node->children.s[i];
		if (!child)
			continue;

		int not_last = i < node->children.n-1;

		size_t fork = AEM_NFA_PARSE_ERROR;
		if (not_last)
			fork = aem_nfa_append_insn(nfa, aem_nfa_insn_fork(0-0));

		if (aem_nfa_node_compile(ctx, child) == AEM_NFA_PARSE_ERROR)
			return AEM_NFA_PARSE_ERROR;

		if (jmp_prev != AEM_NFA_PARSE_ERROR) {
			aem_nfa_put_insn(nfa, jmp_prev, aem_nfa_insn_jmp(nfa->n_insns));
			re_set_debug(ctx, jmp_prev, node->text);
		}

		jmp_prev = AEM_NFA_PARSE_ERROR;
		if (not_last)
			jmp_prev = aem_nfa_append_insn(nfa, aem_nfa_insn_jmp(0-0));

		if (fork != AEM_NFA_PARSE_ERROR) {
			aem_nfa_put_insn(nfa, fork, aem_nfa_insn_fork(nfa->n_insns));
			re_set_debug(ctx, fork, node->text);
		}
	}
	aem_assert(jmp_prev == AEM_NFA_PARSE_ERROR);

	return entry;
}
static size_t aem_nfa_node_compile(struct aem_nfa_compile_ctx *ctx, struct aem_nfa_node *node)
{
	aem_assert(ctx);
	struct aem_nfa *nfa = ctx->nfa;
	aem_assert(nfa);

	size_t entry = nfa->n_insns;

	if (!node)
		return entry;

	switch (node->type) {
	case AEM_NFA_NODE_RANGE: {
		aem_assert(!node->children.n);
		const struct aem_nfa_node_range range = node->args.range;
		if (range.max >= 0x100) {
			AEM_LOG_MULTI(out, AEM_LOG_BUG) {
				aem_stringbuf_puts(out, "Invalid byte range: ");
				aem_nfa_desc_range(out, range.min, range.max);
			}
		}
		size_t op = aem_nfa_append_insn(nfa, aem_nfa_insn_range(range.min, range.max));
		re_set_debug(ctx, op, node->text);
		break;
	}
	case AEM_NFA_NODE_ATOM: {
		aem_assert(!node->children.n);
		const struct aem_nfa_node_atom atom = node->args.atom;
		struct aem_stringbuf buf = AEM_STRINGBUF_ALLOCA(8);
		aem_stringbuf_put_rune(&buf, atom.c);
		struct aem_stringslice bytes = aem_stringslice_new_str(&buf);
		for (const char *p = bytes.start; p != bytes.end; p++) {
			size_t op = aem_nfa_append_insn(nfa, aem_nfa_insn_char(*p));
			re_set_debug(ctx, op, node->text);
		}
		aem_stringbuf_dtor(&buf);
		break;
	}
	case AEM_NFA_NODE_CLASS: {
		aem_assert(!node->children.n);
		const struct aem_nfa_node_class cclass = node->args.cclass;
		size_t op = aem_nfa_append_insn(nfa, aem_nfa_insn_class(cclass.neg, cclass.frontier, cclass.cclass));
		re_set_debug(ctx, op, node->text);
		break;
	}
	case AEM_NFA_NODE_REPEAT: {
		aem_assert(node->children.n == 1);
		struct aem_nfa_node *child = node->children.s[0];
		aem_assert(child);
		const struct aem_nfa_node_repeat repeat = node->args.repeat;

		if (repeat.min > repeat.max) {
			aem_logf_ctx(AEM_LOG_ERROR, "Repetition min %d > max %d!", repeat.min, repeat.max);
			return AEM_NFA_PARSE_ERROR;
		}

		AEM_LOG_MULTI(out, AEM_LOG_DEBUG2) {
			aem_stringbuf_puts(out, "repeat: {");
			if (repeat.min)
				aem_stringbuf_printf(out, "%d", repeat.min);
			if (repeat.min != repeat.max) {
				aem_stringbuf_puts(out, ",");
				if (repeat.max != UINT_MAX)
					aem_stringbuf_printf(out, "%d", repeat.max);
			}
			aem_stringbuf_puts(out, "}");
		}

		size_t last_rep = entry;
		for (size_t i = 0; i < repeat.min; i++) {
			size_t rep = aem_nfa_node_compile(ctx, child);
			if (rep == AEM_NFA_PARSE_ERROR)
				return AEM_NFA_PARSE_ERROR;
			last_rep = rep;
			if (!i) {
				size_t len = nfa->n_insns - rep;
				size_t est = len * (repeat.max == UINT_MAX ? repeat.min : repeat.max);
				if (est > 10000) {
					aem_logf_ctx(AEM_LOG_WARN, "Repetition will cost at least %zd NFA ops!", est);
				}
			}
		}

		aem_assert(repeat.min <= repeat.max);
		size_t bounds_remain = repeat.max != UINT_MAX ? repeat.max - repeat.min : UINT_MAX;

		if (repeat.min && nfa->n_insns == entry) {
			aem_logf_ctx(AEM_LOG_WARN, "Empty repetition!");
			break;
		}

		if (repeat.reluctant) {
			aem_logf_ctx(AEM_LOG_NYI, "NYI: reluctant repetition operators");
			return AEM_NFA_PARSE_ERROR;
		}

		if (!bounds_remain) {
			// Nothing; we're done.
		} else if (bounds_remain < UINT_MAX) {
			for (size_t i = 0; i < bounds_remain; i++) {
				size_t fork = aem_nfa_append_insn(nfa, aem_nfa_insn_fork(0-0));
				if (aem_nfa_node_compile(ctx, child) == AEM_NFA_PARSE_ERROR)
					return AEM_NFA_PARSE_ERROR;
				aem_nfa_put_insn(nfa, fork, aem_nfa_insn_fork(nfa->n_insns));
				re_set_debug(ctx, fork, node->text);
			}
			// TODO: Patch all the forks to go all the way to the
			// end, instead of each forking to the next.
		} else if (repeat.min && bounds_remain == UINT_MAX) {
			size_t fork = aem_nfa_append_insn(nfa, aem_nfa_insn_fork(last_rep));
			re_set_debug(ctx, fork, node->text);
		} else if (!repeat.min && bounds_remain == UINT_MAX) {
			size_t fork = aem_nfa_append_insn(nfa, aem_nfa_insn_fork(0-0));
			if (aem_nfa_node_compile(ctx, child) == AEM_NFA_PARSE_ERROR)
				return AEM_NFA_PARSE_ERROR;
			if (nfa->n_insns == fork) {
				aem_logf_ctx(AEM_LOG_ERROR, "Nothing inside {,}!");
				return AEM_NFA_PARSE_ERROR;
			}
			size_t jmp = aem_nfa_append_insn(nfa, aem_nfa_insn_jmp(fork));
			aem_nfa_put_insn(nfa, fork, aem_nfa_insn_fork(nfa->n_insns));
			re_set_debug(ctx, fork, node->text);
			re_set_debug(ctx, jmp, node->text);
		} else {
			aem_assert(!"Can't happen!");
		}

		break;
	}
	case AEM_NFA_NODE_CAPTURE: {
		aem_assert(node->children.n == 1);
		struct aem_nfa_node *child = node->children.s[0];
		aem_assert(child);
#if AEM_NFA_CAPTURES
		const struct aem_nfa_node_capture capture = node->args.capture;
		size_t c0 = aem_nfa_append_insn(nfa, aem_nfa_insn_capture(0, capture.capture));
		re_set_debug(ctx, c0, aem_stringslice_new_len(node->text.start, 1));
#else
		aem_logf_ctx_once(AEM_LOG_WARN, "Captures disabled at compile-time!");
#endif
		if (aem_nfa_node_compile(ctx, child) == AEM_NFA_PARSE_ERROR)
			return AEM_NFA_PARSE_ERROR;
#if AEM_NFA_CAPTURES
		size_t c1 = aem_nfa_append_insn(nfa, aem_nfa_insn_capture(1, capture.capture));
		re_set_debug(ctx, c1, aem_stringslice_new_len(node->text.end-1, 1));
#endif
		break;
	}
	case AEM_NFA_NODE_BRANCH: {
		AEM_STACK_FOREACH(i, &node->children) {
			struct aem_nfa_node *child = node->children.s[i];
			if (aem_nfa_node_compile(ctx, child) == AEM_NFA_PARSE_ERROR)
				return AEM_NFA_PARSE_ERROR;
		}
		break;
	}
	case AEM_NFA_NODE_ALTERNATION: {
		aem_nfa_node_gen_alternation(ctx, node);
		break;
	}
	default:
		aem_logf_ctx(AEM_LOG_ERROR, "Invalid node->type %#x", node->type);
		return AEM_NFA_PARSE_ERROR;
	}

	return entry;
}

int aem_nfa_add(struct aem_nfa *nfa, struct aem_stringslice *in, int match, struct aem_stringslice flags, struct aem_nfa_node *(*compile)(struct aem_nfa_compile_ctx *ctx))
{
	aem_assert(nfa);
	aem_assert(in);
	aem_assert(compile);

	struct aem_nfa_compile_ctx ctx = {0};
	ctx.in = *in;
	ctx.nfa = nfa;
	ctx.match = match >= 0 ? match : nfa->n_matches;
	ctx.flags = aem_regex_flags_adj(&flags, AEM_REGEX_FLAG_BINARY/*TODO: just 0*/, 0);
	if (aem_stringslice_ok(flags)) {
		AEM_LOG_MULTI(out, AEM_LOG_ERROR) {
			aem_stringbuf_puts(out, "Garbage after flags: ");
			aem_string_escape(out, flags);
		}
		return -1;
	}

	size_t n_insns = nfa->n_insns;
	size_t n_captures = nfa->n_captures;

	// Call callback to convert given pattern to RE tree
	struct aem_nfa_node *root = compile(&ctx);

	AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
		aem_stringbuf_puts(out, "Parsed RE: ");
		aem_nfa_node_sexpr(out, root);
	}

	if (!root || ctx.rc < 0) {
		aem_logf_ctx(AEM_LOG_ERROR, "Failed to parse pattern! rc = %d", ctx.rc);
		aem_nfa_node_free(root);
		goto fail;
	}

	if (aem_stringslice_ok(ctx.in)) {
		AEM_LOG_MULTI(out, AEM_LOG_ERROR) {
			aem_stringbuf_puts(out, "Garbage remains after pattern: ");
			aem_string_escape(out, ctx.in);
		}
		aem_nfa_node_free(root);
		goto fail;
	}

	// TODO: Automatically process AEM_REGEX_FLAG_IGNCASE
	// TODO: Automatically process !AEM_REGEX_FLAG_BINARY

	// Compile AST
	size_t entry = aem_nfa_node_compile(&ctx, root);
	aem_nfa_node_free(root);

	if (entry == AEM_NFA_PARSE_ERROR) {
		aem_logf_ctx(AEM_LOG_ERROR, "Failed to compile regex tree!");
		ctx.rc = -1;
		goto fail;
	}

	// Make sure every thread allocates as many captures as any thread will ever need.
	if (ctx.n_captures > ctx.nfa->n_captures)
		ctx.nfa->n_captures = ctx.n_captures;

	// If we get to the end, record a match and save
	// the complete regex in the MATCH instruction.
	size_t last = aem_nfa_append_insn(ctx.nfa, aem_nfa_insn_match(ctx.match));
	re_set_debug(&ctx, last, *in);

	if (ctx.nfa->n_matches < ctx.match + 1)
		ctx.nfa->n_matches = ctx.match + 1;

	// Mark entry point as such.
	nfa->thr_init[n_insns >> 5] |= (1 << (n_insns & 0x1f));
	//TODO: bitfield_set(nfa->thr_init, n_insns);

	*in = ctx.in;

	return ctx.match;

fail:
	// Restore NFA to how it was before we started breaking stuff
	nfa->n_insns = n_insns;
	nfa->n_captures = n_captures;
	return ctx.rc;
}
