## Description

These are utility functions that I use in my projects, and I can't include them into ffbase or ffos repositories.
Feel free to use them if you want, everything here is absolutely free.

Unsorted:

	ico-read.h          .ico reader
	jni-helper.h        JNI wrapper
	pe.c
	pe.h                PE (Windows executable) parser
	pixel-conv.h        Convert pixel lines

System:

	sys/aio.h               File AIO
	sys/thpool.c
	sys/thpool.h            Thread pool for offloading the operations that may take a long time to complete.
	sys/wohandler.c
	sys/wohandler.h         Windows object handler - call a user function when a kernel object signals.

Data:

	data/cmdarg-scheme.h    command-line arguments parser with scheme
	data/cmdarg.h           command-line arguments parser
	data/conf2-ltconf.h     ltconf--ffconf bridge
	data/conf2-scheme.h     configuration parser with scheme
	data/conf2-writer.h     ffbase: conf writer
	data/conf2.h            configuration parser
	data/fntree.h           File name tree with economical memory management.
	data/html.h             HTML parser
	data/ltconf.h           Light config parser
	data/mbuf.h             Buffer that can be linked with another buffer
	data/range.h            memory region pointer
	data/stream.h           stream buffer
	data/taskqueue.h        task queue: First in, first out.  One reader/deleter, multiple writers.

Network:

	net/dns-client.h    DNS client
	net/dns.h           DNS constants and message read/write functions
	net/http-client.h   HTTP client
	net/http1-status.h  HTTP status codes and messages
	net/http1.h         Read/write HTTP/1 data
	net/intel-dpdk.h    Intel DPDK wrapper
	net/ipaddr.h        IPv4 address
	net/socks.h         SOCKS4
	net/tls.h           TLS reader
	net/url.h
	net/websocket.h     WebSocket reader/writer

DB:

	db/db.h
	db/postgre.h        PostgreSQL libpq wrapper
	db/sqlite.h         SQLite wrapper

GUI:

	gui-gtk/ffgui-gtk-loader.c
	gui-gtk/ffgui-gtk.c
	gui-gtk/gtk.h                   GUI based on GTK+
	gui-gtk/loader.h                GUI based on GTK+
	gui-gtk/unix-shell.h            UNIX shell utils
	gui-vars.h                      GUI variables
	gui-winapi/ffgui-winapi-loader.c
	gui-winapi/ffgui-winapi.c
	gui-winapi/loader.h             GUI loader
	gui-winapi/winapi-shell.h
	gui-winapi/winapi.h             GUI based on Windows API


## Requirements

ffbase & ffos


## License

This code is in public domain.
