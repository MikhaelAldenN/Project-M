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

namespace
{
    // Default windowed layout: the game window on the left, the debug window beside it.
    // Client-area rects in desktop pixels, laid out for a 1920x1080 display at 100%
    // scale; y = 31 leaves room for the title bar.
    constexpr Beyond::PixelRect k_defaultMainRect{ 0, 31, 1510, 1000 };
#if defined(_DEBUG)
    constexpr Beyond::PixelRect k_defaultDebugRect{ 1515, 31, 400, 1000 };
#endif

    // The smallest size the user can drag the main window to.
    constexpr int k_minWindowWidth{ 640 };
    constexpr int k_minWindowHeight{ 360 };

    // Borderless mode is one row taller than the display. Carried over unchanged from
    // the per-scene code this replaces (screenH + 1); the original reason is not
    // documented, so it is kept until proven unnecessary.
    constexpr int k_borderlessExtraHeight{ 1 };

#if defined(_DEBUG)
    // Length of one stepped frame. Fixed, so a step always advances the same amount.
    constexpr float k_simStepSeconds{ 1.0f / 60.0f };

    // Upper bound of a scaled frame, the same limit Main.cpp applies to real time,
    // so a high time scale cannot tunnel bullets through the player.
    constexpr float k_maxSceneDeltaTime{ 0.05f };
#endif
}

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

    // Why before ShowWindow: the window appears in its final shape, without a
    // visible jump from the creation size.
#if defined(_DEBUG)
    m_windowLayout = WindowLayoutStore::Load();
    SetMainWindowMode(WindowMode::windowed);
#else
    SetMainWindowMode(WindowMode::borderless);
#endif
    SDL_ShowWindow(mainWin->GetSDLWindow());

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
    // The saved rect if there is one, otherwise the default when it fits this display.
    // With neither, the config's own size is used and the OS picks the position.
    WindowLayoutStore::SavedRect debugRect{ m_windowLayout.debugWindow };
    if (!debugRect.isSet && WindowLayoutStore::IsOnScreen(k_defaultDebugRect))
    {
        debugRect.rect = k_defaultDebugRect;
        debugRect.isSet = true;
    }

    DebugHostWindowConfig debugHostConfig{};
    if (debugRect.isSet)
    {
        debugHostConfig.x = debugRect.rect.x;
        debugHostConfig.y = debugRect.rect.y;
        debugHostConfig.width = debugRect.rect.width;
        debugHostConfig.height = debugRect.rect.height;
        debugHostConfig.hasPosition = true;
    }

    m_debugHost = std::make_unique<DebugHostWindow>(debugHostConfig);
    if (m_debugHost->IsValid())
    {
        imguiHwnd = m_debugHost->GetHwnd();
        WindowManager::Instance().SetImGuiOnMainWindow(false);
        // Why: showing a new window takes focus; give it back to the game.
        SDL_RaiseWindow(mainWin->GetSDLWindow());
        RegisterDebugMenuBar();
        RegisterDebugToolbar();
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
    scene = std::make_unique<SceneIntro>();
#else
    scene = std::make_unique<SceneBoss>();
#endif
}

Framework::~Framework()
{
#if defined(_DEBUG)
    // Why first: the scene's shutdown and ClearAll() below change or destroy the windows.
    SaveWindowLayout();
#endif
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

void Framework::SetMainWindowMode(WindowMode mode)
{
    Beyond::Window* mainWin{ GetMainWindow() };
    SDL_Window* sdlWin{ mainWin ? mainWin->GetSDLWindow() : nullptr };
    if (!sdlWin) return;

    switch (mode)
    {
    case WindowMode::borderless:
    {
        SDL_Rect display{};
        if (!SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &display))
        {
            OutputDebugStringA("[Framework] SDL_GetDisplayBounds failed, window mode unchanged: ");
            OutputDebugStringA(SDL_GetError());
            OutputDebugStringA("\n");
            return;
        }
#if defined(_DEBUG)
        // Why here: once borderless, the windowed rect can no longer be read back.
        if (m_mainWindowMode == WindowMode::windowed)
        {
            const WindowLayoutStore::SavedRect current{ WindowLayoutStore::ReadWindowRect(sdlWin) };
            if (current.isSet) m_windowLayout.mainWindow = current;
        }
#endif
        SDL_SetWindowResizable(sdlWin, false);
        SDL_SetWindowBordered(sdlWin, false);
        SDL_SetWindowPosition(sdlWin, display.x, display.y);
        SDL_SetWindowSize(sdlWin, display.w, display.h + k_borderlessExtraHeight);
        break;
    }
    case WindowMode::windowed:
    {
        Beyond::PixelRect rect{ k_defaultMainRect };
#if defined(_DEBUG)
        if (m_windowLayout.mainWindow.isSet) rect = m_windowLayout.mainWindow.rect;
#endif
        SDL_SetWindowBordered(sdlWin, true);
        SDL_SetWindowResizable(sdlWin, true);
        SDL_SetWindowMinimumSize(sdlWin, k_minWindowWidth, k_minWindowHeight);
        SDL_SetWindowSize(sdlWin, rect.width, rect.height);
        SDL_SetWindowPosition(sdlWin, rect.x, rect.y);
        break;
    }
    }

    m_mainWindowMode = mode;
}

void Framework::ToggleMainWindowMode()
{
    SetMainWindowMode(m_mainWindowMode == WindowMode::windowed
        ? WindowMode::borderless
        : WindowMode::windowed);
}

#if defined(_DEBUG)
void Framework::SaveWindowLayout()
{
    // In borderless mode the entry already holds the rect captured when leaving windowed mode.
    if (m_mainWindowMode == WindowMode::windowed)
    {
        const Beyond::Window* mainWin{ GetMainWindow() };
        const WindowLayoutStore::SavedRect current{
            WindowLayoutStore::ReadWindowRect(mainWin ? mainWin->GetSDLWindow() : nullptr) };
        if (current.isSet) m_windowLayout.mainWindow = current;
    }

    // A minimized debug window reads as unset, which keeps the rect of the last session.
    if (m_debugHost)
    {
        const WindowLayoutStore::SavedRect current{
            WindowLayoutStore::ReadWindowRect(m_debugHost->GetSDLWindow()) };
        if (current.isSet) m_windowLayout.debugWindow = current;
    }

    WindowLayoutStore::Save(m_windowLayout);
}

namespace
{
    // Why: a maximized window ignores size and position requests.
    void LeaveMaximized(SDL_Window* window)
    {
        if ((SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0)
        {
            SDL_RestoreWindow(window);
        }
    }
}

void Framework::ProcessWindowLayoutResetRequest()
{
    if (!m_isWindowLayoutResetRequested) return;
    m_isWindowLayoutResetRequested = false;

    // Forget the remembered rects, so windowed mode falls back to the defaults.
    m_windowLayout.mainWindow = {};
    m_windowLayout.debugWindow = {};

    // In borderless mode the main window is left alone; F11 then returns to the default rect.
    if (m_mainWindowMode == WindowMode::windowed)
    {
        const Beyond::Window* mainWin{ GetMainWindow() };
        SDL_Window* mainSdlWin{ mainWin ? mainWin->GetSDLWindow() : nullptr };
        if (mainSdlWin)
        {
            LeaveMaximized(mainSdlWin);
            SetMainWindowMode(WindowMode::windowed);
        }
    }

    // Left where it is when the default would land off-screen (smaller display).
    SDL_Window* debugSdlWin{ m_debugHost ? m_debugHost->GetSDLWindow() : nullptr };
    if (debugSdlWin && WindowLayoutStore::IsOnScreen(k_defaultDebugRect))
    {
        LeaveMaximized(debugSdlWin);
        SDL_SetWindowSize(debugSdlWin, k_defaultDebugRect.width, k_defaultDebugRect.height);
        SDL_SetWindowPosition(debugSdlWin, k_defaultDebugRect.x, k_defaultDebugRect.y);
    }
}
#endif

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

    // Why not elapsedTime: render-side animation (post-process time) must stop
    // and scale together with the scene.
    WindowManager::Instance().RenderAll(m_sceneDeltaTime, scene.get(), GetActiveCanvas());

#if defined(_DEBUG)

    // Drawn after every game window so it never affects their render state.
    if (m_debugHost)
    {
        // Last ImGui submission of the frame, after every scene has issued its own.
        // Why before BeginRender: panel callbacks may create or destroy game windows,
        // and that must not be able to replace the debug window's bound render target.
        DebugUI::Instance().Draw(GetDebugLayoutScope());

        m_debugHost->BeginRender();

        // The frame's ImGui draw data goes to the debug window's back buffer.
        ImGuiRenderer::Render(Graphics::Instance().GetDeviceContext());
        m_debugHost->Present();
    }
#endif
}

void Framework::Update(float elapsedTime)
{
#if defined(_DEBUG)
    ProcessDebugSceneRequest();
    ProcessWindowLayoutResetRequest();
#endif

    if (nextScene)
    {
        scene = std::move(nextScene); // the previous scene's destructor runs here
    }

    CalculateFrameStats(elapsedTime);

#if defined(_DEBUG)
    // Keyboard and mouse buttons stop driving the game while the debug window has focus.
    const bool isDebugFocused{ m_debugHost && m_debugHost->HasFocus() };
    Input::Instance().SetKeyboardMouseSuppressed(isDebugFocused);

    // Why: SDL activates a window whenever it is raised or shown, and the boss scene
    // does both constantly (the click blocker is raised every frame). That pulled
    // focus off the debug window, which re-enabled game input and broke ImGui drags.
    // While the debug window is focused, game windows keep their stacking order but
    // do not take focus.
    if (isDebugFocused != m_wasDebugFocused)
    {
        const char* const activation{ isDebugFocused ? "0" : "1" };
        SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED, activation);
        SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, activation);
        m_wasDebugFocused = isDebugFocused;
    }
#endif
    Input::Instance().Update();
    AudioManager::Instance().Update(elapsedTime);

    ImGuiRenderer::NewFrame(); // Now safe

    bool shouldUpdateScene{ true };
    m_sceneDeltaTime = elapsedTime;

#if defined(_DEBUG)
    if (m_isSimPaused)
    {
        shouldUpdateScene = m_isSimStepRequested;
        m_sceneDeltaTime = m_isSimStepRequested ? k_simStepSeconds : 0.0f;
    }
    else
    {
        const float scaled{ elapsedTime * m_simTimeScale };
        m_sceneDeltaTime = scaled < k_maxSceneDeltaTime ? scaled : k_maxSceneDeltaTime;
    }
    m_isSimStepRequested = false;
#endif

    if (scene && shouldUpdateScene) scene->Update(m_sceneDeltaTime);

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

bool Framework::HandleDebugHotkey([[maybe_unused]] const SDL_Event& event)
{
#if defined(_DEBUG)
    if (event.type != SDL_EVENT_KEY_DOWN) return false;

    switch (event.key.key)
    {
    case SDLK_F5:
        if (!event.key.repeat) m_isSimPaused = !m_isSimPaused;
        return true;
    case SDLK_F6:
        // Key repeat is allowed on purpose: holding F6 advances frame after frame.
        m_isSimPaused = true;
        m_isSimStepRequested = true;
        return true;
    case SDLK_F7:
        m_simTimeScale = 1.0f;
        return true;
    default:
        break;
    }
#endif
    return false;
}

#if defined(_DEBUG)
void Framework::RegisterDebugMenuBar()
{
    // Capturing `this` is safe: the handle is a member and is destroyed with this object.
    m_menuBarPanel = DebugUI::Instance().RegisterPanel(DebugPanelSlot::menuBar, "Framework", [this]()
        {
            if (ImGui::BeginMenu("Scene"))
            {
                const DebugScene current{ IdentifyDebugScene() };

                // Why only store the request: a scene constructor registers a panel,
                // which is not allowed while panel callbacks are running.
                const auto sceneItem{ [this, current](const char* label, DebugScene target)
                    {
                        if (ImGui::MenuItem(label, nullptr, current == target))
                        {
                            m_debugSceneRequest = target;
                        }
                    } };

                sceneItem("Intro", DebugScene::intro);
                sceneItem("Title", DebugScene::title);
                sceneItem("Game", DebugScene::game);
                sceneItem("Sandbox", DebugScene::sandbox);
                sceneItem("Boss", DebugScene::boss);

                ImGui::Separator();
                if (ImGui::MenuItem("Reload", nullptr, false, current != DebugScene::none))
                {
                    m_debugSceneRequest = current;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                // Why only store the request: moving and resizing OS windows belongs
                // outside panel callbacks, like the scene change above.
                if (ImGui::MenuItem("Reset"))
                {
                    m_isWindowLayoutResetRequested = true;
                }
                ImGui::EndMenu();
            }


            // Why a fixed sample: the text keeps one position while its digits change.
            constexpr const char* widestText{ "000.00 ms  0000.0 FPS" };
            const float textWidth{ ImGui::CalcTextSize(widestText).x };
            const float nextMenuX{ ImGui::GetCursorPosX() };
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - textWidth - ImGui::GetStyle().WindowPadding.x);

            const float fps{ ImGui::GetIO().Framerate }; // ImGui's rolling average
            ImGui::Text("%6.2f ms  %6.1f FPS", fps > 0.0f ? 1000.0f / fps : 0.0f, fps);

            // Why: menus registered by other owners must continue after "Scene",
            // not to the right of the right-aligned text.
            ImGui::SetCursorPosX(nextMenuX);
        });
}

void Framework::RegisterDebugToolbar()
{
    // Capturing `this` is safe: the handle is a member and is destroyed with this object.
    m_toolbarPanel = DebugUI::Instance().RegisterPanel(DebugPanelSlot::toolbar, "Simulation", [this]()
        {
            constexpr float sliderWidth{ 120.0f };
            constexpr float minTimeScale{ 0.1f };
            constexpr float maxTimeScale{ 3.0f };

            // Why a fixed width: the label changes, the rest of the row must not shift.
            const float toggleWidth{
                ImGui::CalcTextSize("Resume").x + ImGui::GetStyle().FramePadding.x * 2.0f };

            // The accent marks the one toolbar state that is not the default.
            const bool wasPaused{ m_isSimPaused };
            if (wasPaused)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SliderGrabActive));
            }
            // Text after ### is the ID, so it stays the same button under both labels.
            if (ImGui::Button(wasPaused ? "Resume###SimPause" : "Pause###SimPause", ImVec2{ toggleWidth, 0.0f }))
            {
                m_isSimPaused = !m_isSimPaused;
            }
            if (wasPaused) ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("F5");

            // Held down, the button repeats: frame after frame, like holding F6.
            ImGui::SameLine();
            ImGui::PushButtonRepeat(true);
            if (ImGui::Button("Step"))
            {
                m_isSimPaused = true;
                m_isSimStepRequested = true;
            }
            ImGui::PopButtonRepeat();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("F6 (hold to keep stepping)");

            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Speed");

            ImGui::SameLine();
            ImGui::SetNextItemWidth(sliderWidth);
            ImGui::SliderFloat("##SimTimeScale", &m_simTimeScale, minTimeScale, maxTimeScale,
                "%.1fx", ImGuiSliderFlags_AlwaysClamp);

            ImGui::SameLine();
            if (ImGui::Button("1x")) m_simTimeScale = 1.0f;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("F7");
        });
}

Framework::DebugScene Framework::IdentifyDebugScene() const
{
    const Scene* const current{ scene.get() };
    if (dynamic_cast<const SceneIntro*>(current)) return DebugScene::intro;
    if (dynamic_cast<const SceneTitle*>(current)) return DebugScene::title;
    if (dynamic_cast<const SceneGame*>(current)) return DebugScene::game;
    if (dynamic_cast<const SceneSandbox*>(current)) return DebugScene::sandbox;
    if (dynamic_cast<const SceneBoss*>(current)) return DebugScene::boss;
    return DebugScene::none;
}

const char* Framework::GetDebugLayoutScope() const
{
    // Why fixed strings: they are part of the keys saved in imgui.ini.
    switch (IdentifyDebugScene())
    {
    case DebugScene::intro:   return "intro";
    case DebugScene::title:   return "title";
    case DebugScene::game:    return "game";
    case DebugScene::sandbox: return "sandbox";
    case DebugScene::boss:    return "boss";
    case DebugScene::none:    break;
    }
    return "none";
}

void Framework::ProcessDebugSceneRequest()
{
    if (m_debugSceneRequest == DebugScene::none) return;

    const DebugScene request{ m_debugSceneRequest };
    m_debugSceneRequest = DebugScene::none;

    // Why destroy first: scenes own OS windows and drive singletons (WindowManager,
    // audio, effects); the old one must release them before the new one claims them.
    // A scene queued through ChangeScene is dropped, the debug request wins.
    nextScene.reset();
    scene.reset();

    // Why: Windowkill hides the main window and only SceneTitle shows it again.
    // Every scene picked from the menu must start with the window visible.
    Beyond::Window* const mainWin{ GetMainWindow() };
    if (mainWin && mainWin->GetSDLWindow())
    {
        SDL_ShowWindow(mainWin->GetSDLWindow());
    }

    switch (request)
    {
    case DebugScene::intro:   scene = std::make_unique<SceneIntro>(); break;
    case DebugScene::title:   scene = std::make_unique<SceneTitle>(); break;
    case DebugScene::game:    scene = std::make_unique<SceneGame>(); break;
    case DebugScene::sandbox: scene = std::make_unique<SceneSandbox>(); break;
    case DebugScene::boss:    scene = std::make_unique<SceneBoss>(); break;
    case DebugScene::none:    break;
    }
}
#endif