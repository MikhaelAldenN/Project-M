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

#endif // _DEBUG