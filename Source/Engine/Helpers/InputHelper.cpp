#include "InputHelper.h"
#include "Engine/Common/Constants.h"
#include "System/Input.h"
#include <SDL3/SDL.h>

namespace Beyond {
    DirectX::XMFLOAT3 InputHelper::GetMouseWorldPos(const DirectX::XMFLOAT3& cameraPos) {
        // Last cursor position read while input was live. Why: while the debug
        // window has focus the aim must hold still, not follow the cursor.
        static float s_lastMouseX{ 0.0f };
        static float s_lastMouseY{ 0.0f };

        if (!Input::Instance().IsKeyboardMouseSuppressed()) {
            // SDL3: Mengambil posisi mouse terhadap seluruh area monitor
            SDL_GetGlobalMouseState(&s_lastMouseX, &s_lastMouseY);
        }

        const float mouseX{ s_lastMouseX };
        const float mouseY{ s_lastMouseY };

        SDL_Rect displayBounds;
        // SDL3: Mengambil resolusi monitor utama yang aktif
        SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &displayBounds);

        float monitorCenterX = displayBounds.w / 2.0f;
        float monitorCenterY = displayBounds.h / 2.0f;

        // Hitung jarak mouse dari tengah monitor dalam pixel
        float pixelOffsetX = mouseX - monitorCenterX;
        float pixelOffsetY = mouseY - monitorCenterY;

        // Konversi ke unit dunia + Tambahkan posisi kamera untuk mendukung scrolling
        float worldX = (pixelOffsetX / Config::PIXEL_TO_UNIT_RATIO) + cameraPos.x;
        float worldZ = (-pixelOffsetY / Config::PIXEL_TO_UNIT_RATIO) + cameraPos.z;

        return DirectX::XMFLOAT3(worldX, 0.0f, worldZ);
    }
}