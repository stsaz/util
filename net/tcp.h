/** network: TCP */

#pragma once

struct tcp_hdr {
	u_char sport[2], dport[2];
	u_char seq[4];
	u_char ack[4];
	u_char off; // off[4] res[3] f[1]
	u_char flags; // enum TCP_F
	u_char window[2];
	u_char crc[2];
	u_char urgent_ptr[2];
};

enum TCP_F {
	TCP_FIN = 0x01,
	TCP_SYN = 0x02,
	TCP_RST = 0x04,
	TCP_PUSH = 0x08,
	TCP_ACK = 0x10,
};

#define tcp_hdr_len(t)  ((((t)->off & 0xf0) >> 4) * 4)
