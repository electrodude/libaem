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
	int (*consume)(struct aem_stream_sink *sink, struct aem_stringslice *in, int flags);
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
	int providing : 1;
	int consuming : 1;
};

/// Constructor/destructor
struct aem_stream_source *aem_stream_source_init(struct aem_stream_source *source, int (*provide)(struct aem_stream_source *source, int flags));
void aem_stream_source_dtor(struct aem_stream_source *source, int flags);
struct aem_stream_sink *aem_stream_sink_init(struct aem_stream_sink *sink, int (*consume)(struct aem_stream_sink *sink, struct aem_stringslice *in, int flags));
void aem_stream_sink_dtor(struct aem_stream_sink *sink, int flags);

/// Attach/detach
int aem_stream_connect(struct aem_stream_source *source, struct aem_stream_sink *sink);

void aem_stream_source_detach(struct aem_stream_source *source, int flags);
void aem_stream_sink_detach(struct aem_stream_sink *sink, int flags);

/// Stream data flow
//int aem_stream_source_provide(struct aem_stream_source *source, int flags);
//int aem_stream_sink_consume(struct aem_stream_sink *sink, struct aem_stringslice *in, int flags);
int aem_stream_provide(struct aem_stream *buf);
int aem_stream_consume(struct aem_stream *buf);

struct aem_stringbuf *aem_stream_source_getbuf(struct aem_stream_source *source);

#endif /* AEM_STREAM_H */
