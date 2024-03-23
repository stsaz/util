/** network: ARP */

#pragma once

struct arp_hdr {
	u_char hwtype[2]; // enum ARP_HW
	u_char proto[2];
	u_char hlen; // 6 for Ethernet
	u_char plen; // 4 for IPv4
	u_char op[2]; // enum ARP_OP

	u_char sha[6];
	u_char spa[4];
	u_char tha[6];
	u_char tpa[4];
};

enum ARP_HW {
	ARP_ETH = 1,
};

enum ARP_OP {
	ARP_REQ = 1,
	ARP_RESP,
};

static inline int arp_hdr_str(const struct arp_hdr *h, char *buf, size_t cap)
{
	char eth[2][ETH_STRLEN], ip[2][FFIP4_STRLEN];

	eth_str(eth[0], ETH_STRLEN, h->sha);
	eth_str(eth[1], ETH_STRLEN, h->tha);

	size_t n0 = ffip4_tostr((ffip4*)h->spa, ip[0], FFIP4_STRLEN);
	size_t n1 = ffip4_tostr((ffip4*)h->tpa, ip[1], FFIP4_STRLEN);

	return ffs_format(buf, cap, "op:%u  srchw:%*s  srcip:%*s  dsthw:%*s  dstip:%*s"
		, ffint_be_cpu16_ptr(h->op)
		, (size_t)ETH_STRLEN, eth[0], n0, ip[0]
		, (size_t)ETH_STRLEN, eth[1], n1, ip[1]);
}
