#include <aem/compiler.h>
#include <aem/log.h>
//#include <aem/pmcrcu.h>

#include "streams.h"


/// Streams
int aem_stream_sink_lines_consume(struct aem_stream_sink *sink, struct aem_stringslice *in, int flags)
{
	aem_assert(sink);
	aem_assert(in);

	//struct aem_stream_sink_rcu *sink_rcu = aem_container_of(sink, struct aem_stream_sink_rcu, sink);
	struct aem_stream_sink_lines *lines = aem_container_of(sink, struct aem_stream_sink_lines, sink);

	// If the the buffer fed to the previous call to consume() ended with a
	// CR, but then the first byte this time was LF, then it was really a
	// CRLF split across two calls to consume(); just drop the LF.
	if (lines->ended_cr && aem_stringslice_match(in, "\n")) {
		aem_logf_ctx(AEM_LOG_WARN, "Dropping missed LF from a CRLF split across a packet boundary\n");
	}

	struct aem_stringslice p = *in;
	int newline = 0; // Declared outside the loop so we can capture its last value
	while (aem_stringslice_ok(p)) {
		struct aem_stringslice p_prev = p;
		newline = aem_stringslice_match_newline(&p);
		if (newline) {
			// If we found a newline, take everything from after
			// the last newline to after this one, and advance the
			// input buffer to after the line we just found.
			struct aem_stringslice line = aem_stringslice_new(in->start, lines->want_newline ? p.start : p_prev.start);
			*in = p;

			// Only pass FIN to the callback if this is indeed the last of the data.
			int flags_line = flags;
			if (aem_stringslice_ok(*in))
				flags_line &= ~AEM_STREAM_FIN;

			aem_assert(lines->consume_line);
			lines->consume_line(lines, line, flags_line);
		} else {
			aem_stringslice_getc(&p);
		}
	}

	// If FIN is set but a partial line still remains, pass the partial
	// line to the callback with FIN set.
	if ((flags & AEM_STREAM_FIN) && aem_stringslice_ok(*in)) {
		aem_assert(lines->consume_line);
		lines->consume_line(lines, *in, flags);
		// Consume the line
		in->start = in->end;

		//aem_stream_sink_rcu_free(&lines->sink);
	}

	// If the buffer isn't empty, it must not have ended with any sort of
	// newline, so we don't have to worry about it ending with the CR of a
	// split CR-LF.
	lines->ended_cr = !aem_stringslice_ok(*in) && newline == 1;

	return 0;
}
struct aem_stream_sink_lines *aem_stream_sink_lines_init(struct aem_stream_sink_lines *lines)
{
	aem_assert(lines);
	aem_assert(lines->consume_line);
	aem_stream_sink_init(&lines->sink, aem_stream_sink_lines_consume);
	lines->want_newline = 1;
	lines->ended_cr = 0;
	return lines;
}
void aem_stream_sink_lines_dtor(struct aem_stream_sink_lines *lines)
{
	if (!lines)
		return;

	// TODO: Deal with any remaining characters?
	aem_stream_sink_dtor(&lines->sink, AEM_STREAM_FIN);
}
