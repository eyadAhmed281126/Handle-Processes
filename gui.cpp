#define _WIN32_WINNT 0x0601
#define NOMINMAX

#include "gui.h"
#include "theme.h"
#include "scheduler.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <functional>

// ─── Color Palette ─────────────────────────────────────────────────────────────
// Deep blue-black base with vibrant, carefully chosen accents
#define SM_BG           RGB( 10,  12,  20)   // Near-black background
#define SM_PANEL        RGB( 18,  22,  36)   // Card surfaces
#define SM_PANEL2       RGB( 24,  29,  46)   // Alternate row / input bg
#define SM_PANEL3       RGB( 30,  36,  58)   // Hover / selected state
#define SM_HEADER_BAR   RGB( 14,  17,  28)   // Title bar
#define SM_SIDEBAR      RGB( 15,  18,  30)   // Left sidebar
#define SM_BORDER       RGB( 40,  50,  80)   // Subtle borders
#define SM_BORDER2      RGB( 55,  68, 105)   // Highlighted borders
#define SM_TEXT         RGB(230, 238, 255)   // Primary text
#define SM_TEXT2        RGB(140, 158, 200)   // Secondary / muted text
#define SM_TEXT3        RGB( 60,  75, 115)   // Placeholder / disabled text
#define SM_ACCENT       RGB( 99, 162, 255)   // Primary accent blue
#define SM_ACCENT2      RGB( 52, 211, 153)   // Secondary accent teal
#define SM_YELLOW       RGB(255, 200,  60)   // Warning / highlight yellow
#define SM_GREEN        RGB( 52, 211, 120)   // Success green
#define SM_WHITE        RGB(255, 255, 255)
#define SM_RED          RGB(255,  80,  80)   // Error red

// Algorithm brand colors
#define SM_RR           RGB( 99, 162, 255)   // Round Robin — cool blue
#define SM_SRTF         RGB( 52, 211, 153)   // SRTF — mint teal

// Gantt block gradient pairs (top/bottom) — rich, saturated but readable on dark bg
COLORREF PROC_COLORS[N_PROC_COLORS] = {
    RGB( 99, 162, 255),   // P0 — sky blue
    RGB(186, 110, 255),   // P1 — violet
    RGB( 52, 211, 153),   // P2 — mint
    RGB(255, 160,  72),   // P3 — amber
    RGB(255,  85,  85),   // P4 — coral red
    RGB(255, 215,  65),   // P5 — gold
    RGB( 48, 220, 240),   // P6 — cyan
    RGB(250,  85, 140),   // P7 — pink
    RGB(155, 220,  90),   // P8 — lime
    RGB(165, 112, 255),   // P9 — purple
};

COLORREF ProcColor(int pid)
{
    if (pid < 0) return RGB(30, 37, 60);
    return PROC_COLORS[pid % N_PROC_COLORS];
}

// ─── Global handles ─────────────────────────────────────────────────────────────
static HWND g_hMain      = NULL;
static HWND g_hListView  = NULL;
static HWND g_hEditQ     = NULL;
static HWND g_hBtnRun    = NULL;
static HWND g_hBtnClear  = NULL;
static HWND g_hBtnAdd    = NULL;
static HWND g_hBtnDel    = NULL;
static HWND g_hBtnA, g_hBtnB, g_hBtnC, g_hBtnD, g_hBtnE;
static HWND g_hGanttRR   = NULL;
static HWND g_hGanttSRTF = NULL;
static HWND g_hResRR     = NULL;
static HWND g_hResSRTF   = NULL;
static HWND g_hSummary   = NULL;
static HWND g_hConclusion = NULL;

// Fonts
static HFONT g_hFontTitle  = NULL;
static HFONT g_hFontHeader = NULL;
static HFONT g_hFontNormal = NULL;
static HFONT g_hFontSmall  = NULL;
static HFONT g_hFontMono   = NULL;
static HFONT g_hFontBig    = NULL;
static HFONT g_hFontTiny   = NULL;

// Brushes
static HBRUSH g_hbrBG      = NULL;
static HBRUSH g_hbrPanel   = NULL;
static HBRUSH g_hbrPanel2  = NULL;
static HBRUSH g_hbrPanel3  = NULL;
static HBRUSH g_hbrSidebar = NULL;

// State
static std::vector<Process> g_processes;
static SimResult g_rrResult;
static SimResult g_srtfResult;
static bool g_hasResult = false;
static int  g_quantum   = 2;

// Procedures
static LRESULT CALLBACK GanttRRProc    (HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK GanttSRTFProc  (HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK ResultsRRProc  (HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK ResultsSRTFProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK SummaryProc    (HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK ConclusionProc (HWND, UINT, WPARAM, LPARAM);
static void CreateFontsAndBrushes();
static void DestroyFontsAndBrushes();
static void CreateControls(HWND hWnd);
static void LayoutControls(HWND hWnd);
static void RunSimulation();
static void LoadScenario(int s);
static void ClearAll();
static void RefreshListView();
static void InvalidateAllPanels();
static void AddProcess(HWND hParent);
static void PaintGantt   (HWND, HDC, const SimResult&, COLORREF, const wchar_t*, const wchar_t*);
static void PaintResults (HWND, HDC, const SimResult&, COLORREF, const wchar_t*);
static void PaintSummary   (HWND, HDC);
static void PaintConclusion(HWND, HDC);

// ─── Drawing helpers ─────────────────────────────────────────────────────────────

static void DrawText2(HDC hdc, const wchar_t* txt, RECT r, UINT fmt, COLORREF col, HFONT fnt)
{
    HFONT old = (HFONT)SelectObject(hdc, fnt);
    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, txt, -1, &r, fmt);
    SelectObject(hdc, old);
}

static std::wstring FormatDouble(double v, int dec = 2)
{
    std::wostringstream ss;
    ss << std::fixed << std::setprecision(dec) << v;
    return ss.str();
}

// Draw a smooth rounded rect via GDI RoundRect
static void DrawRoundRect(HDC hdc, RECT r, int rx, int ry, HBRUSH hbr, HPEN hpen)
{
    HPEN   op = (HPEN)  SelectObject(hdc, hpen ? hpen : (HPEN)GetStockObject(NULL_PEN));
    HBRUSH ob = (HBRUSH)SelectObject(hdc, hbr  ? hbr  : (HBRUSH)GetStockObject(NULL_BRUSH));
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, rx, ry);
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
}

// Pill badge: rounded rect with centered label
static void DrawPill(HDC hdc, RECT r, COLORREF bg, COLORREF border,
                     const wchar_t* text, COLORREF textCol, HFONT fnt)
{
    HBRUSH hbr = CreateSolidBrush(bg);
    HPEN   hpn = CreatePen(PS_SOLID, 1, border);
    DrawRoundRect(hdc, r, 14, 14, hbr, hpn);
    DeleteObject(hbr); DeleteObject(hpn);
    DrawText2(hdc, text, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE, textCol, fnt);
}

// Draw a horizontal gradient line (for title bar accent)
static void DrawGradientRect(HDC hdc, RECT r, COLORREF c1, COLORREF c2)
{
    TRIVERTEX tv[2] = {
        { r.left,  r.top,    (COLOR16)(GetRValue(c1) << 8), (COLOR16)(GetGValue(c1) << 8), (COLOR16)(GetBValue(c1) << 8), 0xFFFF },
        { r.right, r.bottom, (COLOR16)(GetRValue(c2) << 8), (COLOR16)(GetGValue(c2) << 8), (COLOR16)(GetBValue(c2) << 8), 0xFFFF }
    };
    GRADIENT_RECT gr = { 0, 1 };
    GradientFill(hdc, tv, 2, &gr, 1, GRADIENT_FILL_RECT_H);
}

// Double-buffered paint to eliminate flicker
static void DoubleBufferedPaint(HWND hWnd, std::function<void(HWND, HDC)> painter)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT rc;
    GetClientRect(hWnd, &rc);
    HDC     memDC = CreateCompatibleDC(hdc);
    HBITMAP hBmp  = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP hOld  = (HBITMAP)SelectObject(memDC, hBmp);
    painter(hWnd, memDC);
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, hOld);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    EndPaint(hWnd, &ps);
}

// ─── Registration ─────────────────────────────────────────────────────────────

void RegisterPanelClasses(HINSTANCE hInst)
{
    auto regClass = [&](const wchar_t* name, WNDPROC proc) {
        WNDCLASSEX wc   = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = proc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        wc.lpszClassName = name;
        RegisterClassEx(&wc);
    };
    regClass(L"GanttRRWnd",     GanttRRProc);
    regClass(L"GanttSRTFWnd",   GanttSRTFProc);
    regClass(L"ResultsRRWnd",   ResultsRRProc);
    regClass(L"ResultsSRTFWnd", ResultsSRTFProc);
    regClass(L"SummaryWnd",     SummaryProc);
    regClass(L"ConclusionWnd",  ConclusionProc);
}

// ─── Fonts & Brushes ─────────────────────────────────────────────────────────

static void CreateFontsAndBrushes()
{
    auto mkFont = [](int h, int w, bool italic, const wchar_t* face) {
        return CreateFont(-h, 0, 0, 0, w, italic, 0, 0,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, face);
    };
    g_hFontTitle  = mkFont(17, FW_BOLD,     false, L"Segoe UI");
    g_hFontHeader = mkFont(13, FW_SEMIBOLD, false, L"Segoe UI");
    g_hFontNormal = mkFont(13, FW_NORMAL,   false, L"Segoe UI");
    g_hFontSmall  = mkFont(12, FW_NORMAL,   false, L"Segoe UI");
    g_hFontTiny   = mkFont(11, FW_NORMAL,   false, L"Segoe UI");
    g_hFontMono   = mkFont(12, FW_NORMAL,   false, L"Consolas");
    g_hFontBig    = mkFont(24, FW_BOLD,     false, L"Segoe UI");

    g_hbrBG      = CreateSolidBrush(SM_BG);
    g_hbrPanel   = CreateSolidBrush(SM_PANEL);
    g_hbrPanel2  = CreateSolidBrush(SM_PANEL2);
    g_hbrPanel3  = CreateSolidBrush(SM_PANEL3);
    g_hbrSidebar = CreateSolidBrush(SM_SIDEBAR);
}

static void DestroyFontsAndBrushes()
{
    DeleteObject(g_hFontTitle);  DeleteObject(g_hFontHeader);
    DeleteObject(g_hFontNormal); DeleteObject(g_hFontSmall);
    DeleteObject(g_hFontTiny);   DeleteObject(g_hFontMono);
    DeleteObject(g_hFontBig);
    DeleteObject(g_hbrBG);       DeleteObject(g_hbrPanel);
    DeleteObject(g_hbrPanel2);   DeleteObject(g_hbrPanel3);
    DeleteObject(g_hbrSidebar);
}

// ─── List view ─────────────────────────────────────────────────────────────────

static void RefreshListView()
{
    ListView_DeleteAllItems(g_hListView);
    for (int i = 0; i < (int)g_processes.size(); i++) {
        LVITEM lvi   = {};
        lvi.mask     = LVIF_TEXT;
        lvi.iItem    = i;
        lvi.iSubItem = 0;
        wchar_t buf[64];
        swprintf(buf, 64, L"P%d", g_processes[i].pid);
        lvi.pszText  = buf;
        ListView_InsertItem(g_hListView, &lvi);
        swprintf(buf, 64, L"%d", g_processes[i].arrivalTime);
        ListView_SetItemText(g_hListView, i, COL_AT, buf);
        swprintf(buf, 64, L"%d", g_processes[i].burstTime);
        ListView_SetItemText(g_hListView, i, COL_BT, buf);
    }
}

static void InvalidateAllPanels()
{
    InvalidateRect(g_hGanttRR,    NULL, TRUE);
    InvalidateRect(g_hGanttSRTF,  NULL, TRUE);
    InvalidateRect(g_hResRR,      NULL, TRUE);
    InvalidateRect(g_hResSRTF,    NULL, TRUE);
    InvalidateRect(g_hSummary,    NULL, TRUE);
    InvalidateRect(g_hConclusion, NULL, TRUE);
}

// ─── Scenarios ─────────────────────────────────────────────────────────────────

static void LoadScenario(int s)
{
    g_processes.clear();
    int q = 2;
    switch (s) {
    case 0: g_processes = {{1,0,8},{2,1,4},{3,2,9},{4,3,5}}; q = 4; break;
    case 1: g_processes = {{1,0,20},{2,0,20}};               q = 2; break;
    case 2: g_processes = {{1,0,15},{2,1,2},{3,2,1},{4,3,2}};q = 5; break;
    case 3: g_processes = {{1,0,10},{2,0,10},{3,0,10}};      q = 2; break;
    case 4: g_processes = {{1,0,5},{2,2,3},{3,4,6}};         q = 0; break;
    }
    SetWindowText(g_hEditQ, std::to_wstring(q).c_str());
    RefreshListView();
    g_hasResult = false;
    InvalidateAllPanels();
}

static void RunSimulation()
{
    wchar_t qbuf[64];
    GetWindowText(g_hEditQ, qbuf, 64);
    int q = _wtoi(qbuf);

    if (q <= 0 || wcslen(qbuf) == 0) {
        MessageBox(g_hMain,
            L"Please enter a valid Time Quantum.\n\nThe quantum must be a positive whole number (1 or more).",
            L"Check Time Quantum", MB_OK | MB_ICONINFORMATION);
        SetFocus(g_hEditQ);
        return;
    }
    if (g_processes.empty()) {
        MessageBox(g_hMain,
            L"No processes added yet.\n\nUse 'Add Process' or load a pre-built example below.",
            L"No Processes", MB_OK | MB_ICONINFORMATION);
        return;
    }
    for (auto& p : g_processes) {
        if (p.arrivalTime < 0 || p.burstTime <= 0) {
            MessageBox(g_hMain,
                L"Process data looks incorrect.\n\nArrival Time must be 0 or more, and Burst Time must be at least 1.",
                L"Invalid Process Data", MB_OK | MB_ICONINFORMATION);
            return;
        }
    }

    g_quantum    = q;
    g_rrResult   = SimulateRR(g_processes, q);
    g_srtfResult = SimulateSRTF(g_processes);
    g_hasResult  = true;
    InvalidateAllPanels();
}

static void ClearAll()
{
    g_processes.clear();
    RefreshListView();
    g_hasResult = false;
    SetWindowText(g_hEditQ, L"2");
    InvalidateAllPanels();
}

// ─── "Add Process" dialog ──────────────────────────────────────────────────────

struct AddProcCtx {
    HWND hEdPid, hEdAt, hEdBt;
    int  pid, at, bt;
    bool ok, done;
};

static LRESULT CALLBACK AddProcDlgProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    AddProcCtx* c = (AddProcCtx*)GetWindowLongPtr(h, GWLP_USERDATA);
    switch (m) {
    case WM_CREATE: {
        AddProcCtx* cc = (AddProcCtx*)((CREATESTRUCT*)lp)->lpCreateParams;
        SetWindowLongPtr(h, GWLP_USERDATA, (LONG_PTR)cc);
        auto mk = [&](const wchar_t* cls, const wchar_t* txt, DWORD style,
                      int x, int y, int w, int hh, HMENU id) {
            HWND hw = CreateWindow(cls, txt, WS_CHILD | WS_VISIBLE | style,
                x, y, w, hh, h, id, GetModuleHandle(NULL), NULL);
            SendMessage(hw, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            return hw;
        };
        mk(L"STATIC", L"Process ID  (positive number):",          0, 20, 28, 195, 22, NULL);
        cc->hEdPid = mk(L"EDIT", L"1", WS_BORDER|ES_NUMBER,      225, 26, 80, 28, (HMENU)100);
        mk(L"STATIC", L"Arrival Time  (>= 0):",                   0, 20, 76, 195, 22, NULL);
        cc->hEdAt  = mk(L"EDIT", L"0", WS_BORDER|ES_NUMBER,      225, 74, 80, 28, (HMENU)101);
        mk(L"STATIC", L"Burst Time  (> 0):",                      0, 20,124, 195, 22, NULL);
        cc->hEdBt  = mk(L"EDIT", L"5", WS_BORDER|ES_NUMBER,      225,122, 80, 28, (HMENU)102);
        HWND hOk = mk(L"BUTTON", L"Add Process", BS_DEFPUSHBUTTON, 20,174,160, 36, (HMENU)IDOK);
        mk(L"BUTTON", L"Cancel", BS_PUSHBUTTON,                   198,174,112, 36, (HMENU)IDCANCEL);
        SendMessage(hOk, WM_SETFONT, (WPARAM)g_hFontHeader, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (!c) return 0;
        if (LOWORD(wp) == IDOK) {
            wchar_t pb[32], ab[32], bb[32];
            GetWindowText(c->hEdPid, pb, 32);
            GetWindowText(c->hEdAt,  ab, 32);
            GetWindowText(c->hEdBt,  bb, 32);
            int pid = _wtoi(pb), at = _wtoi(ab), bt = _wtoi(bb);
            if (pid <= 0) {
                MessageBox(h, L"Process ID must be a positive number", L"Check ID", MB_OK|MB_ICONINFORMATION);
                SetFocus(c->hEdPid); return 0;
            }
            for (auto& p : g_processes) {
                if (p.pid == pid) {
                    MessageBox(h, L"That Process ID is already in use.\nChoose a different number.",
                        L"Duplicate ID", MB_OK|MB_ICONINFORMATION);
                    SetFocus(c->hEdPid); return 0;
                }
            }
            if (at < 0 || bt <= 0 || at > 1000 || bt > 1000) {
                MessageBox(h, L"Arrival Time must be 0–1000.\nBurst Time must be 1–1000.",
                    L"Check Values", MB_OK|MB_ICONINFORMATION);
                return 0;
            }
            c->pid = pid; c->at = at; c->bt = bt;
            c->ok = true; c->done = true;
            DestroyWindow(h);
        } else if (LOWORD(wp) == IDCANCEL) {
            c->done = true; DestroyWindow(h);
        }
        return 0;
    case WM_CLOSE:
        if (c) c->done = true;
        DestroyWindow(h); return 0;
    case WM_ERASEBKGND: {
        HDC dc = (HDC)wp; RECT r;
        GetClientRect(h, &r); FillRect(dc, &r, g_hbrPanel); return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, SM_TEXT); SetBkColor(dc, SM_PANEL);
        return (LRESULT)g_hbrPanel;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, SM_TEXT); SetBkColor(dc, SM_PANEL2);
        return (LRESULT)g_hbrPanel2;
    }
    }
    return DefWindowProc(h, m, wp, lp);
}

static void AddProcess(HWND hParent)
{
    static const wchar_t* DLG_CLASS = L"AddProcDlgClass";
    static bool registered = false;
    if (!registered) {
        WNDCLASSEX wc2    = {};
        wc2.cbSize        = sizeof(wc2);
        wc2.lpfnWndProc   = AddProcDlgProc;
        wc2.hInstance     = GetModuleHandle(NULL);
        wc2.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc2.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        wc2.lpszClassName = DLG_CLASS;
        RegisterClassEx(&wc2);
        registered = true;
    }

    AddProcCtx ctx = {};
    RECT pr; GetWindowRect(hParent, &pr);
    int cx = (pr.left + pr.right)  / 2 - 165;
    int cy = (pr.top  + pr.bottom) / 2 - 120;

    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        DLG_CLASS, L"  Add New Process",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        cx, cy, 340, 260, hParent, NULL, GetModuleHandle(NULL), &ctx);
    if (!hDlg) return;

    EnableWindow(hParent, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    MSG msg;
    while (!ctx.done && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            ctx.done = true; DestroyWindow(hDlg); break;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
            continue;
        }
        TranslateMessage(&msg); DispatchMessage(&msg);
    }

    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    if (ctx.ok) {
        g_processes.push_back({ctx.pid, ctx.at, ctx.bt});
        RefreshListView();
    }
}

// ─── Controls ────────────────────────────────────────────────────────────────

static void CreateControls(HWND hWnd)
{
    HINSTANCE hInst = GetModuleHandle(NULL);

    g_hListView = CreateWindowEx(WS_EX_CLIENTEDGE,
        WC_LISTVIEW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        0, 0, 0, 0, hWnd, (HMENU)ID_LISTVIEW, hInst, NULL);
    ListView_SetExtendedListViewStyle(g_hListView,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    LVCOLUMN lvc = {};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    lvc.iSubItem = 0; lvc.cx = 72;  lvc.pszText = (LPWSTR)L"Process";
    ListView_InsertColumn(g_hListView, 0, &lvc);
    lvc.iSubItem = 1; lvc.cx = 90;  lvc.pszText = (LPWSTR)L"Arrival";
    ListView_InsertColumn(g_hListView, 1, &lvc);
    lvc.iSubItem = 2; lvc.cx = 90;  lvc.pszText = (LPWSTR)L"Burst";
    ListView_InsertColumn(g_hListView, 2, &lvc);
    SendMessage(g_hListView, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    g_hEditQ = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"2",
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER,
        0, 0, 0, 0, hWnd, (HMENU)ID_EDIT_QUANTUM, hInst, NULL);
    SendMessage(g_hEditQ, WM_SETFONT, (WPARAM)g_hFontHeader, TRUE);

    auto mkBtn = [&](const wchar_t* txt, int id, HWND& out) {
        out = CreateWindow(L"BUTTON", txt, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hWnd, (HMENU)(UINT_PTR)id, hInst, NULL);
        SendMessage(out, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    };

    mkBtn(L"▶  Run Simulation", ID_BTN_RUN,        g_hBtnRun);
    mkBtn(L"✕  Clear All",      ID_BTN_CLEAR,      g_hBtnClear);
    mkBtn(L"+  Add Process",    ID_BTN_ADD_PROC,   g_hBtnAdd);
    mkBtn(L"−  Remove",         ID_BTN_DEL_PROC,   g_hBtnDel);
    mkBtn(L"Example A",         ID_BTN_SCENARIO_A, g_hBtnA);
    mkBtn(L"Example B",         ID_BTN_SCENARIO_B, g_hBtnB);
    mkBtn(L"Example C",         ID_BTN_SCENARIO_C, g_hBtnC);
    mkBtn(L"Example D",         ID_BTN_SCENARIO_D, g_hBtnD);
    mkBtn(L"Example E",         ID_BTN_SCENARIO_E, g_hBtnE);

    g_hGanttRR    = CreateWindow(L"GanttRRWnd",     L"", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, (HMENU)ID_GANTT_RR,    hInst, NULL);
    g_hGanttSRTF  = CreateWindow(L"GanttSRTFWnd",   L"", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, (HMENU)ID_GANTT_SRTF,  hInst, NULL);
    g_hResRR      = CreateWindow(L"ResultsRRWnd",   L"", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, (HMENU)ID_RESULTS_RR,  hInst, NULL);
    g_hResSRTF    = CreateWindow(L"ResultsSRTFWnd", L"", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, (HMENU)ID_RESULTS_SRTF, hInst, NULL);
    g_hSummary    = CreateWindow(L"SummaryWnd",     L"", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, (HMENU)ID_SUMMARY,     hInst, NULL);
    g_hConclusion = CreateWindow(L"ConclusionWnd",  L"", WS_CHILD|WS_VISIBLE, 0,0,0,0, hWnd, (HMENU)ID_CONCLUSION,  hInst, NULL);
}

static void LayoutControls(HWND hWnd)
{
    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right, H = rc.bottom;
    const int pad = 12, leftW = 304;
    int rightX = leftW + pad;
    int rightW = W - rightX - pad;

    int y = 80;
    const int lvH = 182;
    SetWindowPos(g_hListView, NULL, pad, y, leftW - pad, lvH, SWP_NOZORDER);
    y += lvH + 8;

    int btnW2 = (leftW - pad - 6) / 2;
    SetWindowPos(g_hBtnAdd, NULL, pad,             y, btnW2, 30, SWP_NOZORDER);
    SetWindowPos(g_hBtnDel, NULL, pad + btnW2 + 6, y, btnW2, 30, SWP_NOZORDER);
    y += 40;

    // Quantum label is painted in WM_PAINT — just position the edit box
    SetWindowPos(g_hEditQ, NULL, pad + 168, y, 68, 28, SWP_NOZORDER);
    y += 42;

    int btnW = (leftW - pad - 6) / 2;
    SetWindowPos(g_hBtnRun,   NULL, pad,            y, btnW, 36, SWP_NOZORDER);
    SetWindowPos(g_hBtnClear, NULL, pad + btnW + 6, y, btnW, 36, SWP_NOZORDER);
    y += 52;

    // Separator label + scenario buttons
    int sbW = (leftW - pad - 6) / 2, sbH = 28;
    SetWindowPos(g_hBtnA, NULL, pad,           y, sbW, sbH, SWP_NOZORDER);
    SetWindowPos(g_hBtnB, NULL, pad + sbW + 6, y, sbW, sbH, SWP_NOZORDER);
    y += sbH + 5;
    SetWindowPos(g_hBtnC, NULL, pad,           y, sbW, sbH, SWP_NOZORDER);
    SetWindowPos(g_hBtnD, NULL, pad + sbW + 6, y, sbW, sbH, SWP_NOZORDER);
    y += sbH + 5;
    SetWindowPos(g_hBtnE, NULL, pad,           y, leftW - pad, sbH, SWP_NOZORDER);

    // Panels on the right
    const int ganttH = 145, halfW = (rightW - pad) / 2;
    int panelY = 58;
    SetWindowPos(g_hGanttRR,   NULL, rightX,               panelY, halfW, ganttH, SWP_NOZORDER);
    SetWindowPos(g_hGanttSRTF, NULL, rightX + halfW + pad, panelY, halfW, ganttH, SWP_NOZORDER);

    const int resH = 200, resY = panelY + ganttH + 8;
    SetWindowPos(g_hResRR,   NULL, rightX,               resY, halfW, resH, SWP_NOZORDER);
    SetWindowPos(g_hResSRTF, NULL, rightX + halfW + pad, resY, halfW, resH, SWP_NOZORDER);

    int sumY = resY + resH + 8;
    int sumH = std::max(H - sumY - pad, 64);
    int sumW = (rightW - pad) / 2;
    SetWindowPos(g_hSummary,    NULL, rightX,              sumY, sumW,                sumH, SWP_NOZORDER);
    SetWindowPos(g_hConclusion, NULL, rightX + sumW + pad, sumY, rightW - sumW - pad, sumH, SWP_NOZORDER);
}

// ─── Gantt chart ─────────────────────────────────────────────────────────────────
//
// Redesigned for clarity:
//  • Taller bars (48 px) with bright filled colour + 1-px bright border
//  • Process label is always shown (P-id in white, or "idle")
//  • Time ticks sit below a clean axis line with major/minor tick marks
//  • Empty-state placeholder centred in the card
//  • Card uses a 3-px left accent edge with a coloured header strip

static void PaintGantt(HWND hWnd, HDC hdc, const SimResult& res,
                       COLORREF accentColor, const wchar_t* label, const wchar_t* subtitle)
{
    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right, H = rc.bottom;

    // ── Background ──────────────────────────────────────────────────────────────
    FillRect(hdc, &rc, g_hbrPanel);

    // Rounded card outline
    {
        HPEN hpB = CreatePen(PS_SOLID, 1, SM_BORDER);
        HPEN old = (HPEN)SelectObject(hdc, hpB);
        HBRUSH hbO = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, 0, 0, W, H, 8, 8);
        SelectObject(hdc, old); SelectObject(hdc, hbO);
        DeleteObject(hpB);
    }

    // ── Header strip ────────────────────────────────────────────────────────────
    const int HEADER_H = 32;
    {
        // Dark-tinted background behind the label
        COLORREF stripFill = RGB(
            (int)(GetRValue(accentColor) * 0.22),
            (int)(GetGValue(accentColor) * 0.22),
            (int)(GetBValue(accentColor) * 0.22));
        HBRUSH hbr = CreateSolidBrush(stripFill);
        RECT sr = {1, 1, W - 1, HEADER_H};
        FillRect(hdc, &sr, hbr);
        DeleteObject(hbr);

        // 3-px accent left edge
        HBRUSH hbrEdge = CreateSolidBrush(accentColor);
        RECT er = {1, 1, 4, HEADER_H};
        FillRect(hdc, &er, hbrEdge);
        DeleteObject(hbrEdge);

        // Separator line
        HPEN hpSep = CreatePen(PS_SOLID, 1, SM_BORDER);
        HPEN hpOld = (HPEN)SelectObject(hdc, hpSep);
        MoveToEx(hdc, 1, HEADER_H, NULL); LineTo(hdc, W - 1, HEADER_H);
        SelectObject(hdc, hpOld); DeleteObject(hpSep);
    }

    // Title + subtitle
    {
        RECT tr = {12, 1, W / 2 + 20, HEADER_H};
        DrawText2(hdc, label, tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontHeader);
        RECT sr = {W / 2 + 20, 4, W - 10, HEADER_H};
        DrawText2(hdc, subtitle, sr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, SM_TEXT2, g_hFontTiny);
    }

    // ── Empty state ─────────────────────────────────────────────────────────────
    if (!g_hasResult || res.gantt.empty()) {
        RECT msg = {12, HEADER_H + 4, W - 12, H - 4};
        DrawText2(hdc, L"Run the simulation to see the chart.",
            msg, DT_CENTER | DT_VCENTER | DT_WORDBREAK, SM_TEXT3, g_hFontSmall);
        return;
    }

    // ── Layout constants ─────────────────────────────────────────────────────────
    const int barTop    = HEADER_H + 10;   // top of the bar row
    const int barH      = 46;              // bar height (tall for readability)
    const int axisY     = barTop + barH + 2; // axis line Y
    const int tickH     = 5;              // major tick height below axis
    const int labelY    = axisY + tickH + 2; // top of time labels
    const int chartLeft = 10;
    const int chartRight= W - 10;
    int chartW          = chartRight - chartLeft;

    int tEnd = 0;
    for (auto& b : res.gantt) tEnd = std::max(tEnd, b.end);
    if (tEnd == 0) return;

    float scale = (float)chartW / tEnd;

    // ── Draw bars ────────────────────────────────────────────────────────────────
    for (auto& b : res.gantt) {
        int x1 = chartLeft + (int)(b.start * scale);
        int x2 = chartLeft + (int)(b.end   * scale);
        if (x2 <= x1) x2 = x1 + 2;

        bool isIdle = (b.pid < 0);
        COLORREF col = ProcColor(b.pid);

        // Fill: process gets bright saturated colour; idle gets dim grey
        COLORREF fillCol;
        if (isIdle) {
            fillCol = RGB(28, 35, 56);
        } else {
            // Slightly desaturated so white text is legible
            fillCol = RGB(
                (int)(GetRValue(col) * 0.55 + 10),
                (int)(GetGValue(col) * 0.55 + 10),
                (int)(GetBValue(col) * 0.55 + 10));
        }

        HBRUSH hbr = CreateSolidBrush(fillCol);
        HPEN   hpn = CreatePen(PS_SOLID, 1, isIdle ? SM_BORDER : col);
        RECT br = {x1 + 1, barTop, x2 - 1, barTop + barH};
        DrawRoundRect(hdc, br, 5, 5, hbr, hpn);
        DeleteObject(hbr); DeleteObject(hpn);

        // ── Process label inside bar ───────────────────────────────────────────
        // Always draw the label — scale it to available width
        int bw = x2 - x1;
        if (bw > 10) {
            wchar_t lbl[16];
            if (isIdle) swprintf(lbl, 16, L"idle");
            else        swprintf(lbl, 16, L"P%d", b.pid);

            RECT lr = {x1 + 1, barTop, x2 - 1, barTop + barH};

            // Choose readable text colour: bright accent over dark fill
            COLORREF tc = isIdle ? SM_TEXT3 : col;

            // If bar is very narrow, draw label in tiny font; otherwise normal
            HFONT lf = (bw < 26) ? g_hFontTiny : g_hFontSmall;
            DrawText2(hdc, lbl, lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE, tc, lf);
        }

        // ── Tick mark at start boundary ────────────────────────────────────────
        {
            HPEN hpT = CreatePen(PS_SOLID, 1, SM_BORDER2);
            HPEN hpO = (HPEN)SelectObject(hdc, hpT);
            MoveToEx(hdc, x1, barTop, NULL);
            LineTo(hdc, x1, barTop + barH);
            SelectObject(hdc, hpO); DeleteObject(hpT);
        }
    }

    // ── Axis line ────────────────────────────────────────────────────────────────
    {
        HPEN hpA = CreatePen(PS_SOLID, 1, SM_BORDER2);
        HPEN hpO = (HPEN)SelectObject(hdc, hpA);
        MoveToEx(hdc, chartLeft, axisY, NULL); LineTo(hdc, chartRight, axisY);
        SelectObject(hdc, hpO); DeleteObject(hpA);
    }

    // ── Time ticks + labels ──────────────────────────────────────────────────────
    // Choose tick step so we don't crowd the labels
    int maxTicks = chartW / 28; if (maxTicks < 1) maxTicks = 1;
    int step = std::max(1, (int)ceil((double)tEnd / maxTicks));

    for (int t = 0; t <= tEnd; t += step) {
        int tx = chartLeft + (int)(t * scale);

        // Tick mark
        {
            HPEN hpT = CreatePen(PS_SOLID, 1, SM_BORDER2);
            HPEN hpO = (HPEN)SelectObject(hdc, hpT);
            MoveToEx(hdc, tx, axisY, NULL);
            LineTo(hdc, tx, axisY + tickH);
            SelectObject(hdc, hpO); DeleteObject(hpT);
        }

        // Dashed vertical guide line up through bars (subtle)
        {
            HPEN hpG = CreatePen(PS_DOT, 1, RGB(45, 56, 86));
            HPEN hpO = (HPEN)SelectObject(hdc, hpG);
            SetBkMode(hdc, TRANSPARENT);
            MoveToEx(hdc, tx, barTop, NULL);
            LineTo(hdc, tx, axisY);
            SelectObject(hdc, hpO); DeleteObject(hpG);
        }

        // Numeric label
        wchar_t tstr[16]; swprintf(tstr, 16, L"%d", t);
        RECT tr = {tx - 14, labelY, tx + 14, labelY + 14};
        DrawText2(hdc, tstr, tr, DT_CENTER | DT_SINGLELINE, SM_TEXT2, g_hFontTiny);
    }

    // Mark end time if it wasn't covered by a step
    if (tEnd % step != 0) {
        int tx = chartLeft + (int)(tEnd * scale);
        HPEN hpT = CreatePen(PS_SOLID, 1, SM_BORDER2);
        HPEN hpO = (HPEN)SelectObject(hdc, hpT);
        MoveToEx(hdc, tx, axisY, NULL); LineTo(hdc, tx, axisY + tickH);
        SelectObject(hdc, hpO); DeleteObject(hpT);
        wchar_t tstr[16]; swprintf(tstr, 16, L"%d", tEnd);
        RECT tr = {tx - 14, labelY, tx + 14, labelY + 14};
        DrawText2(hdc, tstr, tr, DT_CENTER | DT_SINGLELINE, SM_TEXT2, g_hFontTiny);
    }
}

// ─── Results table ─────────────────────────────────────────────────────────────

static void PaintResults(HWND hWnd, HDC hdc, const SimResult& res,
                         COLORREF accentColor, const wchar_t* label)
{
    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right, H = rc.bottom;

    FillRect(hdc, &rc, g_hbrPanel);

    // Card outline
    {
        HPEN hpB = CreatePen(PS_SOLID, 1, SM_BORDER);
        HPEN old = (HPEN)SelectObject(hdc, hpB);
        HBRUSH hbO = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, 0, 0, W, H, 8, 8);
        SelectObject(hdc, old); SelectObject(hdc, hbO);
        DeleteObject(hpB);
    }

    const int HEADER_H = 32;

    // Header strip
    {
        COLORREF stripFill = RGB(
            (int)(GetRValue(accentColor) * 0.22),
            (int)(GetGValue(accentColor) * 0.22),
            (int)(GetBValue(accentColor) * 0.22));
        HBRUSH hbr = CreateSolidBrush(stripFill);
        RECT sr = {1, 1, W - 1, HEADER_H};
        FillRect(hdc, &sr, hbr);
        DeleteObject(hbr);

        HBRUSH hbrEdge = CreateSolidBrush(accentColor);
        RECT er = {1, 1, 4, HEADER_H};
        FillRect(hdc, &er, hbrEdge);
        DeleteObject(hbrEdge);

        HPEN hpSep = CreatePen(PS_SOLID, 1, SM_BORDER);
        HPEN hpOld = (HPEN)SelectObject(hdc, hpSep);
        MoveToEx(hdc, 1, HEADER_H, NULL); LineTo(hdc, W - 1, HEADER_H);
        SelectObject(hdc, hpOld); DeleteObject(hpSep);
    }

    RECT trText = {12, 1, W - 8, HEADER_H};
    DrawText2(hdc, label, trText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontHeader);

    if (!g_hasResult || res.results.empty()) {
        RECT msg = {12, HEADER_H + 4, W - 12, H - 4};
        DrawText2(hdc, L"Results will appear here after running the simulation.",
            msg, DT_CENTER | DT_VCENTER | DT_WORDBREAK, SM_TEXT3, g_hFontSmall);
        return;
    }

    // Column definitions
    const wchar_t* cols[] = {L"", L"Proc", L"Arr", L"Burst", L"Finish", L"Turnaround", L"Wait", L"Response"};
    const int nCols = 8;
    const int dotColW = 18;
    int colW = (W - 14 - dotColW) / (nCols - 1);
    const int rowH = 20;
    int tableTop = HEADER_H + 2;

    // Column header row
    {
        HBRUSH hbrHead = CreateSolidBrush(SM_PANEL2);
        RECT hr = {6, tableTop, W - 6, tableTop + rowH};
        FillRect(hdc, &hr, hbrHead); DeleteObject(hbrHead);
    }
    for (int c = 1; c < nCols; c++) {
        RECT cr = {6 + dotColW + (c-1)*colW, tableTop, 6 + dotColW + c*colW, tableTop + rowH};
        DrawText2(hdc, cols[c], cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE, SM_TEXT2, g_hFontTiny);
    }

    int y = tableTop + rowH;
    for (int i = 0; i < (int)res.results.size() && y + rowH < H - 22; i++) {
        const ProcessResult& r = res.results[i];

        // Row background (alternating)
        {
            COLORREF rowBg = (i % 2 == 0) ? SM_PANEL : SM_PANEL2;
            HBRUSH hbrRow = CreateSolidBrush(rowBg);
            RECT rr = {6, y, W - 6, y + rowH};
            FillRect(hdc, &rr, hbrRow); DeleteObject(hbrRow);
        }

        // Colour dot
        {
            COLORREF pcol = ProcColor(r.pid);
            HBRUSH hbrDot = CreateSolidBrush(pcol);
            HPEN   hpDot  = CreatePen(PS_SOLID, 1, pcol);
            HBRUSH hbDOld = (HBRUSH)SelectObject(hdc, hbrDot);
            HPEN   hpDOld = (HPEN)  SelectObject(hdc, hpDot);
            Ellipse(hdc, 10, y + 5, 22, y + 15);
            SelectObject(hdc, hbDOld); SelectObject(hdc, hpDOld);
            DeleteObject(hbrDot); DeleteObject(hpDot);
        }

        // Values
        wchar_t vals[7][32];
        swprintf(vals[0], 32, L"P%d", r.pid);
        swprintf(vals[1], 32, L"%d",  r.arrivalTime);
        swprintf(vals[2], 32, L"%d",  r.burstTime);
        swprintf(vals[3], 32, L"%d",  r.completionTime);
        swprintf(vals[4], 32, L"%d",  r.turnaroundTime);
        swprintf(vals[5], 32, L"%d",  r.waitingTime);
        swprintf(vals[6], 32, L"%d",  r.responseTime);

        for (int c = 0; c < 7; c++) {
            RECT cr = {6 + dotColW + c*colW, y, 6 + dotColW + (c+1)*colW, y + rowH};
            DrawText2(hdc, vals[c], cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontTiny);
        }
        y += rowH;
    }

    // Averages footer
    if (y + rowH < H - 2) {
        // Separator
        {
            HPEN hpLine = CreatePen(PS_SOLID, 1, SM_BORDER);
            HPEN hpLOld = (HPEN)SelectObject(hdc, hpLine);
            MoveToEx(hdc, 6, y, NULL); LineTo(hdc, W - 6, y);
            SelectObject(hdc, hpLOld); DeleteObject(hpLine);
        }
        // Dim highlight row
        {
            HBRUSH hbrAvg = CreateSolidBrush(RGB(20, 32, 56));
            RECT ar = {6, y, W - 6, y + rowH};
            FillRect(hdc, &ar, hbrAvg); DeleteObject(hbrAvg);
        }

        wchar_t avgWT[32], avgTAT[32], avgRT[32];
        swprintf(avgWT,  32, L"%.2f", res.avgWT);
        swprintf(avgTAT, 32, L"%.2f", res.avgTAT);
        swprintf(avgRT,  32, L"%.2f", res.avgRT);

        RECT lblR = {6 + dotColW, y, 6 + dotColW + colW * 3, y + rowH};
        DrawText2(hdc, L"Averages →", lblR, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, SM_TEXT2, g_hFontTiny);

        const wchar_t* avgVals[] = {L"", L"", L"", L"", avgTAT, avgWT, avgRT};
        for (int c = 4; c < 7; c++) {
            RECT cr = {6 + dotColW + c*colW, y, 6 + dotColW + (c+1)*colW, y + rowH};
            DrawText2(hdc, avgVals[c], cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE, SM_YELLOW, g_hFontTiny);
        }
    }
}

// ─── Summary panel ─────────────────────────────────────────────────────────────

static void PaintSummary(HWND hWnd, HDC hdc)
{
    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right, H = rc.bottom;

    FillRect(hdc, &rc, g_hbrPanel);

    // Card outline
    {
        HPEN hpB = CreatePen(PS_SOLID, 1, SM_BORDER);
        HPEN old = (HPEN)SelectObject(hdc, hpB);
        HBRUSH hbO = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, 0, 0, W, H, 8, 8);
        SelectObject(hdc, old); SelectObject(hdc, hbO);
        DeleteObject(hpB);
    }

    // Header
    const int HEADER_H = 32;
    {
        HBRUSH hbrT = CreateSolidBrush(SM_PANEL2);
        RECT tr = {1, 1, W - 1, HEADER_H};
        FillRect(hdc, &tr, hbrT); DeleteObject(hbrT);
        HPEN hpSep = CreatePen(PS_SOLID, 1, SM_BORDER);
        HPEN hpOld = (HPEN)SelectObject(hdc, hpSep);
        MoveToEx(hdc, 1, HEADER_H, NULL); LineTo(hdc, W - 1, HEADER_H);
        SelectObject(hdc, hpOld); DeleteObject(hpSep);
    }
    RECT trText = {12, 1, W, HEADER_H};
    DrawText2(hdc, L"Side-by-Side Comparison", trText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontHeader);

    if (!g_hasResult) {
        RECT msg = {12, HEADER_H + 6, W - 12, H - 8};
        DrawText2(hdc, L"Run a simulation to compare the two algorithms.",
            msg, DT_CENTER | DT_VCENTER | DT_WORDBREAK, SM_TEXT3, g_hFontSmall);
        return;
    }

    struct Metric { const wchar_t* name; double rr; double srtf; };
    Metric metrics[] = {
        {L"Avg Waiting Time",    g_rrResult.avgWT,  g_srtfResult.avgWT},
        {L"Avg Turnaround",      g_rrResult.avgTAT, g_srtfResult.avgTAT},
        {L"Avg Response Time",   g_rrResult.avgRT,  g_srtfResult.avgRT},
    };

    int y = HEADER_H + 2;
    const int rowH = 24;
    const int col1 = 8, col3 = W - 8;
    const int third = W / 3;

    // Column headers
    {
        HBRUSH hbrHh = CreateSolidBrush(SM_PANEL2);
        RECT hhr = {col1, y, col3, y + 20};
        FillRect(hdc, &hhr, hbrHh); DeleteObject(hbrHh);
    }
    {
        RECT lblR = {col1 + 6, y, col1 + third, y + 20};
        DrawText2(hdc, L"Metric", lblR, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT2, g_hFontTiny);

        RECT rrR = {col1 + third + 4, y + 2, col1 + 2*third - 4, y + 18};
        DrawPill(hdc, rrR, RGB(14, 32, 72), SM_RR, L"Round Robin", SM_RR, g_hFontTiny);

        RECT srR = {col1 + 2*third + 4, y + 2, col3 - 4, y + 18};
        DrawPill(hdc, srR, RGB(10, 48, 38), SM_SRTF, L"SRTF", SM_SRTF, g_hFontTiny);
    }
    y += 20;

    for (int i = 0; i < 3; i++) {
        Metric& m = metrics[i];
        bool rrBetter   = m.rr   < m.srtf;
        bool srtfBetter = m.srtf < m.rr;

        // Row background
        {
            HBRUSH hbrR = CreateSolidBrush((i % 2 == 0) ? SM_PANEL : SM_PANEL2);
            RECT rrf = {col1, y, col3, y + rowH};
            FillRect(hdc, &rrf, hbrR); DeleteObject(hbrR);
        }

        RECT nameR = {col1 + 6, y, col1 + third, y + rowH};
        DrawText2(hdc, m.name, nameR, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontTiny);

        wchar_t rrVal[32], srtfVal[32];
        swprintf(rrVal,   32, L"%.2f %ls", m.rr,   rrBetter   ? L"✓" : L"");
        swprintf(srtfVal, 32, L"%.2f %ls", m.srtf, srtfBetter ? L"✓" : L"");

        RECT rrV = {col1 + third, y, col1 + 2*third, y + rowH};
        DrawText2(hdc, rrVal, rrV, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
            rrBetter ? SM_GREEN : SM_TEXT, g_hFontTiny);

        RECT srV = {col1 + 2*third, y, col3, y + rowH};
        DrawText2(hdc, srtfVal, srV, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
            srtfBetter ? SM_GREEN : SM_TEXT, g_hFontTiny);

        y += rowH;
    }

    // Fairness row
    y += 4;
    if (!g_rrResult.results.empty() && !g_srtfResult.results.empty()) {
        double sumRR = 0, sumSRTF = 0;
        for (auto& r : g_rrResult.results)   sumRR   += r.waitingTime;
        for (auto& r : g_srtfResult.results) sumSRTF += r.waitingTime;
        double avgRR   = sumRR   / g_rrResult.results.size();
        double avgSRTF = sumSRTF / g_srtfResult.results.size();
        double varRR = 0, varSRTF = 0;
        for (auto& r : g_rrResult.results)
            varRR   += (r.waitingTime - avgRR)   * (r.waitingTime - avgRR);
        for (auto& r : g_srtfResult.results)
            varSRTF += (r.waitingTime - avgSRTF) * (r.waitingTime - avgSRTF);
        double cvRR   = (avgRR   > 0) ? sqrt(varRR   / g_rrResult.results.size())   / avgRR   : 0;
        double cvSRTF = (avgSRTF > 0) ? sqrt(varSRTF / g_srtfResult.results.size()) / avgSRTF : 0;

        // Amber highlight row
        {
            HBRUSH hbrF = CreateSolidBrush(RGB(38, 30, 8));
            RECT fairR = {col1, y, col3, y + 28};
            FillRect(hdc, &fairR, hbrF); DeleteObject(hbrF);
        }
        wchar_t fairStr[256];
        swprintf(fairStr, 256,
            L"Fairness: RR %.2f   SRTF %.2f   (%ls is fairer)",
            cvRR, cvSRTF, (cvRR < cvSRTF) ? L"RR" : L"SRTF");
        RECT ft = {col1 + 6, y, col3 - 4, y + 28};
        DrawText2(hdc, fairStr, ft, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_YELLOW, g_hFontTiny);
        y += 32;
    }

    // Footer: quantum + count
    wchar_t qstr[128];
    swprintf(qstr, 128, L"Quantum Q = %d   |   %d process(es)", g_quantum, (int)g_processes.size());
    RECT qR = {col1 + 6, y, col3, y + 18};
    DrawText2(hdc, qstr, qR, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT3, g_hFontTiny);
}

// ─── Analysis / Conclusion panel ─────────────────────────────────────────────────

static void PaintConclusion(HWND hWnd, HDC hdc)
{
    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right, H = rc.bottom;

    FillRect(hdc, &rc, g_hbrPanel);

    // Card outline
    {
        HPEN hpB = CreatePen(PS_SOLID, 1, SM_BORDER);
        HPEN old = (HPEN)SelectObject(hdc, hpB);
        HBRUSH hbO = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, 0, 0, W, H, 8, 8);
        SelectObject(hdc, old); SelectObject(hdc, hbO);
        DeleteObject(hpB);
    }

    const int HEADER_H = 32;
    {
        HBRUSH hbrT = CreateSolidBrush(SM_PANEL2);
        RECT tr = {1, 1, W - 1, HEADER_H};
        FillRect(hdc, &tr, hbrT); DeleteObject(hbrT);
        HPEN hpSep = CreatePen(PS_SOLID, 1, SM_BORDER);
        HPEN hpOld = (HPEN)SelectObject(hdc, hpSep);
        MoveToEx(hdc, 1, HEADER_H, NULL); LineTo(hdc, W - 1, HEADER_H);
        SelectObject(hdc, hpOld); DeleteObject(hpSep);
    }
    RECT trText = {12, 1, W, HEADER_H};
    DrawText2(hdc, L"Analysis & Takeaways", trText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontHeader);

    if (!g_hasResult) {
        RECT msg = {12, HEADER_H + 6, W - 12, H - 8};
        DrawText2(hdc, L"Key insights will appear here after running the simulation.",
            msg, DT_CENTER | DT_VCENTER | DT_WORDBREAK, SM_TEXT3, g_hFontSmall);
        return;
    }

    std::wstring txt;
    txt += L"WAITING TIME\r\n";
    txt += (g_rrResult.avgWT < g_srtfResult.avgWT) ? L"Round Robin" : L"SRTF";
    txt += L" had a shorter average waiting time (";
    txt += FormatDouble(std::min(g_rrResult.avgWT,  g_srtfResult.avgWT));
    txt += L" vs ";
    txt += FormatDouble(std::max(g_rrResult.avgWT,  g_srtfResult.avgWT));
    txt += L"). Lower is better — processes spent less time in the queue.\r\n\r\n";

    txt += L"RESPONSE TIME\r\n";
    txt += (g_rrResult.avgRT < g_srtfResult.avgRT) ? L"Round Robin" : L"SRTF";
    txt += L" responded faster on average (";
    txt += FormatDouble(std::min(g_rrResult.avgRT,  g_srtfResult.avgRT));
    txt += L" vs ";
    txt += FormatDouble(std::max(g_rrResult.avgRT,  g_srtfResult.avgRT));
    txt += L"). Matters in interactive systems expecting quick feedback.\r\n\r\n";

    txt += L"FAIRNESS\r\n";
    txt += L"Round Robin gives every process equal time-slices (Q=";
    txt += std::to_wstring(g_quantum);
    txt += L"). SRTF can starve long jobs — always picking the shortest remaining job.\r\n\r\n";

    txt += L"QUANTUM (Q=" + std::to_wstring(g_quantum) + L")\r\n";
    if      (g_quantum <= 2) txt += L"Very small: lots of context switches, fastest response times.";
    else if (g_quantum <= 5) txt += L"Medium: good balance between responsiveness and efficiency.";
    else                     txt += L"Large: fewer switches, but less fair. Approaches FCFS behaviour.";
    txt += L"\r\n\r\n";

    txt += L"VERDICT\r\n";
    bool rrBetter = (g_rrResult.avgWT + g_rrResult.avgRT) < (g_srtfResult.avgWT + g_srtfResult.avgRT);
    txt += rrBetter
        ? L"For this workload, Round Robin performed better overall. Good for time-sharing systems."
        : L"For this workload, SRTF performed better overall. Suits batch systems where short jobs finish first.";

    RECT textR = {12, HEADER_H + 6, W - 12, H - 8};
    DrawText2(hdc, txt.c_str(), textR, DT_LEFT | DT_WORDBREAK, SM_TEXT, g_hFontTiny);
}

// ─── Panel WndProcs ────────────────────────────────────────────────────────────

static LRESULT CALLBACK GanttRRProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        DoubleBufferedPaint(hWnd, [](HWND h, HDC dc) {
            PaintGantt(h, dc, g_rrResult, SM_RR, L"Round Robin — Gantt Chart", L"time-sharing");
        });
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProc(hWnd, msg, wp, lp);
}

static LRESULT CALLBACK GanttSRTFProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        DoubleBufferedPaint(hWnd, [](HWND h, HDC dc) {
            PaintGantt(h, dc, g_srtfResult, SM_SRTF, L"SRTF — Gantt Chart", L"shortest remaining");
        });
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProc(hWnd, msg, wp, lp);
}

static LRESULT CALLBACK ResultsRRProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        DoubleBufferedPaint(hWnd, [](HWND h, HDC dc) {
            PaintResults(h, dc, g_rrResult, SM_RR, L"Round Robin — Results");
        });
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProc(hWnd, msg, wp, lp);
}

static LRESULT CALLBACK ResultsSRTFProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        DoubleBufferedPaint(hWnd, [](HWND h, HDC dc) {
            PaintResults(h, dc, g_srtfResult, SM_SRTF, L"SRTF — Results");
        });
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProc(hWnd, msg, wp, lp);
}

static LRESULT CALLBACK SummaryProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        DoubleBufferedPaint(hWnd, [](HWND h, HDC dc) { PaintSummary(h, dc); });
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProc(hWnd, msg, wp, lp);
}

static LRESULT CALLBACK ConclusionProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        DoubleBufferedPaint(hWnd, [](HWND h, HDC dc) { PaintConclusion(h, dc); });
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProc(hWnd, msg, wp, lp);
}

// ─── Main Window ──────────────────────────────────────────────────────────────

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        g_hMain = hWnd;
        CreateFontsAndBrushes();
        CreateControls(hWnd);
        LoadScenario(0);
        return 0;

    case WM_SIZE:
        LayoutControls(hWnd);
        InvalidateRect(hWnd, NULL, TRUE);
        InvalidateAllPanels();
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        int W = rc.right;

        // Full dark background
        FillRect(hdc, &rc, g_hbrBG);

        // ── Title / top bar ────────────────────────────────────────────────────
        const int TOPBAR_H = 52;
        {
            RECT barR = {0, 0, W, TOPBAR_H};
            HBRUSH hbrBar = CreateSolidBrush(SM_HEADER_BAR);
            FillRect(hdc, &barR, hbrBar); DeleteObject(hbrBar);

            // Very thin bottom border on the title bar
            HPEN hpSep = CreatePen(PS_SOLID, 1, SM_BORDER);
            HPEN hpOld = (HPEN)SelectObject(hdc, hpSep);
            MoveToEx(hdc, 0, TOPBAR_H - 1, NULL); LineTo(hdc, W, TOPBAR_H - 1);
            SelectObject(hdc, hpOld); DeleteObject(hpSep);

            // Gradient accent bar at top (blue → teal over ~300 px, then fades)
            DrawGradientRect(hdc, {0, 0, 260, 2}, SM_RR, SM_SRTF);
            RECT fade = {260, 0, W, 2};
            HBRUSH hbrFade = CreateSolidBrush(SM_BG);
            FillRect(hdc, &fade, hbrFade); DeleteObject(hbrFade);
        }

        // App title
        RECT t1 = {16, 8, W / 2, TOPBAR_H - 4};
        DrawText2(hdc, L"CPU Scheduling Simulator", t1, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontTitle);

        // Algorithm pills
        RECT rrTag = {W / 2, 14, W / 2 + 115, 38};
        DrawPill(hdc, rrTag, RGB(14, 32, 72), SM_RR, L"Round Robin", SM_RR, g_hFontSmall);

        RECT vsR = {W / 2 + 120, 14, W / 2 + 138, 38};
        DrawText2(hdc, L"vs", vsR, DT_CENTER | DT_VCENTER | DT_SINGLELINE, SM_TEXT3, g_hFontSmall);

        RECT srTag = {W / 2 + 142, 14, W / 2 + 206, 38};
        DrawPill(hdc, srTag, RGB(10, 48, 38), SM_SRTF, L"SRTF", SM_SRTF, g_hFontSmall);

        // ── Left sidebar ──────────────────────────────────────────────────────
        const int SIDEBAR_W = 304;
        {
            RECT sideR = {0, TOPBAR_H, SIDEBAR_W, rc.bottom};
            FillRect(hdc, &sideR, g_hbrSidebar);

            // Right edge line
            HPEN hpSide = CreatePen(PS_SOLID, 1, SM_BORDER);
            HPEN hpSOld = (HPEN)SelectObject(hdc, hpSide);
            MoveToEx(hdc, SIDEBAR_W - 1, TOPBAR_H, NULL);
            LineTo(hdc, SIDEBAR_W - 1, rc.bottom);
            SelectObject(hdc, hpSOld); DeleteObject(hpSide);
        }

        // Sidebar section labels
        {
            RECT lbl1 = {14, 60, SIDEBAR_W - 6, 76};
            DrawText2(hdc, L"PROCESSES", lbl1, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT3, g_hFontTiny);
        }

        // Quantum label (positioned above the edit box in LayoutControls)
        {
            int lvH = 182;
            int yQ  = TOPBAR_H + 8 + lvH + 8 + 32 + 6; // matches LayoutControls
            RECT lbl2 = {14, yQ + 4, 170, yQ + 30};
            DrawText2(hdc, L"Time Quantum (Q):", lbl2, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontNormal);
        }

        // Pre-built examples separator
        {
            int lvH = 182;
            int ySepTop = TOPBAR_H + 8 + lvH + 8 + 32 + 6 + 42 + 52;
            HPEN hpSep = CreatePen(PS_SOLID, 1, SM_BORDER);
            HPEN hpSOld = (HPEN)SelectObject(hdc, hpSep);
            MoveToEx(hdc, 14, ySepTop, NULL); LineTo(hdc, SIDEBAR_W - 16, ySepTop);
            SelectObject(hdc, hpSOld); DeleteObject(hpSep);

            RECT lbl3 = {14, ySepTop + 5, SIDEBAR_W - 6, ySepTop + 22};
            DrawText2(hdc, L"PRE-BUILT EXAMPLES", lbl3, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT3, g_hFontTiny);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }

    // ── Theming ────────────────────────────────────────────────────────────────
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, SM_SIDEBAR); SetTextColor(hdc, SM_TEXT);
        return (LRESULT)g_hbrSidebar;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, SM_PANEL2); SetTextColor(hdc, SM_TEXT);
        return (LRESULT)g_hbrPanel2;
    }
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, SM_SIDEBAR); SetTextColor(hdc, SM_TEXT);
        return (LRESULT)g_hbrSidebar;
    }

    // ── ListView custom draw ───────────────────────────────────────────────────
    case WM_NOTIFY: {
        NMHDR* nmh = (NMHDR*)lParam;
        if (nmh->idFrom == ID_LISTVIEW && nmh->code == NM_CUSTOMDRAW) {
            NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)lParam;
            switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT:    return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT: {
                int idx = (int)cd->nmcd.dwItemSpec;
                cd->clrTextBk = (idx % 2 == 0) ? SM_PANEL : SM_PANEL2;
                cd->clrText   = SM_TEXT;
                return CDRF_NEWFONT;
            }
            }
        }
        return CDRF_DODEFAULT;
    }

    // ── Button commands ────────────────────────────────────────────────────────
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN_RUN:      RunSimulation();  break;
        case ID_BTN_CLEAR:    ClearAll();       break;
        case ID_BTN_ADD_PROC: AddProcess(hWnd); break;
        case ID_BTN_DEL_PROC: {
            int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (sel >= 0 && sel < (int)g_processes.size()) {
                g_processes.erase(g_processes.begin() + sel);
                for (int i = 0; i < (int)g_processes.size(); i++)
                    g_processes[i].pid = i + 1;
                RefreshListView();
            }
            break;
        }
        case ID_BTN_SCENARIO_A: LoadScenario(0); break;
        case ID_BTN_SCENARIO_B: LoadScenario(1); break;
        case ID_BTN_SCENARIO_C: LoadScenario(2); break;
        case ID_BTN_SCENARIO_D: LoadScenario(3); break;
        case ID_BTN_SCENARIO_E: LoadScenario(4); break;
        }
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)lParam;
        mm->ptMinTrackSize.x = 960;
        mm->ptMinTrackSize.y = 640;
        return 0;
    }

    case WM_DESTROY:
        DestroyFontsAndBrushes();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
