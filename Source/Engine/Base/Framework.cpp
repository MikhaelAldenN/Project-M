#include "Framework.h"
#include "DebugUI.h"
#include "Engine/Common/FitRect.h"
#include "Engine/Graphics/GameCanvas.h"
#include "Engine/Common/Constants.h"

// ========================================================
// Jembatan Win32 ke ImGui
// ========================================================
static WNDPROC s_OriginalWndProc = nullptr;
static HWND s_HookedWnd = nullptr; // window whose WndProc is currently replaced

LRESULT CALLBACK ImGuiHookWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // 1. Berikan event klik/keyboard ke ImGui terlebih dahulu
    if (ImGuiRenderer::HandleMessage(hWnd, msg, wParam, lParam)) {
        return true;
    }

    // 2. Teruskan sisa pesannya ke sistem SDL3
    if (s_OriginalWndProc) {
        return CallWindowProc(s_OriginalWndProc, hWnd, msg, wParam, lParam);
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
// ========================================================

Framework* Framework::pInstance = nullptr;

Framework::Framework()
{
    pInstance = this;
    Graphics::Instance().Initialize();

    if (!AudioManager::Instance().Initialize()) {  }

    AttackParamManager::Instance().Load("AttackParams.json");

    // Buat Main Window (Fullscreen Borderless)
    auto mainWin = WindowManager::Instance().CreateGameWindow("Main Window (close here)", 1600, 900);
    mainWin->SetPriority(0);
    mainWin->SetDraggable(false);

    SDL_ShowWindow(mainWin->GetSDLWindow());
    // Tambahkan flag Resizable agar bisa di-drag ujungnya
    SDL_SetWindowResizable(mainWin->GetSDLWindow(), true);
    SDL_SetWindowBordered(mainWin->GetSDLWindow(), true);
    //SDL_SetWindowPosition(mainWin->GetSDLWindow(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_SetWindowPosition(mainWin->GetSDLWindow(), 5, 35);

    // Posisikan di tengah saat awal

    HWND hwnd = (HWND)SDL_GetPointerProperty(
        SDL_GetWindowProperties(mainWin->GetSDLWindow()),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER,
        NULL
    );

    Input::Instance().Initialize(hwnd);

    // ImGui binds to exactly one OS window: the debug host in Debug builds,
    // the game window otherwise.
    HWND imguiHwnd{ hwnd };

#if defined(_DEBUG)
    m_debugHost = std::make_unique<DebugHostWindow>();
    if (m_debugHost->IsValid())
    {
        imguiHwnd = m_debugHost->GetHwnd();
        WindowManager::Instance().SetImGuiOnMainWindow(false);
        // Why: showing a new window takes focus; give it back to the game.
        SDL_RaiseWindow(mainWin->GetSDLWindow());
        RegisterSampleDebugPanels();
    }
    else
    {
        m_debugHost.reset(); // fall back to drawing ImGui on the game window
    }
#endif

    ImGuiRenderer::Initialize(imguiHwnd, Graphics::Instance().GetDevice(), Graphics::Instance().GetDeviceContext());
    s_HookedWnd = imguiHwnd;
    s_OriginalWndProc = (WNDPROC)SetWindowLongPtr(imguiHwnd, GWLP_WNDPROC, (LONG_PTR)ImGuiHookWndProc);
    // Load Resources
    ResourceManager::Instance().LoadFont("VGA_FONT", "Data/Font/IBM_VGA_32px_0.png", "Data/Font/IBM_VGA_32px.fnt");

    // Why before the scene: needs the device, and must exist before the first Render.
    m_gameCanvas = std::make_unique<GameCanvas>();
    if (m_gameCanvas->Initialize(Graphics::Instance().GetDevice()))
    {
        OutputDebugStringA("[GameCanvas] Ready.\n");
    }
    else
    {
        OutputDebugStringA("[GameCanvas] Initialize failed, scenes render directly to the main window.\n");
        m_gameCanvas.reset();
    }

    // Init Scene
#if 0
    scene = std::make_unique<SceneSandbox>();
#else
    scene = std::make_unique<SceneBoss>();
#endif
}

Framework::~Framework()
{
    scene.reset();

    // Why: restore SDL's WndProc while the hooked window still exists, so its
    // destroy messages never reach ImGui after the context is gone.
    if (s_HookedWnd && s_OriginalWndProc)
    {
        SetWindowLongPtr(s_HookedWnd, GWLP_WNDPROC, (LONG_PTR)s_OriginalWndProc);
        s_HookedWnd = nullptr;
        s_OriginalWndProc = nullptr;
    }

    WindowManager::Instance().ClearAll();
    ImGuiRenderer::Finalize();
    pInstance = nullptr;
}

Framework* Framework::Instance() { return pInstance; }
void Framework::ChangeScene(std::unique_ptr<Scene> newScene) { nextScene = std::move(newScene); }

Beyond::Window* Framework::GetMainWindow() const
{
    return WindowManager::Instance().GetWindowByIndex(0);
}

const GameCanvas* Framework::GetActiveCanvas() const
{
    return m_gameCanvas.get();
}

Beyond::PixelRect Framework::GetGameImageRect() const
{
    const Beyond::Window* mainWin{ GetMainWindow() };
    SDL_Window* sdlWin{ mainWin ? mainWin->GetSDLWindow() : nullptr };

    const SDL_WindowFlags notShowing{ SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED };
    if (sdlWin && (SDL_GetWindowFlags(sdlWin) & notShowing) == 0)
    {
        int windowX{ 0 };
        int windowY{ 0 };
        SDL_GetWindowPosition(sdlWin, &windowX, &windowY);

        // Same rect WindowManager::RenderAll blits the canvas into, moved to desktop space.
        return Beyond::FitRect(
            Beyond::PixelRect{ windowX, windowY, mainWin->GetWidth(), mainWin->GetHeight() },
            Beyond::Config::CANVAS_WIDTH, Beyond::Config::CANVAS_HEIGHT);
    }

    SDL_Rect display{};
    if (!SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &display))
    {
        return Beyond::PixelRect{};
    }
    return Beyond::FitRect(
        Beyond::PixelRect{ display.x, display.y, display.w, display.h },
        Beyond::Config::CANVAS_WIDTH, Beyond::Config::CANVAS_HEIGHT);
}

void Framework::Render(float elapsedTime)
{
    // SceneBoss is the one scene that skips the canvas: its main window covers the
    // monitor and it treats the desktop as the world.
    const bool isSceneBoss{ dynamic_cast<SceneBoss*>(scene.get()) != nullptr };
    const GameCanvas* canvas{ isSceneBoss ? nullptr : m_gameCanvas.get() };

    WindowManager::Instance().RenderAll(elapsedTime, scene.get(), GetActiveCanvas());

#if defined(_DEBUG)

    // Drawn after every game window so it never affects their render state.
    if (m_debugHost)
    {
        // Last ImGui submission of the frame, after every scene has issued its own.
        // Why before BeginRender: panel callbacks may create or destroy game windows,
        // and that must not be able to replace the debug window's bound render target.
        DebugUI::Instance().Draw();

        m_debugHost->BeginRender();

        // The frame's ImGui draw data goes to the debug window's back buffer.
        ImGuiRenderer::Render(Graphics::Instance().GetDeviceContext());
        m_debugHost->Present();
    }
#endif
}

void Framework::Update(float elapsedTime)
{
    if (nextScene)
    {
        scene = std::move(nextScene); // the previous scene's destructor runs here
    }

    CalculateFrameStats(elapsedTime);

#if defined(_DEBUG)
    // Keyboard and mouse buttons stop driving the game while the debug window has focus.
    Input::Instance().SetKeyboardMouseSuppressed(m_debugHost && m_debugHost->HasFocus());
#endif

    Input::Instance().Update();
    AudioManager::Instance().Update(elapsedTime);

    ImGuiRenderer::NewFrame(); // Now safe

    if (scene) scene->Update(elapsedTime);

    if (auto* boss = dynamic_cast<SceneBoss*>(scene.get())) {
        if (boss->IsPendingSceneChange()) {
            ChangeScene(std::make_unique<SceneTitle>());
            return;
        }
    }
}

void Framework::ForceUpdateRender()
{
    static Uint64 lastTime = 0;
    if (lastTime == 0) lastTime = SDL_GetPerformanceCounter();
    Uint64 currentTime = SDL_GetPerformanceCounter();
    float dt = (float)(currentTime - lastTime) / (float)SDL_GetPerformanceFrequency();
    lastTime = currentTime;
    if (dt > 0.05f) dt = 0.05f;
    Update(dt);
    Render(dt);
}

void Framework::CalculateFrameStats(float dt)
{
    static int frames = 0;
    static float timeAccumulator = 0.0f;

    frames++;
    timeAccumulator += dt;

    if (timeAccumulator >= 1.0f)
    {
        //float fps = static_cast<float>(frames);
        //std::ostringstream outs;
        //outs.precision(6);

        //// Menggunakan judul resmi game barumu
        //outs << "FPS: " << fps << " (" << (1000.0f / fps) << " ms)";

        // Loop melalui semua window yang ada di WindowManager
        //size_t index = 0;
        //while (Beyond::Window* win = WindowManager::Instance().GetWindowByIndex(index))
        //{
        //    // Hanya update title pada window yang tidak disembunyikan (visible)
        //    if (win->IsVisible() && win->GetSDLWindow())
        //    {
        //        SDL_SetWindowTitle(win->GetSDLWindow(), outs.str().c_str());
        //    }
        //    index++;
        //}

        frames = 0;
        timeAccumulator -= 1.0f;
    }
}

void Framework::Quit()
{
    PostQuitMessage(0);
    SDL_Event quitEvent;
    SDL_zero(quitEvent);
    quitEvent.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quitEvent);
}

LRESULT CALLBACK Framework::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Beyond::Window* mainWin = GetMainWindow();
    HWND mainHwnd = NULL;

    // Ekstrak HWND dari main window yang menggunakan SDL3
    if (mainWin && mainWin->GetSDLWindow()) {
        mainHwnd = (HWND)SDL_GetPointerProperty(
            SDL_GetWindowProperties(mainWin->GetSDLWindow()),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER,
            NULL
        );
    }

    // Bandingkan hWnd dengan mainHwnd hasil ekstrak
    if (mainWin && hWnd == mainHwnd)
    {
        if (ImGuiRenderer::HandleMessage(hWnd, msg, wParam, lParam)) return true;
    }
    return 0;
}
void Framework::OnSubWindowClosed(Uint32 sdlWindowID)
{
    // Casting ke SceneBoss untuk mengakses fungsi spesifiknya
    SceneBoss* boss = dynamic_cast<SceneBoss*>(scene.get());
    if (boss) {
        boss->CloseSubWindowBySDLID(sdlWindowID);
    }
}

bool Framework::HandleDebugHostEvent([[maybe_unused]] const SDL_Event& event)
{
#if defined(_DEBUG)
    if (m_debugHost) return m_debugHost->HandleEvent(event);
#endif
    return false;
}

#if defined(_DEBUG)
void Framework::RegisterSampleDebugPanels()
{
    // Column sample: a stateless panel.
    m_sampleColumnPanel = DebugUI::Instance().RegisterPanel(DebugPanelSlot::column, "Frame", []()
        {
            const float fps{ ImGui::GetIO().Framerate }; // ImGui's rolling average
            ImGui::Text("%.2f ms", fps > 0.0f ? 1000.0f / fps : 0.0f);
            ImGui::Text("%.1f FPS", fps);
        });

    // Tab sample: a panel that reads its owner's state. Capturing `this` is safe
    // because the handle is a member and is destroyed together with this object.
    m_sampleTabPanel = DebugUI::Instance().RegisterPanel(DebugPanelSlot::tab, "Framework", [this]()
        {
            const Beyond::Window* mainWin{ GetMainWindow() };
            if (!mainWin)
            {
                ImGui::TextUnformatted("Main window: none");
                return;
            }

            ImGui::Text("Main window: %d x %d", mainWin->GetWidth(), mainWin->GetHeight());
            ImGui::Text("Scene: %s", scene ? "loaded" : "none");
        });
}
#endif
