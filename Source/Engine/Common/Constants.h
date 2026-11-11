#pragma once

namespace Beyond {
    namespace Config {
        // 40 pixels on the canvas = 1 world unit
        constexpr float PIXEL_TO_UNIT_RATIO = 40.0f;

        // Default camera
        constexpr float CAM_FOV = 60.0f;
        constexpr float CAM_NEAR = 0.1f;
        constexpr float CAM_FAR = 1000.0f;

        constexpr int CANVAS_WIDTH = 1920;
        constexpr int CANVAS_HEIGHT = 1080;

        // Boss arena size in world units. The game canvas shows exactly this area,
        // which is 40 pixels per unit at canvas resolution.
        constexpr float ARENA_WIDTH_UNITS = 48.0f;
        constexpr float ARENA_HEIGHT_UNITS = 27.0f;

        // Act 2 arena in world units, larger than the visible area above, so the
        // camera follows the player. USULAN DESAIN: 1.5x the visible area per axis.
        constexpr float ACT2_ARENA_WIDTH_UNITS = 72.0f;
        constexpr float ACT2_ARENA_HEIGHT_UNITS = 40.5f;
        static_assert(ACT2_ARENA_WIDTH_UNITS >= ARENA_WIDTH_UNITS
            && ACT2_ARENA_HEIGHT_UNITS >= ARENA_HEIGHT_UNITS,
            "the Act 2 arena must contain the visible area");
    }
}