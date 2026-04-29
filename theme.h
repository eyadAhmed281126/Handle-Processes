#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#define C_BG       RGB(15, 17, 26)
#define C_PANEL    RGB(22, 27, 42)
#define C_PANEL2   RGB(28, 35, 54)
#define C_BORDER   RGB(45, 55, 85)
#define C_ACCENT   RGB(99, 179, 237)
#define C_ACCENT2  RGB(154, 117, 234)
#define C_GREEN    RGB(72, 199, 142)
#define C_YELLOW   RGB(251, 211, 80)
#define C_TEXT     RGB(226, 232, 240)
#define C_TEXT2    RGB(148, 163, 184)
#define C_TEXT3    RGB(71, 85, 105)
#define C_RR       RGB(99, 179, 237)
#define C_SRTF     RGB(154, 117, 234)
#define C_WHITE    RGB(255, 255, 255)

#define N_PROC_COLORS 10
extern COLORREF PROC_COLORS[N_PROC_COLORS];

COLORREF ProcColor(int pid);

#define ID_BTN_RUN        201
#define ID_BTN_CLEAR      202
#define ID_BTN_SCENARIO_A 211
#define ID_BTN_SCENARIO_B 212
#define ID_BTN_SCENARIO_C 213
#define ID_BTN_SCENARIO_D 214
#define ID_BTN_SCENARIO_E 215
#define ID_BTN_ADD_PROC   220
#define ID_BTN_DEL_PROC   221
#define ID_EDIT_QUANTUM   230
#define ID_LISTVIEW       240
#define ID_GANTT_RR       250
#define ID_GANTT_SRTF     251
#define ID_RESULTS_RR     260
#define ID_RESULTS_SRTF   261
#define ID_SUMMARY        270
#define ID_CONCLUSION     271

#define COL_PID 0
#define COL_AT  1
#define COL_BT  2
