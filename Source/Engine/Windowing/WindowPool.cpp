#include "WindowPool.h"
#include <SDL3/SDL.h>

namespace Beyond
{
    bool WindowPool::Acquire(
        WindowTrackingSystem& trackingSystem,
        const TrackedWindowConfig& config,
        std::function<DirectX::XMFLOAT3()> getTargetPos,
        std::function<DirectX::XMFLOAT2()> getTargetSize
    )
    {
        if (m_pool.empty())
        {
            return false; // the caller creates a new window instead
        }

        auto recycled = std::move(m_pool.back());
        m_pool.pop_back();

        // Reset data
        recycled->name = config.name;
        recycled->trackingOffset = config.trackingOffset;
        recycled->getTargetPositionFunc = getTargetPos;
        recycled->getTargetSizeFunc = getTargetSize;

        if (recycled->window)
        {
            recycled->window->SetTitle(config.title.c_str());
            if (config.fpsLimit > 0.0f)
            {
                recycled->window->SetTargetFPS(config.fpsLimit);
            }

            SDL_ShowWindow(recycled->window->GetSDLWindow());
        }

        // Ownership goes back to the tracking system, which positions the window again.
        trackingSystem.RestoreFromPool(std::move(recycled));

        return true;
    }

    void WindowPool::Release(WindowTrackingSystem& trackingSystem, const std::string& name)
    {
        // Take ownership of the window away from the tracking system.
        std::unique_ptr<TrackedWindow> extractedWindow = trackingSystem.ExtractForPool(name);

        if (extractedWindow)
        {
            if (extractedWindow->window)
            {
                SDL_HideWindow(extractedWindow->window->GetSDLWindow());
            }

            m_pool.push_back(std::move(extractedWindow));
        }
    }

    void WindowPool::Clear()
    {
        // TrackedWindow only borrows its Beyond::Window (WindowManager owns it),
        // so clearing the pool does not destroy the OS windows.
        m_pool.clear();
    }
}