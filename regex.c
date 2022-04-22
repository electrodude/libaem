#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define AEM_INTERNAL
#include <aem/ansi-term.h>
#include <aem/log.h>
#include <aem/nfa-util.h>
#include <aem/stack.h>
#include <aem/stringbuf.h>
#include <aem/translate.h>
#include <aem/utf8.h>

#include "regex.h"


// Failure, invalid address, no address assigned yet, etc.
#define RE_PARSE_ERROR ((size_t)-1)

/// Regex parser AST structore
struct re_node {
	enum re_node_type {
		RE_NODE_RANGE,
		RE_NODE_BRACKETS,
		RE_NODE_ATOM,
		RE_NODE_CLASS,
		RE_NODE_CAPTURE,
		RE_NODE_REPEAT,
		RE_NODE_BRANCH,
		RE_NODE_ALTERNATION,
	} type;
	struct aem_stringslice text;
	struct aem_stack children;
	union re_node_args {
		struct re_node_range {
			uint32_t min;
			uint32_t max;
		} range;
		struct re_node_brackets {
		} brackets;
		struct re_node_atom {
			uint32_t c;
			int esc;
		} atom;
		struct re_node_class {
			enum aem_nfa_cclass cclass;
			uint8_t neg      : 1;
			uint8_t frontier : 1;
		} cclass;
		struct re_node_capture {
			size_t capture;
		} capture;
		struct re_node_repeat {
			unsigned int min;
			unsigned int max;
			int reluctant : 1;
		} repeat;
	} args;
};
static struct re_node *re_node_new(enum re_node_type type)
{
	struct re_node *node = malloc(sizeof(*node));
	if (!node) {
		aem_logf_ctx(AEM_LOG_ERROR, "malloc() failed: %s", strerror(errno));
		return NULL;
	}

	node->type = type;
	node->text = AEM_STRINGSLICE_EMPTY;
	aem_stack_init(&node->children);

	return node;
}
static void re_node_free(struct re_node *node)
{
	if (!node)
		return;

	while (node->children.n) {
		struct re_node *child = aem_stack_pop(&node->children);
		re_node_free(child);
	}
	aem_stack_dtor(&node->children);

	free(node);
}
static void re_node_push(struct re_node *node, struct re_node *child)
{
	aem_assert(node);

	aem_stack_push(&node->children, child);
}
static void re_node_sexpr(struct aem_stringbuf *out, const struct re_node *node)
{
	aem_assert(out);

	if (!node) {
		aem_stringbuf_puts(out, "()");
		return;
	}

	int do_parens = node->children.n > 0;
	int want_space = 1;
	switch (node->type) {
	case RE_NODE_REPEAT:
		do_parens = 1;
		break;
	default:
		break;
	}

	if (do_parens)
		aem_stringbuf_printf(out, AEM_SGR("1;96") "(" AEM_SGR("0"));

	switch (node->type) {
	case RE_NODE_RANGE: {
		const struct re_node_range range = node->args.range;

		aem_nfa_desc_range(out, range.min, range.max);
		break;
	}
	case RE_NODE_ATOM:
		aem_stringbuf_putc(out, '\'');
		aem_stringbuf_putss(out, node->text);
		aem_stringbuf_putc(out, '\'');
		break;
	case RE_NODE_CLASS:
		aem_stringbuf_putc(out, '\'');
		aem_stringbuf_putss(out, node->text);
		aem_stringbuf_putc(out, '\'');
		break;
	case RE_NODE_REPEAT: {
		/*
		aem_stringbuf_puts(out, "repeat ");
		aem_stringbuf_putc(out, '\'');
		aem_stringbuf_putss(out, node->text);
		aem_stringbuf_puts(out, "\' ");
		*/
		aem_stringbuf_puts(out, "{");
		const struct re_node_repeat repeat = node->args.repeat;
		if (repeat.min)
			aem_stringbuf_printf(out, "%d", repeat.min);
		aem_stringbuf_puts(out, ",");
		if (repeat.max != UINT_MAX)
			aem_stringbuf_printf(out, "%d", repeat.max);
		aem_stringbuf_puts(out, "}");
		break;
	}
	case RE_NODE_CAPTURE:
		aem_stringbuf_puts(out, "capture");
		const struct re_node_capture capture = node->args.capture;
		aem_stringbuf_printf(out, " %d", capture.capture);
		break;
	case RE_NODE_BRANCH:
		want_space = 0;
		break;
	case RE_NODE_BRACKETS:
	case RE_NODE_ALTERNATION:
		aem_stringbuf_putss(out, node->text);
		break;
	default:
		aem_stringbuf_puts(out, "<invalid>");
		break;
	}

	AEM_STACK_FOREACH(i, &node->children) {
		struct re_node *child = node->children.s[i];
		if (want_space)
			aem_stringbuf_putc(out, ' ');
		re_node_sexpr(out, child);
		want_space = 1;
	}

	if (do_parens)
		aem_stringbuf_printf(out, AEM_SGR("1;96") ")" AEM_SGR("0"));
}


/// AST construction
// Flags
static enum aem_regex_flags re_flags_parse(struct aem_stringslice *in, int sandbox)
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
static enum aem_regex_flags re_flags_adj(struct aem_stringslice *in, enum aem_regex_flags flags, int sandbox)
{
	aem_assert(in);
	flags |= re_flags_parse(in, sandbox);
	if (aem_stringslice_match(in, "-"))
		flags &= ~re_flags_parse(in, sandbox);
	return flags;
}
static void re_flags_describe(struct aem_stringbuf *out, enum aem_regex_flags flags, int sandbox)
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


struct re_compile_ctx {
	struct aem_stringslice in;
	struct aem_nfa *nfa;
	unsigned int n_captures;
	int match;

	enum aem_regex_flags flags;
};

static int match_escape(struct re_compile_ctx *ctx, uint32_t *c_p, int *esc_p)
{
	aem_assert(ctx);

	return aem_string_unescape_rune(&ctx->in, c_p, esc_p);
}

/// RE => AST
static struct re_node *re_parse_named_class(struct re_compile_ctx *ctx)
{
	aem_assert(ctx);

	struct aem_stringslice in = ctx->in;

	if (!aem_stringslice_match(&in, "[:"))
		return NULL;

	int neg = aem_stringslice_match(&in, "^");

	struct aem_stringslice classname = aem_stringslice_match_alnum(&in);
	if (!aem_stringslice_ok(classname))
		return NULL;

	if (!aem_stringslice_match(&in, ":]"))
		return NULL;

	enum aem_nfa_cclass cclass;
	for (cclass = 0; cclass < AEM_NFA_CCLASS_MAX; cclass++)
		if (aem_stringslice_eq(classname, aem_nfa_cclass_name(cclass)))
			break;

	if (cclass >= AEM_NFA_CCLASS_MAX)
		return NULL;

	struct re_node *node = re_node_new(RE_NODE_CLASS);
	if (!node)
		return NULL;

	node->text = aem_stringslice_new(ctx->in.start, in.start);
	ctx->in = in;
	node->args.cclass = (struct re_node_class){.neg = neg, .cclass = cclass};

	return node;
}
static struct re_node *re_parse_range(struct re_compile_ctx *ctx)
{
	aem_assert(ctx);

	{
		struct re_node *node = re_parse_named_class(ctx);
		if (node)
			return node;
	}

	struct re_node *node = re_node_new(RE_NODE_RANGE);
	if (!node)
		return NULL;
	node->text = ctx->in;

	struct aem_stringslice orig = ctx->in;

	uint32_t lo;
	if (!match_escape(ctx, &lo, NULL))
		goto fail;
	uint32_t hi = lo;
	if (aem_stringslice_match(&ctx->in, "-")) {
		if (!match_escape(ctx, &hi, NULL))
			goto fail;
	}

	node->args.range.min = lo;
	node->args.range.max = hi;

	node->text.end = ctx->in.start;
	return node;

fail:
	re_node_free(node);
	ctx->in = orig;
	return NULL;
}
static int aem_nfa_brackets_compar(const void *p1, const void *p2)
{
	aem_assert(p1);
	aem_assert(p2);
	struct re_node *n1 = *(struct re_node **)p1;
	struct re_node *n2 = *(struct re_node **)p2;
	aem_assert(n1);
	aem_assert(n2);

	if (n1->type != RE_NODE_RANGE || n2->type != RE_NODE_RANGE) {
		// If they somehow aren't both ranges, sort
		// in input order (i.e. the original order).
		return n1->text.start - n2->text.start;
	}

	struct re_node_range r1 = n1->args.range;
	struct re_node_range r2 = n2->args.range;

	return r1.min - r2.min;
}
static struct re_node *re_parse_brackets(struct re_compile_ctx *ctx)
{
	aem_assert(ctx);

	struct aem_stringslice orig = ctx->in;

	if (!aem_stringslice_match(&ctx->in, "["))
		goto fail_nofree;

	struct re_node *node = re_node_new(RE_NODE_BRACKETS);
	if (!node)
		goto fail_nofree;
	node->text = orig;

	int negate = aem_stringslice_match(&ctx->in, "^");

	while (aem_stringslice_ok(ctx->in)) {
		struct re_node *range = re_parse_range(ctx);
		if (!range)
			goto fail;
		re_node_push(node, range);
		if (aem_stringslice_match(&ctx->in, "]"))
			break;
	}
	node->text.end = ctx->in.start;

	// Sort ranges
	aem_stack_qsort(&node->children, aem_nfa_brackets_compar);

	if (negate) {
		// Complement ranges

		// Move old node->children into temporary
		struct aem_stack stk = node->children;
		// Make new node->children
		aem_stack_init_prealloc(&node->children, stk.n+1);

		struct re_node_range range_prev = {.min = 0, .max = UINT_MAX};
		AEM_STACK_FOREACH(i, &stk) {
			struct re_node *child = stk.s[i];
			if (!child)
				continue;

			if (child->type != RE_NODE_RANGE) {
				aem_logf_ctx(AEM_LOG_ERROR, "Can't complement non-range inside [^...]!");
				aem_stack_dtor(&stk);
				goto fail;
			}
			const struct re_node_range range = child->args.range;

			// TODO BUG: range.min == 0
			// TODO HACK: UINT_MAX + 1 == 0, so skip if first range starts at 0
			// TODO: This would all be a lot simpler if ranges were [min, max).
			struct re_node_range range_new = {.min = range_prev.max+1, .max = range.min-1};
			if (range_new.max != UINT_MAX && range_new.min <= range_new.max) {
				// Reuse this range to represent the
				// characters between it and the
				// previous one.
				child->args.range = range_new;
				re_node_push(node, child);
			} else {
				// Overlapping/null ranges
				re_node_free(child);
			}

			range_prev = range;
		}

		struct re_node_range range_last = {.min = range_prev.max+1, .max = UINT_MAX};
		// TODO HACK: UINT_MAX + 1 == 0, so skip if final range ends at UINT_MAX
		if (range_last.min && range_last.min <= range_last.max) {
			struct re_node *child = re_node_new(RE_NODE_RANGE);
			if (!child) {
				aem_stack_dtor(&stk);
				goto fail;
			}
			child->args.range = range_last;
			re_node_push(node, child);
		}

		// Destroy old node->children
		aem_stack_dtor(&stk);
	} else {
		// TODO: Else merge adjacent or overlapping ranges
	}

	if (!(ctx->flags & AEM_REGEX_FLAG_BINARY)) {
		aem_logf_ctx(AEM_LOG_NYI, "NYI: expand UTF-8 ranges");
	}

	return node;

fail:
	re_node_free(node);
fail_nofree:
	ctx->in = orig;
	return NULL;
}

static struct re_node *re_parse_pattern(struct re_compile_ctx *ctx);
static struct re_node *re_parse_atom(struct re_compile_ctx *ctx)
{
	aem_assert(ctx);

	struct aem_stringslice orig = ctx->in;

	struct aem_stringslice out = ctx->in;
	if (aem_stringslice_match(&ctx->in, "[")) {
		ctx->in = orig;
		struct re_node *brackets = re_parse_brackets(ctx);
		return brackets;
	} else if (aem_stringslice_match(&ctx->in, "(")) {
		int do_capture = 1;

		enum aem_regex_flags flags = ctx->flags;
		if (aem_stringslice_match(&ctx->in, "?")) {
			ctx->flags = re_flags_adj(&ctx->in, ctx->flags, 1);
			AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
				aem_stringbuf_puts(out, "Change flags from ");
				re_flags_describe(out, flags, 0);
				aem_stringbuf_puts(out, " to ");
				re_flags_describe(out, ctx->flags, 0);
			}
			if (aem_stringslice_match(&ctx->in, ":")) {
				do_capture = 0;
			} else {
				aem_logf_ctx(AEM_LOG_NYI, "NYI: set flags for current group (?flags)");
			}
		}

		size_t i = ctx->n_captures;
		if (do_capture)
			ctx->n_captures++; // Count captures in lexical order

		struct re_node *pattern = re_parse_pattern(ctx);
		ctx->flags = flags; // Restore flags
		if (!aem_stringslice_match(&ctx->in, ")")) {
			re_node_free(pattern);
			ctx->n_captures = i;
			goto fail;
		}
		out.end = ctx->in.start;

		if (!do_capture)
			return pattern;
		if ((ctx->flags & AEM_REGEX_FLAG_EXPLICIT_CAPTURES) && pattern->type == RE_NODE_ALTERNATION)
			return pattern;

		struct re_node *capture = re_node_new(RE_NODE_CAPTURE);
		if (!capture) {
			re_node_free(pattern);
			ctx->n_captures = i;
			goto fail;
		}
		capture->text = out;
		capture->args.capture.capture = i;
		re_node_push(capture, pattern);
		return capture;
	} else {
		uint32_t c;
		int esc;
		if (!match_escape(ctx, &c, &esc))
			goto fail;

		enum re_node_type type = RE_NODE_ATOM;
		union re_node_args args = {.atom = {.c = c, .esc = esc}};

		switch (esc) {
		case 0: // Unescaped
			switch (c) {
			case '.':
				type = RE_NODE_CLASS;
				args.cclass = (struct re_node_class){.neg = 0, .frontier = 0, .cclass = (ctx->flags & AEM_REGEX_FLAG_BINARY) ? AEM_NFA_CCLASS_ANY : AEM_NFA_CCLASS_LINE};
				break;
			case '^':
				type = RE_NODE_CLASS;
				args.cclass = (struct re_node_class){.neg = 0, .frontier = 1, .cclass = AEM_NFA_CCLASS_LINE};
				break;
			case '$':
				type = RE_NODE_CLASS;
				args.cclass = (struct re_node_class){.neg = 1, .frontier = 1, .cclass = AEM_NFA_CCLASS_LINE};
				break;
			case ')':
			case '?':
			case '*':
			case '+':
			case '|':
			case '\\':
				// Not an atom
				goto fail;
			default:
				// Plain character
				break;
			}
			break;
		case 1: // Substituted escape: do nothing else
			break;
		case 2: {
			// Unsubstitued escape
			int neg = isupper(c) != 0;
			switch (c) {
			case '<':
				type = RE_NODE_CLASS;
				args.cclass = (struct re_node_class){.neg = 0, .frontier = 1, .cclass = AEM_NFA_CCLASS_ALNUM};
				break;
			case '>':
				type = RE_NODE_CLASS;
				args.cclass = (struct re_node_class){.neg = 1, .frontier = 1, .cclass = AEM_NFA_CCLASS_ALNUM};
				break;
			case 'A':
				type = RE_NODE_CLASS;
				args.cclass = (struct re_node_class){.neg = 0, .frontier = 1, .cclass = AEM_NFA_CCLASS_ANY};
				break;
			case 'z':
				type = RE_NODE_CLASS;
				args.cclass = (struct re_node_class){.neg = 1, .frontier = 1, .cclass = AEM_NFA_CCLASS_ANY};
				break;

			case 'w':
			case 'W':
				type = RE_NODE_CLASS;
				args.cclass = (struct re_node_class){.neg = 0, .frontier = 0, .cclass = AEM_NFA_CCLASS_ALNUM};
				break;
			case 'd':
			case 'D':
				type = RE_NODE_CLASS;
				args.cclass = (struct re_node_class){.neg = neg, .frontier = 0, .cclass = AEM_NFA_CCLASS_DIGIT};
				break;
			case 's':
			case 'S':
				type = RE_NODE_CLASS;
				args.cclass = (struct re_node_class){.neg = neg, .frontier = 0, .cclass = AEM_NFA_CCLASS_SPACE};
				break;

			case '(':
			case ')':
			case '[':
			case '?':
			case '*':
			case '+':
			case '|':
			case '\\':
				break;

			default:
				aem_logf_ctx(AEM_LOG_WARN, "Unnecessary escape: \\%c", c);
			}
			break;
		}
		default:
			aem_logf_ctx(AEM_LOG_BUG, "Invalid esc: %d (char %08x)", esc, c);
			break;
		}

		struct re_node *node = re_node_new(type);
		if (!node)
			goto fail;

		out.end = ctx->in.start;
		node->text = out;
		node->args = args;

		return node;
	}

fail:
	ctx->in = orig;
	return NULL;
}

// Atom, possibly followed by a postfix repetition operator
static struct re_node *re_parse_postfix(struct re_compile_ctx *ctx)
{
	aem_assert(ctx);

	struct re_node *atom = re_parse_atom(ctx);
	if (!atom)
		return NULL;

	struct aem_stringslice out = ctx->in;

	struct re_node_repeat repeat = {.min = 0, .max = UINT_MAX};

	struct aem_stringslice orig = ctx->in;
	if (aem_stringslice_match(&ctx->in, "?")) {
		repeat.min = 0;
		repeat.max = 1;
	} else if (aem_stringslice_match(&ctx->in, "*")) {
		repeat.min = 0;
		repeat.max = UINT_MAX;
	} else if (aem_stringslice_match(&ctx->in, "+")) {
		repeat.min = 1;
		repeat.max = UINT_MAX;
	} else if (aem_stringslice_match(&ctx->in, "{")) {
		// Try to get a lower bound
		int lower = aem_stringslice_match_uint_base(&ctx->in, 10, &repeat.min);
		if (!lower) {
			repeat.min = 0;
		}

		// Try to get a comma
		int comma = aem_stringslice_match(&ctx->in, ",");

		if (!lower && !comma)
			return atom;

		// Try to get a upper bound, but only if we got a comma
		int upper = comma && aem_stringslice_match_uint_base(&ctx->in, 10, &repeat.max);
		if (!upper) {
			repeat.max = UINT_MAX;
		}

		if (lower && !comma)
			repeat.max = repeat.min;

		if (!aem_stringslice_match(&ctx->in, "}"))
			goto fail;
	} else {
		return atom;
	}
	repeat.reluctant = aem_stringslice_match(&ctx->in, "?");

	if (repeat.min > repeat.max) {
		aem_logf_ctx(AEM_LOG_ERROR, "Repetition min %d > max %d!", repeat.min, repeat.max);
		goto fail;
	}

	out.end = ctx->in.start;
	if (!aem_stringslice_ok(out))
		return atom;

	if ((ctx->flags & AEM_REGEX_FLAG_EXPLICIT_CAPTURES) && atom->type == RE_NODE_CAPTURE) {
		const struct re_node_capture capture = atom->args.capture;
		aem_logf_ctx(AEM_LOG_NOTICE, "Deleting capture %zd/%zd", capture.capture, ctx->n_captures);
		if (capture.capture == ctx->n_captures-1) {
			ctx->n_captures--;
		}
		struct re_node *child = aem_stack_pop(&atom->children);
		aem_assert(!atom->children.n);
		re_node_free(atom);
		atom = child;
	}

	struct re_node *node = re_node_new(RE_NODE_REPEAT);
	if (!node) {
		re_node_free(atom);
		return NULL;
	}
	node->text = out;
	node->args.repeat = repeat;
	re_node_push(node, atom);

	return node;

fail:
	re_node_free(atom);
	ctx->in = orig;
	return NULL;
}

// Zero or more postfix'd atoms
static struct re_node *re_parse_branch(struct re_compile_ctx *ctx)
{
	aem_assert(ctx);

	struct re_node *node = re_node_new(RE_NODE_BRANCH);
	if (!node)
		return NULL;

	while (aem_stringslice_ok(ctx->in)) {
		struct re_node *atom = re_parse_postfix(ctx);
		if (!atom) {
			// TODO: No more is indistinguishable from a real error.
			break;
		}
		re_node_push(node, atom);
	}

	if (node->children.n == 1) {
		struct re_node *child = aem_stack_pop(&node->children);
		re_node_free(node);
		return child;
	}

	return node;
}

static struct re_node *re_parse_pattern(struct re_compile_ctx *ctx)
{
	aem_assert(ctx);

	struct aem_stringslice orig = ctx->in;

	struct re_node *branch = re_parse_branch(ctx);

	struct aem_stringslice out = ctx->in;
	if (!aem_stringslice_match(&ctx->in, "|"))
		return branch;
	out.end = ctx->in.start;

	struct re_node *node = re_node_new(RE_NODE_ALTERNATION);
	if (!node) {
		re_node_free(branch);
		ctx->in = orig;
		return NULL;
	}
	node->text = out;
	re_node_push(node, branch);

	do {
		struct re_node *rest = re_parse_branch(ctx);
		re_node_push(node, rest);
	} while (aem_stringslice_match(&ctx->in, "|"));

	return node;
}


/// AST compilation
void re_set_debug(struct re_compile_ctx *ctx, size_t i, struct aem_stringslice dbg)
{
	aem_assert(ctx);

	if (!(ctx->flags & AEM_REGEX_FLAG_DEBUG))
		dbg = AEM_STRINGSLICE_EMPTY;

	aem_nfa_set_dbg(ctx->nfa, i, dbg, ctx->match);
}

static size_t re_node_compile(struct re_compile_ctx *ctx, struct re_node *node);
static size_t re_node_gen_alternation(struct re_compile_ctx *ctx, const struct re_node *node)
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
	size_t jmp_prev = RE_PARSE_ERROR;
	AEM_STACK_FOREACH(i, &node->children) {
		struct re_node *child = node->children.s[i];
		if (!child)
			continue;

		int not_last = i < node->children.n-1;

		size_t fork = RE_PARSE_ERROR;
		if (not_last)
			fork = aem_nfa_append_insn(nfa, aem_nfa_insn_fork(0-0));

		if (re_node_compile(ctx, child) == RE_PARSE_ERROR)
			return RE_PARSE_ERROR;

		if (jmp_prev != RE_PARSE_ERROR) {
			aem_nfa_put_insn(nfa, jmp_prev, aem_nfa_insn_jmp(nfa->n_insns));
			re_set_debug(ctx, jmp_prev, node->text);
		}

		jmp_prev = RE_PARSE_ERROR;
		if (not_last)
			jmp_prev = aem_nfa_append_insn(nfa, aem_nfa_insn_jmp(0-0));

		if (fork != RE_PARSE_ERROR) {
			aem_nfa_put_insn(nfa, fork, aem_nfa_insn_fork(nfa->n_insns));
			re_set_debug(ctx, fork, node->text);
		}
	}
	aem_assert(jmp_prev == RE_PARSE_ERROR);

	return entry;
}
static size_t re_node_compile(struct re_compile_ctx *ctx, struct re_node *node)
{
	aem_assert(ctx);
	struct aem_nfa *nfa = ctx->nfa;
	aem_assert(nfa);

	size_t entry = nfa->n_insns;

	if (!node)
		return entry;

	switch (node->type) {
	case RE_NODE_RANGE: {
		aem_assert(!node->children.n);
		const struct re_node_range range = node->args.range;
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
	case RE_NODE_ATOM: {
		aem_assert(!node->children.n);
		const struct re_node_atom atom = node->args.atom;
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
	case RE_NODE_CLASS: {
		aem_assert(!node->children.n);
		const struct re_node_class cclass = node->args.cclass;
		size_t op = aem_nfa_append_insn(nfa, aem_nfa_insn_class(cclass.neg, cclass.frontier, cclass.cclass));
		re_set_debug(ctx, op, node->text);
		break;
	}
	case RE_NODE_REPEAT: {
		aem_assert(node->children.n == 1);
		struct re_node *child = node->children.s[0];
		aem_assert(child);
		const struct re_node_repeat repeat = node->args.repeat;

		if (repeat.min > repeat.max) {
			aem_logf_ctx(AEM_LOG_ERROR, "Repetition min %d > max %d!", repeat.min, repeat.max);
			return RE_PARSE_ERROR;
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
			size_t rep = re_node_compile(ctx, child);
			if (rep == RE_PARSE_ERROR)
				return RE_PARSE_ERROR;
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
			return RE_PARSE_ERROR;
		}

		if (!bounds_remain) {
			// Nothing; we're done.
		} else if (bounds_remain < UINT_MAX) {
			for (size_t i = 0; i < bounds_remain; i++) {
				size_t fork = aem_nfa_append_insn(nfa, aem_nfa_insn_fork(0-0));
				if (re_node_compile(ctx, child) == RE_PARSE_ERROR)
					return RE_PARSE_ERROR;
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
			if (re_node_compile(ctx, child) == RE_PARSE_ERROR)
				return RE_PARSE_ERROR;
			if (nfa->n_insns == fork) {
				aem_logf_ctx(AEM_LOG_ERROR, "Nothing inside {,}!");
				return RE_PARSE_ERROR;
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
	case RE_NODE_CAPTURE: {
		aem_assert(node->children.n == 1);
		struct re_node *child = node->children.s[0];
		aem_assert(child);
#if AEM_NFA_CAPTURES
		const struct re_node_capture capture = node->args.capture;
		size_t c0 = aem_nfa_append_insn(nfa, aem_nfa_insn_capture(0, capture.capture));
		re_set_debug(ctx, c0, aem_stringslice_new_len(node->text.start, 1));
#else
		aem_logf_ctx_once(AEM_LOG_WARN, "Captures disabled at compile-time!");
#endif
		if (re_node_compile(ctx, child) == RE_PARSE_ERROR)
			return RE_PARSE_ERROR;
#if AEM_NFA_CAPTURES
		size_t c1 = aem_nfa_append_insn(nfa, aem_nfa_insn_capture(1, capture.capture));
		re_set_debug(ctx, c1, aem_stringslice_new_len(node->text.end-1, 1));
#endif
		break;
	}
	case RE_NODE_BRANCH: {
		AEM_STACK_FOREACH(i, &node->children) {
			struct re_node *child = node->children.s[i];
			if (re_node_compile(ctx, child) == RE_PARSE_ERROR)
				return RE_PARSE_ERROR;
		}
		break;
	}
	case RE_NODE_BRACKETS:
	case RE_NODE_ALTERNATION: {
		re_node_gen_alternation(ctx, node);
		break;
	}
	default:
		aem_logf_ctx(AEM_LOG_ERROR, "Invalid node->type %#x", node->type);
		return RE_PARSE_ERROR;
	}

	return entry;
}


static int aem_nfa_add(struct aem_nfa *nfa, struct aem_stringslice *in, int match, struct aem_stringslice flags, int (*compile)(struct re_compile_ctx *ctx))
{
	aem_assert(nfa);
	aem_assert(in);
	aem_assert(compile);

	if (match < 0)
		match = nfa->n_matches;

	struct re_compile_ctx ctx = {0};
	ctx.in = *in;
	ctx.nfa = nfa;
	ctx.match = match;
	ctx.flags = re_flags_adj(&flags, AEM_REGEX_FLAG_BINARY/*TODO 0*/, 0);
	if (aem_stringslice_ok(flags)) {
		AEM_LOG_MULTI(out, AEM_LOG_ERROR) {
			aem_stringbuf_puts(out, "Garbage after flags: ");
			aem_string_escape(out, flags);
		}
		return -1;
	}

	size_t n_insns = nfa->n_insns;
	size_t n_captures = nfa->n_captures;

	int rc = compile(&ctx);

	if (aem_stringslice_ok(ctx.in)) {
		aem_logf_ctx(AEM_LOG_NYI, "NYI: Complain about %zd trailing bytes", aem_stringslice_len(ctx.in));
	}

	if (rc) { // Failure
		// Restore NFA to how it was before we started breaking stuff
		nfa->n_insns = n_insns;
		nfa->n_captures = n_captures;
		return rc;
	}

	// Make sure every thread allocates as many captures as any thread will ever need.
	if (ctx.n_captures > ctx.nfa->n_captures)
		ctx.nfa->n_captures = ctx.n_captures;

	// If we get to the end, record a match and save
	// the complete regex in the MATCH instruction.
	size_t last = aem_nfa_append_insn(ctx.nfa, aem_nfa_insn_match(ctx.match));
	re_set_debug(&ctx, last, *in);

	if (ctx.nfa->n_matches < match + 1)
		ctx.nfa->n_matches = match + 1;

	*in = ctx.in;

	return rc;
}
#define AEM_NFA_ADD_DEFINE(name) \
int aem_nfa_add_##name(struct aem_nfa *nfa, struct aem_stringslice pat, int match, struct aem_stringslice flags) \
{ \
	return aem_nfa_add(nfa, &pat, match, flags, aem_##name##_compile); \
}

static int aem_regex_compile(struct re_compile_ctx *ctx)
{
	aem_assert(ctx);
	aem_assert(ctx->nfa);

	if (!(ctx->flags & AEM_REGEX_FLAG_BINARY))
		aem_logf_ctx_once(AEM_LOG_NYI, "NYI: new UTF-8 mode");

	struct re_node *root = re_parse_pattern(ctx);
	if (aem_stringslice_ok(ctx->in)) {
		AEM_LOG_MULTI(out, AEM_LOG_ERROR) {
			aem_stringbuf_puts(out, "Garbage after RE: ");
			aem_string_escape(out, ctx->in);
		}
		re_node_free(root);
		return 1;
	}
	if (!root) {
		aem_logf_ctx(AEM_LOG_ERROR, "Failed to parse regex!");
		return 1;
	}

	AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
		aem_stringbuf_puts(out, "Parsed RE: ");
		re_node_sexpr(out, root);
	}

	size_t entry = re_node_compile(ctx, root);
	re_node_free(root);

	if (entry == RE_PARSE_ERROR) {
		aem_logf_ctx(AEM_LOG_ERROR, "Invalid regex!");
		return 1;
	}

	// Mark entry point as such.
	ctx->nfa->thr_init[entry >> 5] |= (1 << (entry & 0x1f));
	//TODO: bitfield_set(ctx->nfa->thr_init, entry);

	return 0;
}
AEM_NFA_ADD_DEFINE(regex)

static int aem_string_compile(struct re_compile_ctx *ctx)
{
	aem_assert(ctx);
	aem_assert(ctx->nfa);

	size_t entry = ctx->nfa->n_insns;

	for (;;) {
		struct aem_stringslice atom = ctx->in;
		int c = aem_stringslice_getc(&ctx->in);
		if (c < 0)
			break;
		atom.end = ctx->in.start;
		size_t op = aem_nfa_append_insn(ctx->nfa, aem_nfa_insn_char(c));
		re_set_debug(ctx, op, atom);
	}

	aem_assert(!aem_stringslice_ok(ctx->in));

	// Mark entry point as such.
	ctx->nfa->thr_init[entry >> 5] |= (1 << (entry & 0x1f));
	//TODO: bitfield_set(ctx->nfa->thr_init, entry);

	return 0;
}
AEM_NFA_ADD_DEFINE(string)
