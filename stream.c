#include <stdlib.h>

#include <aem/compiler.h>
#include <aem/log.h>

#include "stream.h"

static int aem_stream_provide(struct aem_stream *stream, int flags_source);
static int aem_stream_consume(struct aem_stream *stream, int flags_sink);

/// Constructor/destructor
struct aem_stream_source *aem_stream_source_init(struct aem_stream_source *source, int (*provide)(struct aem_stream_source *source))
{
	aem_assert(source);
	aem_assert(provide);

	source->stream = NULL;
	source->provide = provide;

	source->flags = 0;

	return source;
}
void aem_stream_source_dtor(struct aem_stream_source *source)
{
	if (!source)
		return;

	if (source->stream)
		aem_stream_source_detach(source);

	source->provide = NULL;
	aem_assert(!source->stream); // aem_stream_source_detach should have ensured this
}

struct aem_stream_sink *aem_stream_sink_init(struct aem_stream_sink *sink, int (*consume)(struct aem_stream_sink *sink))
{
	aem_assert(sink);
	aem_assert(consume);

	sink->stream = NULL;
	sink->consume = consume;

	sink->flags = 0;

	return sink;
}
void aem_stream_sink_dtor(struct aem_stream_sink *sink)
{
	if (!sink)
		return;

	if (sink->stream)
		aem_stream_sink_detach(sink);

	sink->consume = NULL;
	aem_assert(!sink->stream); // aem_stream_sink_detach should have ensured this
}

static struct aem_stream *aem_stream_new(void)
{
	struct aem_stream *stream = malloc(sizeof(*stream));
	aem_assert(stream);

	aem_stringbuf_init(&stream->buf);

	stream->source = NULL;
	stream->sink = NULL;

	stream->state = AEM_STREAM_IDLE;

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
struct aem_stream *aem_stream_connect(struct aem_stream_source *source, struct aem_stream_sink *sink)
{
	aem_assert(source);
	aem_assert(sink);

	if (source->stream)
		aem_assert(source->stream->source == source);
	if (sink->stream)
		aem_assert(sink->stream->sink == sink);

	struct aem_stream *stream = NULL;
	if (source->stream && sink->stream) {
		if (source->stream == sink->stream) {
			aem_assert(stream->state == AEM_STREAM_IDLE);
			// Already connected to each other
			return source->stream;
		} else {
			// Already connected to something else
			return NULL;
		}
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

	aem_assert(stream->state == AEM_STREAM_IDLE);

	return stream;
}

void aem_stream_source_detach(struct aem_stream_source *source)
{
	if (!source)
		return;

	struct aem_stream *stream = source->stream;
	if (!stream)
		return;

	aem_assert(stream->source == source);

	if (stream->sink)
		stream->sink->flags |= AEM_STREAM_FIN;
	if (stream->sink)
		aem_stream_consume(stream, AEM_STREAM_FIN);

	aem_assert(stream->state == AEM_STREAM_IDLE);

	stream->source = NULL;
	source->stream = NULL;

	if (!stream->sink)
		aem_stream_free(stream);
}
void aem_stream_sink_detach(struct aem_stream_sink *sink)
{
	if (!sink)
		return;

	struct aem_stream *stream = sink->stream;
	if (!stream)
		return;

	aem_assert(stream->sink == sink);

	if (stream->source)
		stream->source->flags |= AEM_STREAM_FIN;

	aem_assert(stream->state == AEM_STREAM_IDLE);

	stream->sink = NULL;
	sink->stream = NULL;

	if (!stream->source)
		aem_stream_free(stream);
}


/// Stream data flow
static int aem_stream_provide(struct aem_stream *stream, int flags_source)
{
	aem_assert(stream);

	struct aem_stream_source *source = stream->source;
	aem_assert(source);
	aem_assert(source->stream == stream);
	aem_assert(source->provide);

	// Don't let FIN turn off.
	source->flags = flags_source | (source->flags & AEM_STREAM_FIN);

	if (stream->buf.n > 65536)
		aem_logf_ctx(AEM_LOG_BUG, "Why are you calling provide when you already have %zd bytes?\n", stream->buf.n);

	int flags_sink = source->provide(source);

	aem_assert(!(flags_sink & AEM_STREAM_NEED_MORE));

	if (stream->sink)
		stream->sink->flags = flags_sink;

	return flags_sink;
}

static int aem_stream_consume(struct aem_stream *stream, int flags_sink)
{
	aem_assert(stream);

	aem_assert(!(flags_sink & AEM_STREAM_NEED_MORE));

	struct aem_stream_sink *sink = stream->sink;
	aem_assert(sink);
	aem_assert(sink->stream == stream);
	aem_assert(sink->consume);

	sink->flags = flags_sink | (sink->flags & AEM_STREAM_FIN);

	aem_assert(!(sink->flags & AEM_STREAM_NEED_MORE));

	int flags_source = sink->consume(sink);

	// You can't ask for more if we already said you won't be getting any.
	if (flags_source & AEM_STREAM_FIN)
		aem_assert(!(flags_source & AEM_STREAM_NEED_MORE));

	if (aem_stream_avail(stream) && !(flags_source & AEM_STREAM_NEED_MORE))
		aem_logf_ctx(AEM_LOG_WARN, "Callback left %zd bytes unconsumed without an excuse.\n", stream->buf.n);

	if (stream->source)
		stream->source->flags = flags_source;

	return flags_source;
}

int aem_stream_flow(struct aem_stream *stream, int flags_source)
{
	aem_assert(stream);

	// If nothing is available or if more input is needed, run the whole pipeline
	if (aem_stream_should_provide(stream->source)) {
		return aem_stream_provide(stream, flags_source);
	} else {
		// Otherwise, just try to get rid of data.
		int flags_source2 = aem_stream_consume(stream, flags_source & AEM_STREAM_FIN);
		// If we it turns out we need more input after all, run the whole pipeline.
		if (flags_source2 & AEM_STREAM_NEED_MORE)
			return aem_stream_provide(stream, flags_source2);

		return flags_source2;
	}
}


size_t aem_stream_avail(struct aem_stream *stream)
{
	if (!stream)
		return 0;

	return stream->buf.n;
}


int aem_stream_should_provide(struct aem_stream_source *source)
{
	if (!source)
		return 0;

	struct aem_stream *stream = source->stream;
	if (!stream)
		return 0;

	aem_assert(stream);
	aem_assert(stream->source == source);

	if (stream->state != AEM_STREAM_IDLE)
		return 0;

	return !stream->buf.n || (source->flags & AEM_STREAM_NEED_MORE);
}
struct aem_stringbuf *aem_stream_provide_begin(struct aem_stream_source *source)
{
	aem_assert(source);

	struct aem_stream *stream = source->stream;
	if (!stream)
		return NULL;

	if (stream->state == AEM_STREAM_PROVIDING) {
		aem_logf_ctx(AEM_LOG_WARN, "Nested stream provide!\n");
		return NULL;
	}

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

	struct aem_stream_sink *sink = stream->sink;
	if (sink)
		aem_stream_consume(stream, 0);
}

struct aem_stringslice aem_stream_consume_begin(struct aem_stream_sink *sink)
{
	aem_assert(sink);

	struct aem_stream *stream = sink->stream;
	if (!stream)
		return AEM_STRINGSLICE_EMPTY;

	if (stream->state == AEM_STREAM_CONSUMING) {
		aem_logf_ctx(AEM_LOG_WARN, "Nested stream consume!\n");
		return AEM_STRINGSLICE_EMPTY;
	}

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

	if (s.start) {
		struct aem_stringslice consumed = aem_stringslice_new(stream->buf.s, s.start);

		size_t n_less = aem_stringslice_len(consumed);

		aem_stringbuf_pop_front(&stream->buf, n_less);
	}

	if ((sink->flags & AEM_STREAM_FIN) && stream->buf.n) {
		aem_logf_ctx(AEM_LOG_WARN, "Stream got FIN, but consume left %zd bytes unprocessed!\n", stream->buf.n);
	}

	aem_assert(stream->state == AEM_STREAM_CONSUMING);
	stream->state = AEM_STREAM_IDLE;
}
