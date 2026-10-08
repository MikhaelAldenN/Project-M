#include "WindowLayoutStore.h"

#if defined(_DEBUG)

#include <fstream>
#include <windows.h>
#include <SDL3/SDL.h>
#include <json.hpp>

namespace
{
    constexpr const char* k_filePath{ "window_layout.json" };
    constexpr const char* k_mainWindowKey{ "main_window" };
    constexpr const char* k_debugWindowKey{ "debug_window" };

    // Smaller than this is treated as a corrupt entry, not a layout the user chose.
    constexpr int k_minWidth{ 200 };
    constexpr int k_minHeight{ 150 };

    // The rect shrunk by this margin must still touch a monitor, so enough of the
    // window is on screen to grab and move it.
    constexpr int k_visibleMargin{ 64 };

    // True if the rect is large enough and still reachable on the current monitors.
    bool IsUsable(const Beyond::PixelRect& rect)
    {
        if (rect.width < k_minWidth || rect.height < k_minHeight) return false;

        const RECT inner{
            rect.x + k_visibleMargin,
            rect.y + k_visibleMargin,
            rect.x + rect.width - k_visibleMargin,
            rect.y + rect.height - k_visibleMargin };
        return ::MonitorFromRect(&inner, MONITOR_DEFAULTTONULL) != nullptr;
    }

    // Throws nlohmann::json::exception if the entry exists but is malformed.
    WindowLayoutStore::SavedRect ParseRect(const nlohmann::json& root, const char* key)
    {
        WindowLayoutStore::SavedRect saved{};
        if (!root.contains(key)) return saved;

        const nlohmann::json& entry{ root.at(key) };
        saved.rect.x = entry.at("x").get<int>();
        saved.rect.y = entry.at("y").get<int>();
        saved.rect.width = entry.at("width").get<int>();
        saved.rect.height = entry.at("height").get<int>();
        saved.isSet = IsUsable(saved.rect);
        return saved;
    }

    void WriteRect(nlohmann::json& root, const char* key, const WindowLayoutStore::SavedRect& saved)
    {
        if (!saved.isSet) return;

        root[key] = {
            { "x", saved.rect.x },
            { "y", saved.rect.y },
            { "width", saved.rect.width },
            { "height", saved.rect.height } };
    }
}

namespace WindowLayoutStore
{
    bool IsOnScreen(const Beyond::PixelRect& rect)
    {
        return IsUsable(rect);
    }

    Layout Load()
    {
        std::ifstream file{ k_filePath };
        if (!file) return {}; // first run: nothing saved yet

        try
        {
            const nlohmann::json root = nlohmann::json::parse(file);

            Layout layout{};
            layout.mainWindow = ParseRect(root, k_mainWindowKey);
            layout.debugWindow = ParseRect(root, k_debugWindowKey);
            return layout;
        }
        catch (const nlohmann::json::exception& e)
        {
            OutputDebugStringA("[WindowLayoutStore] window_layout.json ignored: ");
            OutputDebugStringA(e.what());
            OutputDebugStringA("\n");
            return {};
        }
    }

    void Save(const Layout& layout)
    {
        nlohmann::json root = nlohmann::json::object();
        WriteRect(root, k_mainWindowKey, layout.mainWindow);
        WriteRect(root, k_debugWindowKey, layout.debugWindow);

        std::ofstream file{ k_filePath };
        if (!file)
        {
            OutputDebugStringA("[WindowLayoutStore] Could not write window_layout.json.\n");
            return;
        }
        file << root.dump(4);
    }

    SavedRect ReadWindowRect(SDL_Window* window)
    {
        SavedRect saved{};
        if (!window) return saved;

        // Why: a minimized window reports a position and size that are not its real layout.
        if ((SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0) return saved;

        if (!SDL_GetWindowPosition(window, &saved.rect.x, &saved.rect.y)) return saved;
        if (!SDL_GetWindowSize(window, &saved.rect.width, &saved.rect.height)) return saved;

        saved.isSet = true;
        return saved;
    }
}

#endif // _DEBUG