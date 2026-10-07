#pragma once

#if defined(_DEBUG)

#include "Engine/Common/FitRect.h"

struct SDL_Window;

// Remembers where the main window and the debug window were when the game last
// closed, so a Debug session starts with the same desktop layout.
// Debug builds only: Release always covers the primary display.
namespace WindowLayoutStore
{
    // A window's client area in desktop pixels. isSet is false when nothing usable was saved.
    struct SavedRect
    {
        Beyond::PixelRect rect{};
        bool isSet{ false };
    };

    struct Layout
    {
        SavedRect mainWindow{};
        SavedRect debugWindow{};
    };

    // Reads window_layout.json from the working directory. A missing or broken file,
    // or a rect that is too small or no longer on any monitor, leaves that entry unset.
    [[nodiscard]] Layout Load();

    // Writes the entries that are set. Failure is logged and otherwise ignored.
    void Save(const Layout& layout);

    // Current client-area position and size of `window`. Unset if the window is null,
    // minimized, or cannot be queried.
    [[nodiscard]] SavedRect ReadWindowRect(SDL_Window* window);
}

#endif // _DEBUG