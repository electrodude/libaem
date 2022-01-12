#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define AEM_INTERNAL
#include <aem/log.h>
#include <aem/stack.h>
#include <aem/stringbuf.h>
#include <aem/translate.h>

#include "regex.h"


// Failure, invalid address, no address assigned yet, etc.
#define RE_PARSE_ERROR ((size_t)-1)

/// Regex parser AST structore
struct re_node {
	enum re_node_type {
		//RE_NODE_CHAR,
		//RE_NODE_DOT,
		RE_NODE_RANGE,
		RE_NODE_BRACKETS,
		RE_NODE_ATOM,
		RE_NODE_CAPTURE,
		RE_NODE_REPEAT,
		RE_NODE_BRANCH,
		RE_NODE_ALTERNATION,
	} type;
	struct aem_stringslice text;
	struct aem_stack children;
	union {
		struct re_node_range {
			unsigned int min;
			unsigned int max;
		} range;
		struct re_node_brackets {
		} brackets;
		struct re_node_atom {
			unsigned int c;
			int esc : 1;
		} atom;
		struct re_node_capture {
#if AEM_NFA_CAPTURES
			size_t capture;
#endif
		} capture;
		struct re_node_repeat {
			unsigned int min;
			unsigned int max;
			int reluctant : 1;
		} repeat;
	} args;
};
struct re_node *re_node_new(enum re_node_type type)
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
void re_node_free(struct re_node *node)
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
void re_node_push(struct re_node *node, struct re_node *child)
{
	aem_assert(node);

	aem_stack_push(&node->children, child);
}
void re_node_sexpr(struct aem_stringbuf *out, const struct re_node *node)
{
	aem_assert(out);

	if (!node) {
		aem_stringbuf_puts(out, "()");
		return;
	}

	int force_parens = 0;
	switch (node->type) {
	case RE_NODE_RANGE:
	case RE_NODE_REPEAT:
		force_parens = 1;
		break;
	default:
		break;
	}

	if (force_parens || node->children.n)
		aem_stringbuf_putc(out, '(');

	switch (node->type) {
	case RE_NODE_RANGE: {
		aem_stringbuf_putss(out, node->text);
		struct re_node_range range = node->args.range;
		aem_stringbuf_puts(out, " [");
		aem_stringbuf_putq(out, range.min);
		aem_stringbuf_puts(out, "-");
		aem_stringbuf_putq(out, range.max);
		aem_stringbuf_puts(out, "]");
		if (node->children.n)
			aem_stringbuf_puts(out, " ");
		break;
	}
	case RE_NODE_BRACKETS:
		aem_stringbuf_putss(out, node->text);
		if (node->children.n)
			aem_stringbuf_puts(out, " ");
		break;
	case RE_NODE_ATOM:
		aem_stringbuf_putc(out, '\'');
		aem_stringbuf_putss(out, node->text);
		aem_stringbuf_puts(out, "\'");
		if (node->children.n)
			aem_stringbuf_puts(out, " ");
		break;
	case RE_NODE_REPEAT: {
		/*
		aem_stringbuf_puts(out, "repeat ");
		aem_stringbuf_putc(out, '\'');
		aem_stringbuf_putss(out, node->text);
		aem_stringbuf_puts(out, "\' ");
		*/
		aem_stringbuf_puts(out, "{");
		struct re_node_repeat repeat = node->args.repeat;
		if (repeat.min)
			aem_stringbuf_printf(out, "%d", repeat.min);
		aem_stringbuf_puts(out, ",");
		if (repeat.max != UINT_MAX)
			aem_stringbuf_printf(out, "%d", repeat.max);
		aem_stringbuf_puts(out, "}");
		if (node->children.n)
			aem_stringbuf_puts(out, " ");
		break;
	}
	case RE_NODE_CAPTURE:
		aem_stringbuf_puts(out, "capture");
		if (node->children.n)
			aem_stringbuf_puts(out, " ");
		break;
	case RE_NODE_BRANCH:
		break;
	case RE_NODE_ALTERNATION:
		aem_stringbuf_puts(out, "|");
		if (node->children.n)
			aem_stringbuf_puts(out, " ");
		break;
	default:
		aem_stringbuf_puts(out, "<invalid>");
		if (node->children.n)
			aem_stringbuf_puts(out, " ");
		break;
	}

	AEM_STACK_FOREACH(i, &node->children) {
		struct re_node *child = node->children.s[i];
		if (i > 0)
			aem_stringbuf_putc(out, ' ');
		re_node_sexpr(out, child);
	}

	if (force_parens || node->children.n)
		aem_stringbuf_putc(out, ')');
}


/// AST construction
static int match_escape(struct aem_stringslice *in, int *esc_p)
{
	aem_assert(in);

	struct aem_stringslice orig = *in;
	int esc = aem_stringslice_match(in, "\\");
	int c = aem_stringslice_get(in);
	if (c < 0) {
		*in = orig;
		return c;
	}

	if (esc) {
		int esc_unused = 0;
		switch (c) {
		case '0': c = '\0'  ; break;
		case 'e': c = '\x1b'; break;
		case 't': c = '\t'  ; break;
		case 'n': c = '\n'  ; break;
		case 'r': c = '\r'  ; break;
		/*
		// collides with \x AEM_NFA_CCLASS_XDIGIT
		case 'x':
			c = aem_stringslice_match_hexbyte(in);
			break;
		*/
		case 'u': {
			unsigned int out;
			aem_assert(aem_stringslice_match_uint_base(in, 16, &out));
			c = out;
			break;
		}
		default:
			esc_unused = 1;
			break;
		}

		// Only report that the character was escaped if we didn't know how to handle the escape sequence.
		if (!esc_unused)
			esc = 0;
	}

	if (esc_p)
		*esc_p = esc;

	return c;
}
static struct re_node *re_parse_range(struct aem_stringslice *re)
{
	aem_assert(re);

	struct aem_stringslice in = *re;

	struct re_node *node = re_node_new(RE_NODE_RANGE);
	if (!node)
		return NULL;
	node->text = *re;

	int lo = match_escape(&in, NULL);
	if (lo < 0)
		goto fail;
	int hi = lo;
	if (aem_stringslice_match(&in, "-")) {
		hi = match_escape(&in, NULL);
		if (hi < 0)
			goto fail;
	}

	node->args.range.min = lo;
	node->args.range.max = hi;

	*re = in;
	node->text.end = re->start;
	return node;

fail:
	re_node_free(node);
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

	/*
	AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
		re_node_sexpr(out, n1);
		aem_stringbuf_puts(out, " <=> ");
		re_node_sexpr(out, n2);
	}
	*/

	if (n1->type != RE_NODE_RANGE || n2->type != RE_NODE_RANGE) {
		// If they somehow aren't both ranges, sort
		// in input order (i.e. the original order).
		return n1->text.start - n2->text.start;
	}

	struct re_node_range r1 = n1->args.range;
	struct re_node_range r2 = n2->args.range;

	return r1.min - r2.min;
}
static struct re_node *re_parse_brackets(struct aem_stringslice *re)
{
	aem_assert(re);

	struct aem_stringslice in = *re;

	if (!aem_stringslice_match(&in, "["))
		return NULL;

	struct re_node *node = re_node_new(RE_NODE_BRACKETS);
	if (!node)
		return NULL;
	node->text = *re;

	int negate = aem_stringslice_match(&in, "^");

	while (aem_stringslice_ok(in)) {
		struct re_node *range = re_parse_range(&in);
		if (!range)
			goto fail;
		re_node_push(node, range);
		if (aem_stringslice_match(&in, "]"))
			break;
	}

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

			/*
			AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
				aem_stringbuf_puts(out, "Have ");
				re_node_sexpr(out, child);
			}
			*/

			if (child->type != RE_NODE_RANGE) {
				aem_logf_ctx(AEM_LOG_ERROR, "Can't complement non-range inside [^...]!");
				goto fail;
			}
			struct re_node_range range = child->args.range;

			// TODO BUG: range.min == 0
			// TODO HACK: UINT_MAX + 1 == 0, so skip if first range starts at 0
			// TODO: This would all be a lot simpler if ranges were [min, max).
			struct re_node_range range_new = {.min = range_prev.max+1, .max = range.min-1};
			if (range_new.max != UINT_MAX && range_new.min <= range_new.max) {
				// Reuse this range to represent the
				// characters between it and the
				// previous one.
				child->args.range = range_new;
				aem_stack_push(&node->children, child);
				/*
				AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
					aem_stringbuf_puts(out, "Produce ");
					re_node_sexpr(out, child);
				}
				*/
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
			if (!child)
				goto fail;
			child->args.range = range_last;
			aem_stack_push(&node->children, child);
			/*
			AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
				aem_stringbuf_puts(out, "Produce ");
				re_node_sexpr(out, child);
			}
			*/
		}

		// Destroy old node->children
		aem_stack_dtor(&stk);
	}

	*re = in;
	node->text.end = re->start;
	return node;
fail:
	re_node_free(node);
	return NULL;
}
static struct re_node *re_parse_pattern(struct aem_stringslice *re);
static struct re_node *re_parse_atom(struct aem_stringslice *re)
{
	aem_assert(re);

	struct aem_stringslice out = *re;
	struct aem_stringslice in = *re;
	if (aem_stringslice_match(&in, "[")) {
		return re_parse_brackets(re);
	} else if (aem_stringslice_match(&in, "(")) {
		struct re_node *pattern = re_parse_pattern(&in);
		if (!aem_stringslice_match(&in, ")")) {
			re_node_free(pattern);
			return NULL;
		}
		*re = in;
		out.end = re->start;
		struct re_node *capture = re_node_new(RE_NODE_CAPTURE);
		if (!capture) {
			re_node_free(pattern);
			return NULL;
		}
		capture->text = out;
#if AEM_NFA_CAPTURES
		capture->args.capture.capture = RE_PARSE_ERROR;
#endif
		re_node_push(capture, pattern);
		return capture;
	} else {
		int esc = 0;
		int c = match_escape(&in, &esc);
		if (c < 0)
			return NULL;
		if (!esc) {
			switch (c) {
			case ')':
			case '?':
			case '*':
			case '+':
			case '|':
			case '\\':
				return NULL;
			default:
				break;
			}
		}

		*re = in;

		struct re_node *node = re_node_new(RE_NODE_ATOM);
		if (!node)
			return NULL;

		out.end = re->start;
		node->text = out;
		node->args.atom.c = c;
		node->args.atom.esc = esc;
		return node;
	}
}
// Atom, possibly followed by a postfix repetition operator
static struct re_node *re_parse_postfix(struct aem_stringslice *re)
{
	aem_assert(re);

	struct re_node *atom = re_parse_atom(re);
	if (!atom)
		return NULL;

	struct aem_stringslice out = *re;

	struct re_node_repeat repeat = {.min = 0, .max = UINT_MAX};

	struct aem_stringslice in = *re;
	if (aem_stringslice_match(&in, "?")) {
		repeat.min = 0;
		repeat.max = 1;
	} else if (aem_stringslice_match(&in, "*")) {
		repeat.min = 0;
		repeat.max = UINT_MAX;
	} else if (aem_stringslice_match(&in, "+")) {
		repeat.min = 1;
		repeat.max = UINT_MAX;
	} else if (aem_stringslice_match(&in, "{")) {
		// Try to get a lower bound
		int lower = aem_stringslice_match_uint_base(&in, 10, &repeat.min);
		if (!lower) {
			repeat.min = 0;
		}

		// Try to get a comma
		int comma = aem_stringslice_match(&in, ",");

		if (!lower && !comma)
			return atom;

		// Try to get a upper bound, but only if we got a comma
		int upper = comma && aem_stringslice_match_uint_base(&in, 10, &repeat.max);
		if (!upper) {
			repeat.max = UINT_MAX;
		}

		if (lower && !comma)
			repeat.max = repeat.min;

		if (!aem_stringslice_match(&in, "}"))
			goto fail;
	} else {
		return atom;
	}
	repeat.reluctant = aem_stringslice_match(&in, "?");
	*re = in;

	if (repeat.min > repeat.max) {
		aem_logf_ctx(AEM_LOG_ERROR, "Bounds min %d > max %d!", repeat.min, repeat.max);
		goto fail;
	}

	out.end = re->start;
	if (!aem_stringslice_ok(out))
		return atom;

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
	return NULL;
}
// zero or more postfix'd atoms
static struct re_node *re_parse_branch(struct aem_stringslice *re)
{
	aem_assert(re);

	struct re_node *node = re_node_new(RE_NODE_BRANCH);
	if (!node)
		return NULL;

	while (aem_stringslice_ok(*re)) {
		struct aem_stringslice in = *re;
		struct re_node *atom = re_parse_postfix(&in);
		if (!atom)
			break;
		re_node_push(node, atom);
		*re = in;
	}

	if (node->children.n == 1) {
		struct re_node *child = aem_stack_pop(&node->children);
		re_node_free(node);
		return child;
	}

	return node;
}
static struct re_node *re_parse_pattern(struct aem_stringslice *re)
{
	aem_assert(re);

	struct re_node *branch = re_parse_branch(re);

	struct aem_stringslice out = *re;
	if (!aem_stringslice_match(re, "|"))
		return branch;
	out.end = re->start;

	struct re_node *node = re_node_new(RE_NODE_ALTERNATION);
	if (!node) {
		re_node_free(branch);
		return NULL;
	}
	node->text = out;
	re_node_push(node, branch);

	struct re_node *rest = re_parse_pattern(re);
	re_node_push(node, rest);

	return node;
}


/// AST compilation
static size_t re_node_compile(struct aem_nfa *nfa, struct re_node *node, int match);
static size_t re_node_gen_alternation(struct aem_nfa *nfa, struct re_node *node, int match)
{
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

		if (re_node_compile(nfa, child, match) == RE_PARSE_ERROR)
			return RE_PARSE_ERROR;

		if (jmp_prev != RE_PARSE_ERROR) {
			aem_nfa_put_insn(nfa, jmp_prev, aem_nfa_insn_jmp(nfa->n_insns));
			aem_nfa_set_dbg(nfa, jmp_prev, node->text, match);
		}

		jmp_prev = RE_PARSE_ERROR;
		if (not_last)
			jmp_prev = aem_nfa_append_insn(nfa, aem_nfa_insn_jmp(0-0));

		if (fork != RE_PARSE_ERROR) {
			aem_nfa_put_insn(nfa, fork, aem_nfa_insn_fork(nfa->n_insns));
			aem_nfa_set_dbg(nfa, fork, node->text, match);
		}
	}
	aem_assert(jmp_prev == RE_PARSE_ERROR);

	return entry;
}
static size_t re_node_compile(struct aem_nfa *nfa, struct re_node *node, int match)
{
	aem_assert(nfa);

	size_t entry = nfa->n_insns;

	if (!node)
		return entry;

	switch (node->type) {
	case RE_NODE_RANGE: {
		aem_assert(!node->children.n);
		struct re_node_range range = node->args.range;
		size_t op = aem_nfa_append_insn(nfa, aem_nfa_insn_range(range.min, range.max));
		aem_nfa_set_dbg(nfa, op, node->text, match);
		break;
	}
	case RE_NODE_BRACKETS: {
		re_node_gen_alternation(nfa, node, match);
		break;
	}
	case RE_NODE_ATOM: {
		aem_assert(!node->children.n);
		struct re_node_atom atom = node->args.atom;
		size_t op = RE_PARSE_ERROR;
		if (!atom.esc) {
			// Unescaped
			switch (atom.c) {
			case '.':
				op = aem_nfa_append_insn(nfa, aem_nfa_insn_class(0, 0, AEM_NFA_CCLASS_LINE));
				break;
			case '^':
				op = aem_nfa_append_insn(nfa, aem_nfa_insn_class(0, 1, AEM_NFA_CCLASS_LINE));
				break;
			case '$':
				op = aem_nfa_append_insn(nfa, aem_nfa_insn_class(1, 1, AEM_NFA_CCLASS_LINE));
				break;
			default:
				op = aem_nfa_append_insn(nfa, aem_nfa_insn_char(atom.c));
			}
		} else {
			// Escaped
			switch (atom.c) {
			case '<':
				op = aem_nfa_append_insn(nfa, aem_nfa_insn_class(0, 1, AEM_NFA_CCLASS_ALNUM));
				break;
			case '>':
				op = aem_nfa_append_insn(nfa, aem_nfa_insn_class(1, 1, AEM_NFA_CCLASS_ALNUM));
				break;
			}

			if (op == RE_PARSE_ERROR) {
				int neg = isupper(atom.c) != 0;
				enum aem_nfa_cclass cclass = AEM_NFA_CCLASS_MAX;
				switch (tolower(atom.c)) {
					case 'w': cclass = AEM_NFA_CCLASS_ALNUM ; break;
					case 'a': cclass = AEM_NFA_CCLASS_ALPHA ; break;
					case 'b': cclass = AEM_NFA_CCLASS_BLANK ; break;
					//case ' ': cclass = AEM_NFA_CCLASS_CNTRL ; break;
					case 'd': cclass = AEM_NFA_CCLASS_DIGIT ; break;
					//case ' ': cclass = AEM_NFA_CCLASS_GRAPH ; break;
					case 'l': cclass = AEM_NFA_CCLASS_LOWER ; break;
					//case ' ': cclass = AEM_NFA_CCLASS_PRINT ; break;
					case 'p': cclass = AEM_NFA_CCLASS_PUNCT ; break;
					case 's': cclass = AEM_NFA_CCLASS_SPACE ; break;
					// collides with unicode \u, so this never happens
					case 'u': cclass = AEM_NFA_CCLASS_UPPER ; break;
					// collides with hex \x, but that's disabled
					case 'x': cclass = AEM_NFA_CCLASS_XDIGIT; break;
				}
				if (cclass != AEM_NFA_CCLASS_MAX) {
					op = aem_nfa_append_insn(nfa, aem_nfa_insn_class(neg, 0, cclass));
				}
			}
			if (op == RE_PARSE_ERROR) {
				aem_logf_ctx(AEM_LOG_WARN, "Unnecessary escape: \\%c", atom.c);
				op = aem_nfa_append_insn(nfa, aem_nfa_insn_char(atom.c));
			}
		}
		aem_nfa_set_dbg(nfa, op, node->text, match);
		break;
	}
	case RE_NODE_REPEAT: {
		aem_assert(node->children.n == 1);
		struct re_node *child = node->children.s[0];
		struct re_node_repeat repeat = node->args.repeat;

		AEM_LOG_MULTI(out, AEM_LOG_DEBUG) {
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
			size_t rep = re_node_compile(nfa, child, match);
			if (rep == RE_PARSE_ERROR)
				return RE_PARSE_ERROR;
			last_rep = rep;
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
				if (re_node_compile(nfa, child, match) == RE_PARSE_ERROR)
					return RE_PARSE_ERROR;
				aem_nfa_put_insn(nfa, fork, aem_nfa_insn_fork(nfa->n_insns));
				aem_nfa_set_dbg(nfa, fork, node->text, match);
			}
		} else if (repeat.min && bounds_remain == UINT_MAX) {
			size_t fork = aem_nfa_append_insn(nfa, aem_nfa_insn_fork(last_rep));
			aem_nfa_set_dbg(nfa, fork, node->text, match);
		} else if (!repeat.min && bounds_remain == UINT_MAX) {
			size_t fork = aem_nfa_append_insn(nfa, aem_nfa_insn_fork(0-0));
			if (re_node_compile(nfa, child, match) == RE_PARSE_ERROR)
				return RE_PARSE_ERROR;
			if (nfa->n_insns == fork) {
				aem_logf_ctx(AEM_LOG_ERROR, "Nothing inside {,}!");
				return RE_PARSE_ERROR;
			}
			size_t jmp = aem_nfa_append_insn(nfa, aem_nfa_insn_jmp(fork));
			aem_nfa_put_insn(nfa, fork, aem_nfa_insn_fork(nfa->n_insns));
			aem_nfa_set_dbg(nfa, fork, node->text, match);
			aem_nfa_set_dbg(nfa, jmp, node->text, match);
		} else {
			aem_assert(!"Can't happen!");
		}

		break;
	}
	case RE_NODE_CAPTURE: {
		aem_assert(node->children.n == 1);
		struct re_node *child = node->children.s[0];
#if AEM_NFA_CAPTURES
		struct re_node_capture capture = node->args.capture;
		if (capture.capture == RE_PARSE_ERROR)
			capture.capture = aem_nfa_new_capture(nfa, match);
		size_t c0 = aem_nfa_append_insn(nfa, aem_nfa_insn_capture(0, capture.capture));
		aem_nfa_set_dbg(nfa, c0, aem_stringslice_new_len(node->text.start, 1), match);
#endif
		if (re_node_compile(nfa, child, match) == RE_PARSE_ERROR)
			return RE_PARSE_ERROR;
#if AEM_NFA_CAPTURES
		if (nfa->n_insns == c0) {
			aem_logf_ctx(AEM_LOG_ERROR, "Nothing to capture!");
			return RE_PARSE_ERROR;
		}
		size_t c1 = aem_nfa_append_insn(nfa, aem_nfa_insn_capture(1, capture.capture));
		aem_nfa_set_dbg(nfa, c1, aem_stringslice_new_len(node->text.end-1, 1), match);
#endif
		break;
	}
	case RE_NODE_BRANCH: {
		AEM_STACK_FOREACH(i, &node->children) {
			struct re_node *child = node->children.s[i];
			if (re_node_compile(nfa, child, match) == RE_PARSE_ERROR)
				return RE_PARSE_ERROR;
		}
		break;
	}
	case RE_NODE_ALTERNATION: {
#if 0
		re_node_gen_alternation(nfa, node, match);
#else
		aem_assert(node->children.n == 2);

		size_t fork = aem_nfa_append_insn(nfa, aem_nfa_insn_fork(0-0));

		struct re_node *child0 = node->children.s[0];
		aem_assert(child0);
		size_t left = re_node_compile(nfa, child0, match);
		if (left == RE_PARSE_ERROR)
			return RE_PARSE_ERROR;

		size_t jmp = aem_nfa_append_insn(nfa, aem_nfa_insn_jmp(0-0));
		aem_nfa_put_insn(nfa, fork, aem_nfa_insn_fork(nfa->n_insns));
		aem_nfa_set_dbg(nfa, fork, node->text, match);

		struct re_node *child1 = node->children.s[1];
		aem_assert(child1);
		size_t right = re_node_compile(nfa, child1, match);
		if (right == RE_PARSE_ERROR)
			return RE_PARSE_ERROR;

		aem_nfa_put_insn(nfa, jmp, aem_nfa_insn_jmp(nfa->n_insns));
		aem_nfa_set_dbg(nfa, jmp, node->text, match);
#endif
		break;
	}
	default:
		aem_logf_ctx(AEM_LOG_ERROR, "Invalid node->type %#x", node->type);
		return RE_PARSE_ERROR;
	}

	return entry;
}

int aem_regex_parse(struct aem_nfa *nfa, struct aem_stringslice re, unsigned int match)
{
	aem_assert(nfa);

	struct aem_stringslice re_init = re;

	struct re_node *root = re_parse_pattern(&re);
	if (aem_stringslice_ok(re)) {
		AEM_LOG_MULTI(out, AEM_LOG_ERROR) {
			aem_stringbuf_puts(out, "Garbage after RE: ");
			aem_string_escape(out, re);
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

	size_t n_insns = nfa->n_insns;
#if AEM_NFA_CAPTURES
	size_t n_captures = nfa->n_captures;
#endif
	size_t entry = re_node_compile(nfa, root, match);
	re_node_free(root);

	if (entry == RE_PARSE_ERROR) {
		aem_logf_ctx(AEM_LOG_ERROR, "Invalid regex!");
		// Restore NFA to how it was before we started breaking stuff
		nfa->n_insns = n_insns;
#if AEM_NFA_CAPTURES
		nfa->n_captures = n_captures;
#endif
		return 1;
	}

	// If we get to the end, record a match and save
	// the complete regex in the MATCH instruction.
	size_t last = aem_nfa_append_insn(nfa, aem_nfa_insn_match(match));
	aem_nfa_set_dbg(nfa, last, re_init, match);

	// Mark entry point as such.
	nfa->thr_init[entry >> 5] |= (1 << (entry & 0x1f));

	return 0;
}
