#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RegisterPanelClasses(HINSTANCE hInst);
