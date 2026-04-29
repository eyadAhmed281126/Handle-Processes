#define _WIN32_WINNT 0x0601
#define NOMINMAX

#include <windows.h>
#include <commctrl.h>
#include <algorithm>

#include "gui.h"

#pragma comment(lib, "comctl32.lib")

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = L"SchedCmpWnd";
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassEx(&wc);

    RegisterPanelClasses(hInst);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = std::min(1400, sw - 40);
    int wh = std::min(860, sh - 60);
    int wx = (sw - ww) / 2;
    int wy = (sh - wh) / 2;

    HWND hWnd = CreateWindowEx(WS_EX_APPWINDOW,
        L"SchedCmpWnd",
        L"  Round Robin vs SRTF - CPU Scheduling Comparison",
        WS_OVERLAPPEDWINDOW,
        wx, wy, ww, wh,
        NULL, NULL, hInst, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
