#include "WindowManager.h"
#include "Scene.h" 
#include <algorithm>
#include "System/ImGuiRenderer.h" 
#include "System/Graphics.h"
#include "SceneBoss.h"
#include <mutex>
#include "Framework.h"
#include <SDL3/SDL.h> // Wajib untuk SDL_RaiseWindow
#include "Engine/Common/Constants.h"
#include "Engine/Graphics/GameCanvas.h"

void WindowManager::Update(float dt)
{
    if (m_dirtyPriority)
    {
        EnforceWindowPriorities();
        m_dirtyPriority = false;
    }
}

void WindowManager::EnforceWindowPriorities()
{
    // 1. Kumpulkan semua window yang valid
    std::vector<Beyond::Window*> sortedWindows;
    sortedWindows.reserve(windows.size());

    for (auto& win : windows)
    {
        if (win.get() != debugWindow && win->GetPriority() < 100)
        {
            sortedWindows.push_back(win.get());
        }
    }

    // 2. Urutkan berdasarkan prioritas (Ascending / Terendah dulu)
    std::sort(sortedWindows.begin(), sortedWindows.end(),
        [](Beyond::Window* a, Beyond::Window* b) {
            return a->GetPriority() < b->GetPriority();
        });

    // =========================================================
    // [PENGGANTI WIN32] Gunakan SDL3 untuk mengatur Z-Order
    // Kita menaikkan (Raise) window dari prioritas terendah ke tertinggi
    // sehingga yang tertinggi akan menumpuk di paling depan.
    // =========================================================
    for (Beyond::Window* win : sortedWindows)
    {
        if (win->GetSDLWindow())
        {
            SDL_RaiseWindow(win->GetSDLWindow());
        }
    }
}

void WindowManager::RenderAll(float dt, Scene* scene, const GameCanvas* canvas)
{
    bool vsyncApplied = false;
    auto context = Graphics::Instance().GetDeviceContext();

    for (auto& win : windows)
    {
        if (!win->GetSDLWindow()) continue;

        const bool isMainWindow{ win.get() == windows.front().get() };
        const bool usesCanvas{ isMainWindow && canvas != nullptr };

        // Canvas pass comes first: the scene draws at canvas size before the
        // window's own render target is bound.
        if (usesCanvas)
        {
            canvas->Begin(context);
            if (scene) scene->Render(dt, win->GetCamera());
        }

        // Ambil Alpha dan mulai render
        float bgAlpha = win->GetBackgroundAlpha();
        win->BeginRender(0.0f, 0.0f, 0.0f, bgAlpha);

        // Jika ini Main Window (Index 0)
        if (isMainWindow)
        {
            if (usesCanvas)
            {
                // Largest canvas-shaped rect centered in the window; the cleared
                // black area around it is the letterbox.
                const Beyond::PixelRect windowRect{ 0, 0, win->GetWidth(), win->GetHeight() };
                canvas->Blit(context, Beyond::FitRect(windowRect,
                    Beyond::Config::CANVAS_WIDTH, Beyond::Config::CANVAS_HEIGHT));

                // Why: Blit leaves the viewport on the fitted rect; anything drawn
                // after it (ImGui in Release) addresses the whole window.
                D3D11_VIEWPORT windowViewport{};
                windowViewport.Width = static_cast<float>(win->GetWidth());
                windowViewport.Height = static_cast<float>(win->GetHeight());
                windowViewport.MaxDepth = 1.0f;
                context->RSSetViewports(1, &windowViewport);
            }
            else if (scene)
            {
                scene->Render(dt, win->GetCamera());
            }

            if (m_imguiOnMainWindow) {
                ImGuiRenderer::Render(Graphics::Instance().GetDeviceContext());
            }
        }        // Jika ini Sub Window (Windowkill, dsb)
        else
        {
            if (scene && win->ShouldRenderScene()) {
                scene->Render(dt, win->GetCamera());
            }
        }

        // Pengaturan V-Sync (Biarkan window pertama atau transparan yang mengatur pacing)
        int syncInterval = (win->IsTransparent() || vsyncApplied) ? 0 : 1;
        if (!vsyncApplied && syncInterval == 1) vsyncApplied = true;

        win->EndRender(syncInterval);
    }
}

// Perhatikan parameternya sekarang menggunakan SDL_Window*
void WindowManager::HandleResize(SDL_Window* sdlWindow, int width, int height)
{
    for (auto& win : windows)
    {
        if (win->GetSDLWindow() == sdlWindow)
        {
            win->Resize(width, height);
            return;
        }
    }
}

Beyond::Window* WindowManager::CreateGameWindow(const char* title, int width, int height, bool isTransparent)
{
    auto newWindow = std::make_unique<Beyond::Window>();

    if (!newWindow->Initialize(title, width, height, isTransparent))
    {
        return nullptr;
    }

    newWindow->SetTickCallback([]() {
        Framework::Instance()->Update(0.016f);
        Framework::Instance()->Render(0.016f);
        });

    Beyond::Window* ptr = newWindow.get();
    windows.push_back(std::move(newWindow));

    MarkPriorityDirty();

    return ptr;
}

void WindowManager::DestroyWindow(Beyond::Window* targetWindow)
{
    windows.erase(
        std::remove_if(windows.begin(), windows.end(),
            [targetWindow](const std::unique_ptr<Beyond::Window>& w) {
                return w.get() == targetWindow;
            }),
        windows.end()
    );
}

void WindowManager::ClearAll()
{
    // Menghapus semua window dari memori dan memanggil destructor-nya
    windows.clear();
}