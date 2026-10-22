#pragma once

#include <memory>
#include <imgui.h>
#include <wrl/client.h>
#include "Scene.h"
#include "Camera.h"
#include "System/AudioManager.h"
#include "System/Sprite.h" 
#include "System/Input.h"
#include "System/Graphics.h"
#include "BitmapFont.h"
#include "Framework.h"
#include "ResourceManager.h"
#include "UberShader.h"
#include "DebugUI.h"

class SceneIntro : public Scene
{
public:
    SceneIntro();
    ~SceneIntro() override = default;

    void Update(float elapsedTime) override;
    void Render(float dt, Camera* camera = nullptr) override;
    void DrawGUI() override;
    [[nodiscard]] Kind GetKind() const override { return Kind::intro; }

    Camera* GetCamera() const { return m_camera.get(); }

private:
    void CreateRenderTarget();

    std::unique_ptr<Camera> m_camera;

    // Helper for the GUI
    void GUIPostProcessTab();

    // Draws the debug controls only, with no ImGui window around them.
    void DrawDebugPanel();

    // GUI State
    struct PostProcessState
    {
        bool MasterEnabled = true; 
        bool EnableVignette = true;
        bool EnableLens = true;
        bool EnableCRT = true;
    };
    PostProcessState m_fxState;

    // Post-Processing Resources
    std::unique_ptr<UberShader> m_uberShader;
    UberShader::UberData m_uberParams;
    float m_globalTime = 0.0f;

    // DirectX Render Targets
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_renderTargetTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthStencilTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilView;

#if defined(_DEBUG)
    // Keeps the "Bios Inspector" tab registered while this scene lives.
    // Declared last: destroyed first, before any member the callback reads.
    DebugPanelHandle m_debugPanel{};
#endif
};