#pragma once

#include <windows.h>
#include "System/HighResolutionTimer.h"

/// Framework
/// - Main application framework: handles update, render, window events
class Framework
{
public:
    Framework(HWND hWnd);
    ~Framework();

private:
    void Update(float elapsedTime);
    void Render(float elapsedTime);
    void CalculateFrameStats();

public:
    // Main Entry Points
    int Run();
    LRESULT CALLBACK HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    // Data Members
    const HWND hWnd;
    HighResolutionTimer timer;
};