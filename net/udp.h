/** network: UDP */

#pragma once

struct udp_hdr {
	u_char sport[2], dport[2];
	u_char length[2];
	u_char crc[2];
};
