#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <DirectXMath.h>

// TrackedWindow and TrackedWindowConfig are defined here.
#include "WindowTrackingSystem.h" 

namespace Beyond
{
    class WindowPool
    {
    public:
        // Singleton, so any scene can reach the pool without passing it around.
        static WindowPool& Instance()
        {
            static WindowPool instance;
            return instance;
        }

        // --- CORE FUNCTIONS ---
        // Moves a pooled window back into tracking. Returns false if the pool is empty.
        bool Acquire(
            WindowTrackingSystem& trackingSystem,
            const TrackedWindowConfig& config,
            std::function<DirectX::XMFLOAT3()> getTargetPos,
            std::function<DirectX::XMFLOAT2()> getTargetSize = nullptr
        );

        // Hides the window and moves it from tracking into the pool.
        void Release(WindowTrackingSystem& trackingSystem, const std::string& name);

        // Drops every pooled window (scene change or reset).
        void Clear();

    private:
        WindowPool() = default;
        ~WindowPool() = default;
        WindowPool(const WindowPool&) = delete;
        WindowPool& operator=(const WindowPool&) = delete;

        // Hidden windows waiting to be reused.
        std::vector<std::unique_ptr<TrackedWindow>> m_pool;
    };
}