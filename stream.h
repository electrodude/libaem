#ifndef AEM_STREAM_H
#define AEM_STREAM_H

#include <aem/pmcrcu.h>
#include <aem/stringbuf.h>
#include <aem/stringslice.h>

// Stream finished flag
// Meaning:
// - sink/downstream/consume input/provide output:
// 	- Upstream terminated connection and will provide no further data.
// - source/upstream/provide input/consume output:
// 	- Downstream terminated connection and will accept no further data.
#define AEM_STREAM_FIN 0x01

// Stream needs more input
// Meaning:
// - sink/downstream/consume input/provide output:
// 	- Invalid
// - source/upstream/provide input/consume output:
// 	- Downstream has some pending data but can't process it until it receives further input.
#define AEM_STREAM_NEED_MORE 0x02

// TODO: Does it make sense to FIN and disconnect a stream, and then reconnect the source or sink it somewhere else?

struct aem_stream;

struct aem_stream_source {
	struct aem_stream *stream;
	int (*provide)(struct aem_stream_source *source);

	int flags;
};

struct aem_stream_sink {
	struct aem_stream *stream;
	int (*consume)(struct aem_stream_sink *sink);

	int flags;
};

enum aem_stream_state {
	AEM_STREAM_IDLE = 0,
	AEM_STREAM_PROVIDING,
	AEM_STREAM_CONSUMING,
};
struct aem_stream {
	struct aem_stringbuf buf;

	struct aem_stream_source *source;
	struct aem_stream_sink *sink;

	struct rcu_head rcu_head;

	// Used to ensure that consume() never calls provide(), and that the
	// stream is never simultaneously active more than once in the same
	// direction.
	enum aem_stream_state state;
};

/// Constructor/destructor
struct aem_stream_source *aem_stream_source_init(struct aem_stream_source *source, int (*provide)(struct aem_stream_source *source));
void aem_stream_source_dtor(struct aem_stream_source *source);
struct aem_stream_sink *aem_stream_sink_init(struct aem_stream_sink *sink, int (*consume)(struct aem_stream_sink *sink));
void aem_stream_sink_dtor(struct aem_stream_sink *sink);

/// Attach/detach
struct aem_stream *aem_stream_connect(struct aem_stream_source *source, struct aem_stream_sink *sink);

// These must not be called from within aem_stream_{provide,consume}_{begin,end} pairs.
void aem_stream_source_detach(struct aem_stream_source *source);
void aem_stream_sink_detach(struct aem_stream_sink *sink);

/// Stream data flow
int aem_stream_flow(struct aem_stream *stream, int flags);

size_t aem_stream_avail(struct aem_stream *stream);

int aem_stream_should_provide(struct aem_stream_source *source);
struct aem_stringbuf *aem_stream_provide_begin(struct aem_stream_source *source);
void aem_stream_provide_end(struct aem_stream_source *source);

struct aem_stringslice aem_stream_consume_begin(struct aem_stream_sink *sink);
// Pass in a NULL stringslice to cancel consume.
void aem_stream_consume_end(struct aem_stream_sink *sink, struct aem_stringslice s);

#endif /* AEM_STREAM_H */
