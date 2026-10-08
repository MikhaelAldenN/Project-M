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

void TestPureWin32Transparency()
{
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "TestTransparent";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        WS_EX_LAYERED,
        "TestTransparent", "Test Transparent",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,  // ← pakai border agar kelihatan
        200, 200, 400, 400,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    // Alpha 128 = 50% transparan seluruh window
    SetLayeredWindowAttributes(hwnd, 0, 128, LWA_ALPHA);

    Sleep(3000); // Lihat selama 3 detik tanpa MessageBox menghalangi
    DestroyWindow(hwnd);
}

// Di main(), panggil sebelum framework:
// TestPureWin32Transparency();

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

    //TestPureWin32Transparency();
    try
    {
        auto framework = std::make_unique<Framework>();

        bool running = true;
        Uint64 lastTime = SDL_GetPerformanceCounter();
        Uint64 frequency = SDL_GetPerformanceFrequency();

        // ==========================================
        // KONFIGURASI FPS CAP
        // ==========================================
        const double targetFPS = 60.0; // Silakan ganti ke 30, 90, atau 120
        const Uint64 targetTicksPerFrame = frequency / targetFPS;
        const Uint64 yieldThreshold = frequency / 500; // Batas aman 2ms untuk CPU napas

        while (running)
        {
            // 1. Catat waktu persis saat frame dimulai
            Uint64 frameStart = SDL_GetPerformanceCounter();


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

                if (event.type == SDL_EVENT_QUIT) running = false;                if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
                if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                {
                    // Cek window mana yang barusan diklik tombol silangnya
                    SDL_Window* closedWin = SDL_GetWindowFromID(event.window.windowID);
                    Beyond::Window* mainWin = framework ? framework->GetMainWindow() : nullptr;

                    if (mainWin && closedWin == mainWin->GetSDLWindow()) {
                        // Yang disilang adalah MAIN WINDOW -> Matikan game!
                        running = false;
                    }
                    else {
                        // Yang disilang adalah SUB-WINDOW (Dummy) -> Suruh Framework hapus!
                        if (framework) framework->OnSubWindowClosed(event.window.windowID);
                    }
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

            // ==========================================
            // 2. LOGIKA PEMBATAS FPS (HYBRID SPIN-WAIT)
            // ==========================================
            while (true)
            {
                Uint64 now = SDL_GetPerformanceCounter();
                Uint64 ticksPassed = now - frameStart;

                // Jika waktu frame sudah mencapai batas (misal 16.66ms), lanjut ke frame berikutnya!
                if (ticksPassed >= targetTicksPerFrame)
                {
                    break;
                }

                // Jika sisa waktu masih > 2ms, suruh thread CPU mengalah sebentar
                if (targetTicksPerFrame - ticksPassed > yieldThreshold)
                {
                    std::this_thread::yield();
                }
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