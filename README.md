# libaem

AEM's personal C library

## Features

* `aem_stringbuf`: string builder/storage
* `aem_stringslice`: string slice/iterator/parser helper
* `aem_stack`: dynamically resizeable vector of `void *`

* `aem_log`: logging facility: shows context, filter by loglevel, redirect output

* `aem_serial`: cross-platform serial port interface (only tested on Unix)

* `AEM_LL`: embedded circular doubly linked list (all macros)
* `aem_iter_gen`: graph iterator helper

* `aem_gc`: simple mark/sweep garbage collector


## Planned Features

* `aem_hash`: hash table
* `aem_childproc`: child process manager
* `aem_poll`: `poll(2)`-based event loop
	* Works with `aem_net`.
* `aem_net`: abstracted network interface
	* Uses `aem_stream`.
* `aem_stream`: data stream abstraction
	* Includes utility stream transducers to e.g. split a stream into lines.
