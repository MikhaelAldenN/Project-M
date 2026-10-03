#pragma once
#include <DirectXMath.h>

namespace Beyond {
    struct MouseViewPos
    {
        float x{ 0.0f };
        float y{ 0.0f };
        float viewWidth{ 0.0f };
        float viewHeight{ 0.0f };
    };

    class InputHelper {
    public:
        // Where the mouse is inside the scene's image: canvas pixels when the scene is
        // drawn through the game canvas, main-window pixels otherwise. Use the returned
        // view size as the viewport when unprojecting. The position may lie outside the
        // view (cursor on a letterbox bar or outside the window).
        static MouseViewPos GetMouseViewPos();
    };
}