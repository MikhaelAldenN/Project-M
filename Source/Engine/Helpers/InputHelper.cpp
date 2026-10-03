#include "InputHelper.h"
#include "Engine/Common/Constants.h"
#include "System/Input.h"
#include <SDL3/SDL.h>
#include "Framework.h"
#include "Engine/Common/FitRect.h"

namespace Beyond {
    MouseViewPos InputHelper::GetMouseViewPos()
    {
        const Framework* framework{ Framework::Instance() };
        const Window* mainWindow{ framework ? framework->GetMainWindow() : nullptr };
        if (!mainWindow || !mainWindow->GetSDLWindow())
        {
            return MouseViewPos{};
        }

        // Why global minus window origin: SDL_GetMouseState is relative to whichever
        // window holds mouse focus, which is a sub-window whenever the cursor is over one.
        float globalX{ 0.0f };
        float globalY{ 0.0f };
        SDL_GetGlobalMouseState(&globalX, &globalY);

        int windowX{ 0 };
        int windowY{ 0 };
        SDL_GetWindowPosition(mainWindow->GetSDLWindow(), &windowX, &windowY);

        const float localX{ globalX - static_cast<float>(windowX) };
        const float localY{ globalY - static_cast<float>(windowY) };
        const int windowWidth{ mainWindow->GetWidth() };
        const int windowHeight{ mainWindow->GetHeight() };

        if (!framework->GetActiveCanvas())
        {
            // The scene fills the window 1:1.
            return MouseViewPos{ localX, localY,
                static_cast<float>(windowWidth), static_cast<float>(windowHeight) };
        }

        // Same rect WindowManager::RenderAll blits the canvas into.
        const PixelRect image{ FitRect(PixelRect{ 0, 0, windowWidth, windowHeight },
            Config::CANVAS_WIDTH, Config::CANVAS_HEIGHT) };
        if (image.width <= 0 || image.height <= 0)
        {
            return MouseViewPos{}; // minimized window
        }

        const float canvasWidth{ static_cast<float>(Config::CANVAS_WIDTH) };
        const float canvasHeight{ static_cast<float>(Config::CANVAS_HEIGHT) };
        return MouseViewPos{
            (localX - static_cast<float>(image.x)) * canvasWidth / static_cast<float>(image.width),
            (localY - static_cast<float>(image.y)) * canvasHeight / static_cast<float>(image.height),
            canvasWidth, canvasHeight };
    }
}