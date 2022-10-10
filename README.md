## Description

These are utility functions that I use in my projects, and I can't include them into ffbase or ffos repositories.
Feel free to use them if you want, everything here is absolutely free.

	aio.h - file AIO
	cmdarg-scheme.h - command-line arguments parser with scheme
	cmdarg.h - command-line arguments parser
	conf2-ltconf.h - ltconf--ffconf bridge
	conf2-scheme.h - configuration parser with scheme
	conf2-writer.h - ffbase: conf writer
	conf2.h - configuration parser
	ffthpool.c
	ffwohandler.c
	fntree.h - File name tree with economical memory management.
	html.h - HTML parser
	ltconf.h - Light config parser
	range.h - memory region pointer
	stream.h - stream buffer
	taskqueue.h - task queue: First in, first out.  One reader/deleter, multiple writers.
	thpool.h - Thread pool for offloading the operations that may take a long time to complete.
	wohandler.h - Windows object handler - call a user function when a kernel object signals.

Network:

	net/dns-client.h+ffdns-client.c - DNS client.
	net/dns.h - DNS constants and message read/write functions
	net/http-client.h+ffhttp-client.c - HTTP client
	net/http1-status.h - HTTP status codes and messages
	net/http1.h - Read/write HTTP/1 data
	net/ipaddr.h - IPv4 address
	net/socks.h - SOCKS4
	net/tls.h+fftls.c - TLS reader.
	net/url.h
	net/websocket.h+ffwebskt.c - WebSocket reader/writer.

DB:

	db/db.h
	db/ffdb-postgre.c
	db/ffdb-sqlite.c
	db/postgre.h - PostgreSQL libpq wrapper.
	db/sqlite.h

GUI:

	gui-gtk/ffgui-gtk-loader.c
	gui-gtk/ffgui-gtk.c
	gui-gtk/gtk.h - GUI based on GTK+.
	gui-gtk/loader.h - GUI based on GTK+
	gui-vars.h - GUI variables
	gui-winapi/ffgui-winapi-loader.c
	gui-winapi/ffgui-winapi.c
	gui-winapi/loader.h - GUI loader.
	gui-winapi/winapi-shell.h
	gui-winapi/winapi.h - GUI based on Windows API.


## Requirements

ffbase & ffos
