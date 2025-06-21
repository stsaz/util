/** ffmpeg wrapper
2025, Simon Zolin */

#include "ffmpeg.h"

#define ERR(d, code, func)  d->error_code = code, d->err_func = func, -1

void ffmpeg_packet_init(ffmpeg_packet *p) {
	p->pkt = av_packet_alloc();
}

void ffmpeg_packet_destroy(ffmpeg_packet *p) {
	av_packet_free(&p->pkt);
}


void ffmpeg_frame_init(ffmpeg_frame *f) {
	f->frame = av_frame_alloc();
}

void ffmpeg_frame_destroy(ffmpeg_frame *f) {
	av_frame_free(&f->frame);
}

void ffmpeg_frame_unref(ffmpeg_frame *f) {
	av_frame_unref(f->frame);
}


void ffmpeg_dec_init(ffmpeg_dec *d) {
}

void ffmpeg_dec_destroy(ffmpeg_dec *d) {
	av_buffer_unref(&d->hw_device_ctx);
	av_frame_free(&d->hw_frame);

	avcodec_free_context(&d->vcodecx);
	avcodec_free_context(&d->acodecx);
	avformat_close_input(&d->fmt);
}

const char* ffmpeg_dec_error(ffmpeg_dec *d) {
	uint cap = sizeof(d->error_buf);
	char *p = stpncpy(d->error_buf, d->err_func, cap - 2);
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

int ffmpeg_dec_open(ffmpeg_dec *d, ffmpeg_io_read read, ffmpeg_io_seek seek, void *opaque, uint flags) {
	d->hw_pix_fmt = -1;
	d->video_stream = d->audio_stream = -1;

	if (!(d->fmt = avformat_alloc_context()))
		return ERR(d, 0, "avformat_alloc_context()");

	uint cap = 32*1024;
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
	for (uint i = 0;;  i++) {
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
	return 0;
}

int ffmpeg_dec_seek(ffmpeg_dec *d, uint64_t pos) {
	int r;
	if ((r = avformat_seek_file(d->fmt, -1, pos, pos, pos, 0)) < 0)
		return ERR(d, r, "avformat_seek_file()");
	return 0;
}

int ffmpeg_dec_pkt_read(ffmpeg_dec *d, ffmpeg_packet *p) {
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
	const ffmpeg_dec *d = ctx->opaque;
	for (const enum AVPixelFormat *p = pix_fmts;  *p != -1;  p++) {
		if (*p == d->hw_pix_fmt)
			return *p;
	}

	return AV_PIX_FMT_NONE;
}

int ffmpeg_dec_hwaccel_enable(ffmpeg_dec *d, const char *hwaccel) {
	enum AVHWDeviceType type = av_hwdevice_find_type_by_name(hwaccel);
	if (type == AV_HWDEVICE_TYPE_NONE)
		return ERR(d, 0, "av_hwdevice_find_type_by_name()");

	for (uint i = 0;; i++) {
		const AVCodecHWConfig *config = avcodec_get_hw_config(d->vcodec, i);
		if (!config)
			return ERR(d, 0, "avcodec_get_hw_config()");
		if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
			&& config->device_type == type) {
			d->hw_pix_fmt = config->pix_fmt;
			break;
		}
	}

	int r;
	if ((r = av_hwdevice_ctx_create(&d->hw_device_ctx, type, NULL, NULL, 0)) < 0)
		return ERR(d, r, "av_hwdevice_ctx_create()");

	d->vcodecx->hw_device_ctx = av_buffer_ref(d->hw_device_ctx);
	d->vcodecx->opaque = d;
	d->vcodecx->get_format = get_format_hw;
	d->hw_frame = av_frame_alloc();
	return 0;
}

int ffmpeg_dec_video_decode(ffmpeg_dec *d, ffmpeg_packet *p, ffmpeg_frame *f) {
	int r;
	if ((r = avcodec_send_packet(d->vcodecx, p->pkt)) < 0)
		return ERR(d, r, "avcodec_send_packet()");

	av_frame_unref(f->frame);
	if ((r = avcodec_receive_frame(d->vcodecx, f->frame)) < 0)
		return ERR(d, r, "avcodec_receive_frame()");

	if (f->frame->format == d->hw_pix_fmt) {
		av_frame_unref(d->hw_frame);
		d->hw_frame->format = AV_PIX_FMT_YUV420P;
		if ((r = av_hwframe_transfer_data(d->hw_frame, f->frame, 0)) < 0) {
			av_frame_unref(f->frame);
			return ERR(d, r, "av_hwframe_transfer_data()");
		}
		void *tmp = f->frame;
		f->frame = d->hw_frame;
		d->hw_frame = tmp;
	}

	return 0;
}

int ffmpeg_dec_audio_decode(ffmpeg_dec *d, ffmpeg_packet *p, ffmpeg_frame *f) {
	int r;
	if ((r = avcodec_send_packet(d->acodecx, p->pkt)) < 0)
		return ERR(d, r, "avcodec_send_packet()");

	av_frame_unref(f->frame);
	if ((r = avcodec_receive_frame(d->acodecx, f->frame)) < 0)
		return ERR(d, r, "avcodec_receive_frame()");

	return 0;
}
