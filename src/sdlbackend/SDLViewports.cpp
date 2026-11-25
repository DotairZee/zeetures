/*
 *  SDLViewports.cpp
 *  Zeetures (openc2e fork): kits in their own windows (experimental)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 */

#include "SDLViewports.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <imgui.h>
#include "imgui_impl_sdl3.h"
#include <string>
#include <unordered_map>
#include <vector>

// How it works. Dear ImGui (docking branch, multi-viewports on) asks the
// platform backend (imgui_impl_sdl3) for an SDL window per extra viewport,
// and the renderer backend (here) to draw into it. An SDL_Texture belongs to
// the renderer that made it, and every texture the kits draw with was made
// by the game window's renderer. So each extra window keeps copies: the
// first time a main texture is drawn there, its pixels are read back through
// the main renderer and uploaded to the window's own renderer.
//
// A copy is made again when its original changes:
// - a texture the game draws into (a render target: the Creature's View) is
//   read back every frame it is shown in an extra window;
// - a static texture is read back again after SDLBackend::updateTexture
//   (a version number in the texture's SDL properties);
// - a destroyed texture (the font atlas is remade as it gains letters) is
//   noticed through a cleanup callback on its SDL properties, and its copies
//   dropped before anything is drawn (the address may be reused at once).
//
// A static texture's pixels, once read back, are kept (one copy for all
// windows), so a new window uploads them without asking the GPU again. A
// window's renderer, and its copies, live as long as its SDL window, which
// imgui_impl_sdl3 (openc2e) hides and reuses rather than destroying: a kit
// dragged in and out again finds its pictures already there.

namespace SDLViewports {

namespace {

const char* const VERSION_PROP = "zeetures.mirror.version";
const char* const WATCH_PROP = "zeetures.mirror.watch";

struct Mirror {
	SDL_Texture* copy = nullptr;
	Sint64 version = -1;
	Uint64 frame = 0; // when a render target was last read back
};

struct WindowRenderer {
	SDL_Renderer* renderer = nullptr;
	std::unordered_map<SDL_Texture*, Mirror> mirrors;
	bool showPending = false; // shown once its first frame is drawn
};

const char* const RENDERER_PROP = "zeetures.window.renderer";

struct Pixels {
	SDL_Surface* surface = nullptr;
	Sint64 version = -1;
};

SDL_Renderer* s_main = nullptr;
void (*s_platformShowWindow)(ImGuiViewport*) = nullptr; // imgui_impl_sdl3's
std::vector<WindowRenderer*> s_windows;
std::unordered_map<SDL_Texture*, Pixels> s_pixels; // static textures' pixels, read back once
std::vector<SDL_Texture*> s_gone; // main textures destroyed since the last sweep
SDL_Texture* s_scratch = nullptr; // main-renderer target for reading back static textures
int s_scratch_w = 0, s_scratch_h = 0;
Uint64 s_frame = 0;
std::vector<float> s_xy;
std::vector<SDL_FColor> s_colors;

void SDLCALL onTextureGone(void*, void* value) {
	s_gone.push_back((SDL_Texture*)value);
}

void sweepGone() {
	if (s_gone.empty())
		return;
	for (SDL_Texture* t : s_gone) {
		auto px = s_pixels.find(t);
		if (px != s_pixels.end()) {
			SDL_DestroySurface(px->second.surface);
			s_pixels.erase(px);
		}
		for (WindowRenderer* w : s_windows) {
			auto it = w->mirrors.find(t);
			if (it != w->mirrors.end()) {
				if (it->second.copy)
					SDL_DestroyTexture(it->second.copy);
				w->mirrors.erase(it);
			}
		}
	}
	s_gone.clear();
}

bool watching() {
	static const bool w = getenv("OPENC2E_WATCH_KIT_WINDOWS") != nullptr;
	return w;
}

// a main texture's pixels, through the main renderer (nullptr on failure)
SDL_Surface* readBack(SDL_Texture* t, bool target, int w, int h) {
	struct Timer {
		Uint64 t0 = SDL_GetTicksNS();
		int w, h;
		bool target;
		~Timer() {
			double ms = (SDL_GetTicksNS() - t0) / 1e6;
			if (watching() && !target && ms > 2)
				SDL_Log("Kit windows: read back %dx%d in %.1f ms", w, h, ms);
		}
	} timer{SDL_GetTicksNS(), w, h, target};
	SDL_Texture* prev = SDL_GetRenderTarget(s_main);
	SDL_Surface* got = nullptr;
	if (target) {
		SDL_SetRenderTarget(s_main, t);
		got = SDL_RenderReadPixels(s_main, nullptr);
	} else {
		// draw it unblended onto a scratch target at its own size, and read that
		if (!s_scratch || s_scratch_w < w || s_scratch_h < h) {
			if (s_scratch)
				SDL_DestroyTexture(s_scratch);
			s_scratch_w = std::max(w, s_scratch_w);
			s_scratch_h = std::max(h, s_scratch_h);
			s_scratch = SDL_CreateTexture(s_main, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, s_scratch_w, s_scratch_h);
			if (!s_scratch) {
				SDL_SetRenderTarget(s_main, prev);
				return nullptr;
			}
		}
		SDL_SetRenderTarget(s_main, s_scratch);
		SDL_SetRenderViewport(s_main, nullptr);
		SDL_SetRenderClipRect(s_main, nullptr);
		SDL_SetRenderDrawColor(s_main, 0, 0, 0, 0);
		SDL_RenderClear(s_main);
		SDL_BlendMode blend;
		SDL_ScaleMode scale;
		Uint8 alpha, r, g, b;
		SDL_GetTextureBlendMode(t, &blend);
		SDL_GetTextureScaleMode(t, &scale);
		SDL_GetTextureAlphaMod(t, &alpha);
		SDL_GetTextureColorMod(t, &r, &g, &b);
		SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
		SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
		SDL_SetTextureAlphaMod(t, 255);
		SDL_SetTextureColorMod(t, 255, 255, 255);
		SDL_FRect dst{0, 0, (float)w, (float)h};
		SDL_RenderTexture(s_main, t, nullptr, &dst);
		SDL_SetTextureBlendMode(t, blend);
		SDL_SetTextureScaleMode(t, scale);
		SDL_SetTextureAlphaMod(t, alpha);
		SDL_SetTextureColorMod(t, r, g, b);
		SDL_Rect rect{0, 0, w, h};
		got = SDL_RenderReadPixels(s_main, &rect);
	}
	SDL_SetRenderTarget(s_main, prev);
	if (got && got->format != SDL_PIXELFORMAT_ARGB8888) {
		SDL_Surface* conv = SDL_ConvertSurface(got, SDL_PIXELFORMAT_ARGB8888);
		SDL_DestroySurface(got);
		got = conv;
	}
	return got;
}

// the window's copy of a main texture, made or refreshed as needed
SDL_Texture* mirrorOf(WindowRenderer& wr, SDL_Texture* t) {
	if (!t)
		return nullptr;
	SDL_PropertiesID props = SDL_GetTextureProperties(t);
	const bool target = SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_ACCESS_NUMBER, 0) == SDL_TEXTUREACCESS_TARGET;
	const Sint64 version = SDL_GetNumberProperty(props, VERSION_PROP, 0);
	if (!SDL_HasProperty(props, WATCH_PROP))
		SDL_SetPointerPropertyWithCleanup(props, WATCH_PROP, t, onTextureGone, nullptr);

	Mirror& m = wr.mirrors[t];
	bool fresh = m.copy && (target ? m.frame == s_frame : m.version == version);
	if (!fresh) {
		float fw = 0, fh = 0;
		SDL_GetTextureSize(t, &fw, &fh);
		int w = (int)fw, h = (int)fh;
		if (m.copy && (m.copy->w != w || m.copy->h != h)) {
			SDL_DestroyTexture(m.copy);
			m.copy = nullptr;
		}
		SDL_Surface* px = nullptr;
		if (target) {
			px = readBack(t, true, w, h);
		} else {
			Pixels& kept = s_pixels[t];
			if (!kept.surface || kept.version != version || kept.surface->w != w || kept.surface->h != h) {
				if (kept.surface)
					SDL_DestroySurface(kept.surface);
				kept.surface = readBack(t, false, w, h);
				kept.version = version;
			}
			px = kept.surface;
		}
		if (px) {
			if (!m.copy)
				m.copy = SDL_CreateTexture(wr.renderer, SDL_PIXELFORMAT_ARGB8888,
					target ? SDL_TEXTUREACCESS_STREAMING : SDL_TEXTUREACCESS_STATIC, w, h);
			if (m.copy)
				SDL_UpdateTexture(m.copy, nullptr, px->pixels, px->pitch);
			if (target)
				SDL_DestroySurface(px);
		}
		m.version = version;
		m.frame = s_frame;
	}
	if (m.copy) {
		// drawn as the original is drawn now
		SDL_BlendMode blend;
		SDL_ScaleMode scale;
		Uint8 alpha, r, g, b;
		if (SDL_GetTextureBlendMode(t, &blend))
			SDL_SetTextureBlendMode(m.copy, blend);
		if (SDL_GetTextureScaleMode(t, &scale))
			SDL_SetTextureScaleMode(m.copy, scale);
		if (SDL_GetTextureAlphaMod(t, &alpha))
			SDL_SetTextureAlphaMod(m.copy, alpha);
		if (SDL_GetTextureColorMod(t, &r, &g, &b))
			SDL_SetTextureColorMod(m.copy, r, g, b);
	}
	return m.copy;
}

// imgui_impl_sdlrenderer3's RenderDrawData, with two changes: vertices are
// placed relative to the viewport (DisplayPos: the window's place on the
// desktop), and textures can be swapped for a window's copies
void renderDrawData(ImDrawData* draw_data, SDL_Renderer* renderer, WindowRenderer* mirrors) {
	// (both callers set the renderer's scale to the window's own pixel
	// density times the interface scale, and SDL scales clip rectangles with
	// it, so clip rectangles are in ImGui's units here. Upstream's guess, the
	// draw data's FramebufferScale when the renderer's scale is 1, took the
	// game window's: on a Mac with the game on the Retina screen (2) and a kit
	// on a 1x monitor, every clip rectangle was doubled and the top of each
	// panel drawn black (owner's Mac test, 2026-11-24).)
	const ImVec2 render_scale(1.0f, 1.0f);
	int fb_width = (int)(draw_data->DisplaySize.x * render_scale.x);
	int fb_height = (int)(draw_data->DisplaySize.y * render_scale.y);
	if (fb_width <= 0 || fb_height <= 0)
		return;

	SDL_Rect old_viewport, old_clip;
	bool old_viewport_set = SDL_RenderViewportSet(renderer);
	bool old_clip_set = SDL_RenderClipEnabled(renderer);
	SDL_GetRenderViewport(renderer, &old_viewport);
	SDL_GetRenderClipRect(renderer, &old_clip);
	SDL_SetRenderViewport(renderer, nullptr);
	SDL_SetRenderClipRect(renderer, nullptr);

	const ImVec2 off = draw_data->DisplayPos;
	const ImVec2 clip_scale = render_scale;
	for (int n = 0; n < draw_data->CmdListsCount; n++) {
		const ImDrawList* list = draw_data->CmdLists[n];
		const int nv = list->VtxBuffer.Size;
		s_xy.resize((size_t)nv * 2);
		s_colors.resize((size_t)nv);
		for (int i = 0; i < nv; i++) {
			const ImDrawVert& v = list->VtxBuffer[i];
			s_xy[i * 2] = v.pos.x - off.x;
			s_xy[i * 2 + 1] = v.pos.y - off.y;
			s_colors[i].r = ((v.col >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
			s_colors[i].g = ((v.col >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
			s_colors[i].b = ((v.col >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
			s_colors[i].a = ((v.col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
		}
		for (int c = 0; c < list->CmdBuffer.Size; c++) {
			const ImDrawCmd* pcmd = &list->CmdBuffer[c];
			if (pcmd->UserCallback) {
				if (pcmd->UserCallback == ImDrawCallback_ResetRenderState) {
					SDL_SetRenderViewport(renderer, nullptr);
					SDL_SetRenderClipRect(renderer, nullptr);
				} else {
					pcmd->UserCallback(list, pcmd);
				}
				continue;
			}
			ImVec2 clip_min((pcmd->ClipRect.x - off.x) * clip_scale.x, (pcmd->ClipRect.y - off.y) * clip_scale.y);
			ImVec2 clip_max((pcmd->ClipRect.z - off.x) * clip_scale.x, (pcmd->ClipRect.w - off.y) * clip_scale.y);
			clip_min.x = std::max(clip_min.x, 0.0f);
			clip_min.y = std::max(clip_min.y, 0.0f);
			clip_max.x = std::min(clip_max.x, (float)fb_width);
			clip_max.y = std::min(clip_max.y, (float)fb_height);
			if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
				continue;
			SDL_Rect r = {(int)clip_min.x, (int)clip_min.y, (int)(clip_max.x - clip_min.x), (int)(clip_max.y - clip_min.y)};
			SDL_SetRenderClipRect(renderer, &r);

			SDL_Texture* tex = (SDL_Texture*)(intptr_t)pcmd->GetTexID();
			if (mirrors)
				tex = mirrorOf(*mirrors, tex);
			const float* xy = s_xy.data() + (size_t)pcmd->VtxOffset * 2;
			const float* uv = (const float*)(const void*)((const char*)(list->VtxBuffer.Data + pcmd->VtxOffset) + offsetof(ImDrawVert, uv));
			SDL_RenderGeometryRaw(renderer, tex,
				xy, (int)(sizeof(float) * 2),
				s_colors.data() + pcmd->VtxOffset, (int)sizeof(SDL_FColor),
				uv, (int)sizeof(ImDrawVert),
				nv - (int)pcmd->VtxOffset,
				list->IdxBuffer.Data + pcmd->IdxOffset, (int)pcmd->ElemCount, sizeof(ImDrawIdx));
		}
	}

	SDL_SetRenderViewport(renderer, old_viewport_set ? &old_viewport : nullptr);
	SDL_SetRenderClipRect(renderer, old_clip_set ? &old_clip : nullptr);
}

SDL_Window* windowOf(ImGuiViewport* vp) {
	return SDL_GetWindowFromID((SDL_WindowID)(intptr_t)vp->PlatformHandle);
}

// the SDL window went (for good): its renderer and copies went with it
void SDLCALL onWindowGone(void*, void* value) {
	auto* wr = (WindowRenderer*)value;
	s_windows.erase(std::remove(s_windows.begin(), s_windows.end(), wr), s_windows.end());
	delete wr;
}

void rendererCreateWindow(ImGuiViewport* vp) {
	SDL_Window* window = windowOf(vp);
	if (!window)
		return;
	// a window used before (kept by imgui_impl_sdl3) still has its renderer
	if (auto* kept = (WindowRenderer*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), RENDERER_PROP, nullptr)) {
		vp->RendererUserData = kept;
		return;
	}
	Uint64 t0 = SDL_GetTicksNS();
	auto* wr = new WindowRenderer();
	// the same kind of renderer as the game window's
	wr->renderer = SDL_CreateRenderer(window, SDL_GetRendererName(s_main));
	if (!wr->renderer)
		wr->renderer = SDL_CreateRenderer(window, nullptr);
	if (!wr->renderer) {
		SDL_Log("Zeetures: no renderer for a kit's window: %s", SDL_GetError());
		delete wr;
		return;
	}
	// (the game paces its frames itself; each window waiting for its own
	// vertical blank would halve the frame rate with two windows)
	SDL_SetRenderVSync(wr->renderer, 0);
	if (watching())
		SDL_Log("Kit windows: made a renderer in %.1f ms (window %s)", (SDL_GetTicksNS() - t0) / 1e6,
			(SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN) ? "still hidden" : "already shown");
	vp->RendererUserData = wr;
	s_windows.push_back(wr);
	SDL_SetPointerPropertyWithCleanup(SDL_GetWindowProperties(window), RENDERER_PROP, wr, onWindowGone, nullptr);
}

void rendererDestroyWindow(ImGuiViewport* vp) {
	// (the renderer stays with its SDL window, which may be kept for reuse;
	// it goes when the window does: onWindowGone)
	vp->RendererUserData = nullptr;
}

void rendererSetWindowSize(ImGuiViewport*, ImVec2) {
	// (SDL resizes the window's drawing area with the window)
}

// A window's pixels per window unit, taken from what its renderer really
// draws into (its output size over the window's size) rather than from
// SDL_GetWindowPixelDensity. They differ for a moment when a window moves
// between displays of different scale: macOS reports the new density at
// once, but the Metal drawing surface is resized only when SDL handles the
// change, and drawing at the new density into the old surface showed the
// kit at twice its size for a frame or two (owner's Mac test, 2026-11-24:
// Retina laptop and 1x monitors). Elsewhere the two agree.
static float drawDensity(SDL_Renderer* renderer, SDL_Window* window) {
	if (!window)
		return 1.0f;
	int ow = 0, oh = 0, ww = 0, wh = 0;
	float density = SDL_GetWindowPixelDensity(window);
	if (SDL_GetRenderOutputSize(renderer, &ow, &oh) && SDL_GetWindowSize(window, &ww, &wh) && ww > 0 && ow > 0) {
		const float drawn = (float)ow / (float)ww;
		if (watching() && std::fabs(drawn - density) > 0.01f)
			SDL_Log("Kit windows: window %u drawn at %.2f while its density is %.2f (output %dx%d, window %dx%d)",
				(unsigned)SDL_GetWindowID(window), drawn, density, ow, oh, ww, wh);
		density = drawn;
	}
	if (watching()) {
		// (each window's display, scales, drawing size and place, logged
		// when any of them changes: to see what macOS does to a kit window
		// that straddles two displays of different scale)
		static std::unordered_map<SDL_WindowID, std::string> said;
		int x = 0, y = 0;
		SDL_GetWindowPosition(window, &x, &y);
		char now[200];
		SDL_snprintf(now, sizeof now, "display %u scale %.2f density %.2f output %dx%d window %dx%d at %d,%d",
			(unsigned)SDL_GetDisplayForWindow(window), SDL_GetWindowDisplayScale(window), SDL_GetWindowPixelDensity(window),
			ow, oh, ww, wh, x, y);
		std::string& was = said[SDL_GetWindowID(window)];
		if (was.compare(0, was.find(" at "), now, std::string(now).find(" at ")) != 0)
			SDL_Log("Kit windows: window %u %s", (unsigned)SDL_GetWindowID(window), now);
		was = now;
	}
	return density > 0.0f ? density : 1.0f;
}

void rendererRenderWindow(ImGuiViewport* vp, void*) {
	auto* wr = (WindowRenderer*)vp->RendererUserData;
	if (!wr)
		return;
	sweepGone();
	SDL_Window* window = windowOf(vp);
	// (the window's pixel density, times the interface scale: see
	// ImGui_ImplSDL3_Openc2eUiScale)
	SDL_SetRenderTarget(wr->renderer, nullptr);
	float density = drawDensity(wr->renderer, window) * ImGui_ImplSDL3_Openc2eUiScale();
	SDL_SetRenderScale(wr->renderer, density, density);
	if (!(vp->Flags & ImGuiViewportFlags_NoRendererClear)) {
		SDL_SetRenderDrawColor(wr->renderer, 0, 0, 0, 255);
		SDL_RenderClear(wr->renderer);
	}
	renderDrawData(vp->DrawData, wr->renderer, wr);
}

void rendererSwapBuffers(ImGuiViewport* vp, void*) {
	auto* wr = (WindowRenderer*)vp->RendererUserData;
	if (!wr)
		return;
	SDL_RenderPresent(wr->renderer);
	if (wr->showPending) {
		// its first frame is drawn: show it now, and draw it again so the
		// first frame seen has the kit in it (a window shown before it was
		// drawn was black for as long as its first frame took: the first
		// kit dragged out each session flickered, owner 2026-11-23)
		wr->showPending = false;
		if (watching())
			SDL_Log("Kit windows: shown after its first frame");
		if (s_platformShowWindow)
			s_platformShowWindow(vp);
		rendererRenderWindow(vp, nullptr);
		SDL_RenderPresent(wr->renderer);
	}
}

// imgui_impl_sdl3 shows a window as soon as it is made, before it is drawn;
// with a renderer of ours it waits for its first frame (rendererSwapBuffers)
void deferredShowWindow(ImGuiViewport* vp) {
	if (auto* wr = (WindowRenderer*)vp->RendererUserData)
		wr->showPending = true;
	else if (s_platformShowWindow)
		s_platformShowWindow(vp);
}

} // namespace

void init(SDL_Renderer* main) {
	s_main = main;
	ImGuiIO& io = ImGui::GetIO();
	io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
	ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
	pio.Renderer_CreateWindow = rendererCreateWindow;
	pio.Renderer_DestroyWindow = rendererDestroyWindow;
	pio.Renderer_SetWindowSize = rendererSetWindowSize;
	pio.Renderer_RenderWindow = rendererRenderWindow;
	pio.Renderer_SwapBuffers = rendererSwapBuffers;
	if (pio.Platform_ShowWindow != deferredShowWindow) {
		s_platformShowWindow = pio.Platform_ShowWindow;
		pio.Platform_ShowWindow = deferredShowWindow;
	}
}

void shutdown() {
	ImGui::DestroyPlatformWindows();
	if (s_scratch)
		SDL_DestroyTexture(s_scratch);
	s_scratch = nullptr;
	for (auto& [t, px] : s_pixels)
		SDL_DestroySurface(px.surface);
	s_pixels.clear();
}

void renderMainDrawData(ImDrawData* draw_data, SDL_Renderer* main) {
	s_frame++;
	sweepGone();
	// the font's pixels read back now, at the start (or after it gains
	// letters), not as the first kit is dragged out
	if (auto* font = (SDL_Texture*)(intptr_t)ImGui::GetIO().Fonts->TexID) {
		SDL_PropertiesID props = SDL_GetTextureProperties(font);
		const Sint64 version = SDL_GetNumberProperty(props, VERSION_PROP, 0);
		Pixels& kept = s_pixels[font];
		if (!kept.surface || kept.version != version) {
			if (!SDL_HasProperty(props, WATCH_PROP))
				SDL_SetPointerPropertyWithCleanup(props, WATCH_PROP, font, onTextureGone, nullptr);
			if (kept.surface)
				SDL_DestroySurface(kept.surface);
			kept.surface = readBack(font, false, font->w, font->h);
			kept.version = version;
		}
	}
	// with multi-viewports on, ImGui's coordinates are the desktop's, in the
	// window system's units over the interface scale (on Windows and X11,
	// pixels over the display scale): drawn at the window's pixel density
	// times that scale, which comes to the display scale the game itself
	// draws at (set by SDLBackend::run)
	float sx = 1, sy = 1;
	SDL_GetRenderScale(main, &sx, &sy);
	SDL_Window* window = SDL_GetRenderWindow(main);
	float density = drawDensity(main, window) * ImGui_ImplSDL3_Openc2eUiScale();
	SDL_SetRenderScale(main, density, density);
	renderDrawData(draw_data, main, nullptr);
	SDL_SetRenderScale(main, sx, sy);
}

void textureUpdated(SDL_Texture* tex, SDL_Surface* pixels, const SDL_Rect* rect) {
	if (!tex || (s_windows.empty() && s_pixels.empty())) // (no copies anywhere: nothing to mark)
		return;
	SDL_PropertiesID props = SDL_GetTextureProperties(tex);
	const Sint64 version = SDL_GetNumberProperty(props, VERSION_PROP, 0) + 1;
	SDL_SetNumberProperty(props, VERSION_PROP, version);
	// a picture already kept is brought up to date from what was uploaded
	// (the Creature's View's picture changes many times a second: reading
	// it back each time stalled the GPU)
	auto kept = s_pixels.find(tex);
	if (kept == s_pixels.end() || !kept->second.surface || !pixels)
		return;
	SDL_Surface* dst = kept->second.surface;
	SDL_Rect whole{0, 0, dst->w, dst->h};
	const SDL_Rect& at = rect ? *rect : whole;
	if (at.w != pixels->w || at.h != pixels->h || at.x < 0 || at.y < 0 || at.x + at.w > dst->w || at.y + at.h > dst->h)
		return; // (left to be read back)
	// the pixels as uploaded, without the colour key the surface may carry
	SDL_Surface* raw = SDL_CreateSurfaceFrom(pixels->w, pixels->h, pixels->format, pixels->pixels, pixels->pitch);
	if (!raw)
		return;
	SDL_SetSurfaceBlendMode(raw, SDL_BLENDMODE_NONE);
	SDL_Rect to = at;
	if (SDL_BlitSurface(raw, nullptr, dst, &to))
		kept->second.version = version;
	SDL_DestroySurface(raw);
}

} // namespace SDLViewports
