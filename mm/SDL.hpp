/** SDL wrapper
2025, Simon Zolin */

#pragma once
#include <SDL3/SDL.h>
#undef main // Prevent SDL from overriding main()

static inline int sdl_init(unsigned init)
{
	if (!init) {
		SDL_Quit();
		return 0;
	}

	return !SDL_Init(SDL_INIT_VIDEO);
}

static inline int sdl_read_events(SDL_Event *events, unsigned cap, SDL_Window *windows[])
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

struct sdl_frame {
	unsigned height, width;
	const unsigned char *data[3];
	unsigned len[3];
};

struct sdlctx {
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	SDL_FRect rect;
	const char *err_func;
};

#define ERR(c, s)  (c)->err_func = s, -1

static inline void sdl_destroy(struct sdlctx *c)
{
	SDL_DestroyRenderer(c->renderer);
	SDL_DestroyWindow(c->window);
	SDL_DestroyTexture(c->texture);
}

struct sdl_conf {
	unsigned width, height;
	const char *title;
};

static inline int sdl_open(struct sdlctx *c, struct sdl_conf *conf)
{
	unsigned f = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
	if (!(c->window = SDL_CreateWindow(conf->title, conf->width, conf->height, f)))
		return ERR(c, "SDL_CreateWindow()");

	if (!(c->renderer = SDL_CreateRenderer(c->window, NULL)))
		return ERR(c, "SDL_CreateRenderer()");

	SDL_FRect r = {
		.x = 0,
		.y = 0,
		.w = conf->width,
		.h = conf->height,
	};
	c->rect = r;
	return 0;
}

static int _sdl_texture_convert(struct sdlctx *c, SDL_PixelFormat format, const struct sdl_frame *frame)
{
	bool r;
	switch (format) {
	case SDL_PIXELFORMAT_IYUV:
		r = SDL_UpdateYUVTexture(c->texture, NULL
			, frame->data[0], frame->len[0]
			, frame->data[1], frame->len[1]
			, frame->data[2], frame->len[2]);
		break;

	default:
		r = SDL_UpdateTexture(c->texture, NULL, frame->data[0], frame->len[0]);
		break;
	}

	if (!r)
		return ERR(c, "SDL_UpdateTexture()");
	return 0;
}

static inline int sdl_display(struct sdlctx *c, const struct sdl_frame *frame, SDL_PixelFormat format, SDL_BlendMode blendmode)
{
	SDL_SetRenderDrawColor(c->renderer, 0, 0, 0, 255);
	SDL_RenderClear(c->renderer);

	if (!c->texture
		|| frame->width != c->texture->w
		|| frame->height != c->texture->h
		|| format != c->texture->format) {
		if (!(c->texture = SDL_CreateTexture(c->renderer, format, SDL_TEXTUREACCESS_STREAMING, frame->width, frame->height)))
			return ERR(c, "SDL_CreateTexture()");
		if (!SDL_SetTextureBlendMode(c->texture, blendmode))
			return ERR(c, "SDL_SetTextureBlendMode()");
	}

	if (_sdl_texture_convert(c, format, frame))
		return -1;

	SDL_RenderTextureRotated(c->renderer, c->texture, NULL, &c->rect, 0, NULL, SDL_FLIP_NONE);
	SDL_RenderPresent(c->renderer);
	return 0;
}

#ifdef __cplusplus

struct xxsdl : sdlctx {
	xxsdl() {
		memset(this, 0, sizeof(*this));
	}
	~xxsdl() { sdl_destroy(this); }

	bool error(const char *func) {
		err_func = func;
		return 0;
	}
	const char* error() { return err_func; }

	bool open(unsigned width, unsigned height, const char *title) {
		struct sdl_conf conf = {
			.width = width,
			.height = height,
			.title = title,
		};
		return !sdl_open(this, &conf);
	}

	void title(const char *s) { SDL_SetWindowTitle(window, s); }
	void window_size(unsigned w, unsigned h) { SDL_SetWindowSize(window, w, h); }
	void show() { SDL_ShowWindow(window); }
	void present() { SDL_RaiseWindow(window); }

	bool _fullscreen;
	bool fullscreen() const { return _fullscreen; }
	void fullscreen(bool enable) {
		SDL_SetWindowFullscreen(window, enable);
		_fullscreen = enable;
	}

	bool display(const struct sdl_frame *frame, SDL_PixelFormat format, SDL_BlendMode blendmode) { return !sdl_display(this, frame, format, blendmode); }

	void texture_rect(unsigned x, unsigned y, unsigned w, unsigned h) {
		rect.x = x;
		rect.y = y;
		rect.w = w;
		rect.h = h;
	}
};

#endif

#undef ERR
