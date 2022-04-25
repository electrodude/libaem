#include <ctype.h>
#include <limits.h>

#define AEM_INTERNAL
#include <aem/log.h>
#include <aem/nfa-compile.h>
#include <aem/stack.h>
#include <aem/stringbuf.h>
#include <aem/translate.h>
#include <aem/utf8.h>

#include "regex.h"


/// AST construction
static int match_escape(struct aem_nfa_compile_ctx *ctx, uint32_t *c_p, int *esc_p)
{
	aem_assert(ctx);

	return aem_string_unescape_rune(&ctx->in, c_p, esc_p);
}


/// RE => AST
static struct aem_nfa_node *re_parse_named_class(struct aem_nfa_compile_ctx *ctx)
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

	struct aem_nfa_node *node = aem_nfa_node_new(AEM_NFA_NODE_CLASS);
	if (!node)
		return NULL;

	node->text = aem_stringslice_new(ctx->in.start, in.start);
	ctx->in = in;
	node->args.cclass = (struct aem_nfa_node_class){.neg = neg, .cclass = cclass};

	return node;
}
static struct aem_nfa_node *re_parse_range(struct aem_nfa_compile_ctx *ctx)
{
	aem_assert(ctx);

	{
		struct aem_nfa_node *node = re_parse_named_class(ctx);
		if (node)
			return node;
	}

	struct aem_nfa_node *node = aem_nfa_node_new(AEM_NFA_NODE_RANGE);
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
	aem_nfa_node_free(node);
	ctx->in = orig;
	return NULL;
}
static int aem_nfa_brackets_compar(const void *p1, const void *p2)
{
	aem_assert(p1);
	aem_assert(p2);
	struct aem_nfa_node *n1 = *(struct aem_nfa_node **)p1;
	struct aem_nfa_node *n2 = *(struct aem_nfa_node **)p2;
	aem_assert(n1);
	aem_assert(n2);

	if (n1->type != AEM_NFA_NODE_RANGE || n2->type != AEM_NFA_NODE_RANGE) {
		// If they somehow aren't both ranges, sort
		// in input order (i.e. the original order).
		return n1->text.start - n2->text.start;
	}

	struct aem_nfa_node_range r1 = n1->args.range;
	struct aem_nfa_node_range r2 = n2->args.range;

	return r1.min - r2.min;
}
static struct aem_nfa_node *re_parse_brackets(struct aem_nfa_compile_ctx *ctx)
{
	aem_assert(ctx);

	struct aem_stringslice orig = ctx->in;

	if (!aem_stringslice_match(&ctx->in, "["))
		goto fail_nofree;

	struct aem_nfa_node *node = aem_nfa_node_new(AEM_NFA_NODE_BRACKETS);
	if (!node)
		goto fail_nofree;
	node->text = orig;

	int negate = aem_stringslice_match(&ctx->in, "^");

	while (aem_stringslice_ok(ctx->in)) {
		struct aem_nfa_node *range = re_parse_range(ctx);
		if (!range)
			goto fail;
		aem_nfa_node_push(node, range);
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

		struct aem_nfa_node_range range_prev = {.min = 0, .max = UINT_MAX};
		AEM_STACK_FOREACH(i, &stk) {
			struct aem_nfa_node *child = stk.s[i];
			if (!child)
				continue;

			if (child->type != AEM_NFA_NODE_RANGE) {
				aem_logf_ctx(AEM_LOG_ERROR, "Can't complement non-range inside [^...]!");
				aem_stack_dtor(&stk);
				goto fail;
			}
			const struct aem_nfa_node_range range = child->args.range;

			// TODO BUG: range.min == 0
			// TODO HACK: UINT_MAX + 1 == 0, so skip if first range starts at 0
			// TODO: This would all be a lot simpler if ranges were [min, max).
			struct aem_nfa_node_range range_new = {.min = range_prev.max+1, .max = range.min-1};
			if (range_new.max != UINT_MAX && range_new.min <= range_new.max) {
				// Reuse this range to represent the
				// characters between it and the
				// previous one.
				child->args.range = range_new;
				aem_nfa_node_push(node, child);
			} else {
				// Overlapping/null ranges
				aem_nfa_node_free(child);
			}

			range_prev = range;
		}

		struct aem_nfa_node_range range_last = {.min = range_prev.max+1, .max = UINT_MAX};
		// TODO HACK: UINT_MAX + 1 == 0, so skip if final range ends at UINT_MAX
		if (range_last.min && range_last.min <= range_last.max) {
			struct aem_nfa_node *child = aem_nfa_node_new(AEM_NFA_NODE_RANGE);
			if (!child) {
				aem_stack_dtor(&stk);
				goto fail;
			}
			child->args.range = range_last;
			aem_nfa_node_push(node, child);
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
	aem_nfa_node_free(node);
fail_nofree:
	ctx->in = orig;
	return NULL;
}

static struct aem_nfa_node *re_parse_pattern(struct aem_nfa_compile_ctx *ctx);
static struct aem_nfa_node *re_parse_atom(struct aem_nfa_compile_ctx *ctx)
{
	aem_assert(ctx);

	struct aem_stringslice orig = ctx->in;

	struct aem_stringslice out = ctx->in;
	if (aem_stringslice_match(&ctx->in, "[")) {
		ctx->in = orig;
		struct aem_nfa_node *brackets = re_parse_brackets(ctx);
		return brackets;
	} else if (aem_stringslice_match(&ctx->in, "(")) {
		int do_capture = 1;

		enum aem_regex_flags flags = ctx->flags;
		if (aem_stringslice_match(&ctx->in, "?")) {
			ctx->flags = aem_regex_flags_adj(&ctx->in, ctx->flags, 1);
			AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
				aem_stringbuf_puts(out, "Change flags from ");
				aem_regex_flags_describe(out, flags, 0);
				aem_stringbuf_puts(out, " to ");
				aem_regex_flags_describe(out, ctx->flags, 0);
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

		struct aem_nfa_node *pattern = re_parse_pattern(ctx);
		ctx->flags = flags; // Restore flags
		if (!aem_stringslice_match(&ctx->in, ")")) {
			aem_nfa_node_free(pattern);
			ctx->n_captures = i;
			goto fail;
		}
		out.end = ctx->in.start;

		if (!do_capture)
			return pattern;
		if ((ctx->flags & AEM_REGEX_FLAG_EXPLICIT_CAPTURES) && pattern->type == AEM_NFA_NODE_ALTERNATION)
			return pattern;

		struct aem_nfa_node *capture = aem_nfa_node_new(AEM_NFA_NODE_CAPTURE);
		if (!capture) {
			aem_nfa_node_free(pattern);
			ctx->n_captures = i;
			goto fail;
		}
		capture->text = out;
		capture->args.capture.capture = i;
		aem_nfa_node_push(capture, pattern);
		return capture;
	} else {
		uint32_t c;
		int esc;
		if (!match_escape(ctx, &c, &esc))
			goto fail;

		enum aem_nfa_node_type type = AEM_NFA_NODE_ATOM;
		union aem_nfa_node_args args = {.atom = {.c = c, .esc = esc}};

		switch (esc) {
		case 0: // Unescaped
			switch (c) {
			case '.':
				type = AEM_NFA_NODE_CLASS;
				args.cclass = (struct aem_nfa_node_class){.neg = 0, .frontier = 0, .cclass = (ctx->flags & AEM_REGEX_FLAG_BINARY) ? AEM_NFA_CCLASS_ANY : AEM_NFA_CCLASS_LINE};
				break;
			case '^':
				type = AEM_NFA_NODE_CLASS;
				args.cclass = (struct aem_nfa_node_class){.neg = 0, .frontier = 1, .cclass = AEM_NFA_CCLASS_LINE};
				break;
			case '$':
				type = AEM_NFA_NODE_CLASS;
				args.cclass = (struct aem_nfa_node_class){.neg = 1, .frontier = 1, .cclass = AEM_NFA_CCLASS_LINE};
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
				type = AEM_NFA_NODE_CLASS;
				args.cclass = (struct aem_nfa_node_class){.neg = 0, .frontier = 1, .cclass = AEM_NFA_CCLASS_ALNUM};
				break;
			case '>':
				type = AEM_NFA_NODE_CLASS;
				args.cclass = (struct aem_nfa_node_class){.neg = 1, .frontier = 1, .cclass = AEM_NFA_CCLASS_ALNUM};
				break;
			case 'A':
				type = AEM_NFA_NODE_CLASS;
				args.cclass = (struct aem_nfa_node_class){.neg = 0, .frontier = 1, .cclass = AEM_NFA_CCLASS_ANY};
				break;
			case 'z':
				type = AEM_NFA_NODE_CLASS;
				args.cclass = (struct aem_nfa_node_class){.neg = 1, .frontier = 1, .cclass = AEM_NFA_CCLASS_ANY};
				break;

			case 'w':
			case 'W':
				type = AEM_NFA_NODE_CLASS;
				args.cclass = (struct aem_nfa_node_class){.neg = 0, .frontier = 0, .cclass = AEM_NFA_CCLASS_ALNUM};
				break;
			case 'd':
			case 'D':
				type = AEM_NFA_NODE_CLASS;
				args.cclass = (struct aem_nfa_node_class){.neg = neg, .frontier = 0, .cclass = AEM_NFA_CCLASS_DIGIT};
				break;
			case 's':
			case 'S':
				type = AEM_NFA_NODE_CLASS;
				args.cclass = (struct aem_nfa_node_class){.neg = neg, .frontier = 0, .cclass = AEM_NFA_CCLASS_SPACE};
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

		struct aem_nfa_node *node = aem_nfa_node_new(type);
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
static struct aem_nfa_node *re_parse_postfix(struct aem_nfa_compile_ctx *ctx)
{
	aem_assert(ctx);

	struct aem_nfa_node *atom = re_parse_atom(ctx);
	if (!atom)
		return NULL;

	struct aem_stringslice out = ctx->in;

	struct aem_nfa_node_repeat repeat = {.min = 0, .max = UINT_MAX};

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

	if ((ctx->flags & AEM_REGEX_FLAG_EXPLICIT_CAPTURES) && atom->type == AEM_NFA_NODE_CAPTURE) {
		const struct aem_nfa_node_capture capture = atom->args.capture;
		aem_logf_ctx(AEM_LOG_NOTICE, "Deleting capture %zd/%zd", capture.capture, ctx->n_captures);
		if (capture.capture == ctx->n_captures-1) {
			ctx->n_captures--;
		}
		struct aem_nfa_node *child = aem_stack_pop(&atom->children);
		aem_assert(!atom->children.n);
		aem_nfa_node_free(atom);
		atom = child;
	}

	struct aem_nfa_node *node = aem_nfa_node_new(AEM_NFA_NODE_REPEAT);
	if (!node) {
		aem_nfa_node_free(atom);
		return NULL;
	}
	node->text = out;
	node->args.repeat = repeat;
	aem_nfa_node_push(node, atom);

	return node;

fail:
	aem_nfa_node_free(atom);
	ctx->in = orig;
	return NULL;
}

// Zero or more postfix'd atoms
static struct aem_nfa_node *re_parse_branch(struct aem_nfa_compile_ctx *ctx)
{
	aem_assert(ctx);

	struct aem_nfa_node *node = aem_nfa_node_new(AEM_NFA_NODE_BRANCH);
	if (!node)
		return NULL;

	while (aem_stringslice_ok(ctx->in)) {
		struct aem_nfa_node *atom = re_parse_postfix(ctx);
		if (!atom) {
			// TODO: No more is indistinguishable from a real error.
			break;
		}
		aem_nfa_node_push(node, atom);
	}

	if (node->children.n == 1) {
		struct aem_nfa_node *child = aem_stack_pop(&node->children);
		aem_nfa_node_free(node);
		return child;
	}

	return node;
}

static struct aem_nfa_node *re_parse_pattern(struct aem_nfa_compile_ctx *ctx)
{
	aem_assert(ctx);

	struct aem_stringslice orig = ctx->in;

	struct aem_nfa_node *branch = re_parse_branch(ctx);

	struct aem_stringslice out = ctx->in;
	if (!aem_stringslice_match(&ctx->in, "|"))
		return branch;
	out.end = ctx->in.start;

	struct aem_nfa_node *node = aem_nfa_node_new(AEM_NFA_NODE_ALTERNATION);
	if (!node) {
		aem_nfa_node_free(branch);
		ctx->in = orig;
		return NULL;
	}
	node->text = out;
	aem_nfa_node_push(node, branch);

	do {
		struct aem_nfa_node *rest = re_parse_branch(ctx);
		aem_nfa_node_push(node, rest);
	} while (aem_stringslice_match(&ctx->in, "|"));

	return node;
}


static struct aem_nfa_node *aem_regex_compile(struct aem_nfa_compile_ctx *ctx)
{
	aem_assert(ctx);

	if (!(ctx->flags & AEM_REGEX_FLAG_BINARY))
		aem_logf_ctx_once(AEM_LOG_NYI, "NYI: new UTF-8 mode");

	return re_parse_pattern(ctx);
}
AEM_NFA_ADD_DEFINE(regex)

static struct aem_nfa_node *aem_string_compile(struct aem_nfa_compile_ctx *ctx)
{
	aem_assert(ctx);

	struct aem_nfa_node *root = aem_nfa_node_new(AEM_NFA_NODE_BRANCH);
	if (!root)
		return NULL;

	ctx->flags |= AEM_REGEX_FLAG_BINARY;

	for (;;) {
		struct aem_stringslice atom = ctx->in;
		int c = aem_stringslice_getc(&ctx->in);
		if (c < 0)
			break;
		atom.end = ctx->in.start;

		struct aem_nfa_node *node = aem_nfa_node_new(AEM_NFA_NODE_ATOM);
		if (!node) {
			aem_nfa_node_free(root);
			return NULL;
		}

		node->text = atom;
		node->args.atom.c = c;
		node->args.atom.esc = 0;
		aem_nfa_node_push(root, node);
	}

	return root;
}
AEM_NFA_ADD_DEFINE(string)
