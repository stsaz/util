#include <resolv.h>
#include <netdb.h>

/** Get the list of DNS servers currently configured.
'dnsServerArray': output array containing "IP:port" values for each DNS server.
 User must free() its elements and itself.
Return the number of DNS servers. */
uint dnsServers(char ***dnsServerArray)
{
    struct __res_state res;
    res_ninit(&res);

    union res_sockaddr_union servers[NI_MAXSERV];
    int nServers = res_getservers(&res, servers, NI_MAXSERV);
    if (nServers == 0)
        return 0;

    enum { FFIP6_STRLEN = FFSLEN("abcd:") * 8 - 1 };
    enum { FFIP6_PORT_STRLEN = FFIP6_STRLEN + FFSLEN("[]:12345") };

    char **ret = (char**)calloc(nServers, sizeof(char*));
    uint n = 0;

    for (int i = 0;  i != nServers;  i++) {
        union res_sockaddr_union *s = &servers[i];
        if (s->sin.sin_len == 0)
            continue;

        ret[n] = malloc(FFIP6_PORT_STRLEN + 1);

        const void *a = (s->sin.sin_family == AF_INET) ? (void*)&s->sin.sin_addr : (void*)&s->sin6.sin6_addr;
        uint wr = ffip_tostr(ret[n], FFIP6_PORT_STRLEN, s->sin.sin_family, a, 53);
        ret[n][wr] = '\0';
        fffile_fmt(ffstderr, NULL, "%s %s\n", __func__, ret[n]);
        n++;
    }

    res_ndestroy(&res);

    *dnsServerArray = ret;
    return n;
}
