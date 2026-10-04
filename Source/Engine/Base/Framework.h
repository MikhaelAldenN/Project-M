#pragma once
#include <windows.h>
#include <memory>
#include "System/HighResolutionTimer.h"
#include "BeyondWindow.h"
#include "Scene.h"
#include "System/Graphics.h"
#include "System/ImGuiRenderer.h"
#include "System/Input.h"
#include "System/AudioManager.h"
#include "WindowManager.h"
#include "SceneTitle.h"
#include "SceneIntro.h"
#include "SceneGame.h"
#include "SceneBoss.h"
#include "SceneSandbox.h"
#include <memory>
#include <sstream>
#include <iostream> 
#include <imgui.h>
#include <SDL3/SDL.h>
#include "AttackParamManager.h"
#include "DebugHostWindow.h"
#include "DebugUI.h"
#include "Engine/Common/FitRect.h"

class GameCanvas;

class Framework
{
public:
    Framework();
    ~Framework();
    static Framework* Instance();

    void Update(float elapsedTime);
    void Render(float elapsedTime);
    void ForceUpdateRender();
    void ChangeScene(std::unique_ptr<Scene> newScene);
    void Quit();

    // Helper untuk mengambil Main Window (Window index 0)
    Beyond::Window* GetMainWindow() const;

    // How the main window occupies the desktop. The game image is always the 16:9
    // canvas, letterboxed into whatever size the window has.
    enum class WindowMode
    {
        windowed,   // bordered, resizable, free size (Debug default)
        borderless, // no border, covers the primary display (Release default)
    };

    // The only place that changes the main window's border, size and position.
    // Scenes and phases must not call SDL for that themselves.
    void SetMainWindowMode(WindowMode mode);
    [[nodiscard]] WindowMode GetMainWindowMode() const { return m_mainWindowMode; }
    void ToggleMainWindowMode();

    // The canvas every scene is drawn into. Null only when the canvas could not be
    // created; scenes then draw straight to the main window.
    [[nodiscard]] const GameCanvas* GetActiveCanvas() const;

    // Where the game image sits on the desktop, in desktop pixels: the letterboxed
    // canvas area of the main window while that window is showing, otherwise the
    // largest canvas-shaped rect of the primary display (during Windowkill the main
    // window is hidden and the desktop itself is the play field).
    // Empty if neither can be queried.
    [[nodiscard]] Beyond::PixelRect GetGameImageRect() const;

    LRESULT CALLBACK HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnSubWindowClosed(Uint32 sdlWindowID);

    // True if the event belonged to the debug host window (always false outside Debug builds).
    [[nodiscard]] bool HandleDebugHostEvent(const SDL_Event& event);

private:
    void CalculateFrameStats(float dt);

#if defined(_DEBUG)
    // Registers the two placeholder panels that prove the DebugUI flow.
    void RegisterDebugMenuBar(); 
#endif

    static Framework* pInstance;
    HighResolutionTimer timer;

    std::unique_ptr<Scene> scene;
    std::unique_ptr<Scene> nextScene;

    WindowMode m_mainWindowMode{ WindowMode::windowed };

    // Off-screen 1920x1080 target that scenes draw into (see GameCanvas.h).
    // Null if its GPU resources could not be created.
    std::unique_ptr<GameCanvas> m_gameCanvas;

#if defined(_DEBUG)

    // Debug-window focus state of the previous frame, to change SDL hints only on a switch.
    bool m_wasDebugFocused{ false };

    // Created after Graphics::Initialize(), hence a pointer and not a direct member.
    // Declared last: destroyed after ~Framework's body, so after ImGui is finalized.
    std::unique_ptr<DebugHostWindow> m_debugHost;

    // Declared last: unregistered before anything its callback could read is destroyed.
    DebugPanelHandle m_menuBarPanel;

#endif

};