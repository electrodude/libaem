#ifndef AEM_STREAM_H
#define AEM_STREAM_H

#include <aem/pmcrcu.h>
#include <aem/stringbuf.h>
#include <aem/stringslice.h>

#define AEM_STREAM_FIN 0x01

struct aem_stream;

struct aem_stream_source {
	struct aem_stream *stream;
	int (*provide)(struct aem_stream_source *source, int flags);
};

struct aem_stream_sink {
	struct aem_stream *stream;
	int (*consume)(struct aem_stream_sink *sink, int flags);
};

enum aem_stream_state {
	AEM_STREAM_IDLE,
	AEM_STREAM_PROVIDING,
	AEM_STREAM_CONSUMING,
};
struct aem_stream {
	struct aem_stringbuf buf;

	int flags;

	struct aem_stream_source *source;
	struct aem_stream_sink *sink;

	struct rcu_head rcu_head;

	// Used to ensure that consume() never calls provide(), and that the
	// stream is never simultaneously active more than once in the same
	// direction.
	enum aem_stream_state state;
};

/// Constructor/destructor
struct aem_stream_source *aem_stream_source_init(struct aem_stream_source *source, int (*provide)(struct aem_stream_source *source, int flags));
void aem_stream_source_dtor(struct aem_stream_source *source, int flags);
struct aem_stream_sink *aem_stream_sink_init(struct aem_stream_sink *sink, int (*consume)(struct aem_stream_sink *sink, int flags));
void aem_stream_sink_dtor(struct aem_stream_sink *sink, int flags);

/// Attach/detach
int aem_stream_connect(struct aem_stream_source *source, struct aem_stream_sink *sink);

// These must not be called from within aem_stream_{provide,consume}_{begin,end,cancel} pairs.
void aem_stream_source_detach(struct aem_stream_source *source, int flags);
void aem_stream_sink_detach(struct aem_stream_sink *sink, int flags);

int aem_stream_add_flags(struct aem_stream *stream, int flags);

/// Stream data flow
int aem_stream_provide(struct aem_stream *stream, int flags);
int aem_stream_consume(struct aem_stream *stream, int flags);

struct aem_stringbuf *aem_stream_provide_begin(struct aem_stream_source *source);
void aem_stream_provide_end(struct aem_stream_source *source);

struct aem_stringslice aem_stream_consume_begin(struct aem_stream_sink *sink);
void aem_stream_consume_end(struct aem_stream_sink *sink, struct aem_stringslice s);
void aem_stream_consume_cancel(struct aem_stream_sink *sink);

#endif /* AEM_STREAM_H */
