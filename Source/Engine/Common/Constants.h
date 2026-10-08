#pragma once

namespace Beyond {
    namespace Config {
        // Skala dunia: 40 pixel di monitor = 1 unit di dunia 3D
        constexpr float PIXEL_TO_UNIT_RATIO = 40.0f;

        // Konstanta kamera standar
        constexpr float CAM_FOV = 60.0f;
        constexpr float CAM_NEAR = 0.1f;
        constexpr float CAM_FAR = 1000.0f;

        constexpr int CANVAS_WIDTH = 1920;
        constexpr int CANVAS_HEIGHT = 1080;

        // Boss arena size in world units. The game canvas shows exactly this area,
        // which is 40 pixels per unit at canvas resolution.
        constexpr float ARENA_WIDTH_UNITS = 48.0f;
        constexpr float ARENA_HEIGHT_UNITS = 27.0f;
    }
}