#pragma once

// Kits in their own windows (experimental, branch kit-windows): Dear ImGui's
// multi-viewports on our SDL_Renderer. The stock imgui_impl_sdlrenderer3
// draws into one window only; this gives each extra ImGui viewport (a kit
// dragged out of the game window) its own SDL_Renderer, and mirrors into it
// the main renderer's textures that viewport draws with (font atlas, kit
// pictures, sprite frames, the Creature's View).

struct ImDrawData;
struct SDL_Renderer;
struct SDL_Texture;

namespace SDLViewports {

// once, after ImGui_ImplSDLRenderer3_Init: installs the Renderer_* hooks
void init(SDL_Renderer* main);
void shutdown();

// the main viewport's draw data, drawn where it lies on the desktop (with
// multi-viewports on, ImGui's coordinates are the desktop's, not the
// window's); `main` draws with its own textures
void renderMainDrawData(ImDrawData* draw_data, SDL_Renderer* main);

// a main-renderer texture's pixels changed (SDLBackend::updateTexture):
// its mirrors are made again the next time they are drawn
void textureUpdated(SDL_Texture* tex);

} // namespace SDLViewports
