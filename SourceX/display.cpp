#include "display.h"
#include "DiabloUI/diabloui.h"
#include "controls/controller.h"

#ifdef USE_SDL1
#ifndef SDL1_VIDEO_MODE_BPP
#define SDL1_VIDEO_MODE_BPP 0
#endif
#ifndef SDL1_VIDEO_MODE_FLAGS
#define SDL1_VIDEO_MODE_FLAGS SDL_SWSURFACE
#endif
#ifndef SDL1_VIDEO_MODE_WIDTH
#define SDL1_VIDEO_MODE_WIDTH SCREEN_WIDTH
#endif
#ifndef SDL1_VIDEO_MODE_HEIGHT
#define SDL1_VIDEO_MODE_HEIGHT SCREEN_HEIGHT
#endif
#endif

namespace {

int screenWidth = -1;
int screenHeight = -1;

int viewportHeight = -1;

void ForceScreenResInit()
{
	bool videoInitialized = SDL_WasInit(SDL_INIT_VIDEO);

	if (!videoInitialized) {
		SDL_Init(SDL_INIT_VIDEO);
	}
	disp::InitDesiredScreenRes();
	if (!videoInitialized) {
		SDL_Quit();
	}
}

void ForceViewportHeightInit()
{
	if (screenWidth == -1) {
		ForceScreenResInit();
	}

	disp::InitViewportHeight();
}

} // anonymous namespace

namespace disp {

void InitDisplayElementSizes()
{
	if (screenWidth == -1) {
		InitDesiredScreenRes();
	}

	if (viewportHeight == -1) {
		InitViewportHeight();
	}
}

void InitDesiredScreenRes()
{
	SDL_DisplayMode mode;

	if (SDL_GetDesktopDisplayMode(0, &mode) != 0) {
		ErrSdl();
	}

	screenWidth = mode.w;
	dvl::DvlIntSetting("screen width", &screenWidth);
	if (screenWidth < 640) {
		screenWidth = 640;
	}

	screenHeight = mode.h;
	dvl::DvlIntSetting("screen height", &screenHeight);
	if (screenHeight < 480) {
		screenHeight = 480;
	}
}

void InitViewportHeight()
{
	if (screenWidth == -1) {
		ForceScreenResInit();
	}

	if (screenWidth <= PANEL_WIDTH) {
		viewportHeight = (screenHeight - PANEL_HEIGHT);
	} else {
		viewportHeight = screenHeight;
	}
}

const int GetScreenWidth()
{
	if (screenWidth == -1) {
		ForceScreenResInit();
	}
	return screenWidth;
}

const int GetScreenHeight()
{
	if (screenHeight == -1) {
		ForceScreenResInit();
	}
	return screenHeight;
}

const int GetViewportHeight()
{
	if (viewportHeight == -1) {
		ForceViewportHeightInit();
	}
	return viewportHeight;
}

} // namespace disp

namespace dvl {

extern BOOL was_window_init; /** defined in dx.cpp */

extern SDL_Surface *renderer_texture_surface; /** defined in dx.cpp */

#ifdef USE_SDL1
void SetVideoMode(int width, int height, int bpp, uint32_t flags) {
	SDL_Log("Setting video mode %dx%d bpp=%u flags=0x%08X", width, height, bpp, flags);
	SDL_SetVideoMode(width, height, bpp, flags);
	const SDL_VideoInfo &current = *SDL_GetVideoInfo();
	SDL_Log("Video mode is now %dx%d bpp=%u flags=0x%08X",
	    current.current_w, current.current_h, current.vfmt->BitsPerPixel, SDL_GetVideoSurface()->flags);
	ghMainWnd = SDL_GetVideoSurface();
}

void SetVideoModeToPrimary(bool fullscreen) {
	int flags = SDL1_VIDEO_MODE_FLAGS | SDL_HWPALETTE;
	if (fullscreen)
		flags |= SDL_FULLSCREEN;
	SetVideoMode(SDL1_VIDEO_MODE_WIDTH, SDL1_VIDEO_MODE_HEIGHT, SDL1_VIDEO_MODE_BPP, flags);
	if (OutputRequiresScaling())
		SDL_Log("Using software scaling");
}

bool IsFullScreen() {
	return (SDL_GetVideoSurface()->flags & SDL_FULLSCREEN) != 0;
}
#endif

bool SpawnWindow(const char *lpWindowName)
{
	if (SDL_Init(SDL_INIT_EVERYTHING & ~SDL_INIT_HAPTIC) <= -1) {
		ErrSdl();
	}

	disp::InitDisplayElementSizes();

#ifdef USE_SDL1
	SDL_EnableUNICODE(1);
#endif
#if defined(USE_SDL1) || defined(__SWITCH__)
	InitController();
#endif

	int upscale = 1;
	DvlIntSetting("upscale", &upscale);
	if (fullscreen)
		DvlIntSetting("fullscreen", (int *)&fullscreen);

	int grabInput = 1;
	DvlIntSetting("grab input", &grabInput);

#ifdef USE_SDL1
	SDL_WM_SetCaption(lpWindowName, WINDOW_ICON_NAME);
	const SDL_VideoInfo &best = *SDL_GetVideoInfo();
	SDL_Log("Best video mode reported as: %dx%d bpp=%d hw_available=%u",
	    best.current_w, best.current_h, best.vfmt->BitsPerPixel, best.hw_available);
	SetVideoModeToPrimary(fullscreen);
	if (grabInput)
		SDL_WM_GrabInput(SDL_GRAB_ON);
	atexit(SDL_VideoQuit); // Without this video mode is not restored after fullscreen.
#else
	int flags = 0;
	if (upscale) {
		if (fullscreen) {
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		}
		flags |= SDL_WINDOW_RESIZABLE;

		char scaleQuality[2] = "2";
		DvlStringSetting("scaling quality", scaleQuality, 2);
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, scaleQuality);
	} else if (fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN;
	}

	if (grabInput) {
		flags |= SDL_WINDOW_INPUT_GRABBED;
	}

	ghMainWnd = SDL_CreateWindow(lpWindowName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, flags);
#endif
	if (ghMainWnd == NULL) {
		ErrSdl();
	}

	int refreshRate = 60;
#ifndef USE_SDL1
	SDL_DisplayMode mode;
	SDL_GetDisplayMode(0, 0, &mode);
	if (mode.refresh_rate != 0) {
		refreshRate = mode.refresh_rate;
	}
#endif
	refreshDelay = 1000000 / refreshRate;

	if (upscale) {
#ifdef USE_SDL1
		SDL_Log("upscaling not supported with USE_SDL1");
#else
		renderer = SDL_CreateRenderer(ghMainWnd, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
		if (renderer == NULL) {
			ErrSdl();
		}

		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
		if (texture == NULL) {
			ErrSdl();
		}

		if (SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT) <= -1) {
			ErrSdl();
		}
#endif
	}

	return ghMainWnd != NULL;
}

SDL_Surface *GetOutputSurface()
{
#ifdef USE_SDL1
	return SDL_GetVideoSurface();
#else
	if (renderer)
		return renderer_texture_surface;
	return SDL_GetWindowSurface(ghMainWnd);
#endif
}

bool OutputRequiresScaling()
{
#ifdef USE_SDL1
	return SCREEN_WIDTH != GetOutputSurface()->w || SCREEN_HEIGHT != GetOutputSurface()->h;
#else // SDL2, scaling handled by renderer.
	return false;
#endif
}

void ScaleOutputRect(SDL_Rect *rect)
{
	if (!OutputRequiresScaling())
		return;
	const SDL_Surface *surface = GetOutputSurface();
	rect->x = rect->x * surface->w / SCREEN_WIDTH;
	rect->y = rect->y * surface->h / SCREEN_HEIGHT;
	rect->w = rect->w * surface->w / SCREEN_WIDTH;
	rect->h = rect->h * surface->h / SCREEN_HEIGHT;
}

#ifdef USE_SDL1
namespace {

SDL_Surface *CreateScaledSurface(SDL_Surface *src)
{
	SDL_Rect stretched_rect = { 0, 0, static_cast<Uint16>(src->w), static_cast<Uint16>(src->h) };
	ScaleOutputRect(&stretched_rect);
	SDL_Surface *stretched = SDL_CreateRGBSurface(
			SDL_SWSURFACE, stretched_rect.w, stretched_rect.h, src->format->BitsPerPixel,
	    src->format->Rmask, src->format->Gmask, src->format->Bmask, src->format->Amask);
	if (SDL_HasColorKey(src)) {
		SDL_SetColorKey(stretched, SDL_SRCCOLORKEY, src->format->colorkey);
		if (src->format->palette != NULL)
			SDL_SetPalette(stretched, SDL_LOGPAL, src->format->palette->colors, 0, src->format->palette->ncolors);
	}
	if (SDL_SoftStretch((src), NULL, stretched, &stretched_rect) < 0) {
		SDL_FreeSurface(stretched);
		ErrSdl();
	}
	return stretched;
}

} // namespace
#endif // USE_SDL1

void ScaleSurfaceToOutput(SDL_Surface **surface)
{
#ifdef USE_SDL1
	if (!OutputRequiresScaling())
		return;
	SDL_Surface *stretched = CreateScaledSurface(*surface);
	SDL_FreeSurface((*surface));
	*surface = stretched;
#endif
}

} // namespace dvl
