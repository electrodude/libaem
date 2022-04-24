#include <stdlib.h>

#define AEM_INTERNAL
#include <aem/log.h>
#include <aem/memory.h>

#include "stream.h"

static int aem_stream_provide(struct aem_stream *stream);
static int aem_stream_consume(struct aem_stream *stream);

/// Constructor/destructor
struct aem_stream_source *aem_stream_source_init(struct aem_stream_source *source, void (*provide)(struct aem_stream_source *source))
{
	aem_assert(source);
	aem_assert(provide);

	source->stream = NULL;
	source->provide = provide;

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

struct aem_stream_sink *aem_stream_sink_init(struct aem_stream_sink *sink, void (*consume)(struct aem_stream_sink *sink))
{
	aem_assert(sink);
	aem_assert(consume);

	sink->stream = NULL;
	sink->consume = consume;

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

	stream->state = 0;

	stream->flags = 0;

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
			aem_assert(!stream->state);
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

	aem_assert(!stream->state);

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

	stream->flags = AEM_STREAM_FIN;
	if (stream->sink)
		aem_stream_consume(stream);

	aem_assert(!stream->state);

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

	stream->flags |= AEM_STREAM_FIN;

	aem_assert(!stream->state);

	stream->sink = NULL;
	sink->stream = NULL;

	if (!stream->source)
		aem_stream_free(stream);
}


/// Stream data flow
static int aem_stream_provide(struct aem_stream *stream)
{
	aem_assert(stream);

	struct aem_stream_source *source = stream->source;
	if (!source) {
		aem_logf_ctx(AEM_LOG_BUG, "Source was NULL!");
		return -1;
	}
	aem_assert(source);
	aem_assert(source->stream == stream);
	aem_assert(source->provide);

#ifdef AEM_DEBUG
	int flags_orig = stream->flags;
#endif

	if (stream->buf.n > 65536)
		aem_logf_ctx(AEM_LOG_BUG, "Why are you calling provide when you already have %zd bytes?", stream->buf.n);

	source->provide(source);

#ifdef AEM_DEBUG
	if ((flags_orig & AEM_STREAM_FIN) && !(stream->flags & AEM_STREAM_FIN))
		aem_logf_ctx(AEM_LOG_BUG, "FIN got turned off by source %p!", source);
#endif

	return 0;
}

static int aem_stream_consume(struct aem_stream *stream)
{
	aem_assert(stream);

	struct aem_stream_sink *sink = stream->sink;
	if (!sink) {
		aem_logf_ctx(AEM_LOG_BUG, "Sink was NULL!");
		return -1;
	}
	aem_assert(sink);
	aem_assert(sink->stream == stream);
	aem_assert(sink->consume);

#ifdef AEM_DEBUG
	int flags_orig = stream->flags;
#endif

	size_t n_pre = stream->buf.n;

	sink->consume(sink);

	if (stream->buf.n > n_pre)
		aem_logf_ctx(AEM_LOG_BUG, "Stream buffer has more contents (%zd => %zd) after calling ->consume!", n_pre, stream->buf.n);

#ifdef AEM_DEBUG
	if ((flags_orig & AEM_STREAM_FIN) && !(stream->flags & AEM_STREAM_FIN))
		aem_logf_ctx(AEM_LOG_BUG, "FIN got turned off by sink %p!", sink);
#endif

	return 0;
}

int aem_stream_flow(struct aem_stream *stream)
{
	// Just call provide no matter what.  It can decide for itself whether or not it actually wants to provide anything.
	return aem_stream_provide(stream);
}


size_t aem_stream_avail(struct aem_stream *stream)
{
	if (!stream)
		return 0;

	return stream->buf.n;
}

void aem_stream_sink_set_full(struct aem_stream_sink *sink, int full)
{
	if (!sink)
		return;

	struct aem_stream *stream = sink->stream;
	if (!stream)
		return;

	if (full)
		stream->flags |= AEM_STREAM_FULL;
	else
		stream->flags &= ~AEM_STREAM_FULL;
}
int aem_stream_propagate_up(struct aem_stream_source *down, struct aem_stream_sink *up)
{
	int ok = aem_stream_should_provide(down);

	aem_stream_sink_set_full(up, !ok);

	aem_stream_provide(up->stream);

	return ok;
}
int aem_stream_propagate_down(struct aem_stream_source *down, struct aem_stream_sink *up)
{
	aem_assert(down);
	int ok = aem_stream_should_provide(down);

	aem_stream_sink_set_full(up, !ok);

	if (!ok)
		aem_stream_consume(down->stream);

	return ok;
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

	if (stream->state)
		return 0;

	if (!stream->buf.n) {
		if (stream->flags & AEM_STREAM_FULL) {
			aem_logf_ctx(AEM_LOG_WARN, "Clearing FULL flag from empty stream %p", stream);
			stream->flags &= ~AEM_STREAM_FULL;
		}
		return 1;
	}

	if (stream->flags & AEM_STREAM_FULL)
		return 0;

	return 1;
}
struct aem_stringbuf *aem_stream_provide_begin(struct aem_stream_source *source, int force)
{
	aem_assert(source);

	struct aem_stream *stream = source->stream;
	if (!stream)
		return NULL;

	aem_assert(stream);
	aem_assert(stream->source == source);

	// Ensure no consumes are active
	aem_assert(stream->state >= 0);
	// One more provide is now active
	stream->state++;

	// Complain if things get out of hand
	if (stream->state >= 256)
		aem_logf_ctx(AEM_LOG_WARN, "%zd nested provides on stream %s!", stream->state, stream);

	return &stream->buf;
}
void aem_stream_provide_end(struct aem_stream_source *source)
{
	aem_assert(source);

	struct aem_stream *stream = source->stream;
	aem_assert(stream);
	aem_assert(stream->source == source);

	// Ensure at least one provide is active
	aem_assert(stream->state > 0);
	// One fewer provide is now active
	stream->state--;

	if (!stream->state) {
		struct aem_stream_sink *sink = stream->sink;
		if (sink)
			aem_stream_consume(stream);
	}
}

struct aem_stringslice aem_stream_consume_begin(struct aem_stream_sink *sink)
{
	aem_assert(sink);

	struct aem_stream *stream = sink->stream;
	if (!stream)
		return AEM_STRINGSLICE_EMPTY;

	if (stream->state < 0) {
		aem_logf_ctx(AEM_LOG_WARN, "Nested stream consume!");
		return AEM_STRINGSLICE_EMPTY;
	}

	aem_assert(stream);
	aem_assert(stream->sink == sink);

	// Ensure stream is idle.  Unlike nested provides, nested consumes
	// aren't safe because the underlying stringbuf isn't updated until
	// consume_end is called.
	aem_assert(!stream->state);

	// One more consume is now active.
	stream->state--;

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

		size_t n_consumed = aem_stringslice_len(consumed);

		aem_stringbuf_pop_front(&stream->buf, n_consumed);

		// If the buffer is excessively large, shrink it.
		if (stream->buf.maxn > stream->buf.n*4 + 4096) {
			aem_stringbuf_shrinkwrap(&stream->buf);
		}
	}

	if ((stream->flags & AEM_STREAM_FIN) && stream->buf.n)
		aem_logf_ctx(AEM_LOG_WARN, "Stream got FIN, but consume left %zd bytes unprocessed!", stream->buf.n);

	// Ensure at least one consume is active.
	aem_assert(stream->state < 0);
	// One fewer consume is now active.
	stream->state++;
}
