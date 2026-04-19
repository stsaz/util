/** ffmpeg wrapper
2025, Simon Zolin */

#define LOG(fmt, ...)
#ifdef FF_DEBUG
	#include <ffsys/std.h>
	#include <libavutil/pixdesc.h>
	#undef LOG
	#define LOG(fmt, ...) \
		fflog(fmt, ##__VA_ARGS__)
#endif

#include "ffmpeg.h"

#define ERR(d, code, func)  d->error_code = code, d->err_func = func, -1
#define ERRX(d, r, code, func)  d->error_code = code, d->err_func = func, r

void ffmpeg_packet_init(ffmpeg_packet *p)
{
	p->pkt = av_packet_alloc();
}

void ffmpeg_packet_destroy(ffmpeg_packet *p)
{
	av_packet_free(&p->pkt);
}


void ffmpeg_frame_init(ffmpeg_frame *f)
{
	f->frame = av_frame_alloc();
}

void ffmpeg_frame_destroy(ffmpeg_frame *f)
{
	av_frame_free(&f->frame);
}

void ffmpeg_frame_unref(ffmpeg_frame *f)
{
	av_frame_unref(f->frame);
}


void ffmpeg_config(unsigned log_level)
{
	av_log_set_level(log_level);
}


void ffmpeg_dec_init(ffmpeg_dec *d)
{
}

void ffmpeg_dec_destroy(ffmpeg_dec *d)
{
	av_buffer_unref(&d->hw_device_ctx);
	av_frame_free(&d->hw_frame);

	av_frame_free(&d->sws_frame);
	sws_freeContext(d->swsx);
	avcodec_free_context(&d->vcodecx);
	avcodec_free_context(&d->acodecx);
	avformat_close_input(&d->fmt);
}

static int str_copyz(char *dst, size_t cap, const char *sz)
{
	size_t n = strlen(sz);
	if (n >= cap)
		n = cap;
	memcpy(dst, sz, n);
	return n;
}

const char* ffmpeg_dec_error(ffmpeg_dec *d)
{
	unsigned cap = sizeof(d->error_buf);
	char *p = d->error_buf;
	p += str_copyz(d->error_buf, cap - 3, d->err_func);
	*(p++) = ':';
	*(p++) = ' ';
	av_strerror(d->error_code, p, d->error_buf + cap - p);
	return d->error_buf;
}

static int dec_open(ffmpeg_dec *d, int *stream, AVCodecContext **decx, const AVCodec **codec, enum AVMediaType type)
{
	int r;
	if ((r = av_find_best_stream(d->fmt, type, -1, -1, codec, 0)) < 0)
		return 0;
	*stream = r;

	if (!(*decx = avcodec_alloc_context3(*codec)))
		return ERR(d, 0, "avcodec_alloc_context3()");

	const AVStream *stm = d->fmt->streams[*stream];
	if ((r = avcodec_parameters_to_context(*decx, stm->codecpar)) < 0)
		return ERR(d, r, "avcodec_parameters_to_context()");

	if ((r = avcodec_open2(*decx, *codec, NULL)) < 0)
		return ERR(d, r, "avcodec_open2()");
	return 0;
}

int ffmpeg_dec_open(ffmpeg_dec *d, ffmpeg_io_read read, ffmpeg_io_seek seek, void *opaque, unsigned flags)
{
	d->hw_pix_fmt = -1;
	d->video_stream = d->audio_stream = -1;

	if (!(d->fmt = avformat_alloc_context()))
		return ERR(d, 0, "avformat_alloc_context()");

	unsigned cap = 32*1024;
	uint8_t *buf = av_malloc(cap);
	if (!(d->fmt->pb = avio_alloc_context(buf, cap, 0, opaque, read, NULL, seek))) {
		av_free(buf);
		return ERR(d, 0, "avio_alloc_context()");
	}

	int r;
	if ((r = avformat_open_input(&d->fmt, NULL, NULL, NULL)) < 0)
		return ERR(d, r, "avformat_open_input()");

	if ((r = avformat_find_stream_info(d->fmt, NULL)) < 0)
		return ERR(d, r, "avformat_find_stream_info()");

	if (dec_open(d, &d->video_stream, &d->vcodecx, &d->vcodec, AVMEDIA_TYPE_VIDEO) < 0)
		return -1;
	if (dec_open(d, &d->audio_stream, &d->acodecx, &d->acodec, AVMEDIA_TYPE_AUDIO) < 0)
		return -1;
	if (d->video_stream < 0 && d->audio_stream < 0)
		return -1;
	return 0;
}

int ffmpeg_dec_audio_stream_switch(ffmpeg_dec *d)
{
	int r;
	const AVStream *stm;
	for (unsigned i = 0;;  i++) {
		if (i == d->fmt->nb_streams)
			return 1; // no other stream available

		stm = d->fmt->streams[i];
		if (stm->codecpar->codec_type == AVMEDIA_TYPE_AUDIO
			&& i != d->audio_stream) {
			d->audio_stream = i;
			break;
		}
	}

	avcodec_free_context(&d->acodecx);

	if (!(d->acodecx = avcodec_alloc_context3(NULL)))
		return ERR(d, 0, "avcodec_alloc_context3()");

	if ((r = avcodec_parameters_to_context(d->acodecx, stm->codecpar)) < 0)
		return ERR(d, r, "avcodec_parameters_to_context()");

	d->acodec = avcodec_find_decoder(d->acodecx->codec_id);

	if ((r = avcodec_open2(d->acodecx, d->acodec, NULL)) < 0)
		return ERR(d, r, "avcodec_open2()");
	d->reading_frames &= ~2;
	return 0;
}

int ffmpeg_dec_seek(ffmpeg_dec *d, uint64_t pos)
{
	int r;
	if ((r = avformat_seek_file(d->fmt, -1, pos, pos, pos, 0)) < 0)
		return ERR(d, r, "avformat_seek_file()");
	d->reading_frames = 0;
	return 0;
}

int ffmpeg_dec_pkt_read(ffmpeg_dec *d, ffmpeg_packet *p)
{
	av_packet_unref(p->pkt);
	int r;
	if ((r = av_read_frame(d->fmt, p->pkt)) < 0) {
		if (r == AVERROR_EOF)
			return 1;
		return ERR(d, r, "av_read_frame()");
	}
	return 0;
}

static enum AVPixelFormat get_format_hw(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
	ffmpeg_dec *d = ctx->opaque;
	for (const enum AVPixelFormat *p = pix_fmts;  *p != -1;  p++) {
		if (*p == d->hw_pix_fmt)
			return *p;
	}

	d->error_code = FFMPEG_E_HWDEC;
	return AV_PIX_FMT_NONE;
}

static int _ffmpeg_dec_sw_hw_format(ffmpeg_dec *d, AVBufferRef *dev, int hw_format)
{
	int r;

	AVHWFramesConstraints *fc;
	if (!(fc = av_hwdevice_get_hwframe_constraints(dev, NULL)))
		return ERRX(d, AV_PIX_FMT_NONE, 0, "av_hwdevice_get_hwframe_constraints()");

	AVBufferRef *buf;
	if (!(buf = av_hwframe_ctx_alloc(dev)))
		return ERRX(d, AV_PIX_FMT_NONE, 0, "av_hwframe_ctx_alloc()");

	AVHWFramesContext *fx = (void*)buf->data;
	fx->format = hw_format;
	fx->sw_format = fc->valid_sw_formats[0];
	fx->width = 128;
	fx->height = 128;

	if ((r = av_hwframe_ctx_init(buf)) < 0)
		return ERRX(d, AV_PIX_FMT_NONE, 0, "av_hwframe_ctx_init()");

	enum AVPixelFormat *fmts;
	if ((r = av_hwframe_transfer_get_formats(buf, AV_HWFRAME_TRANSFER_DIRECTION_TO, &fmts, 0)) < 0)
		return ERRX(d, AV_PIX_FMT_NONE, 0, "av_hwframe_transfer_get_formats()");

	r = fmts[0];
	for (unsigned i = 0;  fmts[i] != AV_PIX_FMT_NONE;  i++) {
		LOG("fmt: %s", av_get_pix_fmt_name(fmts[i]));
	}

	av_hwframe_constraints_free(&fc);
	av_buffer_unref(&buf);
	av_free(fmts);
	return r;
}

int ffmpeg_dec_hwaccel_enable(ffmpeg_dec *d, const char *hwaccel)
{
	enum AVHWDeviceType type = av_hwdevice_find_type_by_name(hwaccel);
	if (type == AV_HWDEVICE_TYPE_NONE)
		return ERR(d, 0, "av_hwdevice_find_type_by_name()");

	const AVCodecHWConfig *hwc;
	for (unsigned i = 0;; i++) {
		hwc = avcodec_get_hw_config(d->vcodec, i);
		if (!hwc)
			return ERR(d, 0, "avcodec_get_hw_config()");
		LOG("avcodec_get_hw_config: methods:%xu  device_type:%xu  pix_fmt:%s"
			, hwc->methods, hwc->device_type, av_get_pix_fmt_name(hwc->pix_fmt));

		if ((hwc->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
			&& hwc->device_type == type)
			break;
	}

	int r;
	if ((r = av_hwdevice_ctx_create(&d->hw_device_ctx, type, NULL, NULL, 0)) < 0)
		return ERR(d, r, "av_hwdevice_ctx_create()");

	if (AV_PIX_FMT_NONE == (d->sw_pix_fmt = _ffmpeg_dec_sw_hw_format(d, d->hw_device_ctx, hwc->pix_fmt)))
		return -1;
	d->hw_pix_fmt = hwc->pix_fmt;
	d->hw_frame = av_frame_alloc();

	d->vcodecx->hw_device_ctx = av_buffer_ref(d->hw_device_ctx);
	d->vcodecx->opaque = d;
	d->vcodecx->get_format = get_format_hw;
	return 0;
}

int ffmpeg_dec_video_decode(ffmpeg_dec *d, ffmpeg_packet *p, ffmpeg_frame *f)
{
	int r;

	d->error_code = 0;
	if (!(d->reading_frames & 1)
		&& (r = avcodec_send_packet(d->vcodecx, p->pkt)))
		return ERRX(d, (d->error_code) ? d->error_code : -1, r, "avcodec_send_packet()");
	d->reading_frames |= 1;

	if ((r = avcodec_receive_frame(d->vcodecx, f->frame))) {
		if (r == AVERROR(EAGAIN)) {
			d->reading_frames &= ~1;
			return 1;
		}

		return ERR(d, r, "avcodec_receive_frame()");
	}

	if (f->frame->format == d->hw_pix_fmt) {
		d->hw_frame->format = d->sw_pix_fmt;
		if ((r = av_hwframe_transfer_data(d->hw_frame, f->frame, 0)) < 0) {
			av_frame_unref(f->frame);
			return ERR(d, r, "av_hwframe_transfer_data()");
		}
		FFSWAP(AVFrame*, f->frame, d->hw_frame);
		av_frame_unref(d->hw_frame);
	}

	return 0;
}

int ffmpeg_dec_audio_decode(ffmpeg_dec *d, ffmpeg_packet *p, ffmpeg_frame *f)
{
	int r;

	if (!(d->reading_frames & 2)
		&& (r = avcodec_send_packet(d->acodecx, p->pkt)))
		return ERR(d, r, "avcodec_send_packet()");
	d->reading_frames |= 2;

	if ((r = avcodec_receive_frame(d->acodecx, f->frame))) {
		if (r == AVERROR(EAGAIN)) {
			d->reading_frames &= ~2;
			return 1;
		}

		return ERR(d, r, "avcodec_receive_frame()");
	}

	return 0;
}

int ffmpeg_dec_video_convert(ffmpeg_dec *d, ffmpeg_frame *f)
{
	int r;
	AVFrame *avf = f->frame;

	if (!d->swsx) {
		if (!(d->swsx = sws_getContext(avf->width, avf->height, avf->format
			, avf->width, avf->height, AV_PIX_FMT_RGB0, SWS_BICUBIC, NULL, NULL, NULL)))
			return ERR(d, 0, "sws_getContext()");
		d->sws_frame = av_frame_alloc();
	}

	if ((r = sws_scale_frame(d->swsx, d->sws_frame, avf)) < 0)
		return ERR(d, r, "sws_scale_frame()");
	FFSWAP(AVFrame*, f->frame, d->sws_frame);
	av_frame_unref(d->sws_frame);
	return 0;
}
