/** DNS client.
Copyright 2019 Simon Zolin
*/

#include "dns-client.h"
#include "dns.h"
#include "crc.h"
#include "ipaddr.h"
#include "time.h"
#include <FFOS/random.h>


struct ffdnscl_serv {
	fflist_item sib;
	ffdnsclient *r;

	ffskt sk;
	ffaio_task aiotask;
	ffaddr addr;
	char saddr_s[FF_MAXIP4];
	ffstr saddr;
	char *ansbuf;
	unsigned connected :1;

	uint nqueries;
};

typedef struct dns_quser {
	ffdnscl_onresolve ondone;
	void *udata;
} dns_quser;

typedef struct dns_query {
	ffdnsclient *r;
	ffrbt_node rbtnod;
	fftimerqueue_node tmr;
	uint tries_left;
	ffstr name; //hostname to be resolved

	ffdnscl_res *res[2];
	uint ttl[2];
	int status; //aggregated status of both IPv4/6 queries
	fftime firstsend;

	ffvec users; //dns_quser[]. requestors waiting for this query
	ushort txid4;
	ushort txid6;
	unsigned need4 :1
		, need6 :1;
	byte nres; //number of elements in res[2]
	ushort ques_len4;
	ushort ques_len6;
	char question[0];
} dns_query;

struct ffdnscl_res {
	uint is4 :1;
	uint naddrs;
	union {
		ffip4 addrs4[0];
		ffip6 addrs6[0];
	};
};


#define syserrlog_x(r, ...) \
	(r)->log(FFDNSCL_LOG_ERR | FFDNSCL_LOG_SYS, NULL, __VA_ARGS__)
#define errlog_x(r, ...) \
	(r)->log(FFDNSCL_LOG_ERR, NULL, __VA_ARGS__)

#define syserrlog_srv(serv, fmt, ...) \
	(serv)->r->log(FFDNSCL_LOG_ERR | FFDNSCL_LOG_SYS, "%S: " fmt, &(serv)->saddr, __VA_ARGS__)
#define errlog_srv(serv, fmt, ...) \
	(serv)->r->log(FFDNSCL_LOG_ERR, "%S: " fmt, &(serv)->saddr, __VA_ARGS__)
#define dbglog_srv(serv, lev, fmt, ...) \
do { \
	if ((serv)->r->debug_log) \
		(serv)->r->log(FFDNSCL_LOG_DBG, "%S: " fmt, &(serv)->saddr, __VA_ARGS__); \
} while (0)

#define errlog_q(q, fmt, ...)  (q)->r->log(FFDNSCL_LOG_ERR, "%S: " fmt, &(q)->name, __VA_ARGS__)
#define warnlog_q(q, fmt, ...)  (q)->r->log(FFDNSCL_LOG_WARN, "%S: " fmt, &(q)->name, __VA_ARGS__)
#define log_checkdbglevel(q, lev)  (q->r->debug_log)
#define dbglog_q(q, lev, fmt, ...) \
do { \
	if (log_checkdbglevel(q, lev)) \
		(q)->r->log(FFDNSCL_LOG_DBG, "%S: " fmt, &(q)->name, __VA_ARGS__); \
} while (0)


// SERVER
static int serv_init(ffdnscl_serv *serv);
static ffdnscl_serv * serv_next(ffdnsclient *r);
static void serv_fin(ffdnscl_serv *serv);

// QUERY
#define query_sib(pnod)  FF_GETPTR(dns_query, rbtnod, pnod)
static int query_addusr(dns_query *q, ffdnscl_onresolve ondone, void *udata);
static int query_rmuser(ffdnsclient *r, const ffstr *host, ffdnscl_onresolve ondone, void *udata);
static size_t query_prep(ffdnsclient *r, char *buf, size_t cap, uint txid, const ffstr *nm, int type);
static void query_send(dns_query *q, int resend);
static int query_send1(dns_query *q, ffdnscl_serv *serv, int resend);
static void query_onexpire(void *param);
static void query_fin(dns_query *q, int status, ffdnscl_serv *serv);
static void query_free(void *param);

// ANSWER
static void ans_read(void *udata);
static void ans_proc(ffdnscl_serv *serv, const ffstr *resp);
static dns_query * ans_find_query(ffdnscl_serv *serv, ffdns_header *h, const ffstr *resp);
static uint ans_nrecs(dns_query *q, ffdns_header *h, const ffstr *resp, const char *pbuf, int is4);
static ffdnscl_res* ans_proc_resp(dns_query *q, ffdns_header *h, const ffstr *resp, int is4);
static void res_free(ffdnscl_res *dr);

// DUMMY CALLBACKS
static int oncomplete_dummy(ffdnsclient *r, ffdnscl_res *res, const ffstr *name, uint refcount, uint ttl)
{
	return 0;
}
static void log_dummy(uint level, const char *fmt, ...)
{}
static fftime time_dummy(void)
{
	fftime t = {};
	return t;
}


void ffdnscl_conf_init(ffdnscl_conf *c)
{
	ffmem_tzero(c);

	c->kq = FF_BADFD;
	c->retry_timeout = 1000;
	c->enable_ipv6 = 1;
	c->edns = 1;
	c->max_tries = 1;
	c->buf_size = 4*1024;

	c->log = &log_dummy;
	c->time = &time_dummy;
	c->oncomplete = &oncomplete_dummy;
}

ffdnsclient* ffdnscl_new(ffdnscl_conf *conf)
{
	ffdnsclient *r = ffmem_new(ffdnsclient);
	if (r == NULL)
		return NULL;
	ffmemcpy(r, conf, sizeof(*conf));

	fflist_init(&r->servs);
	r->curserv = NULL;
	ffrbt_init(&r->queries);
	return r;
}

int ffdnscl_resolve(ffdnsclient *r, ffstr name, ffdnscl_onresolve ondone, void *udata, uint flags)
{
	uint namecrc;
	char buf4[FFDNS_MAXMSG], buf6[FFDNS_MAXMSG];
	size_t ibuf4, ibuf6 = 0;
	ffrbt_node *found_query;
	dns_query *q = NULL;
	ffstr host = name;
	ushort txid4, txid6 = 0;

	if (flags & FFDNSCL_CANCEL)
		return query_rmuser(r, &host, ondone, udata);

	namecrc = ffcrc32_iget(name.ptr, name.len);

	// determine whether the needed query is already pending and if so, attach to it
	found_query = ffrbt_find(&r->queries, namecrc);
	if (found_query != NULL) {
		q = query_sib(found_query);

		if (!ffstr_eq2(&q->name, &host)) {
			errlog_x(r, "%S: CRC collision with %S", &host, &q->name);
			goto fail;
		}

		dbglog_q(q, LOG_DBGFLOW, "query hit", 0);
		if (0 != query_addusr(q, ondone, udata))
			goto nomem;

		return 0;
	}

	// prepare DNS queries: A and AAAA
	txid4 = ffrnd_get() & 0xffff;
	ibuf4 = query_prep(r, buf4, FF_COUNT(buf4), txid4, &host, FFDNS_A);
	if (ibuf4 == 0) {
		errlog_x(r, "invalid hostname: %S", &host);
		goto fail;
	}

	if (r->enable_ipv6) {
		txid6 = ffrnd_get() & 0xffff;
		ibuf6 = query_prep(r, buf6, FF_COUNT(buf6), txid6, &host, FFDNS_AAAA);
	}

	// initialize DNS query object
	q = ffmem_alloc(sizeof(dns_query) + ibuf4 + ibuf6);
	if (q == NULL)
		goto nomem;
	ffmem_zero(q, sizeof(dns_query));
	q->r = r;

	if (0 != query_addusr(q, ondone, udata))
		goto nomem;

	if (NULL == ffstr_dupstr(&q->name, &name))
		goto nomem;

	q->need4 = 1;
	ffmemcpy(q->question, buf4, ibuf4);
	q->ques_len4 = (ushort)ibuf4;
	q->txid4 = txid4;

	if (r->enable_ipv6) {
		q->need6 = 1;
		ffmemcpy(q->question + ibuf4, buf6, ibuf6);
		q->ques_len6 = (ushort)ibuf6;
		q->txid6 = txid6;
	}

	q->rbtnod.key = namecrc;
	ffrbt_insert(&r->queries, &q->rbtnod, NULL);
	q->tries_left = r->max_tries;
	q->firstsend = r->time();

	query_send(q, 0);
	return 0;

nomem:
	syserrlog_x(r, "ffmem_alloc", 0);

fail:
	if (q != NULL)
		query_free(q);

	ffdnscl_result res = {};
	res.name = host;
	res.status = -1;
	ondone(udata, &res);
	return 0;
}

static void ffrbt_freeall(ffrbtree *tr, void(*func)(void*), size_t off)
{
	ffrbt_node *n, *next;
	FFRBT_FOR(tr, n) {
		next = ffrbt_node_successor(n, &tr->sentl);
		ffrbt_rm(tr, n);
		void *p = FF_PTR(n, -(ssize_t)off);
		func(p);
		n = next;
	}
}

void ffdnscl_free(ffdnsclient *r)
{
	if (r == NULL)
		return;

	ffrbt_freeall(&r->queries, &query_free, FFOFF(dns_query, rbtnod));
	FFLIST_ENUMSAFE(&r->servs, serv_fin, ffdnscl_serv, sib);
	ffmem_free(r);
}

static void query_free(void *param)
{
	dns_query *q = param;
	q->r->timer(&q->tmr, 0);
	ffstr_free(&q->name);
	ffarr_free(&q->users);
	ffmem_free(q);
}

/** Timer expired. */
static void query_onexpire(void *param)
{
	dns_query *q = param;

	if (q->tries_left == 0) {
		errlog_q(q, "reached max_tries limit", 0);
		query_fin(q, -1, NULL);
		return;
	}

	query_send(q, 1);
}

/** One more user wants to send the same query. */
static int query_addusr(dns_query *q, ffdnscl_onresolve ondone, void *udata)
{
	dns_quser *quser;

	if (NULL == ffvec_growT(&q->users, 1, dns_quser))
		return 1;
	quser = ffarr_pushT(&q->users, dns_quser);
	quser->ondone = ondone;
	quser->udata = udata;
	return 0;
}

/** User doesn't want to wait for this query anymore. */
static int query_rmuser(ffdnsclient *r, const ffstr *host, ffdnscl_onresolve ondone, void *udata)
{
	ffrbt_node *found;
	dns_query *q;
	dns_quser *quser;
	uint namecrc;

	namecrc = ffcrc32_iget(host->ptr, host->len);
	found = ffrbt_find(&r->queries, namecrc);
	if (found == NULL) {
		errlog_x(r, "cancel: no query for %S", host);
		return 1;
	}

	q = query_sib(found);

	if (!ffstr_eq2(&q->name, host)) {
		errlog_x(r, "%S: CRC collision with %S", host, &q->name);
		return 1;
	}

	FFSLICE_WALK(&q->users, quser) {

		if (udata == quser->udata && ondone == quser->ondone) {
			ffarr_rmswapT(&q->users, quser, dns_quser);
			dbglog_q(q, LOG_DBGFLOW, "cancel: unref query", 0);
			return 0;
		}
	}

	errlog_q(q, "cancel: no matching reference for the query", 0);
	return 1;
}

static void query_send(dns_query *q, int resend)
{
	ffdnscl_serv *serv;

	for (;;) {

		if (q->tries_left == 0) {
			errlog_q(q, "reached max_tries limit", 0);
			query_fin(q, -1, NULL);
			return;
		}

		q->tries_left--;
		serv = serv_next(q->r);
		if (0 == query_send1(q, serv, resend))
			return;
	}
}

/** Send query to server. */
static int query_send1(dns_query *q, ffdnscl_serv *serv, int resend)
{
	ssize_t rc;
	const char *er;
	ffdnsclient *r = q->r;

	if (!serv->connected) {
		if (0 != serv_init(serv))
			return 1;

		if (0 != ffskt_connect(serv->sk, &serv->addr.a, serv->addr.len)) {
			er = "ffskt_connect";
			goto fail;
		}
		serv->connected = 1;
		ans_read(serv);
	}

	if (q->need6) {
		rc = ffskt_send(serv->sk, q->question + q->ques_len4, q->ques_len6, 0);
		if (rc != q->ques_len6) {
			er = "ffskt_send";
			goto fail_send;
		}

		serv->nqueries++;

		dbglog_q(q, LOG_DBGNET, "%S: %ssent %s query #%u (%u).  [%L]"
			, &serv->saddr
			, (resend ? "re" : ""), "AAAA", (int)q->txid6, serv->nqueries, (size_t)r->queries.len);
	}

	if (q->need4) {
		rc = ffskt_send(serv->sk, q->question, q->ques_len4, 0);
		if (rc != q->ques_len4) {
			er = "ffskt_send";
			goto fail_send;
		}

		serv->nqueries++;

		dbglog_q(q, LOG_DBGNET, "%S: %ssent %s query #%u (%u).  [%L]"
			, &serv->saddr
			, (resend ? "re" : ""), "A", (int)q->txid4, serv->nqueries, (size_t)r->queries.len);
	}

	q->tmr.func = query_onexpire;
	q->tmr.param = q;
	r->timer(&q->tmr, r->retry_timeout);
	return 0;

fail:
	syserrlog_srv(serv, "%s", er);
	return 1;

fail_send:
	syserrlog_srv(serv, "%s", er);
	ffskt_close(serv->sk);
	serv->sk = FF_BADSKT;
	ffaio_fin(&serv->aiotask);
	serv->connected = 0;
	return 1;
}

/** Prepare DNS query. */
static size_t query_prep(ffdnsclient *r, char *buf, size_t cap, uint txid, const ffstr *host, int type)
{
	int rr, i;

	ffdns_header h = {};
	h.id = txid;
	h.questions = 1;
	h.recursion_desired = 1;
	if (r->edns)
		h.additionals = 1;
	if (0 > (rr = ffdns_header_write(buf, cap, &h)))
		return 0;
	i = rr;

	ffdns_question q = {};
	ffstr_set2(&q.name, host);
	q.clas = FFDNS_IN;
	q.type = type;
	if (0 > (rr = ffdns_question_write(&buf[i], cap - i, &q)))
		return 0;
	i += rr;

	if (r->edns) {
		i += ffdns_optinit((struct ffdns_opt*)&buf[i], r->buf_size);
	}

	return i;
}


/** Receive data from DNS server. */
static void ans_read(void *udata)
{
	ffdnscl_serv *serv = udata;
	ssize_t r;
	ffstr resp;

	for (;;) {
		r = ffaio_recv(&serv->aiotask, &ans_read, serv->ansbuf, serv->r->buf_size);
		if (r == FFAIO_ASYNC)
			return;
		else if (r == FFAIO_ERROR) {
			syserrlog_srv(serv, "ffaio_recv", 0);
			return;
		}

		dbglog_srv(serv, LOG_DBGNET, "received response (%L bytes)", r);

		ffstr_set(&resp, serv->ansbuf, r);
		ans_proc(serv, &resp);
	}
}

/** Process response and notify users waiting for it. */
static void ans_proc(ffdnscl_serv *serv, const ffstr *resp)
{
	ffdns_header h;
	dns_query *q;
	int is4;
	ffdnsclient *r = serv->r;

	q = ans_find_query(serv, &h, resp);
	if (q == NULL)
		return;

	if (q->need4 && h.id == q->txid4) {
		q->need4 = 0;
		is4 = 1;

	} else if (q->need6 && h.id == q->txid6) {
		q->need6 = 0;
		is4 = 0;

	} else {
		errlog_q(q, "request/response IDs don't match.  Response ID: #%u", h.id);
		return;
	}

	if (h.rcode != FFDNS_NOERROR) {
		errlog_q(q, "#%u: DNS response: (%u) %s"
			, h.id, h.rcode, ffdns_rcode_str(h.rcode));
		if (q->nres == 0)
			q->status = h.rcode; //set error only from the first response

	} else if (NULL != ans_proc_resp(q, &h, resp, is4))
		q->status = FFDNS_NOERROR;
	else if (q->nres == 0)
		q->status = -1;

	if (log_checkdbglevel(q, LOG_DBGNET)) {
		fftime t = r->time();
		fftime_diff(&q->firstsend, &t);
		dbglog_q(q, LOG_DBGNET, "resolved IPv%u in %u.%03us"
			, (is4) ? 4 : 6, (int)fftime_sec(&t), (int)fftime_msec(&t));
	}

	if (q->need4 || q->need6)
		return; //waiting for the second response

	q->r->timer(&q->tmr, 0);

	uint ttl = (uint)ffmin(q->ttl[0], q->ttl[q->nres - 1]);
	for (uint i = 0;  i < q->nres;  i++) {
		q->r->oncomplete(q->r, q->res[i], &q->name, q->users.len, ttl);
	}

	query_fin(q, q->status, serv);
}

/** Find query by a response from DNS server. */
static dns_query * ans_find_query(ffdnscl_serv *serv, ffdns_header *h, const ffstr *resp)
{
	dns_query *q;
	const char *errmsg = NULL;
	uint namecrc;
	ffstr name;
	ffrbt_node *found_query;
	uint resp_id = 0;
	ffdns_question qh = {};

	if (0 > ffdns_header_read(h, *resp)) {
		errmsg = "too small response";
		goto fail;
	}
	if (h->response != 1) {
		errmsg = "received invalid response";
		goto fail;
	}

	resp_id = h->id;

	if (h->questions != 1) {
		errmsg = "number of questions in response is not 1";
		goto fail;
	}

	if (0 > ffdns_question_read(&qh, *resp)) {
		errmsg = "too small response";
		goto fail;
	}
	if (qh.name.len == 0) {
		errmsg = "invalid name in question";
		goto fail;
	}
	ffstr_set2(&name, &qh.name);
	name.len--;

	serv->r->log(FFDNSCL_LOG_DBG /*LOG_DBGNET*/
		, "%S: DNS response #%u.  Status: %u.  AA: %u, RA: %u.  Q: %u, A: %u, N: %u, R: %u."
		, &name
		, h->id, h->rcode, h->authoritive, h->recursion_available
		, h->questions, h->answers, h->nss, h->additionals);

	if (qh.clas != FFDNS_IN) {
		serv->r->log(FFDNSCL_LOG_ERR
			, "%S: #%u: invalid class %u in DNS response"
			, &name, h->id, qh.clas);
		goto fail;
	}

	namecrc = ffcrc32_get(name.ptr, name.len);

	found_query = ffrbt_find(&serv->r->queries, namecrc);
	if (found_query == NULL) {
		errmsg = "unexpected DNS response";
		goto fail;
	}

	q = query_sib(found_query);
	if (!ffstr_eq2(&q->name, &name)) {
		errmsg = "unexpected DNS response";
		goto fail;
	}

	return q;

fail:
	if (errmsg != NULL)
		errlog_srv(serv, "%s. ID: #%u. Name: %S", errmsg, resp_id, &name);

	ffdns_question_destroy(&qh);
	return NULL;
}

/** Get the number of useful records.  Print debug info about the records in response. */
static uint ans_nrecs(dns_query *q, ffdns_header *h, const ffstr *resp, const char *pbuf, int is4)
{
	uint nrecs = 0;
	uint ir;
	ffdns_answer ans = {};
	ffstr name;
	int rr;

	for (ir = 0;  ir < h->answers;  ir++) {
		ffdns_answer_destroy(&ans);
		if (0 > (rr = ffdns_answer_read(&ans, *resp, pbuf - resp->ptr))) {
			dbglog_q(q, LOG_DBGNET, "#%u: incomplete response", h->id);
			break;
		}
		pbuf += rr;

		ffstr_set2(&name, &ans.name);
		if (name.len == 0) {
			warnlog_q(q, "#%u: invalid name in answer", h->id);
			break;
		}
		name.len--;

		switch (ans.type) {

		case FFDNS_A:
			if (ans.clas != FFDNS_IN) {
				errlog_q(q, "#%u: invalid class in %s record: %u", h->id, "A", ans.clas);
				continue;
			}
			if (ans.data.len != sizeof(ffip4)) {
				errlog_q(q, "#%u: invalid %s address length: %u", h->id, "A", ans.data.len);
				continue;
			}

			if (log_checkdbglevel(q, LOG_DBGFLOW)) {
				char ip[FFIP4_STRLEN];
				size_t iplen = ffip4_tostr((void*)ans.data.ptr, ip, FF_COUNT(ip));
				dbglog_q(q, LOG_DBGFLOW, "%s for %S : %*s, TTL:%u"
					, "A", &name, (size_t)iplen, ip, ans.ttl);
			}

			if (is4)
				nrecs++;
			break;

		case FFDNS_AAAA:
			if (ans.clas != FFDNS_IN) {
				errlog_q(q, "#%u: invalid class in %s record: %u", h->id, "AAAA", ans.clas);
				continue;
			}
			if (ans.data.len != sizeof(ffip6)) {
				errlog_q(q, "#%u: invalid %s address length: %u", h->id, "AAAA", ans.data.len);
				continue;
			}

			if (log_checkdbglevel(q, LOG_DBGFLOW)) {
				char ip[FFIP6_STRLEN];
				size_t iplen = ffip6_tostr((void*)ans.data.ptr, ip, FF_COUNT(ip));
				dbglog_q(q, LOG_DBGFLOW, "%s for %S : %*s, TTL:%u"
					, "AAAA", &name, (size_t)iplen, ip, ans.ttl);
			}

			if (!is4)
				nrecs++;
			break;

		case FFDNS_CNAME:
			if (log_checkdbglevel(q, LOG_DBGFLOW)) {
				/*ffstr scname;
				char cname[NI_MAXHOST];
				const char *tbuf = pbuf;

				scname.len = ffdns_name(cname, sizeof(cname), resp->ptr, resp->len, &tbuf);
				if (scname.len == 0 || tbuf > pbuf + ans.data.len) {
					errlog_q(q, "invalid CNAME", 0);
					continue;
				}
				scname.ptr = cname;
				scname.len--;

				dbglog_q(q, LOG_DBGFLOW, "CNAME for %S : %S", &name, &scname);*/
			}
			break;

		default:
			dbglog_q(q, LOG_DBGFLOW, "record of type %u, length %u", ans.type, ans.data.len);
			break;
		}
	}

	ffdns_answer_destroy(&ans);
	return nrecs;
}

/** Create DNS resource object. */
static ffdnscl_res* ans_proc_resp(dns_query *q, ffdns_header *h, const ffstr *resp, int is4)
{
	const char *pbuf;
	ffdnscl_res *res = NULL;
	ffip4 *acur;
	ffip6 *a6cur;
	uint minttl = (uint)-1;
	int r;

	ffdns_question qh = {};
	if (0 > (r = ffdns_question_read(&qh, *resp))) {
		dbglog_q(q, LOG_DBGFLOW, "#%u: invalid message", h->id);
		return NULL;
	}
	ffdns_question_destroy(&qh);
	pbuf = resp->ptr + sizeof(struct ffdns_hdr) + r;

	uint nrecs = ans_nrecs(q, h, resp, pbuf, is4);

	if (nrecs == 0) {
		dbglog_q(q, LOG_DBGFLOW, "#%u: no useful records in response", h->id);
		return NULL;
	}

	{
		uint adr_sz = (is4 ? sizeof(ffip4) : sizeof(ffip6));
		res = ffmem_alloc(sizeof(ffdnscl_res) + adr_sz * nrecs);
		if (res == NULL) {
			syserrlog_x(q->r, "ffmem_alloc", 0);
			return NULL;
		}
		res->is4 = is4;
		res->naddrs = nrecs;
		acur = res->addrs4;
		a6cur = res->addrs6;
	}

	// set addresses and get the minimum TTL value
	uint ir;
	for (ir = 0;  ir < h->answers;  ir++) {
		ffdns_answer ans = {};
		if (0 > (r = ffdns_answer_read(&ans, *resp, pbuf - resp->ptr)))
			break;
		ffdns_answer_destroy(&ans);
		pbuf += r;
		if (ans.clas != FFDNS_IN)
			continue;

		switch (ans.type) {
		case FFDNS_A:
			if (!is4 || ans.data.len != sizeof(ffip4))
				continue;

			ffmemcpy(acur, ans.data.ptr, sizeof(ffip4));
			acur++;

			minttl = (uint)ffmin(minttl, ans.ttl);
			break;

		case FFDNS_AAAA:
			if (is4 || ans.data.len != sizeof(ffip6))
				continue;

			ffmemcpy(a6cur, ans.data.ptr, sizeof(ffip6));
			a6cur++;

			minttl = (uint)ffmin(minttl, ans.ttl);
			break;
		}
	}

	ir = q->nres++;
	q->res[ir] = res;
	q->ttl[ir] = minttl;
	return res;
}

/** Notify users, waiting for this question.  Free query object. */
static void query_fin(dns_query *q, int status, ffdnscl_serv *serv)
{
	dns_quser *quser;
	uint i, i4 = 0, total = 0;

	ffrbt_rm(&q->r->queries, &q->rbtnod);

	ffdnscl_result res = {};
	res.name = q->name;
	res.status = status;
	for (i = 0;  i != q->nres;  i++) {
		const ffdnscl_res *r = q->res[i];
		total += r->naddrs;
		if (r->naddrs != 0 && r->is4)
			i4 = i;
	}
	if (NULL == ffslice_allocT(&res.ip, total, ffip6)) {
		res.status = -1;
		goto done;
	}
	ffip6 *ip = res.ip.ptr;
	for (i = i4;  i != q->nres;  ) {
		const ffdnscl_res *r = q->res[i];
		for (uint k = 0;  k != r->naddrs;  k++) {
			if (r->is4)
				ffip6_v4mapped_set(&ip[res.ip.len], &r->addrs4[k]);
			else
				ip[res.ip.len] = r->addrs6[k];
			res.ip.len++;
		}

		if (i == i4 && i4 == 0)
			i = 1;
		else if (i == i4 && i4 != 0)
			i = 0;
		else
			break;
	}

	if (serv != NULL)
		res.server_addr = serv->saddr;

done:
	FFSLICE_WALK(&q->users, quser) {
		dbglog_q(q, LOG_DBGFLOW, "calling user function %p, udata:%p"
			, quser->ondone, quser->udata);
		quser->ondone(quser->udata, &res);
	}
	ffslice_free(&res.ip);
	for (i = 0;  i != q->nres;  i++) {
		res_free(q->res[i]);
	}

	dbglog_q(q, LOG_DBGFLOW, "query done [%L]"
		, q->r->queries.len);
	query_free(q);
}


/** Split "IP[:PORT]" address string.
e.g.: "127.0.0.1", "127.0.0.1:80", "[::1]:80", ":80".
@ip: output address.  Brackets aren't included for IPv6 address.
@port: output port.
Return 0 on success. */
static inline int ffip_split(const char *data, size_t len, ffstr *ip, ffstr *port)
{
	const char *pos = ffs_rsplit2by(data, len, ':', ip, port);

	if (ip->len != 0 && ip->ptr[0] == '[') {
		if (data[len - 1] == ']') {
			ip->len = len;
			ffstr_null(port);
			pos = NULL;
		}
		if (!(ip->len >= FFSLEN("[::]") && ip->ptr[ip->len - 1] == ']'))
			return -1;
		ip->ptr += FFSLEN("[");
		ip->len -= FFSLEN("[]");
	}

	if (pos != NULL && port->len == 0)
		return -1; // "ip:"

	return 0;
}

int ffdnscl_serv_add(ffdnsclient *r, const ffstr *saddr)
{
	ffdnscl_serv *serv = ffmem_new(ffdnscl_serv);
	if (serv == NULL)
		return -1;

	serv->r = r;
	serv->ansbuf = ffmem_alloc(r->buf_size);
	if (serv->ansbuf == NULL)
		goto err;

	ffstr ip, sport;
	ffip4 a4;
	if (0 != ffip_split(saddr->ptr, saddr->len, &ip, &sport))
		goto err;
	ushort port = FFDNS_PORT;
	if (sport.len != 0) {
		if (!ffstr_toint(&sport, &port, FFS_INT16))
			goto err;
	}
	if (0 != ffip4_parse(&a4, ip.ptr, ip.len))
		goto err;
	ffaddr_init(&serv->addr);
	ffip4_set(&serv->addr, (void*)&a4);
	ffip_setport(&serv->addr, port);

	char *s = ffs_copy(serv->saddr_s, serv->saddr_s + FF_COUNT(serv->saddr_s), saddr->ptr, saddr->len);
	ffstr_set(&serv->saddr, serv->saddr_s, s - serv->saddr_s);

	fflist_add(&r->servs, &serv->sib);
	r->curserv = FF_GETPTR(ffdnscl_serv, sib, fflist_first(&r->servs));
	return 0;

err:
	ffmem_free(serv);
	return -1;
}

/** Prepare socket to connect to a DNS server. */
static int serv_init(ffdnscl_serv *serv)
{
	const char *er;
	ffskt sk;

	sk = ffskt_create(ffaddr_family(&serv->addr), SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
	if (sk == FF_BADSKT) {
		er = "ffskt_create";
		goto fail;
	}

	{
		ffaddr la;
		ffaddr_init(&la);
		ffaddr_setany(&la, ffaddr_family(&serv->addr));
		if (0 != ffskt_bind(sk, &la.a, la.len)) {
			er = "ffskt_bind";
			goto fail;
		}
	}

	ffaio_init(&serv->aiotask);
	serv->aiotask.udata = serv;
	serv->aiotask.sk = sk;
	serv->aiotask.udp = 1;
	if (0 != ffaio_attach(&serv->aiotask, serv->r->kq, FFKQU_READ)) {
		er = "ffaio_attach";
		goto fail;
	}

	serv->sk = sk;
	return 0;

fail:
	syserrlog_srv(serv, "%s", er);

	if (sk != FF_BADSKT)
		ffskt_close(sk);

	return 1;
}

static void serv_fin(ffdnscl_serv *serv)
{
	FF_SAFECLOSE(serv->sk, FF_BADSKT, ffskt_close);
	FF_SAFECLOSE(serv->ansbuf, NULL, ffmem_free);
	ffmem_free(serv);
}

/** Round-robin balancer. */
static ffdnscl_serv * serv_next(ffdnsclient *r)
{
	ffdnscl_serv *serv = r->curserv;
	fflist_item *next = ((serv->sib.next != fflist_sentl(&r->servs)) ? serv->sib.next : fflist_first(&r->servs));
	r->curserv = FF_GETPTR(ffdnscl_serv, sib, next);
	return serv;
}

void res_free(ffdnscl_res *dr)
{
	ffmem_free(dr);
}
