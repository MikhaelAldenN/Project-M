// =============================================================
// Framework Implementation
// - Initializes, updates, renders, and handles the main loop
// =============================================================
#include <memory>
#include <sstream>
#include <imgui.h>

#include "Framework.h"
#include "System/Input.h"
#include "System/Graphics.h"
#include "System/ImGuiRenderer.h"
#include "SceneGame.h"
#include "SceneTitle.h"
#include "SceneManager.h"

static const int syncInterval = 1;
static SceneGame sceneGame;

Framework::Framework(HWND hWnd)
    : hWnd(hWnd)
{
    Input::Instance().Initialize(hWnd);
    Graphics::Instance().Initialize(hWnd);
    ImGuiRenderer::Initialize(hWnd, Graphics::Instance().GetDevice(), Graphics::Instance().GetDeviceContext());
    SceneManager::Instance().ChangeScene(new SceneTitle);
}

Framework::~Framework()
{
    SceneManager::Instance().Clear();
    ImGuiRenderer::Finalize();
}

void Framework::Update(float elapsedTime)
{
    Input::Instance().Update();
    ImGuiRenderer::NewFrame();
    SceneManager::Instance().Update(elapsedTime);
}

void Framework::Render(float elapsedTime)
{
    ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();

    Graphics::Instance().Clear(0.3f, 0.3f, 0.4f, 1);
    Graphics::Instance().SetRenderTargets();
    SceneManager::Instance().Render();
    SceneManager::Instance().DrawGUI();

#if 0
    ImGui::ShowDemoWindow();
#endif
    ImGuiRenderer::Render(dc);
    Graphics::Instance().Present(syncInterval);
}

void Framework::CalculateFrameStats()
{
    static int frames = 0;
    static float time_tlapsed = 0.0f;

    frames++;
    if ((timer.TimeStamp() - time_tlapsed) >= 1.0f)
    {
        float fps = static_cast<float>(frames);
        float mspf = 1000.0f / fps;
        std::ostringstream outs;
        outs.precision(6);
        outs << "FPS : " << fps << " / " << "Frame Time : " << mspf << " (ms)";
        SetWindowTextA(hWnd, outs.str().c_str());

        frames = 0;
        time_tlapsed += 1.0f;
    }
}

int Framework::Run()
{
    MSG msg = {};

    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            timer.Tick();
            CalculateFrameStats();

            float elapsedTime = timer.TimeInterval();
            Update(elapsedTime);
            Render(elapsedTime);
        }
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK Framework::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGuiRenderer::HandleMessage(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc;
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CREATE:
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) PostMessage(hWnd, WM_CLOSE, 0, 0);
        break;
    case WM_ENTERSIZEMOVE:
        timer.Stop();
        break;
    case WM_EXITSIZEMOVE:
        timer.Start();
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}