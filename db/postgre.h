/** PostgreSQL libpq wrapper.
Copyright 2018 Simon Zolin.
*/

#pragma once
#include <ffbase/string.h>
#include <postgresql/libpq-fe.h>
#include <FFOS/atomic.h>

static ffatomic _ffdb_postgre_stmt_cntr;

/** Parameter for a prepared SQL statement.
n: 1.. */
#define FFDB_Q(n)  "$" #n

enum FFDB_E {
	FFDB_OPEN_OK = CONNECTION_OK,
	FFDB_OK = PGRES_COMMAND_OK,
	FFDB_ROW = PGRES_SINGLE_TUPLE,
	FFDB_DONE = -2,
	FFDB_ERR = -3,
};

typedef PGconn ffdb;

/** Connect to server.
Return FFDB_OPEN_OK on success. */
static inline int ffdb_open(ffdb **pdb, const char *const *names, const char *const *values)
{
	*pdb = PQconnectdbParams(names, values, 0);
	if (*pdb == NULL)
		return FFDB_ERR;
	return PQstatus(*pdb);
}

/** Close connection with server. */
#define ffdb_close(db)  PQfinish(db)

/** Execute a simple query.
Return FFDB_OK on success. */
static inline int ffdb_exec(ffdb *db, const char *sql)
{
	PGresult *res = PQexec(db, sql);
	if (res == NULL)
		return FFDB_ERR;
	int r = PQresultStatus(res);
	PQclear(res);
	return r;
}


/** Prepared SQL statement */
typedef struct ffdb_stmt {
	ffdb *db;
	PGresult *stmt;
	PGresult *res;
	char name[FFINT_MAXCHARS];
	int nparams;
	int ncols;

	const char **data;
	int *lens;
	int *fmts;
	int *data_int4;
	int64 *data_int8;

	const char *errmsg;
} ffdb_stmt;

/** Get error message. */
#define ffdb_errstr(q)  ((q)->errmsg)

/** Prepare SQL statement. */
int ffdb_prepare(ffdb *db, ffdb_stmt *q, const char *sql)
{
	ffmem_tzero(q);

	int rc;

	size_t i = ffatom_incret(&_ffdb_postgre_stmt_cntr);
	int n = ffs_fromint(i, q->name, sizeof(q->name), 0);
	q->name[n] = '\0';

	q->stmt = PQprepare(db, q->name, sql, 0, NULL);
	if (q->stmt == NULL) {
		q->errmsg = PQerrorMessage(db);
		return FFDB_ERR;
	}

	rc = PQresultStatus(q->stmt);
	if (rc != FFDB_OK) {
		q->errmsg = PQresultErrorMessage(q->stmt);
		PQclear(q->stmt);
		q->stmt = NULL;
		return rc;
	}

	PGresult *res = PQdescribePrepared(db, q->name);
	q->nparams = PQnparams(res);
	q->ncols = PQnfields(res);
	PQclear(res);

	if (q->nparams != 0) {
		char *p = ffmem_alloc(q->nparams * (sizeof(void*) + sizeof(int) * 3 + sizeof(int64)));
		if (p == NULL) {
			PQclear(q->stmt);
			q->stmt = NULL;
			q->errmsg = "memory allocation";
			return FFDB_ERR;
		}

		q->data = (const char**)p;
		q->lens = (int*)(p + q->nparams * sizeof(void*));
		q->fmts = (int*)(p + q->nparams * (sizeof(void*) + sizeof(int)));
		q->data_int4 = (int*)(p + q->nparams * (sizeof(void*) + sizeof(int) * 2));
		q->data_int8 = (int64*)(p + q->nparams * (sizeof(void*) + sizeof(int) * 3));
	}

	q->db = db;
	return FFDB_OK;
}

/** Destroy a prepared SQL statement. */
void ffdb_fin(ffdb_stmt *q)
{
	ffdb_reset(q);
	if (q->data != NULL)
		ffmem_free((char*)q->data);
	if (q->stmt != NULL)
		PQclear(q->stmt);
}

/** Execute prepared statement.
Return FFDB_ROW, FFDB_DONE or error. */
int ffdb_next(ffdb_stmt *q)
{
	if (q->res == NULL) {
		PQsendQueryPrepared(q->db, q->name, q->nparams, q->data, q->lens, q->fmts
			, /*resultFormat=*/ 1 /*binary*/);
		PQsetSingleRowMode(q->db);
	} else {
		PQclear(q->res);
		q->res = NULL;
	}

	q->res = PQgetResult(q->db);
	if (q->res == NULL) {
		q->errmsg = PQerrorMessage(q->db);
		return FFDB_ERR;
	}

	int r = PQresultStatus(q->res);
	if (r == PGRES_TUPLES_OK)
		return FFDB_DONE;
	q->errmsg = PQresultErrorMessage(q->res);
	return r;
}

/** Reset statement, but keep bindings. */
void ffdb_reset(ffdb_stmt *q)
{
	if (q->res != NULL) {
		PQclear(q->res);
		q->res = NULL;
	}

	for (;;) {
		q->res = PQgetResult(q->db);
		if (q->res == NULL)
			break;
		PQclear(q->res);
	}
}

/** Get number of rows affected. */
static inline int ffdb_changes(ffdb_stmt *q)
{
	const char *s = PQcmdTuples(q->res);
	int r;
	if (0 == ffs_toint(s, ffsz_len(s), &r, 4))
		return -1;
	return r;
}


/** Get number of parameters. */
static inline int ffdb_params(ffdb_stmt *q)
{
	return q->nparams;
}

/** Set parameter as text. */
static inline int ffdb_settext(ffdb_stmt *q, int i, const char *val, size_t sz)
{
	q->data[i] = val;
	q->lens[i] = sz;
	q->fmts[i] = 1;
	return FFDB_OK;
}

/** Set NULL parameter. */
static inline int ffdb_setnull(ffdb_stmt *q, int i)
{
	q->data[i] = NULL;
	q->lens[i] = 0;
	q->fmts[i] = 1;
	return FFDB_OK;
}

/** Set parameter as int. */
static inline int ffdb_setint(ffdb_stmt *q, int i, int val)
{
	q->data_int4[i] = ffhton32(val);
	q->data[i] = (char*)&q->data_int4[i];
	q->lens[i] = sizeof(int);
	q->fmts[i] = 1;
	return FFDB_OK;
}

/** Set parameter as int64. */
static inline int ffdb_setint64(ffdb_stmt *q, int i, int64 val)
{
	q->data_int8[i] = ffhton64(val);
	q->data[i] = (char*)&q->data_int8[i];
	q->lens[i] = sizeof(int64);
	q->fmts[i] = 1;
	return FFDB_OK;
}


/** Get number of resulting columns. */
static inline int ffdb_cols(ffdb_stmt *q)
{
	return q->ncols;
}

/** Check if result is NULL. */
static inline ffbool ffdb_getnull(ffdb_stmt *q, uint col)
{
	return PQgetisnull(q->res, 0, col);
}

/** Get result as int. */
static inline int ffdb_getint(ffdb_stmt *q, uint col)
{
	const char *p = PQgetvalue(q->res, 0, col);
	return ffhton32(*(int*)p);
}

/** Get result as int64. */
static inline int64 ffdb_getint64(ffdb_stmt *q, uint col)
{
	const char *p = PQgetvalue(q->res, 0, col);
	return ffhton64(*(int64*)p);
}

/** Get result as text. */
static inline ffstr ffdb_getstr(ffdb_stmt *q, uint col)
{
	ffstr s;
	s.ptr = PQgetvalue(q->res, 0, col);
	s.len = PQgetlength(q->res, 0, col);
	return s;
}
