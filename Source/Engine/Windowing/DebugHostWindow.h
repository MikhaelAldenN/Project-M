#pragma once

#if defined(_DEBUG)

#include <string>
#include "BeyondWindow.h"

// Startup settings for the debug host window.
struct DebugHostWindowConfig
{
    std::string title{ "Project M - Debug" };
    int width{ 560 };
    int height{ 900 };
};

// Debug-only OS window that will host the whole debug UI.
// Owned by Framework. It is deliberately NOT created through WindowManager,
// so the game's window systems (RenderAll, priority sort, WindowTrackingSystem,
// Act 3 hazards) never see it.
class DebugHostWindow
{
public:
    explicit DebugHostWindow(const DebugHostWindowConfig& config = {});
    ~DebugHostWindow() = default; // Beyond::Window destroys the SDL window

    DebugHostWindow(const DebugHostWindow&) = delete;
    DebugHostWindow& operator=(const DebugHostWindow&) = delete;

    // False if window or swap chain creation failed; all calls then do nothing.
    [[nodiscard]] bool IsValid() const { return m_isValid; }

    // Binds and clears this window's back buffer.
    void BeginRender();

    // Presents without vsync so the game's frame pacing is untouched.
    void Present();

    // Returns true if the event belongs to this window. The caller must then
    // skip its own handling, so game logic never reacts to debug window events.
    [[nodiscard]] bool HandleEvent(const SDL_Event& event);

    // Native handle for the ImGui platform backend. Null if the window is invalid.
    [[nodiscard]] HWND GetHwnd() const;

    // True while this window is the OS foreground window.
    [[nodiscard]] bool HasFocus() const;

private:
    Beyond::Window m_window{};
    SDL_WindowID m_windowId{ 0 }; // 0 is SDL's "no window" id
    bool m_isValid{ false };
};

#endif // _DEBUG