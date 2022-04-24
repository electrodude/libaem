# libaem

AEM's personal C library

## Features

* `aem_stringbuf`: string builder/storage
* `aem_stringslice`: string slice/iterator/parser helper
* `aem_stack`: dynamically resizeable vector of `void *`

- `aem_nfa`: NFA-based regular expression engine and lexer

* `aem_log`: logging facility: shows context, filter by loglevel, redirect output

* `aem_serial`: cross-platform serial port interface (only tested on Unix)

* `AEM_LL`: embedded circular doubly linked list (all macros)
* `aem_iter_gen`: graph iterator helper

* `aem_gc`: simple mark/sweep garbage collector
* `aem_pmcrcu`: Single-threaded implementations of `call_rcu`, `synchronize_rcu`, and `rcu_barrier`
* `aem_module`: Dynamic module loader


## Planned Features

* `aem_hash`: hash table
* `aem_childproc`: child process manager
* `aem_poll`: `poll(2)`-based event loop
	* Works with `aem_net`.
* `aem_net`: abstracted network interface
	* Uses `aem_stream`.
* `aem_stream`: data stream abstraction
	* Includes utility stream transducers to e.g. split a stream into lines.

## License

Unless otherwise specified, all files in this repository are licensed under the MIT license, found in the file [LICENSE](./LICENSE) in the root of this repository.

The following files in this repository were not written by me (AEM) and may be subject to different licenses:
- `debugbreak.h`: License and copyright at top of file.  Project repository [here](https://github.com/scottt/debugbreak).
- `order32.h`: Taken from [this StackOverflow answer](https://stackoverflow.com/a/2103095).

## FAQ

### You're so conceited that you named a library after yourself?

Well, at least there's [precedent](https://github.com/nothings/stb) for it.
