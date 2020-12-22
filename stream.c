#include <stdlib.h>

#include <aem/compiler.h>
#include <aem/log.h>

#include "stream.h"

/// Constructor/destructor
struct aem_stream_source *aem_stream_source_init(struct aem_stream_source *source, int (*provide)(struct aem_stream_source *source, int flags))
{
	aem_assert(source);
	//aem_assert(provide);

	source->stream = NULL;
	source->provide = provide;

	return source;
}
void aem_stream_source_dtor(struct aem_stream_source *source, int flags)
{
	if (!source)
		return;

	source->provide = NULL;
	aem_assert(!source->stream); // aem_stream_source_detach should have ensured this
}

struct aem_stream_sink *aem_stream_sink_init(struct aem_stream_sink *sink, int (*consume)(struct aem_stream_sink *sink, int flags))
{
	aem_assert(sink);
	aem_assert(consume);

	sink->stream = NULL;
	sink->consume = consume;

	return sink;
}
void aem_stream_sink_dtor(struct aem_stream_sink *sink, int flags)
{
	if (!sink)
		return;

	sink->consume = NULL;
	aem_assert(!sink->stream); // aem_stream_sink_detach should have ensured this
}

static struct aem_stream *aem_stream_new(void)
{
	struct aem_stream *stream = malloc(sizeof(*stream));
	aem_assert(stream);

	aem_stringbuf_init(&stream->buf);

	stream->state = AEM_STREAM_IDLE;
	stream->flags = 0;

	stream->source = NULL;
	stream->sink = NULL;

	return stream;
}
void aem_stream_free_rcu(struct rcu_head *rcu_head)
{
	struct aem_stream *stream = aem_container_of(rcu_head, struct aem_stream, rcu_head);

	aem_stringbuf_dtor(&stream->buf);

	free(stream);
}
void aem_stream_free(struct aem_stream *stream)
{
	if (!stream)
		return;

	aem_assert(!stream->source);
	aem_assert(!stream->sink);

	call_rcu(&stream->rcu_head, aem_stream_free_rcu);
}

/// Attach/detach
int aem_stream_connect(struct aem_stream_source *source, struct aem_stream_sink *sink)
{
	aem_assert(source);
	aem_assert(sink);

	if (source->stream)
		aem_assert(source->stream->source == source);
	if (sink->stream)
		aem_assert(sink->stream->sink == sink);

	struct aem_stream *stream = NULL;
	if (source->stream && sink->stream) {
		if (source->stream == sink->stream)
			// Already connected to each other
			return 0;
		else
			// Already connected to something else
			return -1;
	} else if (source->stream) {
		// The source already has a stream - use that
		stream = source->stream;
	} else if (sink->stream) {
		// The sink already has a stream - use that
		stream = sink->stream;
	} else {
		// Create a new stream
		stream = aem_stream_new();
	}

	aem_assert(stream);

	source->stream = stream;
	sink->stream = stream;

	stream->source = source;
	stream->sink = sink;

	return 0;
}

void aem_stream_source_detach(struct aem_stream_source *source, int flags)
{
	if (!source)
		return;

	struct aem_stream *stream = source->stream;
	if (stream) {
		aem_assert(stream->source == source);

		stream->flags |= flags;

		if (stream->sink)
			aem_stream_consume(stream, flags);

		stream->source = NULL;
		source->stream = NULL;
		if (!stream->sink)
			aem_stream_free(stream);
	}
}
void aem_stream_sink_detach(struct aem_stream_sink *sink, int flags)
{
	if (!sink)
		return;

	struct aem_stream *stream = sink->stream;
	if (stream) {
		aem_assert(stream->sink == sink);

		stream->flags |= flags;

		//if (stream->source)
			//aem_stream_provide(stream, flags);

		stream->sink = NULL;
		sink->stream = NULL;
		if (!stream->source)
			aem_stream_free(stream);
	}
}

int aem_stream_add_flags(struct aem_stream *stream, int flags)
{
	if (!stream)
		return -1;

	stream->flags |= flags;

	return 0;
}

/// Stream data flow
int aem_stream_provide(struct aem_stream *stream, int flags)
{
	aem_assert(stream);

	struct aem_stream_source *source = stream->source;
	aem_assert(source);
	aem_assert(source->stream == stream);
	aem_assert(source->provide);

	stream->flags |= flags & AEM_STREAM_FIN;

	int rc = source->provide(source, stream->flags | flags);

	return rc;
}

int aem_stream_consume(struct aem_stream *stream, int flags)
{
	aem_assert(stream);

	struct aem_stream_sink *sink = stream->sink;
	aem_assert(sink);
	aem_assert(sink->stream == stream);
	aem_assert(sink->consume);

	stream->flags |= flags & AEM_STREAM_FIN;

	int rc = sink->consume(sink, stream->flags | flags);

	return rc;
}


struct aem_stringbuf *aem_stream_provide_begin(struct aem_stream_source *source)
{
	aem_assert(source);

	struct aem_stream *stream = source->stream;
	if (!stream)
		return NULL;

	aem_assert(stream);
	aem_assert(stream->source == source);

	aem_assert(stream->state == AEM_STREAM_IDLE);
	stream->state = AEM_STREAM_PROVIDING;

	return &stream->buf;
}
void aem_stream_provide_end(struct aem_stream_source *source)
{
	aem_assert(source);

	struct aem_stream *stream = source->stream;
	aem_assert(stream);
	aem_assert(stream->source == source);

	aem_assert(stream->state == AEM_STREAM_PROVIDING);
	stream->state = AEM_STREAM_IDLE;
}

struct aem_stringslice aem_stream_consume_begin(struct aem_stream_sink *sink)
{
	aem_assert(sink);

	struct aem_stream *stream = sink->stream;
	if (!stream)
		return AEM_STRINGSLICE_EMPTY;

	aem_assert(stream);
	aem_assert(stream->sink == sink);

	aem_assert(stream->state == AEM_STREAM_IDLE);
	stream->state = AEM_STREAM_CONSUMING;

	return aem_stringslice_new_str(&stream->buf);
}

void aem_stream_consume_end(struct aem_stream_sink *sink, struct aem_stringslice s)
{
	aem_assert(sink);
	struct aem_stream *stream = sink->stream;
	aem_assert(stream);
	aem_assert(stream->sink == sink);

	struct aem_stringslice consumed = aem_stringslice_new(stream->buf.s, s.start);

	size_t n_less = aem_stringslice_len(consumed);

	aem_stringbuf_pop_front(&stream->buf, n_less);

	if ((stream->flags & AEM_STREAM_FIN) && stream->buf.n) {
		aem_logf_ctx(AEM_LOG_WARN, "Stream got FIN, but consume left %zd bytes unprocessed!\n", stream->buf.n);
	}

	aem_assert(stream->state == AEM_STREAM_CONSUMING);
	stream->state = AEM_STREAM_IDLE;
}
void aem_stream_consume_cancel(struct aem_stream_sink *sink)
{
	aem_assert(sink);
	struct aem_stream *stream = sink->stream;
	aem_assert(stream);
	aem_assert(stream->sink == sink);

	if ((stream->flags & AEM_STREAM_FIN) && stream->buf.n) {
		aem_logf_ctx(AEM_LOG_WARN, "Stream got FIN, but consume left %zd bytes unprocessed!\n", stream->buf.n);
	}

	aem_assert(stream->state == AEM_STREAM_CONSUMING);
	stream->state = AEM_STREAM_IDLE;
}
