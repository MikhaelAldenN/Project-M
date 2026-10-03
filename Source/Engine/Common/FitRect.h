#pragma once

// FitRect - largest centered rectangle of a given aspect ratio inside a pixel rectangle.
// Used to letterbox the game canvas into the main window, and (W5) to place the
// 16:9 play area on a monitor of any shape.

namespace Beyond {

    // Axis-aligned rectangle in whole pixels. x/y may be negative (virtual-screen coordinates).
    struct PixelRect
    {
        int x{ 0 };
        int y{ 0 };
        int width{ 0 };
        int height{ 0 };
    };

    // Returns the largest rect with ratio aspectWidth:aspectHeight that fits inside
    // `container`, centered in it. Sizes round down to whole pixels.
    // Returns an empty rect at the container origin when the container or the aspect
    // is not positive (for example a minimized window reports 0x0).
    constexpr PixelRect FitRect(const PixelRect& container, int aspectWidth, int aspectHeight)
    {
        if (container.width <= 0 || container.height <= 0 || aspectWidth <= 0 || aspectHeight <= 0)
        {
            return PixelRect{ container.x, container.y, 0, 0 };
        }

        // Why long long: width * aspect can exceed int range on large virtual desktops.
        const long long containerWidth{ container.width };
        const long long containerHeight{ container.height };

        // Cross-multiplied aspect comparison, so no float rounding decides the branch.
        const bool isContainerWider{ containerWidth * aspectHeight >= containerHeight * aspectWidth };

        PixelRect fitted{};
        if (isContainerWider)
        {
            // Bars left and right (pillarbox).
            fitted.height = container.height;
            fitted.width = static_cast<int>(containerHeight * aspectWidth / aspectHeight);
        }
        else
        {
            // Bars top and bottom (letterbox).
            fitted.width = container.width;
            fitted.height = static_cast<int>(containerWidth * aspectHeight / aspectWidth);
        }

        fitted.x = container.x + (container.width - fitted.width) / 2;
        fitted.y = container.y + (container.height - fitted.height) / 2;
        return fitted;
    }

    // ----- Compile-time tests -----
    namespace FitRectTest {

        constexpr bool IsSameRect(const PixelRect& a, const PixelRect& b)
        {
            return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
        }

        constexpr bool Fits16x9(const PixelRect& container, const PixelRect& expected)
        {
            return IsSameRect(FitRect(container, 16, 9), expected);
        }

        // Exact 16:9 containers are filled completely.
        static_assert(Fits16x9({ 0, 0, 1920, 1080 }, { 0, 0, 1920, 1080 }), "1080p");
        static_assert(Fits16x9({ 0, 0, 1600, 900 }, { 0, 0, 1600, 900 }), "1600x900");
        // Wider than 16:9: bars left and right.
        static_assert(Fits16x9({ 0, 0, 2560, 1080 }, { 320, 0, 1920, 1080 }), "21:9");
        // Taller than 16:9: bars top and bottom.
        static_assert(Fits16x9({ 0, 0, 1920, 1200 }, { 0, 60, 1920, 1080 }), "16:10");
        static_assert(Fits16x9({ 0, 0, 1280, 1024 }, { 0, 152, 1280, 720 }), "5:4");
        static_assert(Fits16x9({ 0, 0, 1080, 1920 }, { 0, 656, 1080, 607 }), "portrait");
        // Not exactly 16:9: size rounds down, never exceeds the container.
        static_assert(Fits16x9({ 0, 0, 1366, 768 }, { 0, 0, 1365, 768 }), "1366x768");
        // Container origin is preserved (second monitor, monitor left of primary).
        static_assert(Fits16x9({ 1920, 0, 2560, 1440 }, { 1920, 0, 2560, 1440 }), "second monitor");
        static_assert(Fits16x9({ -1920, 0, 1920, 1200 }, { -1920, 60, 1920, 1080 }), "negative origin");
        // Degenerate input gives an empty rect, not a division by zero.
        static_assert(Fits16x9({ 5, 35, 0, 0 }, { 5, 35, 0, 0 }), "minimized");
        static_assert(IsSameRect(FitRect({ 0, 0, 1920, 1080 }, 0, 9), { 0, 0, 0, 0 }), "bad aspect");
        // A non-reduced aspect (canvas size) gives the same result as 16:9.
        static_assert(IsSameRect(FitRect({ 0, 0, 2560, 1080 }, 1920, 1080), { 320, 0, 1920, 1080 }), "canvas as aspect");
    }
}
