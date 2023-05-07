/** ff: file AIO
2022 Simon Zolin
*/

/*
ffaio_init ffaio_destroy
ffaio_read_prepare ffaio_write_prepare
ffaio_submit
ffaio_cancel
ffaio_getevents
*/

#include <ffbase/base.h>
#include <linux/aio_abi.h>
#include <sys/syscall.h>

typedef aio_context_t ffaio;

/** Initialize AIO subsystem */
static inline int ffaio_init(ffaio *aio, uint workers)
{
	*aio = 0;
	return syscall(SYS_io_setup, workers, aio);
}

/** Destroy AIO subsystem */
static inline int ffaio_destroy(ffaio aio)
{
	return syscall(SYS_io_destroy, aio);
}

static void _ffaio_op_prepare(struct iocb *acb, int ev_fd, void *ev_data, int fd, void *buf, ffsize n, ffuint64 off)
{
	ffmem_zero_obj(acb);
	acb->aio_data = (ffsize)ev_data;
	acb->aio_flags = IOCB_FLAG_RESFD;
	acb->aio_resfd = ev_fd;

	acb->aio_fildes = fd;
	acb->aio_buf = (ffsize)buf;
	acb->aio_nbytes = n;
	acb->aio_offset = off;
}

/** Prepare read operation */
static inline void ffaio_read_prepare(struct iocb *acb, int ev_fd, void *ev_data, int fd, void *buf, ffsize n, ffuint64 off)
{
	_ffaio_op_prepare(acb, ev_fd, ev_data, fd, buf, n, off);
	acb->aio_lio_opcode = IOCB_CMD_PREAD;
}

/** Prepare write operation */
static inline void ffaio_write_prepare(struct iocb *acb, int ev_fd, void *ev_data, int fd, void *buf, ffsize n, ffuint64 off)
{
	_ffaio_op_prepare(acb, ev_fd, ev_data, fd, buf, n, off);
	acb->aio_lio_opcode = IOCB_CMD_PWRITE;
}

/** Return N of submitted requests */
static inline int ffaio_submit(ffaio aio, struct iocb **cbs, ffsize n)
{
	return syscall(SYS_io_submit, aio, n, cbs);
}

/** Cancel pending operation */
static inline int ffaio_cancel(ffaio aio, struct iocb *cb)
{
	struct io_event ev = {};
	return syscall(SYS_io_cancel, aio, cb, &ev);
}

/** Get completed operations */
static inline int ffaio_getevents(ffaio aio, long min_nr, long nr, struct io_event *events, uint timeout_msec)
{
	struct timespec ts = {};
	if (timeout_msec != 0) {
		ts.tv_sec = timeout_msec / 1000;
		ts.tv_nsec = timeout_msec * 1000000;
	}
	return syscall(SYS_io_getevents, aio, min_nr, nr, events, &ts);
}
