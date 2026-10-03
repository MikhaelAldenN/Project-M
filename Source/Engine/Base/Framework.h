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

    LRESULT CALLBACK HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnSubWindowClosed(Uint32 sdlWindowID);

    // True if the event belonged to the debug host window (always false outside Debug builds).
    [[nodiscard]] bool HandleDebugHostEvent(const SDL_Event& event);

private:
    void CalculateFrameStats(float dt);

#if defined(_DEBUG)
    // Registers the two placeholder panels that prove the DebugUI flow.
    void RegisterSampleDebugPanels();
#endif


    static Framework* pInstance;
    HighResolutionTimer timer;

    std::unique_ptr<Scene> scene;
    std::unique_ptr<Scene> nextScene;

#if defined(_DEBUG)
    // Created after Graphics::Initialize(), hence a pointer and not a direct member.
    // Declared last: destroyed after ~Framework's body, so after ImGui is finalized.
    std::unique_ptr<DebugHostWindow> m_debugHost;

    // Placeholder panels; replace with real ones in step 2.
    DebugPanelHandle m_sampleColumnPanel;
    DebugPanelHandle m_sampleTabPanel;

#endif

};