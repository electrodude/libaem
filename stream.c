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

	aem_stream_source_detach(source, flags);

	source->provide = NULL;
	aem_assert(!source->stream); // aem_stream_source_detach should have ensured this
}

struct aem_stream_sink *aem_stream_sink_init(struct aem_stream_sink *sink, int (*consume)(struct aem_stream_sink *sink, struct aem_stringslice *in, int flags))
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

	aem_stream_sink_detach(sink, flags);

	sink->consume = NULL;
	aem_assert(!sink->stream); // aem_stream_sink_detach should have ensured this
}

static struct aem_stream *aem_stream_new(void)
{
	struct aem_stream *buf = malloc(sizeof(*buf));
	aem_assert(buf);

	aem_stringbuf_init(&buf->buf);

	buf->providing = 0;
	buf->consuming = 0;
	buf->flags = 0;

	buf->source = NULL;
	buf->sink = NULL;

	return buf;
}
static void aem_stream_free_rcu(struct rcu_head *rcu_head)
{
	struct aem_stream *buf = aem_container_of(rcu_head, struct aem_stream, rcu_head);

	aem_stringbuf_dtor(&buf->buf);

	free(buf);
}
void aem_stream_free(struct aem_stream *buf)
{
	if (!buf)
		return;

	//aem_assert(!buf->lock);

	aem_assert(!buf->source);
	aem_assert(!buf->sink);

	call_rcu(&buf->rcu_head, aem_stream_free_rcu);
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
			aem_stream_consume(stream);

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

		if (stream->source)
			aem_stream_provide(stream);

		stream->sink = NULL;
		sink->stream = NULL;
		if (!stream->source)
			aem_stream_free(stream);
	}
}

/// Stream data flow
int aem_stream_provide(struct aem_stream *buf)
{
	aem_assert(buf);

	struct aem_stream_source *source = buf->source;
	aem_assert(source);
	aem_assert(source->stream == buf);
	aem_assert(source->provide);

	aem_assert(!buf->consuming);
	aem_assert(!buf->providing);
	buf->providing = 1;

	int rc = source->provide(source, buf->flags);

	buf->providing = 0;

	return rc;
}

int aem_stream_consume(struct aem_stream *buf)
{
	aem_assert(buf);

	struct aem_stream_sink *sink = buf->sink;
	aem_assert(sink);
	aem_assert(sink->stream == buf);
	aem_assert(sink->consume);

	aem_assert(!buf->consuming);
	buf->consuming = 1;

	const struct aem_stringslice in_orig = aem_stringslice_new_str(&buf->buf);
	struct aem_stringslice in = in_orig;

	int rc = sink->consume(sink, &in, buf->flags);

	// You can't just return some other unrelated stream
	aem_assert(in.start >= in_orig.start);
	aem_assert(in.end == in_orig.end);
	struct aem_stringslice consumed = aem_stringslice_new(in_orig.start, in.start);

	if ((buf->flags & AEM_STREAM_FIN) && aem_stringslice_len(in)) {
		aem_logf_ctx(AEM_LOG_WARN, "Stream finished, but consume left %zd bytes unprocessed!\n", aem_stringslice_len(in));
	}

	size_t n_less = aem_stringslice_len(consumed);

	aem_stringbuf_pop_front(&buf->buf, n_less);

	buf->consuming = 0;

	return rc;
}


struct aem_stringbuf *aem_stream_source_getbuf(struct aem_stream_source *source)
{
	if (!source)
		return NULL;

	struct aem_stream *stream = source->stream;
	if (!stream)
		return NULL;

	return &stream->buf;
}
