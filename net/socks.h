/** SOCKS4
2022, Simon Zolin */

/*
socks4_req_write
socks4_resp_read
*/

enum SOCKS4_CMD {
	SOCKS4_CMD_CONNECT = 1,
	SOCKS4_CMD_BIND,
};

struct socks4_req {
	ffbyte ver; // 4
	ffbyte cmd; // enum SOCKS4_CMD
	ffbyte port[2];
	ffbyte ip[4];
	ffbyte id[1]; // NULL-terminated
};

static inline void socks4_req_write(void *buf, const ffbyte *ip, uint port)
{
	struct socks4_req *r = (struct socks4_req*)buf;
	r->ver = 4;
	r->cmd = SOCKS4_CMD_CONNECT;
	*(short*)r->port = ffint_be_cpu16(port);
	ffmem_copy(r->ip, ip, 4);
	r->id[0] = 0;
}

enum SOCKS4_R {
	SOCKS4_R_GRANTED = 0x5a,
};

struct socks4_resp {
	ffbyte vn; // 0
	ffbyte rcode; // enum SOCKS4_R
	ffbyte bind_port[2];
	ffbyte bind_ip[4];
};

/**
Return
 0: need more data
 >0: success
 <0: error */
static inline int socks4_resp_read(const void *buf, ffsize n)
{
	if (n < sizeof(struct socks4_resp))
		return 0;

	const struct socks4_resp *r = (struct socks4_resp*)buf;
	if (!(r->vn == 0 && r->rcode == SOCKS4_R_GRANTED
		&& *(short*)r->bind_port == 0 && *(int*)r->bind_ip == 0))
		return -1;
	return 1;
}
