#include "WindowShatter.h"
#include "WindowManager.h"
#include <SDL3/SDL.h>
#include <random>
#include <algorithm>
#include <windows.h> 
#include "Framework.h"
#include "Engine/Common/Constants.h"
#include "Engine/Common/FitRect.h"

namespace
{
    // Desktop rect the arena maps onto: the same rect WindowTrackingSystem places its
    // windows against, so shatter windows line up with them.
    Beyond::PixelRect GetArenaRect()
    {
        const Framework* framework{ Framework::Instance() };
        const Beyond::PixelRect rect{ framework ? framework->GetGameImageRect() : Beyond::PixelRect{} };
        if (rect.width > 0 && rect.height > 0)
        {
            return rect;
        }
        // Keeps the math valid if neither the window nor the display can be queried.
        return Beyond::PixelRect{ 0, 0, Beyond::Config::CANVAS_WIDTH, Beyond::Config::CANVAS_HEIGHT };
    }

    // Desktop pixels per canvas pixel. Sizes and speeds in this file are canvas pixels.
    float GetDesktopScale(const Beyond::PixelRect& arenaRect)
    {
        return static_cast<float>(arenaRect.height) / static_cast<float>(Beyond::Config::CANVAS_HEIGHT);
    }
}

WindowShatter::WindowShatter(const char* title, DirectX::XMFLOAT2 startPos, DirectX::XMFLOAT2 velocity, int size, int priority)
    : m_width(static_cast<float>(size))
    , m_height(static_cast<float>(size))
    , m_title(title)
{
    m_isNativeWindow = false;
    m_window = nullptr;
    m_virtualWorldPos = { startPos.x, 0.0f, startPos.y };

    m_physics.velocity = velocity;
    m_physics.deceleration = 0.98f;
}

WindowShatter::~WindowShatter()
{
    m_window = nullptr;
}

void WindowShatter::Cleanup()
{
    if (m_window && !m_preparedForDestroy)
    {
        WindowManager::Instance().DestroyWindow(m_window);
        m_window = nullptr;
        m_preparedForDestroy = true;
    }
}

// [TAMBAH] Implementasi Update yang hilang
void WindowShatter::Update(float dt)
{
    if (m_markedForDestroy) return;
    
    if (m_isSleeping && m_isNativeWindow) return;

    m_timeAlive += dt;

    if (!m_isNativeWindow)
    {
        UpdateVirtualState(dt);
    }
    else
    {
        UpdateNativeState(dt);
    }
}

void WindowShatter::UpdateVirtualState(float dt)
{
    float worldSpeedX = m_physics.velocity.x / PIXEL_TO_UNIT_RATIO;
    float worldSpeedZ = m_physics.velocity.y / PIXEL_TO_UNIT_RATIO;

    m_virtualWorldPos.x += worldSpeedX * dt;
    m_virtualWorldPos.z += worldSpeedZ * dt;

    if (m_timeAlive > 0.05f)
    {
        TransitionToNativeWindow();
    }
}

void WindowShatter::UpdateNativeState(float dt)
{
    if (!m_window) return;

    m_physics.velocity.x *= m_physics.deceleration;
    m_physics.velocity.y *= m_physics.deceleration;

    try {
        // [WRAPPED] Protect SDL calls
        const float desktopScale{ GetDesktopScale(GetArenaRect()) };

        int curX, curY;
        SDL_GetWindowPosition(m_window->GetSDLWindow(), &curX, &curY);

        int nextX = static_cast<int>(roundf(curX + m_physics.velocity.x * desktopScale * dt));
        int nextY = static_cast<int>(roundf(curY + m_physics.velocity.y * desktopScale * dt));

        if (nextX != curX || nextY != curY)
        {
            SDL_SetWindowPosition(m_window->GetSDLWindow(), nextX, nextY);
        }

        float shrinkAmount = m_shrinkRate * dt;
        m_width -= shrinkAmount;
        m_height -= shrinkAmount;

        if (m_width < 10.0f) m_width = 10.0f;
        if (m_height < 10.0f) m_height = 10.0f;

        SDL_SetWindowSize(m_window->GetSDLWindow(),
            static_cast<int>(m_width * desktopScale), static_cast<int>(m_height * desktopScale));

        int realW, realH;
        SDL_GetWindowSize(m_window->GetSDLWindow(), &realW, &realH);
        m_window->Resize(realW, realH);

        if (m_width <= 100.0f)
        {
            m_markedForDestroy = true;
        }

        EnforceScreenBounds();
    }
    catch (...) {
        // [FAILSAFE] Mark for destruction if SDL fails
        m_markedForDestroy = true;
    }
}

// [TAMBAH] Implementasi TransitionToNativeWindow yang hilang
void WindowShatter::TransitionToNativeWindow()
{
    float screenX, screenY;
    ConvertWorldToScreen(m_virtualWorldPos, screenX, screenY);

    const float desktopScale{ GetDesktopScale(GetArenaRect()) };
    const float desktopWidth{ m_width * desktopScale };
    const float desktopHeight{ m_height * desktopScale };

    m_window = WindowManager::Instance().CreateGameWindow(m_title.c_str(), (int)desktopWidth, (int)desktopHeight);

    if (m_window)
    {

        if (m_isSleeping) {
            SDL_HideWindow(m_window->GetSDLWindow());
        }

        m_window->SetPriority(-1);
        m_window->SetAlwaysOnTop(true);

        // Border Aktif
        SDL_SetWindowBordered(m_window->GetSDLWindow(), true);

        int finalX = (int)(screenX - (desktopWidth * 0.5f));
        int finalY = (int)(screenY - (desktopHeight * 0.5f));

        SDL_SetWindowPosition(m_window->GetSDLWindow(), finalX, finalY);

        m_camera = std::make_shared<Camera>();
        m_camera->SetRotation(90.0f, 0.0f, 0.0f);
        m_window->SetCamera(m_camera.get());

        m_isNativeWindow = true;
    }
    else
    {
        m_markedForDestroy = true;
    }
}

void WindowShatter::WakeUp()
{
    m_isSleeping = false;
    if (m_window) {
        // Why re-placed here: the window was created hidden seconds ago, and the arena
        // rect may have moved or changed size since (main window dragged or resized).
        float screenX, screenY;
        ConvertWorldToScreen(m_virtualWorldPos, screenX, screenY);

        const float desktopScale{ GetDesktopScale(GetArenaRect()) };
        const float desktopWidth{ m_width * desktopScale };
        const float desktopHeight{ m_height * desktopScale };

        SDL_SetWindowSize(m_window->GetSDLWindow(), (int)desktopWidth, (int)desktopHeight);
        SDL_SetWindowPosition(m_window->GetSDLWindow(),
            (int)(screenX - (desktopWidth * 0.5f)), (int)(screenY - (desktopHeight * 0.5f)));

        SDL_ShowWindow(m_window->GetSDLWindow());
    }
}

void WindowShatterManager::PreloadExplosion(DirectX::XMFLOAT2 centerWorldPos, int count)
{
    for (int i = 0; i < count; ++i)
    {
        SpawnSingleInstance(centerWorldPos, i, count);
        // Paksa shatter yang baru di-spawn masuk mode tidur
        m_shatters.back()->SetSleeping(true);
    }
}

void WindowShatterManager::WakeUpAll()
{
    for (auto& shatter : m_shatters)
    {
        if (shatter->IsSleeping()) {
            shatter->WakeUp();
        }
    }
}

void WindowShatter::EnforceScreenBounds()
{
    if (!m_window) return;
    int x, y;
    SDL_GetWindowPosition(m_window->GetSDLWindow(), &x, &y);

    // Ambil ukuran FISIK real (karena mungkin beda sama m_width logika)
    int realW, realH;
    SDL_GetWindowSize(m_window->GetSDLWindow(), &realW, &realH);

    bool bounced = false;

    // Shards bounce off the edges of the arena rect, like every other sub-window.
    const Beyond::PixelRect arena{ GetArenaRect() };
    const int left{ arena.x };
    const int top{ arena.y };
    const int right{ arena.x + arena.width };
    const int bottom{ arena.y + arena.height };

    if (x <= left) { x = left; m_physics.velocity.x *= -m_physics.bounceDamping; bounced = true; }
    else if (x + realW >= right) { x = right - realW; m_physics.velocity.x *= -m_physics.bounceDamping; bounced = true; }

    if (y <= top) { y = top; m_physics.velocity.y *= -m_physics.bounceDamping; bounced = true; }
    else if (y + realH >= bottom) { y = bottom - realH; m_physics.velocity.y *= -m_physics.bounceDamping; bounced = true; }

    if (bounced) {
        SDL_SetWindowPosition(m_window->GetSDLWindow(), x, y);
        m_physics.bounceCount++;
    }
}

void WindowShatter::ConvertWorldToScreen(const DirectX::XMFLOAT3& worldPos, float& outX, float& outY) const
{
    // Same mapping as WindowTrackingSystem::WorldToScreenPos: world origin at the
    // centre of the arena rect.
    const Beyond::PixelRect arena{ GetArenaRect() };
    const float desktopPixelsPerUnit{ PIXEL_TO_UNIT_RATIO * GetDesktopScale(arena) };

    outX = static_cast<float>(arena.x) + (static_cast<float>(arena.width) * 0.5f) + (worldPos.x * desktopPixelsPerUnit);
    outY = static_cast<float>(arena.y) + (static_cast<float>(arena.height) * 0.5f) - (worldPos.z * desktopPixelsPerUnit);
}

// =========================================================
// MANAGER IMPLEMENTATION
// =========================================================

void WindowShatterManager::TriggerExplosion(DirectX::XMFLOAT2 centerWorldPos, int count)
{
    for (int i = 0; i < count; ++i)
    {
        SpawnSingleInstance(centerWorldPos, i, count);
    }
}

void WindowShatterManager::Update(float dt)
{
    // 1. Update semua shatter
    for (auto& shatter : m_shatters)
    {
        shatter->Update(dt);
    }

    // 2. [BARU] Cleanup window SEBELUM erase
    for (auto& shatter : m_shatters)
    {
        if (shatter->ShouldDestroy())
        {
            shatter->Cleanup(); // Safe cleanup window dulu
        }
    }

    // 3. Baru hapus dari container (safe karena window sudah di-cleanup)
    m_shatters.erase(
        std::remove_if(m_shatters.begin(), m_shatters.end(),
            [](const std::unique_ptr<WindowShatter>& s) {
                return s->IsPreppedForDestroy();
            }),
        m_shatters.end()
    );
}

void WindowShatterManager::SpawnSingleInstance(DirectX::XMFLOAT2 centerPos, int index, int totalCount)
{
    static std::mt19937 gen(std::random_device{}());

    // 1. Angle & Speed
    float angleStep = 360.0f / totalCount;
    std::uniform_real_distribution<float> jitterAngle(-10.0f, 10.0f);
    float angleRad = DirectX::XMConvertToRadians((index * angleStep) + jitterAngle(gen));

    // Speed kencang (800-1200)
    std::uniform_real_distribution<float> speedDist(4000.0f, 5000.0f);
    float speed = speedDist(gen);
    DirectX::XMFLOAT2 velocity = { cosf(angleRad) * speed, sinf(angleRad) * speed };

    // 2. Position Offset
    std::uniform_real_distribution<float> offsetDist(-2.0f, 2.0f);
    DirectX::XMFLOAT2 spawnPos = { centerPos.x + offsetDist(gen), centerPos.y + offsetDist(gen) };

    // 3. Size (Agak besar supaya kelihatan mengecilnya)
    int size = static_cast<int>(std::uniform_real_distribution<float>(200.0f, 400.0f)(gen));

    // 4. Unique Name
    char title[64];
    sprintf_s(title, "Shatter_%u_%d", SDL_GetTicks(), index);

    m_shatters.push_back(std::make_unique<WindowShatter>(title, spawnPos, velocity, size, 1));
}

void WindowShatterManager::Clear()
{
    m_shatters.clear();
}