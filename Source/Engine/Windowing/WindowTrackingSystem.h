#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <DirectXMath.h>
#include "Camera.h"
#include "BeyondWindow.h"
#include "Engine/Common/Constants.h"
#include "Engine/Common/FitRect.h"
#include <cmath>

// =========================================================
// DATA STRUCTURES
// =========================================================
enum class WindowRole {
    MAIN_VIEWPORT,
    TRACKED_ENTITY,
    SUB_VIEWPORT
};

struct WindowState
{
    int actualX = 0;
    int actualY = 0;
    float targetX = 0.0f;
    float targetY = 0.0f;

    int actualW = 0;
    int actualH = 0;
    float targetW = 0.0f;
    float targetH = 0.0f;
};

struct TrackedWindowConfig
{
    std::string name;
    std::string title;
    int width = 300;
    int height = 300;
    int priority = 0;
    DirectX::XMFLOAT3 trackingOffset = { 0.0f, 0.0f, 0.0f };
    float fpsLimit = 0.0f;
    bool isTransparent = false;

    bool isAlwaysOnTop = false; // <--- TAMBAHKAN INI (Default: false)

    WindowRole role = WindowRole::TRACKED_ENTITY;
};

struct TrackedWindow
{
    std::string name;
    Beyond::Window* window = nullptr;
    std::shared_ptr<Camera> camera;
    WindowState state;
    DirectX::XMFLOAT3 trackingOffset = { 0.0f, 0.0f, 0.0f };
    std::function<DirectX::XMFLOAT3()> getTargetPositionFunc;
    std::function<DirectX::XMFLOAT2()> getTargetSizeFunc = nullptr;

    WindowRole role = WindowRole::TRACKED_ENTITY;

    bool isActive = true;
    bool isTransparent = false;
};

// =========================================================
// WINDOW TRACKING SYSTEM
// =========================================================
class WindowTrackingSystem
{
public:
    WindowTrackingSystem();
    ~WindowTrackingSystem();

    // Core Logic
    void Update(float dt);

    // Management
    void RegisterWindow(Beyond::Window* window, WindowRole role, std::shared_ptr<Camera> camera = nullptr); // <-- BARU
    void UpdateWindowBounds(int windowIndex, int width, int height);

    // Management
    bool AddTrackedWindow(
        const TrackedWindowConfig& config,
        std::function<DirectX::XMFLOAT3()> getTargetPos,
        std::function<DirectX::XMFLOAT2()> getTargetSize = nullptr // Default null
    );
    void RemoveTrackedWindow(const std::string& name);
    TrackedWindow* GetTrackedWindow(const std::string& name);
    void ClearAll();

    // Configuration
    void SetFollowSpeed(float speed) { m_followSpeed = speed; }
    void SetFOV(float fov) { m_fov = fov; }

    // Desktop-pixel rect the arena (ARENA_WIDTH_UNITS x ARENA_HEIGHT_UNITS) maps onto.
    // Window positions, projections and the pixel-to-unit ratio all derive from it.
    // An empty rect is ignored and the previous one stays in effect.
    void SetArenaRect(const Beyond::PixelRect& rect);
    const Beyond::PixelRect& GetArenaRect() const { return m_arenaRect; }

    // Desktop pixels per world unit. Derived from the arena rect, so it changes
    // whenever that rect changes: do not cache it across frames.
    float GetPixelToUnitRatio() const { return Beyond::Config::PIXEL_TO_UNIT_RATIO; }

    // Desktop pixels per canvas pixel for the current arena rect (1.0 when the game
    // image is 1920x1080 on the desktop). Only needed where code talks to the OS.
    float GetDesktopScale() const { return m_desktopScale; }

    // Accessors for Rendering (misal untuk menggambar overlay shatter)
    const std::vector<std::unique_ptr<TrackedWindow>>& GetWindows() const { return m_trackedWindows; }

    // Helpers (Public karena mungkin Scene butuh untuk debug/rendering overlay)
    void WorldToScreenPos(const DirectX::XMFLOAT3& worldPos, float& outScreenX, float& outScreenY);
    float GetUnifiedCameraHeight();

    // Jembatan untuk Pool System
    std::unique_ptr<TrackedWindow> ExtractForPool(const std::string& name);
    void RestoreFromPool(std::unique_ptr<TrackedWindow> window);

private:
    void UpdateSingleWindow(float dt, TrackedWindow& tracked);
    void UpdateOffCenterProjection(Camera* targetCam, int winX, int winY, int winW, int winH, float camHeight);

    int ToDesktopPixels(float canvasPixels) const
    {
        const int scaled{ static_cast<int>(roundf(canvasPixels * m_desktopScale)) };
        return scaled < 10 ? 10 : scaled;
    }

private:
    std::vector<std::unique_ptr<TrackedWindow>> m_trackedWindows;
    std::unordered_map<std::string, TrackedWindow*> m_windowLookup;

    // Starts as a canvas-sized rect at the desktop origin so the math is valid
    // before the scene supplies the real one.
    Beyond::PixelRect m_arenaRect{ 0, 0, Beyond::Config::CANVAS_WIDTH, Beyond::Config::CANVAS_HEIGHT };

    // Cache Screen Metrics
    int m_cachedScreenWidth = 0;
    int m_cachedScreenHeight = 0;
    float m_cacheUpdateTimer = 0.0f;

    // Settings
    float m_followSpeed = 100.0f;
    float m_desktopScale = 1.0f;
    float m_fov = 60.0f;
};