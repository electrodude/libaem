#ifndef AEM_STREAMS_H
#define AEM_STREAMS_H

#include <aem/stream.h>

/// Stream transducer
struct aem_stream_transducer
{
	struct aem_stream_sink sink;
	struct aem_stream_source source;

	void (*go)(struct aem_stream_transducer *tr, struct aem_stringbuf *out, struct aem_stringslice *in, int flags);
	void (*on_close)(struct aem_stream_transducer *tr);
};

struct aem_stream_transducer *aem_stream_transducer_init(struct aem_stream_transducer *tr);
// You must call this from the containing object's _dtor_rcu function.
void aem_stream_transducer_dtor_rcu(struct aem_stream_transducer *tr);

void aem_stream_transducer_close(struct aem_stream_transducer *tr);

#endif /* AEM_STREAMS_H */
