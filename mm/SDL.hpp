/** SDL wrapper
2025, Simon Zolin */

#pragma once
#include <SDL3/SDL.h>
#undef main // Prevent SDL from overriding main()
#include <libavformat/avformat.h>

static inline int sdl_init(uint init)
{
	if (!init) {
		SDL_Quit();
		return 0;
	}

	return !SDL_Init(SDL_INIT_VIDEO);
}

static inline int sdl_read_events(SDL_Event *events, uint cap, SDL_Window *windows[])
{
	SDL_PumpEvents();
	int n = SDL_PeepEvents(events, cap, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
	if (windows) {
		for (int i = 0;  i < n;  i++) {
			windows[i] = SDL_GetWindowFromEvent(events + i);
		}
	}
	return n;
}

static inline SDL_PixelFormat format_sdl_av(int av_format, SDL_BlendMode *blendmode)
{
	*blendmode = (av_format == AV_PIX_FMT_RGB32
			|| av_format == AV_PIX_FMT_RGB32_1
			|| av_format == AV_PIX_FMT_BGR32
			|| av_format == AV_PIX_FMT_BGR32_1)
		? SDL_BLENDMODE_BLEND
		: SDL_BLENDMODE_NONE;

	static const struct {
		enum AVPixelFormat av_format;
		SDL_PixelFormat sdl_format;
	} texture_format_map[] = {
		{ AV_PIX_FMT_RGB8,		SDL_PIXELFORMAT_RGB332 },
		{ AV_PIX_FMT_RGB444,	SDL_PIXELFORMAT_XRGB4444 },
		{ AV_PIX_FMT_RGB555,	SDL_PIXELFORMAT_XRGB1555 },
		{ AV_PIX_FMT_BGR555,	SDL_PIXELFORMAT_XBGR1555 },
		{ AV_PIX_FMT_RGB565,	SDL_PIXELFORMAT_RGB565 },
		{ AV_PIX_FMT_BGR565,	SDL_PIXELFORMAT_BGR565 },
		{ AV_PIX_FMT_RGB24,		SDL_PIXELFORMAT_RGB24 },
		{ AV_PIX_FMT_BGR24,		SDL_PIXELFORMAT_BGR24 },
		{ AV_PIX_FMT_0RGB32,	SDL_PIXELFORMAT_XRGB8888 },
		{ AV_PIX_FMT_0BGR32,	SDL_PIXELFORMAT_XBGR8888 },
		{ AV_PIX_FMT_NE(RGB0, 0BGR),	SDL_PIXELFORMAT_RGBX8888 },
		{ AV_PIX_FMT_NE(BGR0, 0RGB),	SDL_PIXELFORMAT_BGRX8888 },
		{ AV_PIX_FMT_RGB32,		SDL_PIXELFORMAT_ARGB8888 },
		{ AV_PIX_FMT_RGB32_1,	SDL_PIXELFORMAT_RGBA8888 },
		{ AV_PIX_FMT_BGR32,		SDL_PIXELFORMAT_ABGR8888 },
		{ AV_PIX_FMT_BGR32_1,	SDL_PIXELFORMAT_BGRA8888 },
		{ AV_PIX_FMT_YUV420P,	SDL_PIXELFORMAT_IYUV },
		{ AV_PIX_FMT_YUYV422,	SDL_PIXELFORMAT_YUY2 },
		{ AV_PIX_FMT_UYVY422,	SDL_PIXELFORMAT_UYVY },
	};

	for (uint i = 0;  i < FF_ARRAY_ELEMS(texture_format_map) - 1;  i++) {
		if (av_format == texture_format_map[i].av_format)
			return texture_format_map[i].sdl_format;
	}
	return SDL_PIXELFORMAT_UNKNOWN;
}

#ifdef __cplusplus

struct xxsdl {
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;

	const char *err_func;
	bool error(const char *func) {
		err_func = func;
		return 0;
	}
	const char* error() { return err_func; }

	xxsdl() {
		memset(this, 0, sizeof(*this));
	}
	~xxsdl() {
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_DestroyTexture(texture);
	}

	bool open(uint width, uint height, const char *title) {
		uint f = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
		if (!(window = SDL_CreateWindow(title, width, height, f)))
			return error("SDL_CreateWindow()");

		if (!(renderer = SDL_CreateRenderer(window, NULL)))
			return error("SDL_CreateRenderer()");

		texture_rect(0, 0, width, height);
		return 1;
	}

	void title(const char *s) { SDL_SetWindowTitle(window, s); }
	void window_size(uint w, uint h) { SDL_SetWindowSize(window, w, h); }
	void show() { SDL_ShowWindow(window); }
	void present() { SDL_RaiseWindow(window); }

	bool _fullscreen;
	bool fullscreen() const { return _fullscreen; }
	void fullscreen(bool enable) {
		SDL_SetWindowFullscreen(window, enable);
		_fullscreen = enable;
	}

	bool texture_convert(SDL_PixelFormat format, const AVFrame *frame) {
		const int *len = frame->linesize;
		bool r;
		switch (format) {
		case SDL_PIXELFORMAT_IYUV:
			if (len[0] > 0 && len[1] > 0 && len[2] > 0) {
				r = SDL_UpdateYUVTexture(texture, NULL,
					frame->data[0], len[0],
					frame->data[1], len[1],
					frame->data[2], len[2]);
			} else if (len[0] < 0 && len[1] < 0 && len[2] < 0) {
				r = SDL_UpdateYUVTexture(texture, NULL,
					frame->data[0] + len[0] * (frame->height - 1), -len[0],
					frame->data[1] + len[1] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -len[1],
					frame->data[2] + len[2] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -len[2]);
			} else {
				return error("SDL_PIXELFORMAT_IYUV: case not supported");
			}
			break;

		default:
			if (len[0] < 0) {
				r = SDL_UpdateTexture(texture, NULL, frame->data[0] + len[0] * (frame->height - 1), -len[0]);
			} else {
				r = SDL_UpdateTexture(texture, NULL, frame->data[0], len[0]);
			}
			break;
		}

		if (!r)
			return error("SDL_UpdateTexture()");
		return 1;
	}

	bool display(const AVFrame *frame) {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		SDL_BlendMode blendmode;
		SDL_PixelFormat format = format_sdl_av(frame->format, &blendmode);
		if (!texture
			|| frame->width != texture->w
			|| frame->height != texture->h
			|| format != texture->format) {
			if (!(texture = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STREAMING, frame->width, frame->height)))
				return error("SDL_CreateTexture()");
			if (!SDL_SetTextureBlendMode(texture, blendmode))
				return error("SDL_SetTextureBlendMode()");
		}

		if (!this->texture_convert(format, frame))
			return 0;

		SDL_RenderTextureRotated(renderer, texture, NULL, &_rect, 0, NULL, SDL_FLIP_NONE);
		SDL_RenderPresent(renderer);
		return 1;
	}

	SDL_FRect _rect;
	void texture_rect(uint x, uint y, uint w, uint h) {
		_rect.x = x;
		_rect.y = y;
		_rect.w = w;
		_rect.h = h;
	}
};

#endif
