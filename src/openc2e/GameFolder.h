#pragma once

// Finding the player's own Creatures 2 folder without a command line
// (owner, 2026-11-20: players should only have to start the program).
// Zeetures carries no game data; it reads the player's installed copy.
//
// The order: -d on the command line (Engine::parseCommandLine), the folder
// remembered from an earlier run, ./data, the usual Steam and GOG places,
// and last a window that asks the player to choose the folder.

#include <filesystem>

struct SDL_Window;

namespace GameFolder {

// The Creatures 2 folder for p: p itself, or its "Creatures 2" folder when
// p is the "Creatures The Albian Years" folder that holds it. Empty if
// neither is a Creatures 2 folder.
std::filesystem::path normalize(const std::filesystem::path& p);

// The remembered folder, if it is still there; else the usual places
// (remembering what is found). Empty if none.
std::filesystem::path find();

// The first-run window: says what is needed and lets the player choose the
// folder. Remembers the choice. Empty if the player chose Quit.
std::filesystem::path ask();

// File > Creatures 2 Folder: choose another folder, used from the next
// start. pollChooseAnother() shows the result; call it once a frame.
void chooseAnother(SDL_Window* window);
void pollChooseAnother(SDL_Window* window);

// The folder the game is running from (set by Engine::parseCommandLine),
// and whether it came from -d
void setCurrent(const std::filesystem::path& folder, bool fromCommandLine);
const std::filesystem::path& currentFolder();

} // namespace GameFolder
