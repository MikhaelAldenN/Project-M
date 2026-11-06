#include "SceneBoss.h"
#include "System/AudioManager.h"
#include "System/Graphics.h"
#include "System/Input.h"
#include "UIPause.h"
#include "WindowManager.h"
#include "Framework.h"
#include "SceneTitle.h"
#include "InputHelper.h"
#include "PerformanceLogger.h"
#include <algorithm>
#include <array>
#include <ctime>
#include "Game/Collision/CollisionManager.h"
#include "ProjectilePool.h"
#include "EnemyManager.h"
#include "ItemManager.h"
#include "Stage.h"
#include <random>
#include "CueBoss.h"
#include "BossPhase02.h"
#include "BossPhase01.h"
#include "HUDRenderer.h"
#include "TimeManager.h"
#include "EffectManager.h"
#include "WindowShatter.h"
#include "AttackParamManager.h"
#include "Engine/Common/Constants.h"
#include "Engine/Base/MathUtils.h"
using namespace DirectX;

namespace
{
    // Null-safe: no boss phase counts as "not Windowkill".
    [[nodiscard]] bool IsWindowkill(const ICuePhase* phase)
    {
        return phase && phase->GetKind() == BossPhaseKind::windowkill;
    }
}

// =========================================================
// CONSTRUCTOR / DESTRUCTOR
// =========================================================

SceneBoss::SceneBoss()
{
#if defined(_DEBUG)
    // Opens Logs/ChaosLog_*.txt. Release never opens it, so every later log call is a no-op
    // and a shipped build writes nothing to the player's disk.
    PerformanceLogger::Instance().Initialize();
#endif
    PerformanceLogger::Instance().LogInfo("[INIT] SceneBoss constructor begin.");

    // --- Window Tracking System ---
    m_windowSystem = std::make_unique<WindowTrackingSystem>();
    m_windowSystem->SetArenaRect(Framework::Instance()->GetGameImageRect());
    m_windowSystem->SetFOV(k_fov);

    // --- Camera ---
    // Use a fixed 16:9 aspect for the projection; off-center projection per
    // sub-window is handled by WindowTrackingSystem::UpdateOffCenterProjection.
    const float unifiedHeight = m_windowSystem->GetUnifiedCameraHeight();
    m_mainCamera = std::make_shared<Camera>();
    m_mainCamera->SetPerspectiveFov(XMConvertToRadians(k_fov), 1920.0f / 1080.0f, k_camNear, k_camFar);
    m_mainCamera->SetPosition(m_cameraPosition);
    m_mainCamera->LookAt(m_cameraTarget);

    CameraController::Instance().SetActiveCamera(m_mainCamera);
    CameraController::Instance().SetControlMode(CameraControlMode::FixedStatic);
    CameraController::Instance().SetFixedSetting(XMFLOAT3(0.0f, unifiedHeight, 0.0f));

    // --- Player ---
    m_player = std::make_unique<Player>();
    PlayerConfig bossConfig{};
    bossConfig.moveSpeed = 15.0f;         // Fast movement
    bossConfig.dashSpeed = 45.0f;         // Fast dash

    m_player->ApplyConfig(bossConfig);

    m_player->SetPosition(0.0f, 0.0f, -8.0f);

    // --- Primitive Renderers ---
    ID3D11Device* device = Graphics::Instance().GetDevice();
    auto* context = Graphics::Instance().GetDeviceContext();
    m_primitive2D = std::make_unique<Primitive>(device);
    m_primitive3D = std::make_unique<PrimitiveRenderer>(device);
    m_hud = std::make_unique<HUDRenderer>(device);

    EffectManager::Instance().Initialize(device, context);

    m_stage = std::make_unique<Stage>(device); // empty, but systems that read the stage need one

    m_collisionManager = std::make_unique<CollisionManager>();

    m_collisionManager->Initialize(m_player.get(), m_stage.get(), m_enemyManager.get(), m_itemManager.get());

    // The player sends its collision queries through this scene's CollisionManager.
    m_player->SetCollisionManager(m_collisionManager.get());

    m_cue = std::make_unique<CueBoss>();
    m_cue->Initialize(m_windowSystem.get());

    m_cue->ChangePhase(std::make_unique<BossPhase01>(m_player.get()));


    if (m_collisionManager) {
        m_collisionManager->SetBoss(m_cue.get());
        m_playerWindowTransparent = false;
    }

    WindowManager::Instance().MarkPriorityDirty();
    InitializeSubWindows();

    // --- Death Fade Effects ---
    // Post-process only ever runs on the main camera, which draws into the game canvas.
    m_postProcess = std::make_unique<PostProcessManager>();
    m_postProcess->Initialize(Beyond::Config::CANVAS_WIDTH, Beyond::Config::CANVAS_HEIGHT);

    m_fadeSprite = std::make_unique<Sprite>(device, "Data/Sprite/Scene Game/Black.png");
    m_whiteSprite = std::make_unique<Sprite>(device, "Data/Sprite/Scene Game/White.png");
    m_uiPause = std::make_unique<UIPause>();
    m_uiPause->Initialize();
    m_uberParams.intensity = FX_BASE_INTENSITY;
    m_uberParams.smoothness = FX_BASE_SMOOTHNESS;

    AddLog("SceneBoss initialized. Windowkill system online.");
    PerformanceLogger::Instance().LogInfo("[INIT] SceneBoss constructor complete.");

    m_bossPanel = DebugUI::Instance().RegisterPanel(DebugPanelSlot::tab, "Boss", [this]() { DrawBossPanel(); });
    m_attacksPanel = DebugUI::Instance().RegisterPanel(DebugPanelSlot::tab, "Attacks", [this]() { DrawAttacksPanel(); });
    m_playerPanel = DebugUI::Instance().RegisterPanel(DebugPanelSlot::tab, "Player", [this]() {
        if (m_player) m_player->DrawDebugGUI();
        else ImGui::TextDisabled("Player not created.");
        });
    m_windowsPanel = DebugUI::Instance().RegisterPanel(DebugPanelSlot::tab, "Windows", [this]() { DrawWindowsPanel(); });
    m_logPanel = DebugUI::Instance().RegisterPanel(DebugPanelSlot::tab, "Log", [this]() { DrawLogPanel(); });
    m_viewMenu = DebugUI::Instance().RegisterPanel(DebugPanelSlot::menuBar, "Boss view", [this]() { DrawViewMenu(); });
    // Capturing `this` is safe: the handle is a member and dies with this scene.
}

SceneBoss::~SceneBoss()
{
    Shutdown();
}

void SceneBoss::Shutdown()
{
    PerformanceLogger::Instance().LogInfo("[TEARDOWN] SceneBoss Shutdown initiated.");

    // CLEAR WINDOWS FIRST (CRITICAL)
    // Destroys sub-windows and unbinds callbacks before the objects they point to (Cue/Player) are deleted.
    if (m_windowSystem) {
        m_windowSystem->ClearAll();
    }

    // STOP ALL SINGLETON LEAKS
    // Singletons outlive the Scene. If we don't clear them, they bleed into SceneTitle/SceneGame.
    AudioManager::Instance().StopMusic();
    EffectManager::Instance().StopAll();
    WindowShatterManager::Instance().Clear();

    // RESTORE MAIN WINDOW OS STATES
    Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow && mainWindow->GetSDLWindow()) {
        SDL_SetWindowAlwaysOnTop(mainWindow->GetSDLWindow(), false);
        mainWindow->SetPriority(50);
        SDL_RaiseWindow(mainWindow->GetSDLWindow());
        WindowManager::Instance().MarkPriorityDirty();
    }

    // RESTORE ENGINE STATES
    CameraController::Instance().ClearCamera();

    // EXPLICIT ENTITY DESTRUCTION ORDER
    m_cue.reset();
    m_player.reset();
    m_enemyManager.reset();
    m_itemManager.reset();
    m_collisionManager.reset();

    PerformanceLogger::Instance().Shutdown();
}

// =========================================================
// INITIALIZATION HELPERS
// =========================================================

void SceneBoss::InitializeSubWindows()
{
    if (!m_windowSystem || !m_player) return;

    // --- Player-tracking window (kept in every phase, Windowkill included) ---
    m_windowSystem->AddTrackedWindow(
        { "player", "Player", 300, 300, 1 },
        [this]() -> XMFLOAT3 {
            if (!m_player) return XMFLOAT3(0.0f, 0.0f, 0.0f);
            const auto pPos = m_player->GetPosition();
            return XMFLOAT3(
                pPos.x + m_stretchOffset.x,
                0.0f,
                pPos.z + m_stretchOffset.y
            );
        },
        [this]() -> XMFLOAT2 {
            return XMFLOAT2(
                k_defaultWinSize + m_currentStretch.x,
                k_defaultWinSize + m_currentStretch.y
            );
        }
    );

    TrackedWindow* playerWin = m_windowSystem->GetTrackedWindow("player");
    if (playerWin && playerWin->window)
    {
        SDL_Window* sdlWin = playerWin->window->GetSDLWindow();
        SDL_SetWindowBordered(sdlWin, true);
    }

    // --- Register main window with the tracking system ---
    Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow && mainWindow->GetSDLWindow())
    {
        SDL_ShowWindow(mainWindow->GetSDLWindow());

        if (!m_windowSystem->GetTrackedWindow("main_window"))
            m_windowSystem->RegisterWindow(mainWindow, WindowRole::MAIN_VIEWPORT, m_mainCamera);
    }

    TrackedWindow* mainTracked = m_windowSystem->GetTrackedWindow("main_window");
    if (mainTracked && mainTracked->window && mainTracked->window->GetSDLWindow()) {
        SDL_RaiseWindow(mainTracked->window->GetSDLWindow());
    }

    WindowManager::Instance().EnforceWindowPriorities();
}

// =========================================================
// UPDATE
// =========================================================

void SceneBoss::Update(float elapsedTime)
{
    if (CheckPauseToggleTriggered())
    {
        m_isPaused = !m_isPaused;
        // Every pause opens on the first option.
        if (m_isPaused)
        {
            m_uiPause->ResetSelection();
        }
    }

    if (m_isPaused)
    {
        UpdatePauseMenu(elapsedTime);
        return; // the rest of the scene is frozen while paused
    }

    PerformanceLogger::Instance().StartTimer(PerfBucket::Logic);
    TimeManager::Instance().Update(elapsedTime);

    float activeTimeScale = TimeManager::Instance().GetHitStopMultiplier();
    const float scaledDt = elapsedTime * activeTimeScale;

    UpdatePlayerDeathSequence(elapsedTime);

    Camera* activeCam = CameraController::Instance().GetActiveCamera().get();

    UpdateCueDefeatFade(elapsedTime);
    if (UpdateWindowkillMainWindow()) return;

    UpdateArenaCamera(elapsedTime);

    // --- Player update ---
    if (m_player)
    {
        m_player->Update(scaledDt, activeCam);
    }

    UpdatePlayerStretch(scaledDt);

    if (UpdateCue(scaledDt)) return;
    if (m_isPendingSceneChange) return;
    UpdateEntitiesAndEffects(scaledDt, activeCam);

    UpdateSubWindows(elapsedTime);

    PerformanceLogger::Instance().StopTimer(PerfBucket::Logic);
    const int activeWins = m_windowSystem ? static_cast<int>(m_windowSystem->GetWindows().size()) : 0;
    PerformanceLogger::Instance().EndFrameCheck(0.0f, activeWins);
}

bool SceneBoss::CheckPauseToggleTriggered() const
{
    // Timed sequences (death, respawn, defeat, scene change, fade to Title) must keep
    // running; freezing their timers would soft-lock the scene.
    if (m_isDying || m_respawnTimer > 0.0f || m_isCueDefeated
        || m_isPendingSceneChange || m_isExitingToTitle)
    {
        return false;
    }

    // Windowkill hides the main window, so the menu would not be visible there.
    if (m_cue && IsWindowkill(m_cue->GetCurrentPhase()))
    {
        return false;
    }

    return UIPause::IsToggleTriggered();
}

void SceneBoss::UpdatePauseMenu(float elapsedTime)
{
    if (m_isExitingToTitle)
    {
        if (m_isPendingSceneChange)
        {
            return;
        }

        m_exitToTitleTimer += elapsedTime;
        m_fadeAlpha = std::clamp(m_exitToTitleTimer / RESPAWN_FADE_DURATION, 0.0f, 1.0f);

        if (m_fadeAlpha >= 1.0f)
        {
            RequestReturnToTitle();
        }
        return;
    }

    if (!m_uiPause->HandleInput())
    {
        return;
    }

    switch (m_uiPause->GetSelectedOption())
    {
    case UIPause::PauseOption::resume:
        m_isPaused = false;
        break;
    case UIPause::PauseOption::title:
        m_isExitingToTitle = true;
        m_exitToTitleTimer = 0.0f;
        AudioManager::Instance().FadeOutMusic(RESPAWN_FADE_DURATION);
        AudioManager::Instance().FadeOutAmbientSFX(RESPAWN_FADE_DURATION);
        break;
    case UIPause::PauseOption::quit:
        Framework::Instance()->Quit();
        break;
    default:
        break;
    }
}

void SceneBoss::UpdatePlayerDeathSequence(float elapsedTime) 
{
    if (m_player && m_player->GetHP() <= 0 && !m_isDying && m_respawnTimer <= 0.0f)
    {
        StartPlayerDeathSequence();
    }

    if (m_isDying)
    {
        m_deathTimer += elapsedTime; 

        if (m_deathTimer < DEATH_DELAY_DURATION)
        {
            m_uberParams.smoothness = FX_BASE_SMOOTHNESS;
            m_uberParams.intensity = FX_BASE_INTENSITY;
            m_fadeAlpha = 0.0f;
        }
        else
        {
            const float fadeTime{ m_deathTimer - DEATH_DELAY_DURATION };
            const float t{ std::clamp(fadeTime / DEATH_FADE_DURATION, 0.0f, 1.0f) };

            m_uberParams.smoothness = FX_BASE_SMOOTHNESS + (FX_BLACK_SMOOTHNESS - FX_BASE_SMOOTHNESS) * t;
            m_uberParams.intensity = FX_BASE_INTENSITY + (FX_BLACK_INTENSITY - FX_BASE_INTENSITY) * t;
            m_fadeAlpha = t;

            if (t >= 1.0f)
            {
                ResetLevel();
                m_isDying = false;
                m_respawnTimer = RESPAWN_FADE_DURATION;

                m_uberParams.smoothness = FX_BLACK_SMOOTHNESS;
                m_uberParams.intensity = FX_BLACK_INTENSITY;
                m_fadeAlpha = 1.0f;
            }
        }
    }
    else if (m_respawnTimer > 0.0f)
    {
        m_respawnTimer -= elapsedTime;

        if (m_player) m_player->SetInputEnabled(false);

        const float linearT{ std::clamp(m_respawnTimer / RESPAWN_FADE_DURATION, 0.0f, 1.0f) };
        const float t{ linearT * linearT }; // Quadratic Ease-Out

        m_uberParams.smoothness = FX_BASE_SMOOTHNESS + (FX_BLACK_SMOOTHNESS - FX_BASE_SMOOTHNESS) * t;
        m_uberParams.intensity = FX_BASE_INTENSITY + (FX_BLACK_INTENSITY - FX_BASE_INTENSITY) * t;
        m_fadeAlpha = t;

        if (m_respawnTimer <= 0.0f && m_player)
        {
            m_player->SetInputEnabled(true);

            if (m_cue)
            {
                if (ICuePhase * phase{ m_cue->GetCurrentPhase() }) phase->SetAIEnabled(true);
            }
        }
    }
    else
    {
        m_uberParams.smoothness = FX_BASE_SMOOTHNESS;
        m_uberParams.intensity = FX_BASE_INTENSITY;
        m_fadeAlpha = 0.0f;
    }
}

void SceneBoss::UpdateCueDefeatFade(float elapsedTime)
{
    if (!m_cue) return;

    const ICuePhase* phase{ m_cue->GetCurrentPhase() };

    // Trigger the start of the death sequence
    if (IsWindowkill(phase) && phase->IsDead() && !m_isCueDefeated)
    {
        m_isCueDefeated = true;
        m_cueDefeatTimer = 0.0f;
        AddLog("Cue defeated. Starting death sequence.");
    }

    // Logic for the Fade-In
    if (m_isCueDefeated)
    {
        m_cueDefeatTimer += elapsedTime;
        if (m_cueDefeatTimer > CUE_DEATH_ANIM_DURATION)
        {
            float fadeTime = m_cueDefeatTimer - CUE_DEATH_ANIM_DURATION;
            m_whiteAlpha = std::clamp(fadeTime / WHITE_FADE_DURATION, 0.0f, 1.0f);
        }
    }
}

bool SceneBoss::UpdateWindowkillMainWindow()
{
    if (!m_cue) return false;

    const ICuePhase* phase{ m_cue->GetCurrentPhase() };
    Beyond::Window* mw = WindowManager::Instance().GetWindowByIndex(0);

    if (IsWindowkill(phase)) {
        // Visibility logic
        if (!phase->IsDead()) {
            if (mw && mw->GetSDLWindow()) SDL_HideWindow(mw->GetSDLWindow());
        }
        else {
            if (mw && mw->GetSDLWindow()) SDL_ShowWindow(mw->GetSDLWindow());
        }

        // ONLY change scene if the White Fade is complete!
        if (m_whiteAlpha >= 1.0f && !m_isPendingSceneChange) {
            RequestReturnToTitle();
            return true;
        }
    }

    return false;
}

void SceneBoss::UpdateArenaCamera(float elapsedTime)
{
    // --- Sync main window size ---
    Beyond::Window* mainWindow = WindowManager::Instance().GetWindowByIndex(0);
    if (mainWindow)
    {
        m_windowSystem->UpdateWindowBounds(0, mainWindow->GetWidth(), mainWindow->GetHeight());
    }

    // =========================================================
    // STATIC CAMERA (NO ZOOM)
    // =========================================================
    m_targetZoom = 0.0f;
    m_currentZoom = 0.0f;

    m_windowSystem->SetArenaRect(Framework::Instance()->GetGameImageRect());

    // =========================================================
    // CAMERA AND CAMERA SHAKE UPDATE
    // =========================================================
    float newUnifiedHeight = m_windowSystem->GetUnifiedCameraHeight();
    auto& camCtrl = CameraController::Instance();

    DirectX::XMFLOAT3 shake = camCtrl.GetShakeOffset();

    // Camera height from the window system, offset by the shake on every axis.
    camCtrl.SetFixedSetting(DirectX::XMFLOAT3(shake.x, newUnifiedHeight + shake.y, shake.z));
    camCtrl.SetTarget({ shake.x, shake.y, shake.z });

    // Update the camera with unscaled time (elapsedTime) so it keeps shaking during hit stop.
    camCtrl.Update(elapsedTime);
}

void SceneBoss::UpdatePlayerStretch(float scaledDt)
{
    if (!m_player) return;

    const XMFLOAT3 vel = m_player->GetMovement()->GetVelocity();
    const float    currentSpeedSq = vel.x * vel.x + vel.z * vel.z;
    const float    dashThreshold = m_player->GetDashSpeed() * 0.8f;

    XMFLOAT2 targetStretch = { 0.0f, 0.0f };
    XMFLOAT2 targetOffset = { 0.0f, 0.0f };
    constexpr float lerpSpeed = 10.0f;

    if (currentSpeedSq > (dashThreshold * dashThreshold))
    {
        constexpr float kStretchX = 200.0f;
        constexpr float kSquashY = 0.0f;
        constexpr float kStretchZ = 0.0f;
        constexpr float kSquashX = 0.0f;

        const float dashRatio = sqrtf(currentSpeedSq) / m_player->GetDashSpeed();

        if (std::abs(vel.x) > std::abs(vel.z))
        {
            targetStretch.x = dashRatio * kStretchX;
            targetStretch.y = dashRatio * kSquashY;
            const float signX = (vel.x > 0.0f) ? 1.0f : -1.0f;
            targetOffset.x = -signX * (targetStretch.x * 0.5f) / k_pixelToUnitRatio;
        }
        else
        {
            targetStretch.y = dashRatio * kStretchZ;
            targetStretch.x = dashRatio * kSquashX;
            const float signZ = (vel.z > 0.0f) ? 1.0f : -1.0f;
            targetOffset.y = -signZ * (targetStretch.y * 0.5f) / k_pixelToUnitRatio;
        }
    }

    const float follow{ MathUtils::ExpDecayFactor(lerpSpeed, scaledDt) };
    m_currentStretch.x += (targetStretch.x - m_currentStretch.x) * follow;
    m_currentStretch.y += (targetStretch.y - m_currentStretch.y) * follow;
    m_stretchOffset.x += (targetOffset.x - m_stretchOffset.x) * follow;
    m_stretchOffset.y += (targetOffset.y - m_stretchOffset.y) * follow;
}

bool SceneBoss::UpdateCue(float scaledDt)
{
    if (!m_cue) return false;

    // Hand the player to the AI director.
    // Why every frame: a phase created mid-fight has no target until the scene hands it one.
    if (ICuePhase * phase{ m_cue->GetCurrentPhase() }) phase->SetAITarget(m_player.get());

    m_cue->Update(scaledDt);

    // Why fetched here: Boss::Update can swap the phase, so an earlier pointer may be stale.
    const ICuePhase* updatedPhase{ m_cue->GetCurrentPhase() };
    if (updatedPhase && !m_isPendingSceneChange && updatedPhase->IsReadyToChangeScene()) {
        RequestReturnToTitle();
        return true;
    }

    const bool isWindowkillPhase{ IsWindowkill(updatedPhase) };

    if (isWindowkillPhase && !m_playerWindowTransparent) {
        // Windowkill started: the player window becomes transparent.
        m_playerWindowTransparent = true;
    }
    else if (!isWindowkillPhase && m_playerWindowTransparent) {
        // Left Windowkill (respawn or death): the player window is opaque again.
        m_playerWindowTransparent = false;
    }

    return false;
}

void SceneBoss::UpdateEntitiesAndEffects(float scaledDt, Camera* activeCam)
{
    if (m_enemyManager) m_enemyManager->Update(scaledDt, activeCam, m_player->GetPosition(), true);
    if (m_itemManager) m_itemManager->Update(scaledDt, activeCam);
    if (m_collisionManager) m_collisionManager->Update(scaledDt);

    EffectManager::Instance().Update(scaledDt);
    WindowShatterManager::Instance().Update(scaledDt);
}

void SceneBoss::UpdateSubWindows(float elapsedTime)
{
    if (!m_windowSystem) return;

    for (auto& tracked : m_windowSystem->GetWindows())
    {
        if (!tracked->isActive) continue;
        if (!tracked->camera || tracked->camera == m_mainCamera) continue;

        tracked->camera->SetPosition(m_mainCamera->GetPosition());
        tracked->camera->SetRotation(m_mainCamera->GetRotation());
    }

    m_windowSystem->Update(elapsedTime);
}


// =========================================================
// RENDER
// =========================================================

void SceneBoss::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam = camera ? camera : m_mainCamera.get();
    auto dc = Graphics::Instance().GetDeviceContext();
    auto rs = Graphics::Instance().GetRenderState();

    const bool isTransparentWindow{ IsTransparentWindowCamera(camera) };

    if (!isTransparentWindow)
    {
        ClearBoundTargets(dc);
    }

    // Transparent windows need TransparentWindow blend state so the alpha
    // channel is preserved for UpdateLayeredWindow. Normal windows use
    // standard alpha blending.
    dc->OMSetBlendState(
        rs->GetBlendState(isTransparentWindow
            ? BlendState::TransparentWindow
            : BlendState::Transparency),
        nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    const bool usePostProcess{ BeginMainCameraPostProcess(targetCam, isTransparentWindow) };

    RenderScene(elapsedTime, targetCam, isTransparentWindow);

    if (usePostProcess)
    {
        m_postProcess->EndCapture(elapsedTime);
    }

    if (m_showGrid && m_primitive3D)
    {
        m_primitive3D->DrawGrid(25, 1.0f);
        m_primitive3D->Render(dc,
            targetCam->GetView(),
            targetCam->GetProjection(),
            D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    }

    RenderShapesAndHitboxes(dc, targetCam);

    RenderFadeOverlays(dc, rs);

    // Health bars: player bottom-left, boss top-centre. Main camera pass only, so the
    // sub-windows stay clean. HUDRenderer expects to run after the scene draws.
    if (m_hud && !isTransparentWindow && targetCam == m_mainCamera.get())
    {
        const ICuePhase* hudPhase{ m_cue ? m_cue->GetCurrentPhase() : nullptr };
        // Player HP counts hits, so the bar drops one step per hit.
        m_hud->Render(dc,
            m_player ? m_player->GetHP() : 0,
            m_player ? m_player->GetMaxHP() : 0,
            hudPhase ? hudPhase->GetHP() : 0,
            hudPhase ? hudPhase->GetMaxHP() : 0);
    }

    // Sub-windows keep showing the frozen arena; only the main window carries the menu.
    if (m_isPaused && !isTransparentWindow && targetCam == m_mainCamera.get())
    {
        RenderPauseMenu(dc, rs);
    }
}

bool SceneBoss::IsTransparentWindowCamera(const Camera* camera) const
{
    if (!m_windowSystem) return false;

    for (const auto& tracked : m_windowSystem->GetWindows())
    {
        if (!tracked->isActive) continue;

        if (tracked->camera.get() == camera &&
            tracked->window &&
            tracked->window->IsTransparent())
        {
            return true;
        }
    }

    return false;
}

void SceneBoss::ClearBoundTargets(ID3D11DeviceContext* dc)
{
    ID3D11RenderTargetView* currentRTV = nullptr;
    ID3D11DepthStencilView* currentDSV = nullptr;

    dc->OMGetRenderTargets(1, &currentRTV, &currentDSV);

    if (currentRTV)
    {
        // Clear again with this scene's clear color.
        float clearColor[4] = { m_clearColor.x, m_clearColor.y, m_clearColor.z, m_clearColor.w };
        dc->ClearRenderTargetView(currentRTV, clearColor);

        // OMGetRenderTargets adds a reference, so release it.
        currentRTV->Release();
    }

    if (currentDSV)
    {
        // Clear depth and stencil as well, so 3D depth tests start fresh.
        dc->ClearDepthStencilView(currentDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        currentDSV->Release();
    }
}

bool SceneBoss::BeginMainCameraPostProcess(const Camera* targetCam, bool isTransparentWindow)
{
    // =========================================================
    // POST-PROCESS VIGNETTE (Only applied to Main Window)
    // =========================================================
    const ICuePhase* activePhase{ m_cue ? m_cue->GetCurrentPhase() : nullptr };

    // 2. Determine if Windowkill is active AND the boss is NOT dead
    const bool isWindowkillAndAlive{ IsWindowkill(activePhase) && !activePhase->IsDead() };

    // 3. Use the updated boolean to gate post-processing
    const bool usePostProcess = (!isTransparentWindow &&
        targetCam == m_mainCamera.get() &&
        m_postProcess &&
        isWindowkillAndAlive);

    if (!usePostProcess) return false;

    m_postProcess->SetEnabled(true);
    UberShader::UberData& activeData = m_postProcess->GetData();
    activeData = m_uberParams;

    // Turn off unneeded filters just to be safe
    activeData.glitchStrength = 0.0f;
    activeData.distortion = 0.0f;
    activeData.chromaticAberration = 0.0f;
    activeData.scanlineStrength = 0.0f;
    activeData.bloomIntensity = 0.0f;
    activeData.psxEnabled = false;

    m_postProcess->BeginCapture();

    return true;
}

void SceneBoss::RenderShapesAndHitboxes(ID3D11DeviceContext* dc, Camera* targetCam)
{
    auto shapeRenderer = Graphics::Instance().GetShapeRenderer();
    if (m_showHitboxes)
    {
        // 1. Player hurtbox (green)
        // Drawn at y = 1 so it shows on the player's body.
        if (m_player && m_player->GetHP() > 0) {
            DirectX::XMFLOAT3 pPos = m_player->GetPosition();
            pPos.y = 1.0f;
            shapeRenderer->DrawSphere(pPos, 0.3f, { 0.0f, 1.0f, 0.0f, 1.0f });
        }

        // 2. Boss bullet hitboxes: red in bullet hell, lime in Windowkill (bouncing bullets).
        if (const ICuePhase * phase{ m_cue ? m_cue->GetCurrentPhase() : nullptr })
        {
            const bool isWindowkill{ IsWindowkill(phase) };
            if (const ProjectilePool * pool{ phase->GetProjectilePool() })
            {
                const DirectX::XMFLOAT4 color{ isWindowkill
                    ? DirectX::XMFLOAT4{ 0.0f, 1.0f, 0.0f, 1.0f }
                    : DirectX::XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f } };
                for (const Projectile& projectile : pool->Slots())
                {
                    if (projectile.isActive && projectile.hitsPlayer) shapeRenderer->DrawSphere(projectile.position, projectile.radius, color);
                }
            }
        }
    }

    Graphics::Instance().GetShapeRenderer()->Render(
        dc, targetCam->GetView(), targetCam->GetProjection());
}

void SceneBoss::RenderFadeOverlays(ID3D11DeviceContext* dc, RenderState* rs)
{
    // =========================================================
    // FADE SPRITE OVERLAY 
    // =========================================================
    if (m_fadeAlpha > 0.001f && m_fadeSprite)
    {
        // Why the viewport: Render runs once per window (canvas, then each sub-window),
        // and the overlay must cover whichever target is bound right now.
        D3D11_VIEWPORT viewport{};
        UINT viewportCount{ 1 };
        dc->RSGetViewports(&viewportCount, &viewport);
        const float screenW{ viewport.Width };
        const float screenH{ viewport.Height };

        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

        m_fadeSprite->Render(
            dc,
            0.0f, 0.0f, 0.0f,
            screenW, screenH,
            0.0f, 0.0f,
            1920.0f, 1080.0f,
            0.0f,
            0.0f, 0.0f, 0.0f, m_fadeAlpha
        );
    }

    if (m_whiteAlpha > 0.001f && m_whiteSprite)
    {
        D3D11_VIEWPORT viewport{};
        UINT viewportCount{ 1 };
        dc->RSGetViewports(&viewportCount, &viewport);
        const float screenW{ viewport.Width };
        const float screenH{ viewport.Height };

        dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

        m_whiteSprite->Render(
            dc,
            0.0f, 0.0f, 0.0f,
            screenW, screenH,
            0.0f, 0.0f,
            1920.0f, 1080.0f,
            0.0f,
            1.0f, 1.0f, 1.0f, m_whiteAlpha 
        );
    }
}

void SceneBoss::RenderPauseMenu(ID3D11DeviceContext* dc, RenderState* rs)
{
    if (!m_fadeSprite || !m_uiPause) return;

    D3D11_VIEWPORT viewport{};
    UINT viewportCount{ 1 };
    dc->RSGetViewports(&viewportCount, &viewport);

    dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

    m_fadeSprite->Render(
        dc,
        0.0f, 0.0f, 0.0f,
        viewport.Width, viewport.Height,
        0.0f, 0.0f,
        1920.0f, 1080.0f,
        0.0f,
        0.0f, 0.0f, 0.0f, PAUSE_DIM_ALPHA
    );

    // The menu fades with the screen while returning to Title.
    const float uiAlpha{ m_isExitingToTitle ? (1.0f - m_fadeAlpha) : 1.0f };
    m_uiPause->Render(dc, uiAlpha);
}

void SceneBoss::StartPlayerDeathSequence()
{
    if (m_isDying) return;

    m_isDying = true;
    m_deathTimer = 0.0f;

    if (m_player)
    {
        m_player->SetInputEnabled(false);
        m_player->m_scale = { 0.0f, 0.0f, 0.0f }; // Hide player

        // Stop movement sliding
        if (m_player->GetMovement()) {
            m_player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
        }
    }

    // The AI is held off only while the player is dying or respawning. Outside that
    // window the phase owns m_aiEnabled. The respawn branch above turns it on the frame
    // the timer ends, and this check then sees a timer of zero and leaves it on.
    if (m_cue && (m_isDying || m_respawnTimer > 0.0f))
    {
        if (ICuePhase * phase{ m_cue->GetCurrentPhase() }) phase->SetAIEnabled(false);
    }
}

void SceneBoss::RenderScene(float elapsedTime, Camera* camera, bool isTransparentWindow)
{
    PerformanceLogger::Instance().StartTimer(PerfBucket::Render3D);

    if (!camera) return;

    auto dc = Graphics::Instance().GetDeviceContext();
    auto modelRenderer = Graphics::Instance().GetModelRenderer();

    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, nullptr };
    rc.isTransparentWindow = isTransparentWindow;
    // --- 1. Is this the wing (FX) camera? ---
    bool isWingCamera = false;
    if (m_cue) {
        if (auto* wkPhase = dynamic_cast<BossPhase02*>(m_cue->GetCurrentPhase())) {
            isWingCamera = (camera == wkPhase->GetFXCamera());
        }
    }

    // A. Player bullets: drawn in every window, so they appear to cross between windows
    if (m_player) {
        m_player->RenderProjectiles(modelRenderer);
    }

    // B. Player body (conditional)
    if (m_player) {
        // Default rule:
        bool shouldRenderHere = m_playerWindowTransparent ? isWingCamera : !isWingCamera;

        // Why: in Windowkill every tracked window can show the player, not only the wing camera.
        if (m_cue && IsWindowkill(m_cue->GetCurrentPhase())) {
            shouldRenderHere = true;
        }

        if (shouldRenderHere) {
            const XMFLOAT3 pPos = m_player->GetPosition();

            // The sphere check is bypassed while the player is caged (below).
            bool isInView = camera->CheckSphere(pPos.x, pPos.y, pPos.z, 1.5f);

            auto* wkPhase = dynamic_cast<BossPhase02*>(m_cue->GetCurrentPhase());
            if (wkPhase && wkPhase->IsPlayerCaged()) {
                isInView = true; // always drawn while caged
            }

            if (isInView && !m_player->IsBlinkHidden()) {
                m_player->Render(modelRenderer);
            }
        }
    }

    // C. Enemies and items, skipped by the wing camera
    if (!isWingCamera) {
        if (m_enemyManager) m_enemyManager->Render(modelRenderer);
        if (m_itemManager) m_itemManager->Render(modelRenderer);
    }

    // --- 3. Cue (face and background) ---
    if (m_cue) m_cue->Render(dc, camera);

    modelRenderer->Render(rc);
    EffectManager::Instance().Render(camera);

}


// =========================================================
// GUI
// =========================================================

    void SceneBoss::DrawGUI()
    {

        if (m_isPendingSceneChange) return;
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(m_debugPanelSize, ImGuiCond_FirstUseEver);

        ImGui::Begin("WINDOWKILL MASTER CONTROL", nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar);

        m_debugPanelSize = ImGui::GetWindowSize();

        if (ImGui::BeginMenuBar())
        {
            DrawViewMenu();
            ImGui::EndMenuBar();
        }

        // Release has no DebugUI host, so this window provides the tab frame itself.
        if (ImGui::BeginTabBar("##Panels"))
        {
            if (ImGui::BeginTabItem("Boss")) { DrawBossPanel(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Attacks")) { DrawAttacksPanel(); ImGui::EndTabItem(); }
            if (m_player && ImGui::BeginTabItem("Player")) { m_player->DrawDebugGUI(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Windows")) { DrawWindowsPanel(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Log")) { DrawLogPanel(); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }

        ImGui::End();
    }
    
void SceneBoss::DrawBossPanel()
{
    // Why: once the scene change is pending, phase and window state are being torn down.
    if (m_isPendingSceneChange) return;

    if (!m_cue)
    {
        ImGui::TextDisabled("Boss not created.");
        return;
    }

    ICuePhase* phase{ m_cue->GetCurrentPhase() };
    if (!phase)
    {
        ImGui::TextDisabled("No active phase.");
        return;
    }
    const bool isBulletHell{ phase->GetKind() == BossPhaseKind::bulletHell };
    // Why a cast: the cage hint, wing replay and Wings category exist only in Windowkill.
    auto* windowkill{ dynamic_cast<BossPhase02*>(phase) };

    // ---- Head: state ----
    bool isAIEnabled{ phase->IsAIEnabled() };
    if (ImGui::Checkbox("AI enabled", &isAIEnabled))
    {
        phase->SetAIEnabled(isAIEnabled);
        if (isBulletHell && isAIEnabled)
        {
            // Why here: there is no other fight-start trigger yet, so enabling the AI starts the BGM.
            AudioManager::Instance().PlayMusic("Data/Sound/BGM_Boss_Phase_01.wav",
                0.05f * AttackParamManager::Instance().GetUltimateParams().sfxVolume, true);
        }
        AddLog(isAIEnabled ? "Boss AI enabled." : "Boss AI disabled.");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Phase: %s", isBulletHell ? "Bullet hell" : "Windowkill");

    const int maxHP{ phase->GetMaxHP() };
    int hp{ phase->GetHP() };
    const float hpFraction{ (maxHP > 0) ? static_cast<float>(hp) / static_cast<float>(maxHP) : 0.0f };
    const std::string hpText{ "HP " + std::to_string(hp) + " / " + std::to_string(maxHP) };
    ImGui::ProgressBar(hpFraction, ImVec2{ -1.0f, 0.0f }, hpText.c_str());

    if (windowkill && windowkill->IsPlayerCaged())
    {
        ImGui::TextDisabled("AI gated until the cage breaks");
    }

    // ---- Head: actions ----
    // Why 0 is allowed: it runs the real "HP depleted" path (next phase / death sequence).
    // In Windowkill that is one-way; use Scene > Reload afterwards.
    if (DebugProperty::SliderInt("Set HP", hp, 0, maxHP))
    {
        hp = std::clamp(hp, 0, maxHP); // Ctrl+click input can exceed the slider range
        phase->SetHP(hp);
    }

    if (isBulletHell)
    {
        if (ImGui::Button("Go to Windowkill"))
        {
            m_cue->ChangePhase(std::make_unique<BossPhase02>(m_player.get()));
            m_playerWindowTransparent = true;
            AddLog("Phase changed to Windowkill.");
            return; // the phase pointers above are dangling now
        }
    }
    else
    {
        if (ImGui::Button("Replay wing spawn")) windowkill->ReplayAnimation();
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart at Bullet hell"))
    {
        const bool wasWindowkill{ windowkill != nullptr };
        m_cue->ChangePhase(std::make_unique<BossPhase01>(m_player.get()));
        if (wasWindowkill)
        {
            m_playerWindowTransparent = false;
            if (m_player) m_player->RestoreShootDelay();
        }
        AddLog("Boss restarted at Bullet hell.");
        return; // the phase pointers above are dangling now
    }

    // ---- Categories ----
    ImGui::PushID("Core");
    if (ImGui::CollapsingHeader("Core", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float speed{ m_cue->GetCoreBreathSpeed() };
        float intensity{ m_cue->GetCoreBreathIntensity() };
        bool isEdited{ false };
        isEdited |= DebugProperty::SliderFloat("Breath speed", speed, 0.1f, 20.0f);
        isEdited |= DebugProperty::SliderFloat("Breath intensity", intensity, 0.0f, 200.0f);
        if (isEdited) m_cue->SetCoreBreathParams(speed, intensity);
    }
    ImGui::PopID();

    ImGui::PushID("Face");
    if (ImGui::CollapsingHeader("Face"))
    {
        FaceParams& face{ m_cue->GetFaceParams() };
        DebugProperty::Checkbox("Glitch enabled", face.enableGlitch);
        DebugProperty::SliderFloat("Face size", face.faceTotalSize, 1.0f, 15.0f, "%.1f units");
        DebugProperty::DragFloatRange("Interval range", face.minInterval, face.maxInterval, 0.01f, 0.01f, 2.0f, "%.2f s");
        DebugProperty::SliderFloat("2x2 chunk chance", face.chance2x2, 0.0f, 100.0f, "%.1f %%");
        DebugProperty::SliderFloat("Flicker chance", face.flickerChance, 0.0f, 15.0f, "%.2f %%");
        DebugProperty::SliderFloat("Color glitch chance", face.colorGlitchChance, 0.0f, 100.0f, "%.1f %%");
    }
    ImGui::PopID();

    if (windowkill)
    {
        ImGui::PushID("Wings");
        if (ImGui::CollapsingHeader("Wings"))
        {
            float flapSpeed{ windowkill->GetWingFlapSpeed() };
            float flapIntensity{ windowkill->GetWingFlapIntensity() };
            bool isFlapEdited{ false };
            isFlapEdited |= DebugProperty::SliderFloat("Flap speed", flapSpeed, 0.1f, 10.0f);
            isFlapEdited |= DebugProperty::SliderFloat("Flap intensity", flapIntensity, 0.0f, 2.0f);
            if (isFlapEdited) windowkill->SetWingFlapParams(flapSpeed, flapIntensity);

            float offsetX{ windowkill->GetWingOffsetX() };
            float offsetZ{ windowkill->GetWingOffsetZ() };
            if (DebugProperty::DragFloat2("Offset", offsetX, offsetZ, 0.1f, -20.0f, 20.0f, "%.1f units"))
            {
                windowkill->SetWingOffsets(offsetX, offsetZ);
            }

            float scale{ windowkill->GetWingGlobalScale() };
            if (DebugProperty::SliderFloat("Scale", scale, 0.1f, 5.0f))
            {
                // Why GetPixelToUnit: the ratio is a fixed 40 and not tunable here.
                windowkill->SetScalingParams(windowkill->GetPixelToUnit(), scale);
            }

            int seed{ static_cast<int>(windowkill->GetWingSeed()) };
            if (DebugProperty::InputInt("Seed", seed)) windowkill->SetWingSeed(static_cast<unsigned int>(seed));
            if (ImGui::Button("Randomize seed"))
            {
                std::random_device randomDevice;
                windowkill->SetWingSeed(randomDevice());
            }

            float popDuration{ windowkill->GetPopDuration() };
            float spawnDuration{ windowkill->GetSpawnDuration() };
            float spawnChaos{ windowkill->GetSpawnChaos() };
            bool isSpawnEdited{ false };
            isSpawnEdited |= DebugProperty::SliderFloat("Pop duration", popDuration, 0.01f, 1.0f, "%.2f s");
            isSpawnEdited |= DebugProperty::SliderFloat("Spawn duration", spawnDuration, 0.1f, 5.0f, "%.2f s");
            isSpawnEdited |= DebugProperty::SliderFloat("Spawn chaos", spawnChaos, 0.0f, 2.0f);
            if (isSpawnEdited) windowkill->SetSpawnParams(popDuration, spawnDuration, spawnChaos);
        }
        ImGui::PopID();
    }
}

namespace
{
    // Order of the Attacks panel list while the Windowkill phase is active.
    enum class WindowkillSet { blasters, bouncing, boomerangs, spears, count };

    constexpr std::array<const char*, 4> k_windowkillSetNames{ "Blasters", "Bouncing", "Boomerangs", "Spears" };
    static_assert(k_windowkillSetNames.size() == static_cast<std::size_t>(WindowkillSet::count),
        "one name per WindowkillSet enumerator");

    // Row functions: `p` is the live set, `loaded` the same set as of the last file load.
    // A row gets the amber marker while its value differs from `loaded`.
    // Window sizes are in canvas pixels (40 per world unit).

    void DrawBlasterRows(BlasterParams& p, const BlasterParams& loaded)
    {
        DebugProperty::SliderFloat("Window size", p.cannonWindowSize, 100.0f, 800.0f, "%.0f px", p.cannonWindowSize != loaded.cannonWindowSize);
        DebugProperty::SliderFloat("Visual scale", p.cannonVisualScale, 0.1f, 10.0f, "%.2f", p.cannonVisualScale != loaded.cannonVisualScale);
        DebugProperty::SliderFloat("Beam width", p.beamVisualWidth, 1.0f, 20.0f, "%.1f units", p.beamVisualWidth != loaded.beamVisualWidth);
        DebugProperty::SliderFloat("Beam max length", p.beamMaxLength, 50.0f, 500.0f, "%.1f units", p.beamMaxLength != loaded.beamMaxLength);
        DebugProperty::SliderFloat("Beam grow speed", p.beamGrowSpeed, 1.0f, 100.0f, "%.1f", p.beamGrowSpeed != loaded.beamGrowSpeed);
        DebugProperty::SliderFloat("Beam slide speed", p.beamSlideSpeed, 1.0f, 50.0f, "%.1f", p.beamSlideSpeed != loaded.beamSlideSpeed);
        DebugProperty::SliderInt("Damage per tick", p.beamDamage, 1, 100, p.beamDamage != loaded.beamDamage);
        DebugProperty::SliderInt("Spawn count", p.spawnCount, 1, 15, p.spawnCount != loaded.spawnCount);
        DebugProperty::SliderFloat("Spawn delay", p.spawnDelay, 0.0f, 1.0f, "%.2f s", p.spawnDelay != loaded.spawnDelay);
        DebugProperty::SliderFloat("Spawn spread", p.spawnSpreadX, 10.0f, 100.0f, "%.1f units", p.spawnSpreadX != loaded.spawnSpreadX);
        DebugProperty::SliderFloat("Charge delay", p.chargeDelay, 0.1f, 3.0f, "%.2f s", p.chargeDelay != loaded.chargeDelay);
        DebugProperty::SliderFloat("Fire duration", p.fireDuration, 0.1f, 3.0f, "%.2f s", p.fireDuration != loaded.fireDuration);
        DebugProperty::DragFloat("Drop-in duration", p.dropInDuration, 0.05f, 0.1f, 2.0f, "%.2f s", p.dropInDuration != loaded.dropInDuration);
    }

    void DrawBouncingRows(BouncingBulletParams& p, const BouncingBulletParams& loaded)
    {
        DebugProperty::SliderInt("Spawn count", p.spawnCount, 1, 10, p.spawnCount != loaded.spawnCount);
        DebugProperty::SliderFloat("Spawn delay", p.spawnDelay, 0.0f, 1.0f, "%.2f s", p.spawnDelay != loaded.spawnDelay);
        DebugProperty::SliderFloat("Speed", p.speed, 5.0f, 100.0f, "%.1f", p.speed != loaded.speed);
        DebugProperty::SliderInt("Max bounces", p.maxBounces, 1, 30, p.maxBounces != loaded.maxBounces);
        DebugProperty::SliderInt("Damage", p.damage, 1, 50, p.damage != loaded.damage);
        DebugProperty::DragFloat2("Window size", p.windowWidth, p.windowHeight, 1.0f, 100.0f, 1000.0f, "%.0f px",
            p.windowWidth != loaded.windowWidth || p.windowHeight != loaded.windowHeight);
        DebugProperty::SliderFloat("Visual scale", p.visualScale, 0.1f, 5.0f, "%.2f", p.visualScale != loaded.visualScale);
        DebugProperty::SliderFloat("Hitbox radius", p.hitboxRadius, 0.1f, 10.0f, "%.1f units", p.hitboxRadius != loaded.hitboxRadius);
    }

    void DrawBoomerangRows(BoomerangParams& p, const BoomerangParams& loaded)
    {
        DebugProperty::SliderInt("Spawn count", p.spawnCount, 1, 20, p.spawnCount != loaded.spawnCount);
        DebugProperty::SliderFloat("Spawn delay", p.spawnDelay, 0.0f, 2.0f, "%.2f s", p.spawnDelay != loaded.spawnDelay);
        DebugProperty::Checkbox("Spawn bottom half only", p.spawnBottomHalfOnly, p.spawnBottomHalfOnly != loaded.spawnBottomHalfOnly);
        DebugProperty::SliderFloat("Speed", p.speed, 10.0f, 100.0f, "%.1f", p.speed != loaded.speed);
        DebugProperty::SliderFloat("Max travel distance", p.maxTravelDistance, 10.0f, 120.0f, "%.1f units", p.maxTravelDistance != loaded.maxTravelDistance);
        DebugProperty::SliderFloat("Turn speed", p.turnSpeed, 1.0f, 20.0f, "%.1f", p.turnSpeed != loaded.turnSpeed);
        DebugProperty::SliderInt("Damage", p.damage, 1, 200, p.damage != loaded.damage);
        DebugProperty::SliderFloat("Window size", p.windowSize, 100.0f, 500.0f, "%.0f px", p.windowSize != loaded.windowSize);
        DebugProperty::SliderFloat("Visual scale", p.visualScale, 0.1f, 20.0f, "%.2f", p.visualScale != loaded.visualScale);
        DebugProperty::SliderFloat("Hitbox radius", p.hitboxRadius, 0.1f, 10.0f, "%.1f units", p.hitboxRadius != loaded.hitboxRadius);
    }

    void DrawSpearRows(UndyneSpearParams& p, const UndyneSpearParams& loaded)
    {
        DebugProperty::SliderInt("Count", p.count, 1, 20, p.count != loaded.count);
        DebugProperty::DragFloat("Spawn delay", p.spawnDelay, 0.05f, 0.05f, 2.0f, "%.2f s", p.spawnDelay != loaded.spawnDelay);
        DebugProperty::DragFloat("Hover duration", p.hoverDuration, 0.05f, 0.1f, 3.0f, "%.2f s", p.hoverDuration != loaded.hoverDuration);
        DebugProperty::DragFloat("Max speed", p.maxSpeed, 1.0f, 10.0f, 300.0f, "%.1f", p.maxSpeed != loaded.maxSpeed);
        DebugProperty::DragFloat("Arc radius", p.arcRadius, 0.5f, 5.0f, 100.0f, "%.1f units", p.arcRadius != loaded.arcRadius);
        DebugProperty::DragFloat2("Arc center", p.arcCenterX, p.arcCenterZ, 0.5f, -50.0f, 50.0f, "%.1f units",
            p.arcCenterX != loaded.arcCenterX || p.arcCenterZ != loaded.arcCenterZ);
        DebugProperty::DragFloatRange("Arc angle range", p.arcMinAngle, p.arcMaxAngle, 1.0f, 0.0f, 360.0f, "%.0f deg",
            p.arcMinAngle != loaded.arcMinAngle || p.arcMaxAngle != loaded.arcMaxAngle);
        DebugProperty::SliderInt("Damage", p.damage, 1, 500, p.damage != loaded.damage);
    }
}

namespace
{
    // Order of the Attacks panel list while the Bullet hell phase is active.
    enum class BulletHellSet
    {
        direct, radial, radialContinuous, fan, fanContinuous, phalanx,
        wave, ultimate, meteor, rainSweep, rainTargeted, count
    };

    constexpr std::array<const char*, 11> k_bulletHellSetNames{
        "Direct", "Radial", "Radial continuous", "Fan", "Fan continuous", "Phalanx",
        "Wave", "Ultimate", "Meteor", "Rain sweep", "Rain targeted" };
    static_assert(k_bulletHellSetNames.size() == static_cast<std::size_t>(BulletHellSet::count),
        "one name per BulletHellSet enumerator");

    bool IsSameColor(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    }

    // Bordered list of set names, sized to show every row. Updates `selected` on click.
    template <std::size_t N>
    void DrawSetList(const std::array<const char*, N>& names, int& selected)
    {
        const float listHeight{ ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(N)
            + ImGui::GetStyle().WindowPadding.y * 2.0f };
        ImGui::BeginChild("##SetList", ImVec2{ 0.0f, listHeight }, true);
        for (int i{ 0 }; i < static_cast<int>(N); ++i)
        {
            if (ImGui::Selectable(names[static_cast<std::size_t>(i)], selected == i)) selected = i;
        }
        ImGui::EndChild();
    }

    // Tail shared by every set: Revert on the Fire line, then the set's rows.
    template <typename Params, typename DrawRows>
    void DrawRevertAndRows(Params& params, const Params& loaded, DrawRows drawRows)
    {
        ImGui::SameLine();
        if (ImGui::Button("Revert")) params = loaded;
        ImGui::Separator();
        drawRows(params, loaded);
    }

    void DrawDirectRows(DirectParams& p, const DirectParams& loaded)
    {
        DebugProperty::SliderInt("Count", p.count, 1, 50, p.count != loaded.count);
        DebugProperty::SliderFloat("Spawn delay", p.spawnDelay, 0.05f, 1.0f, "%.2f s", p.spawnDelay != loaded.spawnDelay);
        DebugProperty::SliderFloat("Speed", p.speed, 10.0f, 100.0f, "%.1f", p.speed != loaded.speed);
    }

    // Used for both Radial and Radial continuous.
    void DrawRadialRows(RadialParams& p, const RadialParams& loaded)
    {
        DebugProperty::ColorEdit4("Color", &p.color.x, !IsSameColor(p.color, loaded.color));
        DebugProperty::SliderFloat("Speed", p.speed, 1.0f, 100.0f, "%.1f", p.speed != loaded.speed);
        DebugProperty::SliderInt("Count", p.count, 4, 128, p.count != loaded.count);
        DebugProperty::SliderFloat("Burst delay", p.burstDelay, 0.01f, 1.0f, "%.2f s", p.burstDelay != loaded.burstDelay);
        DebugProperty::SliderInt("Burst count", p.burstCount, 1, 20, p.burstCount != loaded.burstCount);
        DebugProperty::SliderInt("Damage", p.damage, 1, 100, p.damage != loaded.damage);
    }

    // Used for both Fan and Fan continuous.
    void DrawFanRows(FanParams& p, const FanParams& loaded)
    {
        DebugProperty::SliderFloat("Speed", p.speed, 1.0f, 100.0f, "%.1f", p.speed != loaded.speed);
        DebugProperty::SliderInt("Rows", p.rows, 1, 10, p.rows != loaded.rows);
        DebugProperty::SliderInt("Waves", p.waves, 1, 10, p.waves != loaded.waves);
        DebugProperty::SliderFloat("Spread angle", p.spreadAngle, 0.05f, 0.5f, "%.3f rad", p.spreadAngle != loaded.spreadAngle);
        DebugProperty::SliderInt("Damage", p.damage, 1, 100, p.damage != loaded.damage);
    }

    void DrawPhalanxRows(PhalanxParams& p, const PhalanxParams& loaded)
    {
        DebugProperty::SliderInt("Count", p.count, 3, 10, p.count != loaded.count);
        DebugProperty::SliderFloat("Speed", p.speed, 10.0f, 80.0f, "%.1f", p.speed != loaded.speed);
        DebugProperty::SliderInt("Damage", p.damage, 1, 150, p.damage != loaded.damage);
    }

    void DrawWaveRows(WaveParams& p, const WaveParams& loaded)
    {
        DebugProperty::SliderInt("Waves", p.waves, 1, 20, p.waves != loaded.waves);
        DebugProperty::SliderFloat("Wave delay", p.waveDelay, 0.1f, 3.0f, "%.2f s", p.waveDelay != loaded.waveDelay);
        DebugProperty::SliderFloat("Speed", p.speed, 5.0f, 60.0f, "%.1f", p.speed != loaded.speed);
        DebugProperty::SliderFloat("Track spacing", p.trackSpacing, 1.0f, 10.0f, "%.1f units", p.trackSpacing != loaded.trackSpacing);
        DebugProperty::SliderFloat("Start Z", p.startZ, -30.0f, 0.0f, "%.1f units", p.startZ != loaded.startZ);
    }

    void DrawUltimateRows(UltimateParams& p, const UltimateParams& loaded)
    {
        DebugProperty::SliderFloat("Laser duration", p.laserDuration, 0.5f, 4.0f, "%.2f s", p.laserDuration != loaded.laserDuration);
        DebugProperty::SliderFloat("Shoot speed", p.shootSpeed, 10.0f, 120.0f, "%.1f", p.shootSpeed != loaded.shootSpeed);
        DebugProperty::SliderFloat("Fire warning", p.fireWarningTime, 0.0f, 2.0f, "%.2f s", p.fireWarningTime != loaded.fireWarningTime);
    }

    void DrawMeteorRows(MeteorParams& p, const MeteorParams& loaded)
    {
        DebugProperty::SliderInt("Count", p.count, 1, 20, p.count != loaded.count);
        DebugProperty::SliderFloat("Spawn delay", p.spawnDelay, 0.05f, 2.0f, "%.2f s", p.spawnDelay != loaded.spawnDelay);
        DebugProperty::SliderFloat("Speed", p.speed, 10.0f, 80.0f, "%.1f", p.speed != loaded.speed);
        DebugProperty::SliderFloat("Speed variance", p.speedVariance, 0.0f, 40.0f, "%.1f", p.speedVariance != loaded.speedVariance);
        DebugProperty::SliderFloat("Visual scale", p.visualScale, 0.5f, 10.0f, "%.2f", p.visualScale != loaded.visualScale);
        DebugProperty::SliderFloat("Spread offset", p.spreadOffset, 0.0f, 10.0f, "%.1f units", p.spreadOffset != loaded.spreadOffset);
        DebugProperty::DragFloat2("Start", p.startX, p.startZ, 0.5f, -50.0f, 50.0f, "%.1f units",
            p.startX != loaded.startX || p.startZ != loaded.startZ);
        DebugProperty::DragFloat2("Target", p.targetX, p.targetZ, 0.5f, -50.0f, 50.0f, "%.1f units",
            p.targetX != loaded.targetX || p.targetZ != loaded.targetZ);
    }

    void DrawRainSweepRows(RainParams& p, const RainParams& loaded)
    {
        DebugProperty::DragFloatRange("Speed range", p.minSpeed, p.maxSpeed, 1.0f, 10.0f, 150.0f, "%.1f",
            p.minSpeed != loaded.minSpeed || p.maxSpeed != loaded.maxSpeed);
        DebugProperty::SliderFloat("Active duration", p.activeDuration, 0.5f, 10.0f, "%.2f s", p.activeDuration != loaded.activeDuration);
        DebugProperty::SliderFloat("Damage per second", p.damagePerSecond, 0.0f, 100.0f, "%.1f HP/s", p.damagePerSecond != loaded.damagePerSecond);

    }

    void DrawRainTargetedRows(RainParams& p, const RainParams& loaded)
    {
        DebugProperty::DragFloatRange("Speed range", p.minSpeed, p.maxSpeed, 1.0f, 10.0f, 150.0f, "%.1f",
            p.minSpeed != loaded.minSpeed || p.maxSpeed != loaded.maxSpeed);
        DebugProperty::SliderFloat("Warning duration", p.warningDuration, 0.1f, 3.0f, "%.2f s", p.warningDuration != loaded.warningDuration);
        DebugProperty::SliderFloat("Active duration", p.activeDuration, 0.1f, 5.0f, "%.2f s", p.activeDuration != loaded.activeDuration);
        DebugProperty::SliderFloat("Zone width", p.width, 2.0f, 50.0f, "%.1f units", p.width != loaded.width);
        DebugProperty::SliderFloat("Zone depth", p.depth, 10.0f, 80.0f, "%.1f units", p.depth != loaded.depth);
        DebugProperty::SliderInt("Trigger count", p.triggerCount, 1, 10, p.triggerCount != loaded.triggerCount);
        DebugProperty::SliderFloat("Trigger delay", p.triggerDelay, 0.1f, 3.0f, "%.2f s", p.triggerDelay != loaded.triggerDelay);
        DebugProperty::SliderFloat("Damage per second", p.damagePerSecond, 0.0f, 100.0f, "%.1f HP/s", p.damagePerSecond != loaded.damagePerSecond);

    }
}

// Attacks panel: list of the active phase's parameter sets on top, the selected set below
// with its Fire and Revert buttons. Edits are not saved; the file is edited by hand and reloaded.
// Must not call ImGui::Begin / End.
void SceneBoss::DrawAttacksPanel()
{
    // Why: once the scene change is pending, phase and window state are being torn down.
    if (m_isPendingSceneChange) return;

    if (!m_cue)
    {
        ImGui::TextDisabled("Boss not created.");
        return;
    }

    AttackParamManager& manager{ AttackParamManager::Instance() };

    // Attacks already running keep their own copy; only new ones use the reloaded values.
    if (ImGui::Button("Reload AttackParams.json"))
    {
        const bool isLoaded{ manager.Reload() };
        AddLog(isLoaded ? "AttackParams.json reloaded."
            : "AttackParams.json reload failed, params unchanged.");
    }

    if (auto * bulletHell{ dynamic_cast<BossPhase01*>(m_cue->GetCurrentPhase()) })
    {
        DrawBulletHellAttacks(*bulletHell);
        return;
    }

    auto* windowkill{ dynamic_cast<BossPhase02*>(m_cue->GetCurrentPhase()) };
    if (!windowkill)
    {
        ImGui::TextDisabled("No active phase.");
        return;
    }

    const AttackParamSet& loaded{ manager.GetLoadedParams() };

    // ---- Master: set list ----
    DrawSetList(k_windowkillSetNames, m_selectedWindowkillSet);

    // ---- Detail: selected set ----
    const char* setName{ k_windowkillSetNames[static_cast<std::size_t>(m_selectedWindowkillSet)] };
    ImGui::PushID(setName); // sets share row labels such as "Damage"
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(setName);
    ImGui::SameLine();

    switch (static_cast<WindowkillSet>(m_selectedWindowkillSet))
    {
    case WindowkillSet::blasters:
    {
        BlasterParams& params{ manager.GetBlasterParams() };
        if (ImGui::Button("Fire random"))
        {
            windowkill->AddAttack(std::make_unique<AttackBlasters>(params, false));
        }
        ImGui::SameLine();
        if (ImGui::Button("Fire targeted"))
        {
            const float playerX{ m_player ? m_player->GetPosition().x : 0.0f };
            windowkill->AddAttack(std::make_unique<AttackBlasters>(params, true, playerX));
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert")) params = loaded.blaster;
        ImGui::Separator();
        DrawBlasterRows(params, loaded.blaster);
        break;
    }
    case WindowkillSet::bouncing:
    {
        BouncingBulletParams& params{ manager.GetBouncingParams() };
        if (ImGui::Button("Fire")) windowkill->AddAttack(std::make_unique<AttackBouncing>(params));
        ImGui::SameLine();
        if (ImGui::Button("Revert")) params = loaded.bouncing;
        ImGui::Separator();
        DrawBouncingRows(params, loaded.bouncing);
        break;
    }
    case WindowkillSet::boomerangs:
    {
        BoomerangParams& params{ manager.GetBoomerangParams() };
        if (ImGui::Button("Fire")) windowkill->AddAttack(std::make_unique<AttackBoomerangs>(params));
        ImGui::SameLine();
        if (ImGui::Button("Revert")) params = loaded.boomerang;
        ImGui::Separator();
        DrawBoomerangRows(params, loaded.boomerang);
        break;
    }
    case WindowkillSet::spears:
    {
        UndyneSpearParams& params{ manager.GetUndyneParams() };
        if (ImGui::Button("Fire")) windowkill->AddAttack(std::make_unique<AttackSpears>(params, m_player.get()));
        ImGui::SameLine();
        if (ImGui::Button("Revert")) params = loaded.spear;
        ImGui::Separator();
        DrawSpearRows(params, loaded.spear);
        break;
    }
    case WindowkillSet::count:
        break;
    }

    ImGui::PopID();
}

        // Bullet hell half of the Attacks panel. Fire buttons do what the old manual triggers did;
// they are not guaranteed to match how BossAI_Phase01 launches the same attack.
void SceneBoss::DrawBulletHellAttacks(BossPhase01& phase)
{
    AttackParamManager& manager{ AttackParamManager::Instance() };
    const AttackParamSet& loaded{ manager.GetLoadedParams() };
    // ---- Master: set list ----
    DrawSetList(k_bulletHellSetNames, m_selectedBulletHellSet);
    // ---- Detail: selected set ----
    const char* setName{ k_bulletHellSetNames[static_cast<std::size_t>(m_selectedBulletHellSet)] };
    ImGui::PushID(setName); // sets share row labels such as "Speed"
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(setName);
    ImGui::SameLine();
    // Angle from boss to player at the moment of the click, for the two Fan sets.
    const auto getAngleToPlayer{ [this]() {
        const DirectX::XMFLOAT3 playerPos{ m_player->GetPosition() };
        const DirectX::XMFLOAT3 bossPos{ m_cue->GetPosition() };
        return std::atan2(playerPos.x - bossPos.x, playerPos.z - bossPos.z);
    } };
    switch (static_cast<BulletHellSet>(m_selectedBulletHellSet))
    {
    case BulletHellSet::direct:
    {
        DirectParams& params{ manager.GetDirectParams() };
        if (ImGui::Button("Fire") && m_player) phase.AddAttack(std::make_unique<AttackDirect>(params, m_player.get()));
        DrawRevertAndRows(params, loaded.direct, DrawDirectRows);
        break;
    }
    case BulletHellSet::radial:
    {
        RadialParams& params{ manager.GetRadialNormalParams() };
        if (ImGui::Button("Fire")) phase.AddAttack(std::make_unique<AttackRadial>(params));
        DrawRevertAndRows(params, loaded.radialNormal, DrawRadialRows);
        break;
    }
    case BulletHellSet::radialContinuous:
    {
        RadialParams& params{ manager.GetRadialContinuousParams() };
        if (ImGui::Button("Fire")) phase.AddAttack(std::make_unique<AttackRadial>(params));
        DrawRevertAndRows(params, loaded.radialContinuous, DrawRadialRows);
        break;
    }
    case BulletHellSet::fan:
    {
        FanParams& params{ manager.GetFanNormalParams() };
        if (ImGui::Button("Fire") && m_player) phase.AddAttack(std::make_unique<AttackFan>(params, getAngleToPlayer()));
        DrawRevertAndRows(params, loaded.fanNormal, DrawFanRows);
        break;
    }
    case BulletHellSet::fanContinuous:
    {
        FanParams& params{ manager.GetFanContinuousParams() };
        if (ImGui::Button("Fire") && m_player)
        {
            phase.AddAttack(std::make_unique<AttackFan>(params, getAngleToPlayer(), m_player.get()));
        }
        DrawRevertAndRows(params, loaded.fanContinuous, DrawFanRows);
        break;
    }
    case BulletHellSet::phalanx:
    {
        PhalanxParams& params{ manager.GetPhalanxParams() };
        if (ImGui::Button("Fire") && m_player) phase.AddPhalanx(std::make_unique<AttackPhalanx>(params, m_player.get()));        DrawRevertAndRows(params, loaded.phalanx, DrawPhalanxRows);
        break;
    }
    case BulletHellSet::wave:
    {
        WaveParams& params{ manager.GetWaveParams() };
        if (ImGui::Button("Fire")) phase.AddAttack(std::make_unique<AttackWave>(params));
        DrawRevertAndRows(params, loaded.wave, DrawWaveRows);
        break;
    }
    case BulletHellSet::ultimate:
    {
        UltimateParams& params{ manager.GetUltimateParams() };
        if (ImGui::Button("Fire") && m_player) phase.AddUltimate(std::make_unique<AttackUltimate>(params, m_player.get()));
        DrawRevertAndRows(params, loaded.ultimate, DrawUltimateRows);
        break;
    }
    case BulletHellSet::meteor:
    {
        MeteorParams& params{ manager.GetMeteorParams() };
        if (ImGui::Button("Fire")) phase.AddAttack(std::make_unique<AttackMeteor>(params));
        DrawRevertAndRows(params, loaded.meteor, DrawMeteorRows);
        break;
    }
    case BulletHellSet::rainSweep:
    {
        RainParams& params{ manager.GetRainParams() };
        if (ImGui::Button("Fire left")) phase.TriggerRain(params, RainMode::VerticalSweep, false);
        ImGui::SameLine();
        if (ImGui::Button("Fire right")) phase.TriggerRain(params, RainMode::VerticalSweep, true);
        DrawRevertAndRows(params, loaded.rain, DrawRainSweepRows);
        break;
    }
    case BulletHellSet::rainTargeted:
    {
        RainParams& params{ manager.GetRainTargetedParams() };
        if (ImGui::Button("Fire")) phase.TriggerRain(params, RainMode::Targeted, true);
        DrawRevertAndRows(params, loaded.rainTargeted, DrawRainTargetedRows);
        break;
    }
    case BulletHellSet::count:
        break;
    }
    ImGui::PopID();
}

namespace
{
    const char* ToLabel(WindowRole role)
    {
        switch (role)
        {
        case WindowRole::MAIN_VIEWPORT:  return "main";
        case WindowRole::TRACKED_ENTITY: return "entity";
        case WindowRole::SUB_VIEWPORT:   return "sub";
        }
        return "unknown";
    }
    // True for windows created by SpawnDebugWindow / SpawnTransparentWindow.
    bool IsTestWindowName(const std::string& name)
    {
        return name.rfind("debug_win_", 0) == 0 || name.rfind("trans_", 0) == 0;
    }
}
// Windows panel: read-only view of every tracked OS window, plus test-window spawning.
// Must not call ImGui::Begin / End.
void SceneBoss::DrawWindowsPanel()
{
    // Why: once the scene change is pending, phase and window state are being torn down.
    if (m_isPendingSceneChange) return;
    if (!m_windowSystem)
    {
        ImGui::TextDisabled("Window tracking system not created.");
        return;
    }
    // ---- Head ----
    const auto& windows{ m_windowSystem->GetWindows() };
    const int activeCount{ static_cast<int>(std::count_if(windows.begin(), windows.end(),
        [](const auto& tracked) { return tracked->isActive; })) };
    const int pooledCount{ static_cast<int>(windows.size()) - activeCount };
    DebugProperty::Text("Windows", "%d active, %d pooled", activeCount, pooledCount);
    DebugProperty::Text("Player window", "%s", m_playerWindowTransparent ? "FX layer" : "own window");
    // ---- Table ----
    ImGui::Columns(4, "##TrackedWindows", true);
    ImGui::TextUnformatted("Name");         ImGui::NextColumn();
    ImGui::TextUnformatted("Role");         ImGui::NextColumn();
    ImGui::TextUnformatted("Desktop rect"); ImGui::NextColumn();
    ImGui::TextUnformatted("State");        ImGui::NextColumn();
    ImGui::Separator();
    for (const auto& tracked : windows)
    {
        ImGui::TextDisabled("%s", tracked->name.c_str());
        ImGui::NextColumn();
        ImGui::TextDisabled("%s", ToLabel(tracked->role));
        ImGui::NextColumn();
        if (tracked->isActive)
        {
            const WindowState& state{ tracked->state };
            ImGui::TextDisabled("%d, %d  %d x %d", state.actualX, state.actualY, state.actualW, state.actualH);
        }
        else
        {
            ImGui::TextDisabled("-"); // a pooled window is hidden; its last rect means nothing
        }
        ImGui::NextColumn();
        ImGui::TextDisabled("%s%s", tracked->isActive ? "active" : "pooled",
            tracked->isTransparent ? ", transparent" : "");
        ImGui::NextColumn();
    }
    ImGui::Columns(1);
    // ---- Test windows ----
    ImGui::PushID("TestWindows");
    if (ImGui::CollapsingHeader("Test windows"))
    {
        if (ImGui::Button("Spawn bordered")) SpawnDebugWindow();
        if (ImGui::Button("Spawn transparent (alpha 0)")) SpawnTransparentWindow(0.0f, "Hollow");
        if (ImGui::Button("Spawn transparent (alpha 1/255)")) SpawnTransparentWindow(1.0f / 255.0f, "Solid");
        // Red: the only destructive action in the debug panels.
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.6f, 0.1f, 0.1f, 1.0f });
        const bool isCloseClicked{ ImGui::Button("Close test windows") };
        ImGui::PopStyleColor();
        if (isCloseClicked) CloseTestWindows();
    }
    ImGui::PopID();
}
// Returns every window made by the two Spawn helpers to the pool. Game windows (player,
// boss head, FX layer, attack windows) are left alone, unlike WindowTrackingSystem::ClearAll().
void SceneBoss::CloseTestWindows()
{
    if (!m_windowSystem) return;
    // Why collect first: RemoveTrackedWindow renames the entry it pools.
    std::vector<std::string> testWindowNames;
    for (const auto& tracked : m_windowSystem->GetWindows())
    {
        if (!tracked->isActive || !IsTestWindowName(tracked->name)) continue;
        // Why: a pooled window is reused by attacks, and reuse does not reset the border
        // the Spawn helpers turned on.
        if (tracked->window)
        {
            if (SDL_Window * sdlWindow{ tracked->window->GetSDLWindow() }) SDL_SetWindowBordered(sdlWindow, false);
            tracked->window->SetBorderVisible(false);
        }
        testWindowNames.push_back(tracked->name);
    }
    for (const std::string& name : testWindowNames)
    {
        m_windowSystem->RemoveTrackedWindow(name);
    }
    m_spawnCount = 0;
    AddLog("Closed " + std::to_string(testWindowNames.size()) + " test window(s).");
}

void SceneBoss::DrawLogPanel()
{
    if (ImGui::Button("Clear")) m_debugLogs.clear();

    ImGui::BeginChild("##LogRegion", ImVec2{ 0.0f, 0.0f }, true);
    for (const std::string& line : m_debugLogs)
    {
        ImGui::TextUnformatted(line.c_str());
    }
    // Why the check: follow new lines only while already at the bottom, so scrolling up to read stays put.
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

// Menu bar entry: debug overlays drawn into the game image.
// Menu bar callbacks may only submit BeginMenu, MenuItem and text.
void SceneBoss::DrawViewMenu()
{
    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Show grid", nullptr, &m_showGrid);
        ImGui::MenuItem("Show hitboxes", nullptr, &m_showHitboxes);
        ImGui::EndMenu();
    }
}

// =========================================================
// DEBUG / SYSTEM HELPERS
// =========================================================

void SceneBoss::AddLog(const std::string& message)
{
    // Wall-clock stamp in the same format as PerformanceLogger, so both logs can be matched.
    const std::time_t now{ std::time(nullptr) };
    std::tm localTime{};
    localtime_s(&localTime, &now);
    std::array<char, 16> stamp{};
    std::strftime(stamp.data(), stamp.size(), "[%H:%M:%S] ", &localTime);

    m_debugLogs.push_back(stamp.data() + message);
    if (m_debugLogs.size() > 50)
        m_debugLogs.erase(m_debugLogs.begin());
}

    void SceneBoss::ResetLevel()
    {
        // Reset Player State (Safe check prevents crashes)
        if (m_player)
        {
            m_player->SetPosition(0.0f, 0.0f, -8.0f);
            m_player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
            m_player->RefillHP();
            m_player->SetInputEnabled(false);
            m_player->m_scale = { 1.0f, 1.0f, 1.0f };

            if (m_player->GetStateMachine()) {
                m_player->GetStateMachine()->ChangeState(m_player.get(), std::make_unique<PlayerIdle>());
            }

            m_player->GetProjectiles().Clear();
            m_player->RestoreShootDelay();
            m_player->SetAimLocked(false);
        }

        // Reset Boss 
        if (m_cue)
        {
            if (auto* normalPhase = dynamic_cast<BossPhase01*>(m_cue->GetCurrentPhase()))
            {
                normalPhase->SetHP(normalPhase->GetMaxHP());
                m_playerWindowTransparent = false; // Normal mode = solid player
            }
            else if (auto* wkPhase = dynamic_cast<BossPhase02*>(m_cue->GetCurrentPhase()))
            {
                wkPhase->SetHP(wkPhase->GetMaxHP());
                m_playerWindowTransparent = true;  // Windowkill mode = transparent player
            }
        }

        // Clean up the Windowkill environment
        WindowShatterManager::Instance().Clear();

        // Smart Camera Reset (Instantly snaps during the black screen)
        CameraController::Instance().SetDynamicZoomOffset(0.0f);
        float unifiedHeight = m_windowSystem ? m_windowSystem->GetUnifiedCameraHeight() : 18.0f;

        CameraController::Instance().SetFixedSetting(DirectX::XMFLOAT3(0.0f, unifiedHeight, 0.0f));
        CameraController::Instance().SetTarget({ 0.0f, 0.0f, 0.0f });

        // Force the camera math to finish instantly
        for (int i = 0; i < 60; ++i)
        {
            CameraController::Instance().Update(0.016f);
        }
    }

void SceneBoss::SpawnDebugWindow()
{
    m_spawnCount++;

    TrackedWindowConfig config;
    config.name = "debug_win_" + std::to_string(m_spawnCount);
    config.title = "D" + std::to_string(m_spawnCount) + " (drag/stretch me!)";
    config.width = 300;
    config.height = 300;
    config.role = WindowRole::SUB_VIEWPORT;

    m_windowSystem->AddTrackedWindow(config,
        []() { return XMFLOAT3(0.0f, 0.0f, 0.0f); });

    TrackedWindow* tracked = m_windowSystem->GetTrackedWindow(config.name);
    if (tracked && tracked->window)
    {
        SDL_SetWindowBordered(tracked->window->GetSDLWindow(), true);
    }

    AddLog("Spawned portal: " + config.name);
    WindowManager::Instance().EnforceWindowPriorities();
}

void SceneBoss::SpawnTransparentWindow(float bgAlpha, const std::string& typeSuffix)
{
    m_spawnCount++;

    TrackedWindowConfig config;
    config.name = "trans_" + typeSuffix + "_" + std::to_string(m_spawnCount);
    config.title = "T-" + typeSuffix + " " + std::to_string(m_spawnCount);
    config.width = 300;
    config.height = 300;
    config.role = WindowRole::SUB_VIEWPORT;
    config.isTransparent = true;

    m_windowSystem->AddTrackedWindow(config,
        []() { return XMFLOAT3(0.0f, 0.0f, 0.0f); });

    TrackedWindow* tracked = m_windowSystem->GetTrackedWindow(config.name);
    if (tracked && tracked->window)
    {
        tracked->window->SetBackgroundAlpha(bgAlpha);
        tracked->window->SetBorderVisible(true);
    }

    AddLog("Spawned " + typeSuffix + " window (alpha: " + std::to_string(bgAlpha) + ")");
}

void SceneBoss::RequestReturnToTitle()
{
    m_isPendingSceneChange = true;
    Framework::Instance()->ChangeScene(Scene::Kind::title);
}

void SceneBoss::OnSubWindowClosed(std::uint32_t sdlWindowID)
{
    if (!m_windowSystem) return;

    for (const auto& tracked : m_windowSystem->GetWindows())
    {
        if (tracked->window &&
            SDL_GetWindowID(tracked->window->GetSDLWindow()) == sdlWindowID)
        {
            const std::string name = tracked->name;
            m_windowSystem->RemoveTrackedWindow(name);
            AddLog("Window closed: " + name);
            return;
        }
    }
}