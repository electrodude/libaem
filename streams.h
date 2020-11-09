#ifndef AEM_STREAMS_H
#define AEM_STREAMS_H

#include <aem/stream.h>

struct aem_stream_sink_lines
{
	struct aem_stream_sink sink;
	int (*consume_line)(struct aem_stream_sink_lines *sink, struct aem_stringslice line, int flags);

	// Does consume_line want to see the newlines?
	// (Defaults to 1; change it yourself if desired)
	char want_newline;

	// Whether the last packet ended with a CR, which could potentially be
	// part of a CRLF split across two packets.
	char ended_cr;
};

struct aem_stream_sink_lines *aem_stream_sink_lines_init(struct aem_stream_sink_lines *lines);
void aem_stream_sink_lines_dtor(struct aem_stream_sink_lines *lines);


#endif /* AEM_STREAMS_H */
