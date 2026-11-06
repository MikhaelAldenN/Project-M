#include <windows.h>
#include <memory>
#include <SDL3/SDL.h> 
#include <iostream> 
#include <exception> 
#include "WindowManager.h"
#include <thread>
#include <array>
#include <cstdio>

#include "Framework.h"

void EmergencyWatchdog()
{
    while (true)
    {
        bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000);
        bool f12Pressed = (GetAsyncKeyState(VK_F12) & 0x8000);

        if (ctrlPressed && f12Pressed)
        {
            Beep(200, 50);
            OutputDebugStringA("!!! EMERGENCY EXIT TRIGGERED !!!\n");
            ExitProcess(-1);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

namespace
{
    // Writes one debug-output line: active DPI awareness and primary display size.
    // Must run after SDL_Init(SDL_INIT_VIDEO), because it queries SDL displays.
    void LogDisplayStartupInfo()
    {
        struct NamedContext
        {
            DPI_AWARENESS_CONTEXT context{ nullptr };
            const char* name{ "" };
        };
        // Not constexpr: the Win32 context macros are pointer casts.
        const std::array<NamedContext, 5> knownContexts{ {
            { DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2, "per-monitor v2" },
            { DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE,    "per-monitor v1" },
            { DPI_AWARENESS_CONTEXT_SYSTEM_AWARE,         "system" },
            { DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED,    "unaware (GDI scaled)" },
            { DPI_AWARENESS_CONTEXT_UNAWARE,              "unaware" },
        } };

        const DPI_AWARENESS_CONTEXT activeContext{ GetThreadDpiAwarenessContext() };
        const char* awarenessName{ "unknown" };
        for (const auto& known : knownContexts)
        {
            if (AreDpiAwarenessContextsEqual(activeContext, known.context))
            {
                awarenessName = known.name;
                break;
            }
        }

        const SDL_DisplayID primaryDisplay{ SDL_GetPrimaryDisplay() };
        SDL_Rect bounds{};
        if (!SDL_GetDisplayBounds(primaryDisplay, &bounds))
        {
            OutputDebugStringA("[Display] SDL_GetDisplayBounds failed: ");
            OutputDebugStringA(SDL_GetError());
            OutputDebugStringA("\n");
        }

        // Why both sizes: SceneBoss and WindowTrackingSystem read GetSystemMetrics,
        // which is DPI-virtualized when the process is not per-monitor aware.
        std::array<char, 256> line{};
        std::snprintf(line.data(), line.size(),
            "[Display] DPI awareness: %s | SDL primary: %dx%d | GetSystemMetrics: %dx%d | content scale: %.2f\n",
            awarenessName, bounds.w, bounds.h,
            GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
            SDL_GetDisplayContentScale(primaryDisplay));
        OutputDebugStringA(line.data());
    }

    // Used when SDL cannot report the display's refresh rate.
    constexpr double k_fallbackRefreshHz{ 60.0 };

    // Refresh rate in Hz of the display that shows window (the primary display if window is null).
    [[nodiscard]] double QueryRefreshRate(SDL_Window* window)
    {
        const SDL_DisplayID display{ window ? SDL_GetDisplayForWindow(window) : SDL_GetPrimaryDisplay() };
        const SDL_DisplayMode* const mode{ SDL_GetCurrentDisplayMode(display) };
        if (!mode || mode->refresh_rate <= 0.0f) return k_fallbackRefreshHz;
        return static_cast<double>(mode->refresh_rate);
    }

    // Performance-counter ticks of one frame at the refresh rate of window's display.
    [[nodiscard]] Uint64 QueryTicksPerFrame(SDL_Window* window, Uint64 frequency)
    {
        const double refreshHz{ QueryRefreshRate(window) };

        std::array<char, 64> line{};
        std::snprintf(line.data(), line.size(), "[FrameLimiter] Cap: %.2f FPS\n", refreshHz);
        OutputDebugStringA(line.data());

        return static_cast<Uint64>(static_cast<double>(frequency) / refreshHz);
    }

    // Blocks the calling thread for a number of performance-counter ticks without spinning.
    // Why a high-resolution waitable timer: Sleep() rounds up to the system tick
    // (about 15.6 ms by default), too coarse for a 144 Hz frame (6.9 ms).
    class FrameSleeper
    {
    public:
        FrameSleeper()
        {
            // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION needs Windows 10 1803 or later;
            // older systems fall back to a timer with the default resolution.
            constexpr DWORD k_highResolutionFlag{ 0x00000002 };
            m_timer = CreateWaitableTimerExW(nullptr, nullptr, k_highResolutionFlag, TIMER_ALL_ACCESS);
            if (!m_timer)
            {
                m_timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
            }
        }

        ~FrameSleeper()
        {
            if (m_timer) CloseHandle(m_timer);
        }

        FrameSleeper(const FrameSleeper&) = delete;
        FrameSleeper& operator=(const FrameSleeper&) = delete;

        void SleepFor(Uint64 ticks, Uint64 frequency) const
        {
            // Due time in 100 ns units; negative means relative to now.
            LARGE_INTEGER dueTime{};
            dueTime.QuadPart = -static_cast<LONGLONG>(ticks * 10'000'000ULL / frequency);

            if (m_timer && SetWaitableTimer(m_timer, &dueTime, 0, nullptr, nullptr, FALSE))
            {
                WaitForSingleObject(m_timer, INFINITE);
                return;
            }
            Sleep(static_cast<DWORD>(ticks * 1000ULL / frequency));
        }

    private:
        HANDLE m_timer{ nullptr };
    };
}

int main(int argc, char* argv[])
{
    std::thread safetyThread(EmergencyWatchdog);
    safetyThread.detach();

    // Init SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        MessageBoxA(NULL, SDL_GetError(), "SDL Init Failed", MB_OK | MB_ICONERROR);
        return -1;
    }

    LogDisplayStartupInfo();

    try
    {
        auto framework = std::make_unique<Framework>();

        bool running = true;
        Uint64 lastTime = SDL_GetPerformanceCounter();
        Uint64 frequency = SDL_GetPerformanceFrequency();

        // Frame cap at the display refresh rate, without vsync: Act 3 presents many
        // windows per frame, and a vsynced Present on each would block the frame.
        const FrameSleeper frameSleeper;
        Beyond::Window* const startWindow{ framework->GetMainWindow() };
        Uint64 ticksPerFrame{ QueryTicksPerFrame(startWindow ? startWindow->GetSDLWindow() : nullptr, frequency) };
        Uint64 nextFrameTick{ SDL_GetPerformanceCounter() + ticksPerFrame };

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                // Why first: these keys must work from the debug window too, and that
                // window swallows its own key events in the next line.
                if (framework && framework->HandleDebugHotkey(event)) continue;

                // Debug host window events never reach game logic.
                if (framework && framework->HandleDebugHostEvent(event)) continue;

#if defined(_DEBUG)
                // F11 switches the main window between windowed and borderless.
                // Debug only: a shipped build always covers the display.
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F11 && !event.key.repeat)
                {
                    if (framework) framework->ToggleMainWindowMode();
                }
#endif

                if (event.type == SDL_EVENT_QUIT)
                {
                    running = false;
                }

#if defined(_DEBUG)
                // Shift+Esc is a developer quick exit; plain Esc belongs to the scenes (pause).
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE
                    && (event.key.mod & SDL_KMOD_SHIFT) != 0 && !event.key.repeat)
                {
                    running = false;
                }
#endif

                // Closing any game window (Alt+F4, title bar X) quits the whole game, so a
                // Windowkill window can neither be dismissed as a cheat nor leave the game
                // running behind a hidden main window. The debug host never gets here.
                if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                {
                    running = false;
                }

                // The main window moved to another display, or a display changed its mode.
                if (event.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED || event.type == SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED)
                {
                    Beyond::Window* const mainWindow{ framework ? framework->GetMainWindow() : nullptr };
                    ticksPerFrame = QueryTicksPerFrame(mainWindow ? mainWindow->GetSDLWindow() : nullptr, frequency);
                }

                if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
                {
                    SDL_Window* resizedWin = SDL_GetWindowFromID(event.window.windowID);
                    if (resizedWin) {
                        WindowManager::Instance().HandleResize(resizedWin, event.window.data1, event.window.data2);
                    }
                }

            }

            Uint64 currentTime = SDL_GetPerformanceCounter();
            float elapsedTime = (float)(currentTime - lastTime) / (float)frequency;
            lastTime = currentTime;

            if (elapsedTime > 0.05f) elapsedTime = 0.05f;

            if (framework)
            {
                framework->Update(elapsedTime);
                framework->Render(elapsedTime);
            }

            if (!WindowManager::Instance().HasWindows())
            {
                running = false;
            }

            // Sleep to the next frame deadline. Deadlines advance by a fixed step, so a
            // frame that oversleeps shortens the next wait instead of lowering the average.
            const Uint64 frameEndTick{ SDL_GetPerformanceCounter() };
            if (frameEndTick < nextFrameTick)
            {
                frameSleeper.SleepFor(nextFrameTick - frameEndTick, frequency);
                nextFrameTick += ticksPerFrame;
            }
            else
            {
                // A late frame drops the debt: catching up would run frames back to back.
                nextFrameTick = frameEndTick + ticksPerFrame;
            }
        }
    }

    catch (const std::exception& e)
    {
        std::string errorMessage = "Runtime Error Occurred:\n";
        errorMessage += e.what();
        MessageBoxA(NULL, errorMessage.c_str(), "CRITICAL ERROR", MB_OK | MB_ICONERROR);

        OutputDebugStringA(errorMessage.c_str());

        SDL_Quit();
        return -1;
    }
    catch (...)
    {
        MessageBoxA(NULL, "Unknown Error Occurred!", "CRITICAL ERROR", MB_OK | MB_ICONERROR);
        SDL_Quit();
        return -1;
    }

    SDL_Quit();
    return 0;
}