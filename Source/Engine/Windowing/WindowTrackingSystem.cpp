#include "WindowTrackingSystem.h"
#include "WindowManager.h"
#include <cmath>
#include "PerformanceLogger.h"
#include <CameraController.h>
#include <SDL3/SDL.h>

using namespace DirectX;

WindowTrackingSystem::WindowTrackingSystem()
{
}

static_assert(static_cast<float>(Beyond::Config::CANVAS_HEIGHT)
    == Beyond::Config::ARENA_HEIGHT_UNITS * Beyond::Config::PIXEL_TO_UNIT_RATIO,
    "canvas height, arena height and pixel-to-unit ratio are out of sync");

void WindowTrackingSystem::SetArenaRect(const Beyond::PixelRect& rect)
{
    if (rect.width <= 0 || rect.height <= 0) return;

    m_arenaRect = rect;
    m_desktopScale = static_cast<float>(rect.height) / static_cast<float>(Beyond::Config::CANVAS_HEIGHT);
}

WindowTrackingSystem::~WindowTrackingSystem()
{
    ClearAll();
}

void WindowTrackingSystem::ClearAll()
{
    for (auto it = m_trackedWindows.begin(); it != m_trackedWindows.end(); )
    {
        // The main viewport window is never destroyed here; only sub-windows are.
        if ((*it)->role != WindowRole::MAIN_VIEWPORT)
        {
            if ((*it)->window) {
                WindowManager::Instance().DestroyWindow((*it)->window);
            }
            m_windowLookup.erase((*it)->name);
            it = m_trackedWindows.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool WindowTrackingSystem::AddTrackedWindow(
    const TrackedWindowConfig& config,
    std::function<DirectX::XMFLOAT3()> getTargetPos,
    std::function<DirectX::XMFLOAT2()> getTargetSize
)
{
    auto start = std::chrono::high_resolution_clock::now();

    // =========================================================
    // [1] Reuse an inactive window with the same transparency
    // =========================================================
    for (auto& tw : m_trackedWindows) {
        if (!tw->isActive && tw->isTransparent == config.isTransparent) {
            tw->isActive = true;
            tw->name = config.name;
            tw->role = config.role;
            tw->trackingOffset = config.trackingOffset;
            tw->getTargetPositionFunc = getTargetPos;
            tw->getTargetSizeFunc = getTargetSize;

            // target* is the authored size in canvas pixels, actual* the OS size.
            tw->state.targetW = (float)config.width;
            tw->state.targetH = (float)config.height;
            tw->state.actualW = ToDesktopPixels(tw->state.targetW);
            tw->state.actualH = ToDesktopPixels(tw->state.targetH);

            if (tw->window && tw->window->GetSDLWindow()) {
                SDL_Window* sdlWin = tw->window->GetSDLWindow();
                SDL_SetWindowTitle(sdlWin, config.title.c_str());
                SDL_SetWindowSize(sdlWin, tw->state.actualW, tw->state.actualH);
                tw->window->SetBackgroundAlpha(config.isTransparent ? 0.0f : 1.0f);
                tw->window->SetPriority(config.priority);
                tw->window->SetAlwaysOnTop(config.isAlwaysOnTop);

                if (config.fpsLimit > 0.0f) tw->window->SetTargetFPS(config.fpsLimit);
                tw->window->LockUserMoveAndResize();

                // Place the window before showing it, so it does not flash at the -10000 parking position.
                if (getTargetPos) {
                    DirectX::XMFLOAT3 initialPos = getTargetPos();
                    float screenX, screenY;
                    WorldToScreenPos(initialPos, screenX, screenY);
                    tw->state.targetX = screenX - (tw->state.actualW * 0.5f);
                    tw->state.targetY = screenY - (tw->state.actualH * 0.5f);
                    tw->state.actualX = static_cast<int>(roundf(tw->state.targetX));
                    tw->state.actualY = static_cast<int>(roundf(tw->state.targetY));
                    SDL_SetWindowPosition(sdlWin, tw->state.actualX, tw->state.actualY);
                }

                SDL_ShowWindow(sdlWin);
            }

            m_windowLookup[config.name] = tw.get();
            WindowManager::Instance().MarkPriorityDirty();

            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float, std::milli> duration = end - start;
            PerformanceLogger::Instance().LogWindowAction("Pooled", config.name, duration.count());

            return true;
        }
    }

    // =========================================================
    // [2] Pool is empty: create a new window
    // =========================================================
    // 1. Create Window via Singleton Manager
    Beyond::Window* window = WindowManager::Instance().CreateGameWindow(
        config.title.c_str(),
        config.width,
        config.height,
        config.isTransparent
    );

    if (!window) return false;

    window->SetPriority(config.priority);
    window->SetAlwaysOnTop(config.isAlwaysOnTop);
    SDL_ShowWindow(window->GetSDLWindow());
    WindowManager::Instance().MarkPriorityDirty();

    if (config.fpsLimit > 0.0f)
    {
        window->SetTargetFPS(config.fpsLimit);
    }

    window->LockUserMoveAndResize();

    // 2. Create Camera for this window
    auto camera = std::make_shared<Camera>();
    window->SetCamera(camera.get());

    // 3. Setup Tracked Object
    auto tracked = std::make_unique<TrackedWindow>();
    tracked->isTransparent = config.isTransparent;
    tracked->name = config.name;
    tracked->window = window;
    tracked->role = config.role;
    tracked->camera = camera;
    tracked->trackingOffset = config.trackingOffset;
    tracked->getTargetPositionFunc = getTargetPos;
    tracked->getTargetSizeFunc = getTargetSize;

    // target* is the authored size in canvas pixels, actual* the OS size.
    tracked->state.targetW = (float)config.width;
    tracked->state.targetH = (float)config.height;
    tracked->state.actualW = ToDesktopPixels(tracked->state.targetW);
    tracked->state.actualH = ToDesktopPixels(tracked->state.targetH);
    SDL_SetWindowSize(window->GetSDLWindow(), tracked->state.actualW, tracked->state.actualH);

    // 4. Initial Position Setup
    if (getTargetPos)
    {
        XMFLOAT3 initialPos = getTargetPos();
        float screenX, screenY;
        WorldToScreenPos(initialPos, screenX, screenY);

        tracked->state.targetX = screenX - (tracked->state.actualW * 0.5f);
        tracked->state.targetY = screenY - (tracked->state.actualH * 0.5f);
        tracked->state.actualX = static_cast<int>(roundf(tracked->state.targetX));
        tracked->state.actualY = static_cast<int>(roundf(tracked->state.targetY));

        SDL_SetWindowPosition(window->GetSDLWindow(), tracked->state.actualX, tracked->state.actualY);
    }

    m_windowLookup[config.name] = tracked.get();
    m_trackedWindows.push_back(std::move(tracked));

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> duration = end - start;
    PerformanceLogger::Instance().LogWindowAction("Spawned", config.name, duration.count());

    return true;
}

TrackedWindow* WindowTrackingSystem::GetTrackedWindow(const std::string& name)
{
    auto it = m_windowLookup.find(name);
    if (it != m_windowLookup.end()) return it->second;
    return nullptr;
}

void WindowTrackingSystem::Update(float dt)
{
    for (auto& tracked : m_trackedWindows)
    {
        if (!tracked->isActive) continue;
        UpdateSingleWindow(dt, *tracked);
    }
}

void WindowTrackingSystem::UpdateSingleWindow(float dt, TrackedWindow& tracked)
{
    if (!tracked.window || !tracked.camera || !tracked.getTargetPositionFunc)
        return;

    // 1. Read the current OS position and size
    int osX, osY, osW, osH;
    SDL_GetWindowPosition(tracked.window->GetSDLWindow(), &osX, &osY);
    SDL_GetWindowSize(tracked.window->GetSDLWindow(), &osW, &osH);

    bool isBeingDragged = false;
    if (osW != tracked.state.actualW || osH != tracked.state.actualH)
    {
        tracked.state.actualW = osW;
        tracked.state.actualH = osH;
        // Keep the target in step so the window does not snap back. Target is kept in canvas pixels.
        tracked.state.targetW = (float)osW / m_desktopScale;
        tracked.state.targetH = (float)osH / m_desktopScale;
    }

    // 2. UPDATE SIZE
    // Why not the main window: its size belongs to the user and the engine.
    if (tracked.role != WindowRole::MAIN_VIEWPORT)
    {
        if (tracked.getTargetSizeFunc)
        {
            DirectX::XMFLOAT2 desiredSize = tracked.getTargetSizeFunc(); // canvas pixels
            float tSize = min(m_followSpeed * dt, 1.0f);
            tracked.state.targetW += (desiredSize.x - tracked.state.targetW) * tSize;
            tracked.state.targetH += (desiredSize.y - tracked.state.targetH) * tSize;
        }

        // Applied every frame, also without a size function, so a window follows
        // the arena rect when the game image is resized on the desktop.
        const int newW{ ToDesktopPixels(tracked.state.targetW) };
        const int newH{ ToDesktopPixels(tracked.state.targetH) };
        if (newW != osW || newH != osH)
        {
            SDL_SetWindowSize(tracked.window->GetSDLWindow(), newW, newH);
            tracked.state.actualW = newW;
            tracked.state.actualH = newH;
        }
    }

    // 3. UPDATE POSITION LOGIC
    // Every sub-window is placed against the current arena rect, so it follows when
    // that rect moves (window dragged in Act 2, or the switch to the monitor rect on
    // entering Windowkill). Only the main window is left where the user put it.
    if (!isBeingDragged && tracked.role != WindowRole::MAIN_VIEWPORT)
    {
        DirectX::XMFLOAT3 targetWorldPos = tracked.getTargetPositionFunc();
        targetWorldPos.x += tracked.trackingOffset.x;
        targetWorldPos.y += tracked.trackingOffset.y;
        targetWorldPos.z += tracked.trackingOffset.z;

        float targetScreenX, targetScreenY;
        WorldToScreenPos(targetWorldPos, targetScreenX, targetScreenY);

        float destX = targetScreenX - (tracked.state.actualW * 0.5f);
        float destY = targetScreenY - (tracked.state.actualH * 0.5f);

        float tPos = min(m_followSpeed * dt, 1.0f);
        tracked.state.targetX += (destX - tracked.state.targetX) * tPos;
        tracked.state.targetY += (destY - tracked.state.targetY) * tPos;

        int newX = static_cast<int>(roundf(tracked.state.targetX));
        int newY = static_cast<int>(roundf(tracked.state.targetY));

        // =========================================================
        // OS WINDOW SHAKE
        // =========================================================
        float trauma = CameraController::Instance().GetTrauma();

        // Why entities only: viewport windows (FX overlay, click blocker) span the whole
        // play field, and their image already shakes through the camera.
        const bool shakesWithCamera{ tracked.role == WindowRole::TRACKED_ENTITY };

        if (shakesWithCamera && trauma > 0.01f) {
            float shakeIntensity = trauma * trauma; // squared for a softer falloff
            float maxShakePixels = 35.0f; // largest offset, in desktop pixels

            // random value in [-1, 0.99]
            float randX = ((rand() % 200) / 100.0f) - 1.0f;
            float randY = ((rand() % 200) / 100.0f) - 1.0f;

            int shakeOffsetX = static_cast<int>(randX * shakeIntensity * maxShakePixels);
            int shakeOffsetY = static_cast<int>(randY * shakeIntensity * maxShakePixels);

            SDL_SetWindowPosition(tracked.window->GetSDLWindow(), newX + shakeOffsetX, newY + shakeOffsetY);

            // actualX/Y keep the unshaken position, so the window is back on its path
            // as soon as the shake ends.
            tracked.state.actualX = newX;
            tracked.state.actualY = newY;
        }
        else {
            // No shake: move the OS window only when the target moved by half a pixel or more.
            float deltaX = fabsf(tracked.state.targetX - tracked.state.actualX);
            float deltaY = fabsf(tracked.state.targetY - tracked.state.actualY);

            if ((newX != tracked.state.actualX || newY != tracked.state.actualY)
                && (deltaX >= 0.5f || deltaY >= 0.5f))
            {
                SDL_SetWindowPosition(tracked.window->GetSDLWindow(), newX, newY);
                tracked.state.actualX = newX;
                tracked.state.actualY = newY;
            }
        }
    }

    if (tracked.role == WindowRole::MAIN_VIEWPORT)
    {
        // The main window shows the whole arena through the game canvas, wherever the
        // OS window sits and whatever its size: project the full screen rect instead
        // of the window's own rect.
        UpdateOffCenterProjection(tracked.camera.get(),
            m_arenaRect.x, m_arenaRect.y, m_arenaRect.width, m_arenaRect.height, GetUnifiedCameraHeight());
        return;
    }

    // 4. Project the 3D view from the window's actual OS position, not its target position.
    UpdateOffCenterProjection(tracked.camera.get(), osX, osY, tracked.state.actualW, tracked.state.actualH, GetUnifiedCameraHeight());
}

void WindowTrackingSystem::UpdateOffCenterProjection(Camera* targetCam, int winX, int winY, int winW, int winH, float camHeight)
{
    // Keep the camera's current position (it already contains the CameraController shake)
    // instead of forcing (0, camHeight, 0), so the shake stays in sync across windows.
    DirectX::XMFLOAT3 currentPos = targetCam->GetPosition();
    targetCam->SetPosition(currentPos);

    // The portal camera always looks straight down, so its projection lines up
    // with desktop coordinates.
    targetCam->LookAt({ currentPos.x, 0.0f, currentPos.z });

    float nearZ = 0.1f;
    float farZ = 1000.0f;
    float halfFovTan = tanf(DirectX::XMConvertToRadians(m_fov) * 0.5f);

    // The full frustum covers the arena rect; a window gets the slice of it
    // that its own desktop rect cuts out.
    const double arenaW{ static_cast<double>(m_arenaRect.width) };
    const double arenaH{ static_cast<double>(m_arenaRect.height) };

    float halfHeight = nearZ * halfFovTan;
    float halfWidth = halfHeight * static_cast<float>(arenaW / arenaH);

    const double localX{ static_cast<double>(winX - m_arenaRect.x) };
    const double localY{ static_cast<double>(winY - m_arenaRect.y) };

    float l = (float)((localX / arenaW) * 2.0 - 1.0);
    float r = (float)(((localX + winW) / arenaW) * 2.0 - 1.0);
    float t = (float)(1.0 - (localY / arenaH) * 2.0);
    float b = (float)(1.0 - ((localY + winH) / arenaH) * 2.0);

    targetCam->SetOffCenterProjection(l * halfWidth, r * halfWidth, b * halfHeight, t * halfHeight, nearZ, farZ);
}
// =========================================================
// MATH HELPERS
// =========================================================

void WindowTrackingSystem::WorldToScreenPos(const DirectX::XMFLOAT3& worldPos, float& outScreenX, float& outScreenY)
{
    // World origin sits at the centre of the arena rect.
    // World +Z maps to screen -Y.
    const float centerX{ static_cast<float>(m_arenaRect.x) + static_cast<float>(m_arenaRect.width) * 0.5f };
    const float centerY{ static_cast<float>(m_arenaRect.y) + static_cast<float>(m_arenaRect.height) * 0.5f };

    const float desktopPixelsPerUnit{ Beyond::Config::PIXEL_TO_UNIT_RATIO * m_desktopScale };
    outScreenX = centerX + (worldPos.x * desktopPixelsPerUnit);
    outScreenY = centerY - (worldPos.z * desktopPixelsPerUnit);
}

float WindowTrackingSystem::GetUnifiedCameraHeight()
{
    // Height at which the vertical FOV spans exactly the arena height. It no longer
    // depends on the monitor: pixels per unit already absorb the rect size.
    float halfFovTan = tanf(XMConvertToRadians(m_fov) * 0.5f);
    return (Beyond::Config::ARENA_HEIGHT_UNITS * 0.5f) / halfFovTan;
}

void WindowTrackingSystem::RemoveTrackedWindow(const std::string& name)
{
    auto it = m_windowLookup.find(name);
    if (it == m_windowLookup.end()) return;

    TrackedWindow* trackedInfo = it->second;

    // Hide the OS window instead of destroying it.
    if (trackedInfo && trackedInfo->window && trackedInfo->window->GetSDLWindow())
    {
        SDL_Window* sdlWin = trackedInfo->window->GetSDLWindow();
        SDL_HideWindow(sdlWin);
        SDL_SetWindowPosition(sdlWin, -10000, -10000); // park it off screen

        trackedInfo->isActive = false;
        trackedInfo->name = "POOL_" + name; // frees the original name for reuse
    }

    // Only the lookup entry is removed: the name is free again, and the object stays in m_trackedWindows as a pooled window.
    m_windowLookup.erase(it);
}

void WindowTrackingSystem::RegisterWindow(Beyond::Window* window, WindowRole role, std::shared_ptr<Camera> camera)
{
    if (!window) return;

    auto tracked = std::make_unique<TrackedWindow>();
    tracked->name = "main_window"; // fixed lookup name
    tracked->window = window;
    tracked->role = role;
    tracked->camera = camera;

    // UpdateSingleWindow skips windows without a target function; the main window needs one.
    tracked->getTargetPositionFunc = []() { return DirectX::XMFLOAT3(0, 0, 0); };

    int osX, osY, osW, osH;
    SDL_GetWindowPosition(window->GetSDLWindow(), &osX, &osY);
    SDL_GetWindowSize(window->GetSDLWindow(), &osW, &osH);

    tracked->state.actualX = osX;
    tracked->state.actualY = osY;
    tracked->state.actualW = osW;
    tracked->state.actualH = osH;
    tracked->state.targetX = (float)osX;
    tracked->state.targetY = (float)osY;
    tracked->state.targetW = (float)osW;
    tracked->state.targetH = (float)osH;

    m_windowLookup[tracked->name] = tracked.get();
    m_trackedWindows.push_back(std::move(tracked));
}

void WindowTrackingSystem::UpdateWindowBounds(int windowIndex, int width, int height)
{
    if (windowIndex == 0) // index 0 is the main window
    {
        TrackedWindow* tracked = GetTrackedWindow("main_window");
        if (tracked)
        {
            tracked->state.actualW = width;
            tracked->state.actualH = height;
            tracked->state.targetW = (float)width;
            tracked->state.targetH = (float)height;
        }
    }
}

std::unique_ptr<TrackedWindow> WindowTrackingSystem::ExtractForPool(const std::string& name)
{
    auto it = m_windowLookup.find(name);
    if (it == m_windowLookup.end()) return nullptr;

    auto vecIt = std::find_if(m_trackedWindows.begin(), m_trackedWindows.end(),
        [&name](const std::unique_ptr<TrackedWindow>& p) {
            return p->name == name;
        });

    if (vecIt != m_trackedWindows.end())
    {
        std::unique_ptr<TrackedWindow> extracted = std::move(*vecIt);
        m_trackedWindows.erase(vecIt);
        m_windowLookup.erase(it);
        return extracted;
    }
    return nullptr;
}

void WindowTrackingSystem::RestoreFromPool(std::unique_ptr<TrackedWindow> window)
{
    if (!window) return;
    m_windowLookup[window->name] = window.get();
    m_trackedWindows.push_back(std::move(window));
}