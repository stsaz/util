
/** Check if domain name is valid
Syntax: [label.]... label.label
  Each label is 1 to 63 characters long, and may contain:
    . ASCII letters a-z and A-Z
    . digits 0-9
    . hyphen ('-')
  . labels may be in "xn--[a-zA-Z0-9]+" format
  . labels cannot start or end with hyphens (RFC 952)
  . max length of ascii hostname including dots is 253 characters
Return domain level;
  <0 on error */
static inline int ffurl_isdomain(const char *name, ffsize len)
{
	if (len > 253)
		return -1;

	int st = 0, nlabel = 0, level = 1, prev_char = 0;
	ffuint xn = 0;

	for (ffsize i = 0;  i != len;  i++) {
		int c = name[i];

		switch (st) {
		case 0:
			// fallthrough
		case 1:
			if (!((c >= 'a' && c <= 'z')
				|| (c >= 'A' && c <= 'Z'))) {

				if (!(c >= '0' && c <= '9'))
					return -1;

			} else if (c == 'x' || c == 'X') {
				xn = 1;
			}
			st = 2;
			nlabel = 1;
			break;

		case 2:
			if (c == '.') {
				if (prev_char == '-')
					return -1;
				level++;
				st = 0;
				xn = 0;
				continue;
			}

			if (nlabel == 63)
				return -1;

			if (!((c >= 'a' && c <= 'z')
				|| (c >= 'A' && c <= 'Z')
				|| (c >= '0' && c <= '9')
				|| c == '-')) {
				return -1;
			}

			if (xn > 0) {
				if (xn < FFS_LEN("xn--")) {
					if (c == "xn--"[xn])
						xn++;
					else
						xn = 0;
				} else {
					xn++;
				}
			}

			prev_char = c;
			nlabel++;
			break;
		}
	}

	if (st != 2
		|| (xn > 0 && xn < FFS_LEN("xn--wwww")))
		return -1;

	return level;
}

/** Convert [IP][:][port] string to a socket address object
Note: IPv6 isn't supported
Return 0 on success */
static inline int ffsockaddr_fromstr(ffsockaddr *a, const char *s, ffsize len, ffuint default_port)
{
	if (len == 0)
		return -1;
	ffstr str = FFSTR_INITN(s, len);
	ffip4 ip;
	int r = ffip4_parse((ffip4*)&ip, str.ptr, str.len);
	if (r < 0)
		return -1;
	ffstr_shift(&str, r);

	ffushort port = default_port;
	if (r > 0 && str.ptr[0] == ':') {
		ffstr_shift(&str, 1);
		if (!ffstr_toint(&str, &port, FFS_INT16))
			return -1;
	}

	ffsockaddr_set_ipv4(a, &ip, port);
	return 0;
}
