## Description

These are utility functions that I use in my projects, and I can't include them into ffbase or ffsys repositories, so they end up here.
Some stuff here is obsolete/untested/incorrect while some stuff is production quality.
Feel free to use this code if you want, everything here is absolutely free.

System:

| File | Description |
| --- | --- |
| [sys/aio.h](sys/aio.h)           | File AIO |
| [sys/crash.h](sys/crash.h)       | Crash handler |
| [sys/kcq.h](sys/kcq.h)           | Process events from kernel call queue; multiple workers |
| [sys/kq-kcq.h](sys/kq-kcq.h)     | Bridge between KQ and KCQ |
| [sys/kq-timer.h](sys/kq-timer.h) | KQ timer |
| [sys/kq-tq.h](sys/kq-tq.h)       | Bridge between KQ and TQ |
| [sys/kq.h](sys/kq.h)             | Process events from kernel queue |
| [sys/log.h](sys/log.h)           | Logger |
| [sys/thpool.h](sys/thpool.h) + [sys/thpool.c](sys/thpool.c) | Thread pool for offloading the operations that may take a long time to complete. |
| [sys/unix-shell.h](sys/unix-shell.h)       | UNIX shell utils |
| [sys/windows-shell.h](sys/windows-shell.h) | Windows shell utils |
| [sys/woeh.h](sys/woeh.h)                   | Windows object handler - call a user function when a kernel object signals. |

Data:

| File | Description |
| --- | --- |
| [data/cmdarg-scheme.h](data/cmdarg-scheme.h)  | command-line arguments parser with scheme |
| [data/cmdarg.h](data/cmdarg.h)                | command-line arguments parser |
| [data/conf-args.h](data/conf-args.h)          | ffconf-ffargs bridge |
| [data/conf-obj.h](data/conf-obj.h)            | ffconf extension: `object {...}`; partial input |
| [data/conf-scheme.h](data/conf-scheme.h)      | configuration parser with scheme |
| [data/conf-write.h](data/conf-write.h)        | conf writer |
| [data/conf2.h](data/conf2.h)                  | configuration parser |
| [data/html.h](data/html.h)                    | HTML parser |
| [data/mbuf.h](data/mbuf.h)                    | Buffer that can be linked with another buffer |
| [data/range.h](data/range.h)                  | memory region pointer |
| [data/stream.h](data/stream.h)                | stream buffer |
| [data/taskqueue.h](data/taskqueue.h)          | task queue: First in, first out.  One reader/deleter, multiple writers. |
| [data/util.hpp](data/util.hpp)                | C++ utility functions |

Network:

| File | Description |
| --- | --- |
| [netf/dns-client.h](netf/dns-client.h) + [netf/dns-client.c](netf/dns-client.c)     | DNS client |
| [netf/dpdk.h](netf/dpdk.h) + [netf/dpdk.c](netf/dpdk.c)     | Intel DPDK wrapper |
| [netf/http-client.h](netf/http-client.h) + [netf/http-client.c](netf/http-client.c) | HTTP client |
| [netf/lxdp.h](netf/lxdp.h)                 | Linux XDP interface|
| [netf/ssl.h](netf/ssl.h) + [netf/ffssl.c](netf/ffssl.c) | OpenSSL wrapper |
| [netf/url.h](netf/url.h)                   | URL functions |

Network protocols (Low layer):

| File | Description |
| --- | --- |
| [net/arp.h](net/arp.h)                   | ARP |
| [net/ethernet.h](net/ethernet.h)         | Ethernet |
| [net/icmp.h](net/icmp.h)                 | ICMP |
| [net/ipaddr.h](net/ipaddr.h)             | IP |
| [net/tcp.h](net/tcp.h)                   | TCP |
| [net/udp.h](net/udp.h)                   | UDP |
| [net/vlan.h](net/vlan.h)                 | VLAN |

Network protocols (High layer):

| File | Description |
| --- | --- |
| [net/dns.h](net/dns.h)                   | DNS constants and message read/write functions |
| [net/http1-status.h](net/http1-status.h) | HTTP status codes and messages |
| [net/http1.h](net/http1.h)               | Read/write HTTP/1 data |
| [net/socks.h](net/socks.h)               | SOCKS4 |
| [net/tls.h](net/tls.h) + [net/tls.c](net/tls.c) | TLS reader |
| [net/websocket.h](net/websocket.h) + [net/websocket.c](net/websocket.c) | WebSocket reader/writer |

DB:

| File | Description |
| --- | --- |
| [db/db.h](db/db.h)           | DB helper functions |
| [db/postgre.h](db/postgre.h) | PostgreSQL libpq wrapper |
| [db/sqlite.h](db/sqlite.h)   | SQLite wrapper |

Unsorted:

| File | Description |
| --- | --- |
| [misc/ico-read.h](misc/ico-read.h)              | .ico reader |
| [misc/jni-helper.h](misc/jni-helper.h)          | JNI wrapper |
| [misc/pe.h](misc/pe.h) + [misc/pe.c](misc/pe.c) | PE (Windows executable) parser |
| [misc/pixel-conv.h](misc/pixel-conv.h)          | Convert pixel lines |


## Requirements

ffbase & ffsys


## License

This code is in public domain.
