#define AEM_INTERNAL
#include <aem/log.h>
#include <aem/memory.h>

#include "streams.h"


/// Stream transducer
static void aem_stream_transducer_consume(struct aem_stream_sink *sink)
{
	aem_assert(sink);
	aem_assert(sink->stream);

	struct aem_stream_transducer *tr = aem_container_of(sink, struct aem_stream_transducer, sink);

	struct aem_stream *stream_sink = sink->stream;
	if (!stream_sink) {
		aem_logf_ctx(AEM_LOG_BUG, "RX disconnected");
		aem_stream_transducer_close(tr);
		return;
	}

	struct aem_stream_source *source = &tr->source;

	struct aem_stream *stream_source = source->stream;
	if (!stream_source) {
		aem_logf_ctx(AEM_LOG_BUG, "TX disconnected");
		aem_stream_transducer_close(tr);
		return;
	}

	if (!aem_stream_propagate_down(source, sink))
	// TODO BUG: Check for stream closure here
		return;

	struct aem_stringbuf *out = aem_stream_provide_begin(source, 1);
	aem_assert(out);

	struct aem_stringslice in = aem_stream_consume_begin(sink);
	if (!in.start)
		goto done_out;

	aem_logf_ctx(AEM_LOG_DEBUG2, "%zd bytes, flags up %d, down %d", aem_stringslice_len(in), stream_sink->flags, stream_source->flags);

	while (aem_stringslice_ok(in)) {
		struct aem_stringslice in_prev = in;

		aem_assert(tr->go);
		tr->go(tr, out, &in, stream_sink->flags & AEM_STREAM_FIN);

		if (in.start == in_prev.start) {
			break;
		}
	}

	if (!aem_stringslice_ok(in))
		stream_sink->flags &= ~AEM_STREAM_FULL;

	aem_logf_ctx(AEM_LOG_DEBUG, "done: %zd bytes remain", aem_stringslice_len(in));

	if (stream_sink->flags & AEM_STREAM_FIN || stream_source->flags & AEM_STREAM_FIN) {
		if (aem_stringslice_ok(in))
			aem_logf_ctx(AEM_LOG_WARN, "go() left %zd bytes unconsumed at stream termination!", aem_stringslice_len(in));
		else
			stream_source->flags |= AEM_STREAM_FIN;
	}

	aem_stream_consume_end(sink, in);

done_out:
	aem_stream_provide_end(source);
}
static void aem_stream_transducer_provide(struct aem_stream_source *source)
{
	aem_assert(source);

	struct aem_stream_transducer *tr = aem_container_of(source, struct aem_stream_transducer, source);

	struct aem_stream_sink *sink = &tr->sink;

	aem_stream_propagate_up(source, sink);
}

struct aem_stream_transducer *aem_stream_transducer_init(struct aem_stream_transducer *tr)
{
	aem_assert(tr);

	aem_stream_sink_init(&tr->sink, aem_stream_transducer_consume);
	aem_stream_source_init(&tr->source, aem_stream_transducer_provide);

	tr->go = NULL;
	tr->on_close = NULL;

	return tr;
}
void aem_stream_transducer_dtor_rcu(struct aem_stream_transducer *tr)
{
	aem_assert(tr);

	// Shouldn't matter
	// TODO: In fact, would it be better to `aem_assert(!tr->on_close);`?
	aem_stream_transducer_close(tr);

	aem_stream_sink_dtor(&tr->sink);
	aem_stream_source_dtor(&tr->source);
}

void aem_stream_transducer_close(struct aem_stream_transducer *tr)
{
	aem_assert(tr);

	if (!tr->on_close)
		return;

	void (*on_close)(struct aem_stream_transducer *tr) = tr->on_close;
	// Ensure on_close is never called more than once
	tr->on_close = NULL;

	on_close(tr);
}
