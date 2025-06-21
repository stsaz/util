/** ffmpeg wrapper
2025, Simon Zolin */

#pragma once
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

typedef struct ffmpeg_packet ffmpeg_packet;
struct ffmpeg_packet {
	AVPacket *pkt;
};

typedef struct ffmpeg_frame ffmpeg_frame;
struct ffmpeg_frame {
	AVFrame *frame;

#ifdef __cplusplus
	bool key() const { return !!(frame->flags & AV_FRAME_FLAG_KEY); }
#endif
};

#ifdef __cplusplus
extern "C" {
#endif
void ffmpeg_packet_init(ffmpeg_packet *p);
void ffmpeg_packet_destroy(ffmpeg_packet *p);

void ffmpeg_frame_init(ffmpeg_frame *f);
void ffmpeg_frame_destroy(ffmpeg_frame *f);
void ffmpeg_frame_unref(ffmpeg_frame *f);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
struct xxffmpeg_packet : ffmpeg_packet {
	xxffmpeg_packet() { ffmpeg_packet_init(this); }
	~xxffmpeg_packet() { ffmpeg_packet_destroy(this); }
	int stream_index() const { return pkt->stream_index; }
	uint64_t pts() const { return (pkt->pts != AV_NOPTS_VALUE) ? pkt->pts : pkt->dts; }
	int size() const { return pkt->size; }
	// pts
	int duration() const { return pkt->duration; }
};
#endif

typedef struct ffmpeg_dec ffmpeg_dec;
struct ffmpeg_dec {
	AVFormatContext *fmt;
	AVCodecContext *vcodecx, *acodecx;
	const AVCodec *vcodec, *acodec;
	int video_stream, audio_stream;

	AVBufferRef *hw_device_ctx;
	enum AVPixelFormat hw_pix_fmt;
	AVFrame *hw_frame;

	const char *err_func;
	int error_code;
	char error_buf[64];
};

typedef int (*ffmpeg_io_read)(void *opaque, uint8_t *buf, int buf_size);
typedef int64_t (*ffmpeg_io_seek)(void *opaque, int64_t offset, int whence);

#ifdef __cplusplus
extern "C" {
#endif
void ffmpeg_dec_init(ffmpeg_dec *d);
void ffmpeg_dec_destroy(ffmpeg_dec *d);
const char* ffmpeg_dec_error(ffmpeg_dec *d);
int ffmpeg_dec_open(ffmpeg_dec *d, ffmpeg_io_read read, ffmpeg_io_seek seek, void *opaque, uint flags);
int ffmpeg_dec_seek(ffmpeg_dec *d, uint64_t pos);
/**
Return 0: success; 1: EOF; <0: error */
int ffmpeg_dec_pkt_read(ffmpeg_dec *d, ffmpeg_packet *p);
int ffmpeg_dec_video_decode(ffmpeg_dec *d, ffmpeg_packet *p, ffmpeg_frame *f);
int ffmpeg_dec_audio_decode(ffmpeg_dec *d, ffmpeg_packet *p, ffmpeg_frame *f);
int ffmpeg_dec_hwaccel_enable(ffmpeg_dec *d, const char *hwaccel);
int ffmpeg_dec_audio_stream_switch(ffmpeg_dec *d);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
struct xxffmpeg_dec : ffmpeg_dec {
	xxffmpeg_dec() {
		memset(this, 0, sizeof(*this));
		ffmpeg_dec_init(this);
	}
	~xxffmpeg_dec() { ffmpeg_dec_destroy(this); }
	const char* error() { return ffmpeg_dec_error(this); }

	bool open(ffmpeg_io_read read, ffmpeg_io_seek seek, void *opaque) { return !ffmpeg_dec_open(this, read, seek, opaque, 0); }
	bool seek(uint64_t pos) { return ffmpeg_dec_seek(this, pos); }
	int read(ffmpeg_packet *p) { return ffmpeg_dec_pkt_read(this, p); }
	bool video_decode(ffmpeg_packet &p, ffmpeg_frame *f) { return !ffmpeg_dec_video_decode(this, &p, f); }
	bool audio_decode(ffmpeg_packet &p, ffmpeg_frame *f) { return !ffmpeg_dec_audio_decode(this, &p, f); }

	// msec
	uint64_t duration() const { return fmt->duration / 1000; }
	uint streams() const { return fmt->nb_streams; }

	bool have_video() const { return !!vcodecx; }
	bool hwaccel_enable(const char *hwaccel) { return !ffmpeg_dec_hwaccel_enable(this, hwaccel); }
	int video_width() const { return vcodecx->width; }
	int video_height() const { return vcodecx->height; }
	int video_codec() const { return vcodecx->codec_id; } // AV_CODEC_ID_*
	double video_time_base() const { return av_q2d(fmt->streams[video_stream]->time_base); }

	bool have_audio() const { return !!acodecx; }
	int audio_stream_switch() { return ffmpeg_dec_audio_stream_switch(this); }
	// AV_SAMPLE_FMT_*
	int audio_format() const { return acodecx->sample_fmt; }
	int audio_rate() const { return acodecx->sample_rate; }
	int audio_channels() const { return acodecx->ch_layout.nb_channels; }
	double audio_time_base() const { return av_q2d(fmt->streams[audio_stream]->time_base); }
};
#endif
