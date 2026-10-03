#include "DebugHostWindow.h"

#if defined(_DEBUG)

#include <windows.h>
#include <SDL3/SDL.h>

namespace
{
    // Only visible until the UI fills the client area.
    constexpr float kClearGray{ 0.10f };
}

DebugHostWindow::DebugHostWindow(const DebugHostWindowConfig& config)
{
    // Creates a hidden, resizable SDL window plus its own flip-model swap chain.
    m_isValid = m_window.Initialize(config.title.c_str(), config.width, config.height, false);
    if (!m_isValid)
    {
        OutputDebugStringA("[DebugHostWindow] Initialize failed, debug UI disabled.\n");
        return;
    }

    // Why: Beyond::Window's hit test turns the whole client area into a drag
    // handle by default, which would swallow every click meant for the UI.
    m_window.SetDraggable(false);

    m_windowId = SDL_GetWindowID(m_window.GetSDLWindow());
    SDL_ShowWindow(m_window.GetSDLWindow());
}

void DebugHostWindow::BeginRender()
{
    if (!m_isValid) return;
    m_window.BeginRender(kClearGray, kClearGray, kClearGray, 1.0f);
}

void DebugHostWindow::Present()
{
    if (!m_isValid) return;
    m_window.EndRender(0); // sync interval 0: never wait for vblank
}

bool DebugHostWindow::HandleEvent(const SDL_Event& event)
{
    if (!m_isValid) return false;

    const bool isWindowEvent{ event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST };
    if (isWindowEvent)
    {
        if (event.window.windowID != m_windowId) return false;

        switch (event.type)
        {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            // Why: destruction is RAII-only; minimizing keeps the tool reachable from the taskbar.
            SDL_MinimizeWindow(m_window.GetSDLWindow());
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            // Why: the swap chain is sized in pixels, not window coordinates.
            m_window.Resize(event.window.data1, event.window.data2);
            break;
        default:
            break;
        }
        return true;
    }

    // Why: Main.cpp quits on Escape from any window; keys typed here must not reach it.
    const bool isKeyEvent{ event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP };
    if (isKeyEvent) return event.key.windowID == m_windowId;

    return false;
}

HWND DebugHostWindow::GetHwnd() const
{
    if (!m_isValid) return nullptr;

    return static_cast<HWND>(SDL_GetPointerProperty(
        SDL_GetWindowProperties(m_window.GetSDLWindow()),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER,
        nullptr));
}

bool DebugHostWindow::HasFocus() const
{
    return m_isValid && ::GetForegroundWindow() == GetHwnd();
}

#endif // _DEBUG