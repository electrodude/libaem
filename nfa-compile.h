#ifndef AEM_NFA_COMPILE_H
#define AEM_NFA_COMPILE_H

// Don't include this yourself unless you're defining your own pattern compiler.

#include <aem/nfa.h>
#include <aem/stack.h>
#include <aem/stringbuf.h>

// Failure, invalid address, no address assigned yet, etc.
#define AEM_NFA_PARSE_ERROR ((size_t)-1)

/// Regex parser AST structore
struct aem_nfa_node {
	enum aem_nfa_node_type {
		AEM_NFA_NODE_RANGE,
		AEM_NFA_NODE_BRACKETS,
		AEM_NFA_NODE_ATOM,
		AEM_NFA_NODE_CLASS,
		AEM_NFA_NODE_CAPTURE,
		AEM_NFA_NODE_REPEAT,
		AEM_NFA_NODE_BRANCH,
		AEM_NFA_NODE_ALTERNATION,
	} type;
	struct aem_stringslice text;
	struct aem_stack children;
	union aem_nfa_node_args {
		struct aem_nfa_node_range {
			uint32_t min;
			uint32_t max;
		} range;
		struct aem_nfa_node_brackets {
		} brackets;
		struct aem_nfa_node_atom {
			uint32_t c;
			int esc;
		} atom;
		struct aem_nfa_node_class {
			enum aem_nfa_cclass cclass;
			uint8_t neg      : 1;
			uint8_t frontier : 1;
		} cclass;
		struct aem_nfa_node_capture {
			size_t capture;
		} capture;
		struct aem_nfa_node_repeat {
			unsigned int min;
			unsigned int max;
			int reluctant : 1;
		} repeat;
	} args;
};

struct aem_nfa_node *aem_nfa_node_new(enum aem_nfa_node_type type);
void aem_nfa_node_free(struct aem_nfa_node *node);

/// AST construction
void aem_nfa_node_push(struct aem_nfa_node *node, struct aem_nfa_node *child);
void aem_nfa_node_sexpr(struct aem_stringbuf *out, const struct aem_nfa_node *node);

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

enum aem_regex_flags aem_regex_flags_parse(struct aem_stringslice *in, int sandbox);
enum aem_regex_flags aem_regex_flags_adj(struct aem_stringslice *in, enum aem_regex_flags flags, int sandbox);
void aem_regex_flags_describe(struct aem_stringbuf *out, enum aem_regex_flags flags, int sandbox);


/// AST compilation
struct aem_nfa_compile_ctx {
	struct aem_stringslice in;
	struct aem_nfa *nfa;
	unsigned int n_captures;
	int match;

	int rc;
	//uint32_t final; // Stop parsing when this codepoint is found unescaped.

	enum aem_regex_flags flags;

	//void *arg;
};

int aem_nfa_add(struct aem_nfa *nfa, struct aem_stringslice *in, int match, struct aem_stringslice flags, struct aem_nfa_node *(*compile)(struct aem_nfa_compile_ctx *ctx));

#define AEM_NFA_ADD_DEFINE(name) \
int aem_nfa_add_##name(struct aem_nfa *nfa, struct aem_stringslice pat, int match, struct aem_stringslice flags) \
{ \
	return aem_nfa_add(nfa, &pat, match, flags, aem_##name##_compile); \
}

#endif /* AEM_NFA_COMPILE_H */
