#ifndef AEM_STREAM_H
#define AEM_STREAM_H

#include <aem/rcu.h>
#include <aem/stringbuf.h>
#include <aem/stringslice.h>

// Stream finished flag
// - Upstream terminated connection and will provide no further data.
#define AEM_STREAM_FIN 0x01

// Stream is full (backpressure)
// - Downstream can accept no more data at the moment
#define AEM_STREAM_FULL 0x02

// TODO: Does it make sense to FIN and disconnect a stream, and then reconnect the source or sink it somewhere else?

struct aem_stream;

struct aem_stream_source {
	struct aem_stream *stream;
	void (*provide)(struct aem_stream_source *source);
};

struct aem_stream_sink {
	struct aem_stream *stream;
	void (*consume)(struct aem_stream_sink *sink);
};

struct aem_stream {
	struct aem_stringbuf buf;

	struct aem_stream_source *source;
	struct aem_stream_sink *sink;

	struct rcu_head rcu_head;

	// Used to ensure that the stream is only ever in consume or provide
	// mode, and used to ensure nested providing doesn't get out of hand.
	// Positive: providing
	// Negative: consuming
	int state;

	int flags;
};

/// Constructor/destructor
struct aem_stream_source *aem_stream_source_init(struct aem_stream_source *source, void (*provide)(struct aem_stream_source *source));
void aem_stream_source_dtor(struct aem_stream_source *source);
struct aem_stream_sink *aem_stream_sink_init(struct aem_stream_sink *sink, void (*consume)(struct aem_stream_sink *sink));
void aem_stream_sink_dtor(struct aem_stream_sink *sink);

/// Attach/detach
struct aem_stream *aem_stream_connect(struct aem_stream_source *source, struct aem_stream_sink *sink);

// These must not be called from within aem_stream_{provide,consume}_{begin,end} pairs.
void aem_stream_source_detach(struct aem_stream_source *source);
void aem_stream_sink_detach(struct aem_stream_sink *sink);

/// Stream data flow
int aem_stream_flow(struct aem_stream *stream);

size_t aem_stream_avail(struct aem_stream *stream);

void aem_stream_sink_set_full(struct aem_stream_sink *sink, int full);
int aem_stream_propagate_up(struct aem_stream_source *down, struct aem_stream_sink *up);
int aem_stream_propagate_down(struct aem_stream_source *down, struct aem_stream_sink *up);
int aem_stream_should_provide(struct aem_stream_source *source);
// TODO: if (!nested && !force && stream->buf.n) return NULL;
struct aem_stringbuf *aem_stream_provide_begin(struct aem_stream_source *source, int force);
void aem_stream_provide_end(struct aem_stream_source *source);

struct aem_stringslice aem_stream_consume_begin(struct aem_stream_sink *sink);
// Pass in a NULL stringslice to cancel consume.
void aem_stream_consume_end(struct aem_stream_sink *sink, struct aem_stringslice s);

#endif /* AEM_STREAM_H */
