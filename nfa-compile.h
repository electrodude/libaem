#ifndef AEM_NFA_COMPILE_H
#define AEM_NFA_COMPILE_H

// Don't include this yourself unless you're defining your own pattern compiler.

#include <aem/nfa.h>
#include <aem/stack.h>
#include <aem/stringbuf.h>

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

struct re_node *re_node_new(enum re_node_type type);
void re_node_free(struct re_node *node);

/// AST construction
void re_node_push(struct re_node *node, struct re_node *child);
void re_node_sexpr(struct aem_stringbuf *out, const struct re_node *node);

// Flags
#define AEM_REGEX_FLAGS_DEFINE(FLAG) \
	/*                  name            ,flag,safe,value*/ \
	FLAG(AEM_REGEX_FLAG_DEBUG            , "d", 0, 0x01) \
	FLAG(AEM_REGEX_FLAG_EXPLICIT_CAPTURES, "c", 1, 0x02) \
	FLAG(AEM_REGEX_FLAG_BINARY           , "b", 1, 0x20)

enum aem_regex_flags {
#define X(name, flag, safe, value) \
	name = value,
	AEM_REGEX_FLAGS_DEFINE(X)
#undef X
};

enum aem_regex_flags re_flags_parse(struct aem_stringslice *in, int sandbox);
enum aem_regex_flags re_flags_adj(struct aem_stringslice *in, enum aem_regex_flags flags, int sandbox);
void re_flags_describe(struct aem_stringbuf *out, enum aem_regex_flags flags, int sandbox);


/// AST compilation
struct re_compile_ctx {
	struct aem_stringslice in;
	struct aem_nfa *nfa;
	unsigned int n_captures;
	int match;

	int rc;
	//uint32_t final; // Stop parsing when this codepoint is found unescaped.

	enum aem_regex_flags flags;

	//void *arg;
};

int aem_nfa_add(struct aem_nfa *nfa, struct aem_stringslice *in, int match, struct aem_stringslice flags, struct re_node *(*compile)(struct re_compile_ctx *ctx));

#define AEM_NFA_ADD_DEFINE(name) \
int aem_nfa_add_##name(struct aem_nfa *nfa, struct aem_stringslice pat, int match, struct aem_stringslice flags) \
{ \
	return aem_nfa_add(nfa, &pat, match, flags, aem_##name##_compile); \
}

#endif /* AEM_NFA_COMPILE_H */
