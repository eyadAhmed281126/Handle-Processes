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
#define SM_BG          RGB( 18,  20,  28)
#define SM_PANEL       RGB( 26,  29,  40)
#define SM_PANEL2      RGB( 34,  38,  54)
#define SM_HEADER_BAR  RGB( 26,  29,  40)
#define SM_BORDER      RGB( 52,  58,  80)
#define SM_TEXT        RGB(220, 228, 245)
#define SM_TEXT2       RGB(130, 148, 185)
#define SM_TEXT3       RGB( 65,  78, 110)
#define SM_ACCENT      RGB( 94, 158, 255)
#define SM_ACCENT2     RGB( 56, 195, 155)
#define SM_YELLOW      RGB(255, 196,  57)
#define SM_GREEN       RGB( 52, 211, 120)
#define SM_WHITE       RGB(255, 255, 255)
#define SM_RR          RGB( 94, 158, 255)
#define SM_SRTF        RGB( 56, 195, 155)
#define SM_SIDEBAR     RGB( 22,  25,  36)
COLORREF PROC_COLORS[N_PROC_COLORS] = {
    RGB( 94, 158, 255), RGB(178, 110, 240), RGB( 56, 195, 155),
    RGB(255, 155,  70), RGB(240,  90,  90), RGB(255, 205,  60),
    RGB( 45, 215, 235), RGB(245,  80, 135), RGB(150, 210,  85),
    RGB(160, 110, 240)
};
COLORREF ProcColor(int pid)
{
    if (pid < 0) return RGB(44, 50, 70);
    return PROC_COLORS[pid % N_PROC_COLORS];
}
static HWND g_hMain        = NULL;
static HWND g_hListView    = NULL;
static HWND g_hEditQ       = NULL;
static HWND g_hBtnRun      = NULL;
static HWND g_hBtnClear    = NULL;
static HWND g_hBtnAdd      = NULL;
static HWND g_hBtnDel      = NULL;
static HWND g_hBtnA, g_hBtnB, g_hBtnC, g_hBtnD, g_hBtnE;
static HWND g_hGanttRR     = NULL;
static HWND g_hGanttSRTF   = NULL;
static HWND g_hResRR       = NULL;
static HWND g_hResSRTF     = NULL;
static HWND g_hSummary     = NULL;
static HWND g_hConclusion  = NULL;
static HFONT  g_hFontTitle  = NULL;
static HFONT  g_hFontHeader = NULL;
static HFONT  g_hFontNormal = NULL;
static HFONT  g_hFontSmall  = NULL;
static HFONT  g_hFontMono   = NULL;
static HFONT  g_hFontBig    = NULL;
static HBRUSH g_hbrBG      = NULL;
static HBRUSH g_hbrPanel   = NULL;
static HBRUSH g_hbrPanel2  = NULL;
static HBRUSH g_hbrSidebar = NULL;
static std::vector<Process> g_processes;
static SimResult            g_rrResult;
static SimResult            g_srtfResult;
static bool                 g_hasResult = false;
static int                  g_quantum   = 2;
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
static void PaintGantt     (HWND, HDC, const SimResult&, COLORREF, const wchar_t*, const wchar_t*);
static void PaintResults   (HWND, HDC, const SimResult&, COLORREF, const wchar_t*);
static void PaintSummary   (HWND, HDC);
static void PaintConclusion(HWND, HDC);
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

static void DrawRoundRect(HDC hdc, RECT r, int rx, int ry, HBRUSH hbr, HPEN hpen)
{
    HPEN   op = (HPEN)  SelectObject(hdc, hpen ? hpen : (HPEN)GetStockObject(NULL_PEN));
    HBRUSH ob = (HBRUSH)SelectObject(hdc, hbr  ? hbr  : (HBRUSH)GetStockObject(NULL_BRUSH));
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, rx, ry);
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
}

static void DrawPill(HDC hdc, RECT r, COLORREF bg, const wchar_t* text, COLORREF textCol, HFONT fnt)
{
    HBRUSH hbr = CreateSolidBrush(bg);
    HPEN   hpn = CreatePen(PS_SOLID, 1, bg);
    DrawRoundRect(hdc, r, 12, 12, hbr, hpn);
    DeleteObject(hbr); DeleteObject(hpn);
    DrawText2(hdc, text, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE, textCol, fnt);
}

static void DoubleBufferedPaint(HWND hWnd, std::function<void(HWND, HDC)> painter)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT rc;
    GetClientRect(hWnd, &rc);
    HDC      memDC = CreateCompatibleDC(hdc);
    HBITMAP  hBmp  = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP  hOld  = (HBITMAP)SelectObject(memDC, hBmp);
    painter(hWnd, memDC);
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, hOld);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    EndPaint(hWnd, &ps);
}
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

static void CreateFontsAndBrushes()
{
    auto mkFont = [](int h, int w, bool italic, const wchar_t* face) {
        return CreateFont(-h, 0, 0, 0, w, italic, 0, 0,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, face);
    };
    g_hFontTitle  = mkFont(18, FW_BOLD,     false, L"Segoe UI");
    g_hFontHeader = mkFont(13, FW_SEMIBOLD, false, L"Segoe UI");
    g_hFontNormal = mkFont(13, FW_NORMAL,   false, L"Segoe UI");
    g_hFontSmall  = mkFont(12, FW_NORMAL,   false, L"Segoe UI");
    g_hFontMono   = mkFont(12, FW_NORMAL,   false, L"Consolas");
    g_hFontBig    = mkFont(26, FW_BOLD,     false, L"Segoe UI");

    g_hbrBG      = CreateSolidBrush(SM_BG);
    g_hbrPanel   = CreateSolidBrush(SM_PANEL);
    g_hbrPanel2  = CreateSolidBrush(SM_PANEL2);
    g_hbrSidebar = CreateSolidBrush(SM_SIDEBAR);
}

static void DestroyFontsAndBrushes()
{
    DeleteObject(g_hFontTitle);  DeleteObject(g_hFontHeader);
    DeleteObject(g_hFontNormal); DeleteObject(g_hFontSmall);
    DeleteObject(g_hFontMono);   DeleteObject(g_hFontBig);
    DeleteObject(g_hbrBG);       DeleteObject(g_hbrPanel);
    DeleteObject(g_hbrPanel2);   DeleteObject(g_hbrSidebar);
}

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
static void LoadScenario(int s)
{
    g_processes.clear();
    int q = 2;
    switch (s) {
    case 0: g_processes = {{1,0,8},{2,1,4},{3,2,9},{4,3,5},{5,4,2}}; q = 3; break;
    case 1: g_processes = {{1,0,10},{2,2,6},{3,4,2},{4,6,8},{5,8,4}}; q = 4; break;
    case 2: g_processes = {{1,0,2},{2,0,1},{3,1,3},{4,1,1},{5,2,2},{6,2,4},{7,3,1}}; q = 2; break;
    case 3: g_processes = {{1,0,6},{2,0,6},{3,0,6},{4,0,6},{5,0,6}}; q = 2; break;
    case 4: g_processes = {{1,0,5},{2,2,3},{3,4,6}}; q = 0; break;
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
            L"No processes added yet.\n\nUse 'Add Process' or load a pre-built scenario below.",
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

// ─── "Add Process" modal ──────────────────────────────────────────────────────

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

        mk(L"STATIC", L"Process ID  (a positive number):",  0,             20, 28,  210, 22, NULL);
        cc->hEdPid = mk(L"EDIT", L"1", WS_BORDER | ES_NUMBER,             240, 26,  80,  28, (HMENU)100);
        mk(L"STATIC", L"Arrival Time  (when it enters, >= 0):", 0,         20, 76,  210, 22, NULL);
        cc->hEdAt  = mk(L"EDIT", L"0", WS_BORDER | ES_NUMBER,             240, 74,  80,  28, (HMENU)101);
        mk(L"STATIC", L"Burst Time  (how long it needs, > 0):", 0,         20, 124, 210, 22, NULL);
        cc->hEdBt  = mk(L"EDIT", L"5", WS_BORDER | ES_NUMBER,             240, 122, 80,  28, (HMENU)102);

        HWND hOk = mk(L"BUTTON", L"Add Process", BS_DEFPUSHBUTTON,         20, 174, 160, 36, (HMENU)IDOK);
        mk(L"BUTTON", L"Cancel", BS_PUSHBUTTON,                            200, 174, 110, 36, (HMENU)IDCANCEL);
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
                MessageBox(h, L"Process ID must be a positive number (like 1, 2, 3...).", L"Check ID", MB_OK | MB_ICONINFORMATION);
                SetFocus(c->hEdPid); return 0;
            }
            for (auto& p : g_processes) {
                if (p.pid == pid) {
                    MessageBox(h, L"That Process ID is already in use.\nPlease choose a different number.",
                        L"Duplicate ID", MB_OK | MB_ICONINFORMATION);
                    SetFocus(c->hEdPid); return 0;
                }
            }
            if (at < 0 || bt <= 0) {
                MessageBox(h, L"Arrival Time must be 0 or more.\nBurst Time must be at least 1.", L"Check Values", MB_OK | MB_ICONINFORMATION);
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
    int cx = (pr.left + pr.right)  / 2 - 175;
    int cy = (pr.top  + pr.bottom) / 2 - 120;

    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        DLG_CLASS, L"  Add New Process",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        cx, cy, 360, 270, hParent, NULL, GetModuleHandle(NULL), &ctx);
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

// ─── Control creation & layout ────────────────────────────────────────────────

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
    lvc.iSubItem = 1; lvc.cx = 100; lvc.pszText = (LPWSTR)L"Arrival";
    ListView_InsertColumn(g_hListView, 1, &lvc);
    lvc.iSubItem = 2; lvc.cx = 100; lvc.pszText = (LPWSTR)L"Burst";
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

    mkBtn(L"▶ Run Simulation", ID_BTN_RUN,        g_hBtnRun);
    mkBtn(L"✕ Clear All",      ID_BTN_CLEAR,      g_hBtnClear);
    mkBtn(L"+ Add Process",    ID_BTN_ADD_PROC,   g_hBtnAdd);
    mkBtn(L"− Remove",         ID_BTN_DEL_PROC,   g_hBtnDel);
    mkBtn(L"Example A",        ID_BTN_SCENARIO_A, g_hBtnA);
    mkBtn(L"Example B",        ID_BTN_SCENARIO_B, g_hBtnB);
    mkBtn(L"Example C",        ID_BTN_SCENARIO_C, g_hBtnC);
    mkBtn(L"Example D",        ID_BTN_SCENARIO_D, g_hBtnD);
    mkBtn(L"Example E",        ID_BTN_SCENARIO_E, g_hBtnE);

    g_hGanttRR    = CreateWindow(L"GanttRRWnd",     L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)ID_GANTT_RR,     hInst, NULL);
    g_hGanttSRTF  = CreateWindow(L"GanttSRTFWnd",   L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)ID_GANTT_SRTF,   hInst, NULL);
    g_hResRR      = CreateWindow(L"ResultsRRWnd",   L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)ID_RESULTS_RR,   hInst, NULL);
    g_hResSRTF    = CreateWindow(L"ResultsSRTFWnd", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)ID_RESULTS_SRTF,  hInst, NULL);
    g_hSummary    = CreateWindow(L"SummaryWnd",     L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)ID_SUMMARY,      hInst, NULL);
    g_hConclusion = CreateWindow(L"ConclusionWnd",  L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)ID_CONCLUSION,   hInst, NULL);
}

static void LayoutControls(HWND hWnd)
{
    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right, H = rc.bottom;
    const int pad = 12, leftW = 300;
    int rightX = leftW + pad * 2;
    int rightW = W - rightX - pad;

    int y = 78;
    const int lvH = 190;
    SetWindowPos(g_hListView, NULL, pad, y, leftW - pad, lvH, SWP_NOZORDER);
    y += lvH + 8;

    int btnW2 = (leftW - pad - 8) / 2;
    SetWindowPos(g_hBtnAdd, NULL, pad,             y, btnW2, 30, SWP_NOZORDER);
    SetWindowPos(g_hBtnDel, NULL, pad + btnW2 + 8, y, btnW2, 30, SWP_NOZORDER);
    y += 40;

    SetWindowPos(g_hEditQ, NULL, pad + 170, y, 72, 28, SWP_NOZORDER);
    y += 40;

    int btnW = (leftW - pad - 8) / 2;
    SetWindowPos(g_hBtnRun,   NULL, pad,            y, btnW, 36, SWP_NOZORDER);
    SetWindowPos(g_hBtnClear, NULL, pad + btnW + 8, y, btnW, 36, SWP_NOZORDER);
    y += 50;

    int sbW = (leftW - pad - 8) / 2, sbH = 32;
    SetWindowPos(g_hBtnA, NULL, pad,           y, sbW, sbH, SWP_NOZORDER);
    SetWindowPos(g_hBtnB, NULL, pad + sbW + 8, y, sbW, sbH, SWP_NOZORDER);
    y += sbH + 6;
    SetWindowPos(g_hBtnC, NULL, pad,           y, sbW, sbH, SWP_NOZORDER);
    SetWindowPos(g_hBtnD, NULL, pad + sbW + 8, y, sbW, sbH, SWP_NOZORDER);
    y += sbH + 6;
    SetWindowPos(g_hBtnE, NULL, pad,           y, leftW - pad, sbH, SWP_NOZORDER);

    const int ganttH = 135, halfW = (rightW - pad) / 2;
    SetWindowPos(g_hGanttRR,   NULL, rightX,               58, halfW, ganttH, SWP_NOZORDER);
    SetWindowPos(g_hGanttSRTF, NULL, rightX + halfW + pad, 58, halfW, ganttH, SWP_NOZORDER);

    const int resH = 195, resY = 58 + ganttH + 10;
    SetWindowPos(g_hResRR,   NULL, rightX,               resY, halfW, resH, SWP_NOZORDER);
    SetWindowPos(g_hResSRTF, NULL, rightX + halfW + pad, resY, halfW, resH, SWP_NOZORDER);

    int sumY = resY + resH + 10;
    int sumH = std::max(H - sumY - pad, 60);
    int sumW = (rightW - pad) / 2;
    SetWindowPos(g_hSummary,    NULL, rightX,              sumY, sumW,                sumH, SWP_NOZORDER);
    SetWindowPos(g_hConclusion, NULL, rightX + sumW + pad, sumY, rightW - sumW - pad, sumH, SWP_NOZORDER);
}

// ─── Paint: Gantt chart ───────────────────────────────────────────────────────

static void PaintGantt(HWND hWnd, HDC hdc, const SimResult& res,
                       COLORREF accentColor, const wchar_t* label, const wchar_t* subtitle)
{
    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right, H = rc.bottom;

    // Dark card background with border
    FillRect(hdc, &rc, g_hbrPanel);
    HPEN hpBorder = CreatePen(PS_SOLID, 1, SM_BORDER);
    HPEN hpOld    = (HPEN)  SelectObject(hdc, hpBorder);
    HBRUSH hbOld  = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, 0, 0, W, H, 6, 6);
    SelectObject(hdc, hpOld); SelectObject(hdc, hbOld);
    DeleteObject(hpBorder);

    // Coloured title strip — slightly dimmed accent so it reads well on dark
    RECT titleR = {0, 0, W, 30};
    // Darken accent slightly for strip
    COLORREF stripCol = RGB(
        (int)(GetRValue(accentColor) * 0.55),
        (int)(GetGValue(accentColor) * 0.55),
        (int)(GetBValue(accentColor) * 0.55));
    HBRUSH hbrAcc = CreateSolidBrush(stripCol);
    FillRect(hdc, &titleR, hbrAcc); DeleteObject(hbrAcc);

    // Thin accent left edge
    HBRUSH hbrEdge = CreateSolidBrush(accentColor);
    RECT edgeR = {0, 0, 3, 30};
    FillRect(hdc, &edgeR, hbrEdge); DeleteObject(hbrEdge);

    RECT trText = {10, 4, W - 8, 30};
    DrawText2(hdc, label, trText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontHeader);
    RECT subText = {W / 2, 7, W - 6, 28};
    DrawText2(hdc, subtitle, subText, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, SM_TEXT2, g_hFontSmall);

    if (!g_hasResult || res.gantt.empty()) {
        RECT msg = {12, 38, W - 12, H - 8};
        DrawText2(hdc, L"Run the simulation to see the Gantt chart here.",
            msg, DT_CENTER | DT_VCENTER | DT_WORDBREAK, SM_TEXT3, g_hFontSmall);
        return;
    }

    int tEnd = 0;
    for (auto& b : res.gantt) tEnd = std::max(tEnd, b.end);
    if (tEnd == 0) return;

    const int chartTop  = 36, chartH = 44;
    const int chartLeft = 10, chartRight = W - 10;
    int chartW = chartRight - chartLeft;
    float scale = (float)chartW / tEnd;

    for (auto& b : res.gantt) {
        int x1 = chartLeft + (int)(b.start * scale);
        int x2 = chartLeft + (int)(b.end   * scale);
        if (x2 <= x1) x2 = x1 + 1;

        COLORREF col = ProcColor(b.pid);
        // Slightly darken the fill to sit well on dark background
        COLORREF fillCol = RGB(
            (int)(GetRValue(col) * 0.75),
            (int)(GetGValue(col) * 0.75),
            (int)(GetBValue(col) * 0.75));
        HBRUSH hbr = CreateSolidBrush(fillCol);
        HPEN   hpn = CreatePen(PS_SOLID, 1, col);  // bright border
        RECT br = {x1 + 1, chartTop, x2 - 1, chartTop + chartH - 2};
        DrawRoundRect(hdc, br, 4, 4, hbr, hpn);
        DeleteObject(hbr); DeleteObject(hpn);

        if (x2 - x1 > 16) {
            wchar_t lbl[16];
            if (b.pid < 0) swprintf(lbl, 16, L"idle");
            else           swprintf(lbl, 16, L"P%d", b.pid);
            RECT lr = {x1, chartTop, x2, chartTop + chartH - 2};
            // Use bright accent colour for text on dark blocks
            COLORREF textC = (b.pid < 0) ? SM_TEXT3 : col;
            DrawText2(hdc, lbl, lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE, textC, g_hFontSmall);
        }
    }

    // Axis line
    int axisY = chartTop + chartH + 2;
    HPEN hpAxis = CreatePen(PS_SOLID, 1, SM_BORDER);
    HPEN hpaOld = (HPEN)SelectObject(hdc, hpAxis);
    MoveToEx(hdc, chartLeft, axisY, NULL); LineTo(hdc, chartRight, axisY);
    SelectObject(hdc, hpaOld); DeleteObject(hpAxis);

    // Time ticks
    int maxTicks = chartW / 24; if (maxTicks < 1) maxTicks = 1;
    int step = std::max(1, (int)ceil((double)tEnd / maxTicks));
    for (int t = 0; t <= tEnd; t += step) {
        int tx = chartLeft + (int)(t * scale);
        HPEN hpTick = CreatePen(PS_SOLID, 1, SM_BORDER);
        HPEN hptOld = (HPEN)SelectObject(hdc, hpTick);
        MoveToEx(hdc, tx, axisY, NULL); LineTo(hdc, tx, axisY + 4);
        SelectObject(hdc, hptOld); DeleteObject(hpTick);

        wchar_t tstr[16]; swprintf(tstr, 16, L"%d", t);
        RECT tr = {tx - 14, axisY + 5, tx + 14, axisY + 17};
        DrawText2(hdc, tstr, tr, DT_CENTER | DT_SINGLELINE, SM_TEXT2, g_hFontSmall);
    }
}

// ─── Paint: Results table ─────────────────────────────────────────────────────

static void PaintResults(HWND hWnd, HDC hdc, const SimResult& res,
                         COLORREF accentColor, const wchar_t* label)
{
    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right, H = rc.bottom;

    FillRect(hdc, &rc, g_hbrPanel);
    HPEN hpB  = CreatePen(PS_SOLID, 1, SM_BORDER);
    HPEN hpOld = (HPEN)  SelectObject(hdc, hpB);
    HBRUSH hbOld = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, 0, 0, W, H, 6, 6);
    SelectObject(hdc, hpOld); SelectObject(hdc, hbOld); DeleteObject(hpB);

    // Header strip — dark tinted accent
    RECT titleR = {0, 0, W, 30};
    COLORREF stripCol = RGB(
        (int)(GetRValue(accentColor) * 0.55),
        (int)(GetGValue(accentColor) * 0.55),
        (int)(GetBValue(accentColor) * 0.55));
    HBRUSH hbrAcc = CreateSolidBrush(stripCol);
    FillRect(hdc, &titleR, hbrAcc); DeleteObject(hbrAcc);

    // Accent left edge
    HBRUSH hbrEdge = CreateSolidBrush(accentColor);
    RECT edgeR = {0, 0, 3, 30};
    FillRect(hdc, &edgeR, hbrEdge); DeleteObject(hbrEdge);

    RECT trText = {10, 4, W - 8, 30};
    DrawText2(hdc, label, trText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontHeader);

    if (!g_hasResult || res.results.empty()) {
        RECT msg = {12, 38, W - 12, H - 8};
        DrawText2(hdc, L"Results will appear here after running the simulation.",
            msg, DT_CENTER | DT_VCENTER | DT_WORDBREAK, SM_TEXT3, g_hFontSmall);
        return;
    }

    const wchar_t* cols[] = {L"", L"Process", L"Arrival", L"Burst", L"Finish", L"Turnaround", L"Wait", L"Response"};
    const int nCols = 8;
    int dotColW = 18;
    int colW = (W - 20 - dotColW) / (nCols - 1);
    int rowH = 22, tableTop = 32;

    // Column header row — darker panel
    HBRUSH hbrHead = CreateSolidBrush(SM_PANEL2);
    RECT hr = {10, tableTop, W - 10, tableTop + rowH};
    FillRect(hdc, &hr, hbrHead); DeleteObject(hbrHead);

    for (int c = 1; c < nCols; c++) {
        RECT cr = {10 + dotColW + (c - 1) * colW, tableTop, 10 + dotColW + c * colW, tableTop + rowH};
        DrawText2(hdc, cols[c], cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE, SM_TEXT2, g_hFontSmall);
    }

    int y = tableTop + rowH;
    for (int i = 0; i < (int)res.results.size() && y + rowH < H - 40; i++) {
        const ProcessResult& r = res.results[i];
        // Alternating: panel vs panel2
        HBRUSH hbrRow = CreateSolidBrush((i % 2 == 0) ? SM_PANEL : SM_PANEL2);
        RECT rr = {10, y, W - 10, y + rowH};
        FillRect(hdc, &rr, hbrRow); DeleteObject(hbrRow);

        COLORREF pcol = ProcColor(r.pid);
        HBRUSH hbrDot = CreateSolidBrush(pcol);
        HPEN   hpDot  = CreatePen(PS_SOLID, 1, pcol);
        HBRUSH hbDOld = (HBRUSH)SelectObject(hdc, hbrDot);
        HPEN   hpDOld = (HPEN)  SelectObject(hdc, hpDot);
        Ellipse(hdc, 13, y + 6, 23, y + 16);
        SelectObject(hdc, hbDOld); SelectObject(hdc, hpDOld);
        DeleteObject(hbrDot); DeleteObject(hpDot);

        wchar_t vals[7][32];
        swprintf(vals[0], 32, L"P%d", r.pid);
        swprintf(vals[1], 32, L"%d",  r.arrivalTime);
        swprintf(vals[2], 32, L"%d",  r.burstTime);
        swprintf(vals[3], 32, L"%d",  r.completionTime);
        swprintf(vals[4], 32, L"%d",  r.turnaroundTime);
        swprintf(vals[5], 32, L"%d",  r.waitingTime);
        swprintf(vals[6], 32, L"%d",  r.responseTime);

        for (int c = 0; c < 7; c++) {
            RECT cr = {10 + dotColW + c * colW, y, 10 + dotColW + (c + 1) * colW, y + rowH};
            DrawText2(hdc, vals[c], cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontSmall);
        }
        y += rowH;
    }

    // Averages footer
    if (y + rowH < H - 4) {
        HPEN hpLine = CreatePen(PS_SOLID, 1, SM_BORDER);
        HPEN hpLOld = (HPEN)SelectObject(hdc, hpLine);
        MoveToEx(hdc, 10, y, NULL); LineTo(hdc, W - 10, y);
        SelectObject(hdc, hpLOld); DeleteObject(hpLine);

        // Dark highlight for averages row
        HBRUSH hbrAvg = CreateSolidBrush(RGB(24, 38, 58));
        RECT ar = {10, y, W - 10, y + rowH};
        FillRect(hdc, &ar, hbrAvg); DeleteObject(hbrAvg);

        wchar_t avgWT[32], avgTAT[32], avgRT[32];
        swprintf(avgWT,  32, L"%.2f", res.avgWT);
        swprintf(avgTAT, 32, L"%.2f", res.avgTAT);
        swprintf(avgRT,  32, L"%.2f", res.avgRT);

        RECT lblR = {10 + dotColW, y, 10 + dotColW + colW * 4, y + rowH};
        DrawText2(hdc, L"Averages", lblR, DT_CENTER | DT_VCENTER | DT_SINGLELINE, SM_TEXT2, g_hFontSmall);

        const wchar_t* avgVals[] = {L"", L"", L"", L"", avgTAT, avgWT, avgRT};
        for (int c = 4; c < 7; c++) {
            RECT cr = {10 + dotColW + c * colW, y, 10 + dotColW + (c + 1) * colW, y + rowH};
            DrawText2(hdc, avgVals[c], cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE, SM_YELLOW, g_hFontSmall);
        }
    }
}

// ─── Paint: Comparison summary ────────────────────────────────────────────────

static void PaintSummary(HWND hWnd, HDC hdc)
{
    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right, H = rc.bottom;

    FillRect(hdc, &rc, g_hbrPanel);
    HPEN hpB   = CreatePen(PS_SOLID, 1, SM_BORDER);
    HPEN hpOld = (HPEN)  SelectObject(hdc, hpB);
    HBRUSH hbOld = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, 0, 0, W, H, 6, 6);
    SelectObject(hdc, hpOld); SelectObject(hdc, hbOld); DeleteObject(hpB);

    // Dark header strip
    RECT tr = {0, 0, W, 28};
    HBRUSH hbrT = CreateSolidBrush(SM_PANEL2);
    FillRect(hdc, &tr, hbrT); DeleteObject(hbrT);
    RECT trText = {10, 4, W, 28};
    DrawText2(hdc, L"📊 Side-by-Side Comparison", trText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontHeader);

    if (!g_hasResult) {
        RECT msg = {12, 36, W - 12, H - 8};
        DrawText2(hdc, L"Run a simulation to compare the two algorithms here.", msg, DT_CENTER | DT_VCENTER | DT_WORDBREAK, SM_TEXT3, g_hFontSmall);
        return;
    }

    struct Metric { const wchar_t* name; double rr; double srtf; };
    Metric metrics[] = {
        {L"Avg Waiting Time",    g_rrResult.avgWT,  g_srtfResult.avgWT},
        {L"Avg Turnaround Time", g_rrResult.avgTAT, g_srtfResult.avgTAT},
        {L"Avg Response Time",   g_rrResult.avgRT,  g_srtfResult.avgRT},
    };

    int y = 30, rowH = 28;
    int col1 = 10, col3 = W - 10, third = W / 3;

    // Column headers
    RECT hhr = {col1, y, col3, y + 22};
    HBRUSH hbrHh = CreateSolidBrush(SM_PANEL2);
    FillRect(hdc, &hhr, hbrHh); DeleteObject(hbrHh);

    RECT lblR = {col1 + 6, y, col1 + third, y + 22};
    DrawText2(hdc, L"Metric", lblR, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT2, g_hFontSmall);

    // RR header pill — use muted accent so it pops on dark
    RECT rrR = {col1 + third + 4, y + 2, col1 + 2 * third - 4, y + 20};
    DrawPill(hdc, rrR, RGB(30, 60, 110), L"Round Robin", SM_RR, g_hFontSmall);
    RECT srR = {col1 + 2 * third + 4, y + 2, col3 - 4, y + 20};
    DrawPill(hdc, srR, RGB(20, 70, 58), L"SRTF", SM_SRTF, g_hFontSmall);
    y += 22;

    for (int i = 0; i < 3; i++) {
        Metric& m = metrics[i];
        HBRUSH hbrR = CreateSolidBrush((i % 2 == 0) ? SM_PANEL : SM_PANEL2);
        RECT rrf = {col1, y, col3, y + rowH};
        FillRect(hdc, &rrf, hbrR); DeleteObject(hbrR);

        bool rrBetter   = m.rr   < m.srtf;
        bool srtfBetter = m.srtf < m.rr;

        RECT nameR = {col1 + 6, y, col1 + third, y + rowH};
        DrawText2(hdc, m.name, nameR, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontSmall);

        wchar_t rrVal[32], srtfVal[32];
        swprintf(rrVal,   32, L"%.2f %ls", m.rr,   rrBetter   ? L"✓" : L"");
        swprintf(srtfVal, 32, L"%.2f %ls", m.srtf, srtfBetter ? L"✓" : L"");

        RECT rrV = {col1 + third, y, col1 + 2 * third, y + rowH};
        DrawText2(hdc, rrVal, rrV, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
            rrBetter ? SM_GREEN : SM_TEXT, g_hFontSmall);
        RECT srV = {col1 + 2 * third, y, col3, y + rowH};
        DrawText2(hdc, srtfVal, srV, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
            srtfBetter ? SM_GREEN : SM_TEXT, g_hFontSmall);
        y += rowH;
    }

    // Fairness row
    y += 6;
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

        // Dark amber row
        RECT fairR = {col1, y, col3, y + 30};
        HBRUSH hbrF = CreateSolidBrush(RGB(40, 32, 10));
        FillRect(hdc, &fairR, hbrF); DeleteObject(hbrF);

        wchar_t fairStr[256];
        swprintf(fairStr, 256, L"Fairness (lower = fairer)  →  RR: %.2f   SRTF: %.2f   →  %ls distributes waiting time more evenly",
            cvRR, cvSRTF, (cvRR < cvSRTF) ? L"Round Robin" : L"SRTF");
        RECT ft = {col1 + 6, y, col3 - 4, y + 30};
        DrawText2(hdc, fairStr, ft, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_YELLOW, g_hFontSmall);
        y += 36;
    }

    wchar_t qstr[128];
    swprintf(qstr, 128, L"Time Quantum (Q) = %d   |   Processes: %d", g_quantum, (int)g_processes.size());
    RECT qR = {col1 + 6, y, col3, y + 20};
    DrawText2(hdc, qstr, qR, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT2, g_hFontSmall);
}

// ─── Paint: Conclusion / analysis ─────────────────────────────────────────────

static void PaintConclusion(HWND hWnd, HDC hdc)
{
    RECT rc; GetClientRect(hWnd, &rc);
    int W = rc.right, H = rc.bottom;

    FillRect(hdc, &rc, g_hbrPanel);
    HPEN hpB   = CreatePen(PS_SOLID, 1, SM_BORDER);
    HPEN hpOld = (HPEN)  SelectObject(hdc, hpB);
    HBRUSH hbOld = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, 0, 0, W, H, 6, 6);
    SelectObject(hdc, hpOld); SelectObject(hdc, hbOld); DeleteObject(hpB);

    RECT tr = {0, 0, W, 28};
    HBRUSH hbrT = CreateSolidBrush(SM_PANEL2);
    FillRect(hdc, &tr, hbrT); DeleteObject(hbrT);
    RECT trText = {10, 4, W, 28};
    DrawText2(hdc, L"📝 Analysis & Takeaways", trText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontHeader);

    if (!g_hasResult) {
        RECT msg = {12, 36, W - 12, H - 8};
        DrawText2(hdc, L"Key insights will appear here after running the simulation.", msg, DT_CENTER | DT_VCENTER | DT_WORDBREAK, SM_TEXT3, g_hFontSmall);
        return;
    }

    std::wstring txt;
    txt += L"WAITING TIME\r\n";
    txt += (g_rrResult.avgWT < g_srtfResult.avgWT) ? L"Round Robin" : L"SRTF";
    txt += L" had a shorter average waiting time (";
    txt += FormatDouble(std::min(g_rrResult.avgWT, g_srtfResult.avgWT));
    txt += L" vs " + FormatDouble(std::max(g_rrResult.avgWT, g_srtfResult.avgWT)) + L"). ";
    txt += L"Lower is better — it means processes spent less time just sitting in the queue.\r\n\r\n";

    txt += L"RESPONSE TIME\r\n";
    txt += (g_rrResult.avgRT < g_srtfResult.avgRT) ? L"Round Robin" : L"SRTF";
    txt += L" responded faster on average (";
    txt += FormatDouble(std::min(g_rrResult.avgRT, g_srtfResult.avgRT));
    txt += L" vs " + FormatDouble(std::max(g_rrResult.avgRT, g_srtfResult.avgRT)) + L"). ";
    txt += L"This matters in interactive systems where users expect quick feedback.\r\n\r\n";

    txt += L"FAIRNESS\r\n";
    txt += L"Round Robin gives every process a fair time-slice (Q=";
    txt += std::to_wstring(g_quantum);
    txt += L"), so no process waits too long. SRTF always picks the shortest job next, ";
    txt += L"which means long processes can be delayed (called starvation).\r\n\r\n";

    txt += L"QUANTUM SIZE (Q=" + std::to_wstring(g_quantum) + L")\r\n";
    if      (g_quantum <= 2) txt += L"Very small quantum → lots of context switches, but very quick responses.";
    else if (g_quantum <= 5) txt += L"Medium quantum → good balance between responsiveness and efficiency.";
    else                     txt += L"Large quantum → fewer switches, but fairness suffers. Starts to behave like FCFS.";
    txt += L"\r\n\r\n";

    txt += L"RECOMMENDATION\r\n";
    bool rrBetter = (g_rrResult.avgWT + g_rrResult.avgRT) < (g_srtfResult.avgWT + g_srtfResult.avgRT);
    txt += rrBetter
        ? L"For this workload, Round Robin performed better overall. It is a good choice for time-sharing systems."
        : L"For this workload, SRTF performed better overall. It suits batch systems where finishing short jobs quickly matters.";

    RECT textR = {12, 34, W - 12, H - 8};
    DrawText2(hdc, txt.c_str(), textR, DT_LEFT | DT_WORDBREAK, SM_TEXT, g_hFontSmall);
}

// ─── Panel window procedures ──────────────────────────────────────────────────

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
            PaintGantt(h, dc, g_srtfResult, SM_SRTF, L"SRTF — Gantt Chart", L"shortest job first");
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
            PaintResults(h, dc, g_rrResult, SM_RR, L"Round Robin — Results Table");
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
            PaintResults(h, dc, g_srtfResult, SM_SRTF, L"SRTF — Results Table");
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

// ─── Main window procedure ────────────────────────────────────────────────────

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

        // Deep dark page background
        FillRect(hdc, &rc, g_hbrBG);

        // ── Title bar ─────────────────────────────────────────────────────
        RECT titleArea = {0, 0, W, 52};
        FillRect(hdc, &titleArea, g_hbrPanel);

        // Thin blue accent bar at very top
        RECT accentLine = {0, 0, W, 3};
        HBRUSH hbrLine = CreateSolidBrush(SM_ACCENT);
        FillRect(hdc, &accentLine, hbrLine); DeleteObject(hbrLine);

        // Soft separator below title
        RECT sepLine = {0, 51, W, 52};
        HBRUSH hbrSep = CreateSolidBrush(SM_BORDER);
        FillRect(hdc, &sepLine, hbrSep); DeleteObject(hbrSep);

        // App title
        RECT t1 = {14, 10, W / 2, 50};
        DrawText2(hdc, L"CPU Scheduling Simulator", t1,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontTitle);

        // Algorithm pills — dark-tinted backgrounds with vivid text
        RECT rrTag = {W / 2, 14, W / 2 + 120, 38};
        DrawPill(hdc, rrTag, RGB(20, 42, 85), L"Round Robin", SM_RR, g_hFontSmall);

        RECT srTag = {W / 2 + 128, 14, W / 2 + 208, 38};
        DrawPill(hdc, srTag, RGB(15, 55, 45), L"SRTF", SM_SRTF, g_hFontSmall);

        RECT sub = {W / 2 + 218, 14, W - 10, 38};
        DrawText2(hdc, L"vs.", sub, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT3, g_hFontNormal);

        // ── Left sidebar ──────────────────────────────────────────────────
        RECT leftBg = {0, 52, 300, rc.bottom};
        FillRect(hdc, &leftBg, g_hbrSidebar);

        // Sidebar soft right border
        HPEN hpSide = CreatePen(PS_SOLID, 1, SM_BORDER);
        HPEN hpSOldA = (HPEN)SelectObject(hdc, hpSide);
        MoveToEx(hdc, 299, 52, NULL); LineTo(hdc, 299, rc.bottom);
        SelectObject(hdc, hpSOldA); DeleteObject(hpSide);

        // Sidebar labels
        RECT lbl1 = {12, 58, 290, 76};
        DrawText2(hdc, L"PROCESSES", lbl1, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT2, g_hFontSmall);

        int lvH = 190;
        int yQ  = 52 + 8 + lvH + 8 + 30 + 8;
        RECT lbl2 = {12, yQ + 4, 175, yQ + 30};
        DrawText2(hdc, L"Time Quantum (Q):", lbl2, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT, g_hFontNormal);

        int ySep = yQ + 40 + 44;
        HPEN hpSep = CreatePen(PS_SOLID, 1, SM_BORDER);
        HPEN hpSOld = (HPEN)SelectObject(hdc, hpSep);
        MoveToEx(hdc, 12, ySep, NULL); LineTo(hdc, 286, ySep);
        SelectObject(hdc, hpSOld); DeleteObject(hpSep);

        RECT lbl3 = {12, ySep + 6, 290, ySep + 22};
        DrawText2(hdc, L"PRE-BUILT EXAMPLES", lbl3, DT_LEFT | DT_VCENTER | DT_SINGLELINE, SM_TEXT2, g_hFontSmall);

        EndPaint(hWnd, &ps);
        return 0;
    }

    // ── Control colouring — dark theme ────────────────────────────────────
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

    // ── List-view custom draw (alternating dark rows) ──────────────────────
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

    // ── Button commands ────────────────────────────────────────────────────
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
        mm->ptMinTrackSize.x = 920;
        mm->ptMinTrackSize.y = 620;
        return 0;
    }

    case WM_DESTROY:
        DestroyFontsAndBrushes();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
