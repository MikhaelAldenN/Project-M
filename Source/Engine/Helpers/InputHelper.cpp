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
        if (!framework)
        {
            return MouseViewPos{};
        }

        // Why the global position: SDL_GetMouseState is relative to whichever window
        // holds mouse focus, which is a sub-window whenever the cursor is over one.
        float globalX{ 0.0f };
        float globalY{ 0.0f };
        SDL_GetGlobalMouseState(&globalX, &globalY);

        if (!framework->GetActiveCanvas())
        {
            // No canvas: the scene fills the main window 1:1.
            const Window* mainWindow{ framework->GetMainWindow() };
            if (!mainWindow || !mainWindow->GetSDLWindow())
            {
                return MouseViewPos{};
            }
            int windowX{ 0 };
            int windowY{ 0 };
            SDL_GetWindowPosition(mainWindow->GetSDLWindow(), &windowX, &windowY);
            return MouseViewPos{
                globalX - static_cast<float>(windowX), globalY - static_cast<float>(windowY),
                static_cast<float>(mainWindow->GetWidth()), static_cast<float>(mainWindow->GetHeight()) };
        }

        // The same rect the sub-windows are placed against, so aim and windows agree.
        const PixelRect image{ framework->GetGameImageRect() };
        if (image.width <= 0 || image.height <= 0)
        {
            return MouseViewPos{};
        }

        const float canvasWidth{ static_cast<float>(Config::CANVAS_WIDTH) };
        const float canvasHeight{ static_cast<float>(Config::CANVAS_HEIGHT) };
        return MouseViewPos{
            (globalX - static_cast<float>(image.x)) * canvasWidth / static_cast<float>(image.width),
            (globalY - static_cast<float>(image.y)) * canvasHeight / static_cast<float>(image.height),
            canvasWidth, canvasHeight };
    }
}