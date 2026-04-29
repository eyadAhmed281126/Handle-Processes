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

COLORREF PROC_COLORS[N_PROC_COLORS] = {
    RGB(99,179,237), RGB(154,117,234), RGB(72,199,142),
    RGB(251,146,60), RGB(248,113,113), RGB(251,211,80),
    RGB(129,229,230), RGB(246,135,179), RGB(167,211,128),
    RGB(203,166,247)
};

COLORREF ProcColor(int pid)
{
    if (pid < 0) return RGB(40, 50, 70);
    return PROC_COLORS[pid % N_PROC_COLORS];
}

static HWND g_hMain        = NULL;
static HWND g_hListView    = NULL;
static HWND g_hEditQ       = NULL;
static HWND g_hBtnRun      = NULL;
static HWND g_hBtnClear    = NULL;
static HWND g_hBtnAdd      = NULL;
static HWND g_hBtnDel      = NULL;
static HWND g_hBtnA;
static HWND g_hBtnB;
static HWND g_hBtnC;
static HWND g_hBtnD;
static HWND g_hBtnE;
static HWND g_hGanttRR     = NULL;
static HWND g_hGanttSRTF   = NULL;
static HWND g_hResRR       = NULL;
static HWND g_hResSRTF     = NULL;
static HWND g_hSummary     = NULL;
static HWND g_hConclusion  = NULL;

static HFONT g_hFontTitle  = NULL;
static HFONT g_hFontHeader = NULL;
static HFONT g_hFontNormal = NULL;
static HFONT g_hFontSmall  = NULL;
static HFONT g_hFontMono   = NULL;
static HFONT g_hFontBig    = NULL;

static HBRUSH g_hbrBG     = NULL;
static HBRUSH g_hbrPanel  = NULL;
static HBRUSH g_hbrPanel2 = NULL;

static std::vector<Process> g_processes;
static SimResult            g_rrResult;
static SimResult            g_srtfResult;
static bool                 g_hasResult = false;
static int                  g_quantum   = 2;

static LRESULT CALLBACK GanttRRProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK GanttSRTFProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK ResultsRRProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK ResultsSRTFProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK SummaryProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK ConclusionProc(HWND, UINT, WPARAM, LPARAM);

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

static void PaintGantt(HWND hWnd, HDC hdc, const SimResult& res, COLORREF accentColor, const wchar_t* label);
static void PaintResults(HWND hWnd, HDC hdc, const SimResult& res, COLORREF accentColor, const wchar_t* label);
static void PaintSummary(HWND hWnd, HDC hdc);
static void PaintConclusion(HWND hWnd, HDC hdc);

static void DrawText2(HDC hdc, const wchar_t* txt, RECT r, UINT fmt, COLORREF col, HFONT fnt);
static void DrawRoundRect(HDC hdc, RECT r, int rx, int ry, HBRUSH hbr, HPEN hpen);
static std::wstring FormatDouble(double v, int dec = 2);

static void DoubleBufferedPaint(HWND hWnd, std::function<void(HWND, HDC)> painter)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT rc;
    GetClientRect(hWnd, &rc);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP hOld = (HBITMAP)SelectObject(memDC, hBmp);
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
        WNDCLASSEX wc = {};
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

static void DrawText2(HDC hdc, const wchar_t* txt, RECT r, UINT fmt, COLORREF col, HFONT fnt)
{
    HFONT old = (HFONT)SelectObject(hdc, fnt);
    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, txt, -1, &r, fmt);
    SelectObject(hdc, old);
}

static std::wstring FormatDouble(double v, int dec)
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

static void CreateFontsAndBrushes()
{
    auto mkFont = [](int h, int w, bool italic, const wchar_t* face) {
        return CreateFont(-h, 0, 0, 0, w, italic, 0, 0,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, face);
    };
    g_hFontTitle  = mkFont(22, FW_BOLD,   false, L"Segoe UI");
    g_hFontHeader = mkFont(14, FW_BOLD,   false, L"Segoe UI");
    g_hFontNormal = mkFont(13, FW_NORMAL, false, L"Segoe UI");
    g_hFontSmall  = mkFont(11, FW_NORMAL, false, L"Segoe UI");
    g_hFontMono   = mkFont(12, FW_NORMAL, false, L"Consolas");
    g_hFontBig    = mkFont(30, FW_BOLD,   false, L"Segoe UI");

    g_hbrBG     = CreateSolidBrush(C_BG);
    g_hbrPanel  = CreateSolidBrush(C_PANEL);
    g_hbrPanel2 = CreateSolidBrush(C_PANEL2);
}

static void DestroyFontsAndBrushes()
{
    DeleteObject(g_hFontTitle);
    DeleteObject(g_hFontHeader);
    DeleteObject(g_hFontNormal);
    DeleteObject(g_hFontSmall);
    DeleteObject(g_hFontMono);
    DeleteObject(g_hFontBig);
    DeleteObject(g_hbrBG);
    DeleteObject(g_hbrPanel);
    DeleteObject(g_hbrPanel2);
}

static void RefreshListView()
{
    ListView_DeleteAllItems(g_hListView);
    for (int i = 0; i < (int)g_processes.size(); i++) {
        LVITEM lvi = {};
        lvi.mask     = LVIF_TEXT;
        lvi.iItem    = i;
        lvi.iSubItem = 0;
        wchar_t buf[64];
        swprintf(buf, 64, L"P%d", g_processes[i].pid);
        lvi.pszText = buf;
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
    case 0:
        g_processes = {{1,0,8},{2,1,4},{3,2,9},{4,3,5},{5,4,2}};
        q = 3;
        break;
    case 1:
        g_processes = {{1,0,10},{2,2,6},{3,4,2},{4,6,8},{5,8,4}};
        q = 4;
        break;
    case 2:
        g_processes = {{1,0,2},{2,0,1},{3,1,3},{4,1,1},{5,2,2},{6,2,4},{7,3,1}};
        q = 2;
        break;
    case 3:
        g_processes = {{1,0,6},{2,0,6},{3,0,6},{4,0,6},{5,0,6}};
        q = 2;
        break;
    case 4:
        g_processes = {{1,0,5},{2,2,3},{3,4,6}};
        q = 0;
        break;
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
            L"Invalid Time Quantum.\n\nThe quantum must be a positive integer (>= 1).",
            L"Validation Error", MB_OK | MB_ICONWARNING);
        SetFocus(g_hEditQ);
        return;
    }
    if (g_processes.empty()) {
        MessageBox(g_hMain,
            L"No processes defined.\n\nAdd at least one process or load a scenario.",
            L"Validation Error", MB_OK | MB_ICONWARNING);
        return;
    }
    for (auto& p : g_processes) {
        if (p.arrivalTime < 0 || p.burstTime <= 0) {
            MessageBox(g_hMain,
                L"Invalid process data.\n\nArrival >= 0 and burst > 0 required.",
                L"Validation Error", MB_OK | MB_ICONWARNING);
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

struct AddProcCtx {
    HWND hEdPid;
    HWND hEdAt;
    HWND hEdBt;
    int  pid;
    int  at;
    int  bt;
    bool ok;
    bool done;
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
        mk(L"STATIC", L"PID (> 0, unique):", 0, 20, 30, 160, 22, NULL);
        cc->hEdPid = mk(L"EDIT", L"1", WS_BORDER | ES_NUMBER, 190, 28, 120, 26, (HMENU)100);
        mk(L"STATIC", L"Arrival Time (>= 0):", 0, 20, 80, 160, 22, NULL);
        cc->hEdAt = mk(L"EDIT", L"0", WS_BORDER | ES_NUMBER, 190, 78, 120, 26, (HMENU)101);
        mk(L"STATIC", L"Burst Time (> 0):", 0, 20, 130, 160, 22, NULL);
        cc->hEdBt = mk(L"EDIT", L"5", WS_BORDER | ES_NUMBER, 190, 128, 120, 26, (HMENU)102);

        HWND hOk  = mk(L"BUTTON", L"Add Process", BS_DEFPUSHBUTTON, 30, 190, 160, 38, (HMENU)IDOK);
        mk(L"BUTTON", L"Cancel", BS_PUSHBUTTON, 210, 190, 100, 38, (HMENU)IDCANCEL);
        SendMessage(hOk, WM_SETFONT, (WPARAM)g_hFontHeader, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (!c) return 0;
        if (LOWORD(wp) == IDOK) {
            wchar_t pb[32], ab[32], bb[32];
            GetWindowText(c->hEdPid, pb, 32);
            GetWindowText(c->hEdAt, ab, 32);
            GetWindowText(c->hEdBt, bb, 32);
            int pid = _wtoi(pb);
            int at  = _wtoi(ab);
            int bt  = _wtoi(bb);
            if (pid <= 0) {
                MessageBox(h, L"PID must be a positive integer (> 0).",
                    L"Invalid", MB_OK | MB_ICONWARNING);
                SetFocus(c->hEdPid);
                return 0;
            }
            for (auto& p : g_processes) {
                if (p.pid == pid) {
                    MessageBox(h, L"A process with this PID already exists.\nPlease choose a unique PID.",
                        L"Duplicate PID", MB_OK | MB_ICONWARNING);
                    SetFocus(c->hEdPid);
                    return 0;
                }
            }
            if (at < 0 || bt <= 0) {
                MessageBox(h, L"Arrival >= 0 and Burst > 0 required.",
                    L"Invalid", MB_OK | MB_ICONWARNING);
                return 0;
            }
            c->pid = pid;
            c->at  = at;
            c->bt  = bt;
            c->ok   = true;
            c->done = true;
            DestroyWindow(h);
        } else if (LOWORD(wp) == IDCANCEL) {
            c->done = true;
            DestroyWindow(h);
        }
        return 0;
    case WM_CLOSE:
        if (c) c->done = true;
        DestroyWindow(h);
        return 0;
    case WM_ERASEBKGND: {
        HDC dc = (HDC)wp;
        RECT r;
        GetClientRect(h, &r);
        FillRect(dc, &r, g_hbrPanel);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, C_PANEL);
        return (LRESULT)g_hbrPanel;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, C_PANEL2);
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
        WNDCLASSEX wc2 = {};
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
    ctx.ok   = false;
    ctx.done = false;

    RECT pr;
    GetWindowRect(hParent, &pr);
    int cx = (pr.left + pr.right) / 2 - 170;
    int cy = (pr.top  + pr.bottom) / 2 - 110;

    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        DLG_CLASS, L"  Add New Process",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        cx, cy, 340, 270, hParent, NULL, GetModuleHandle(NULL), &ctx);

    if (!hDlg) return;

    EnableWindow(hParent, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    MSG msg;
    while (!ctx.done && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            ctx.done = true;
            DestroyWindow(hDlg);
            break;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    if (ctx.ok) {
        g_processes.push_back({ctx.pid, ctx.at, ctx.bt});
        RefreshListView();
    }
}

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
    lvc.iSubItem = 0; lvc.cx = 80;  lvc.pszText = (LPWSTR)L"Process";
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
        SendMessage(out, WM_SETFONT, (WPARAM)g_hFontHeader, TRUE);
    };

    mkBtn(L"Run Simulation",  ID_BTN_RUN,        g_hBtnRun);
    mkBtn(L"Clear All",       ID_BTN_CLEAR,      g_hBtnClear);
    mkBtn(L"Add Process",     ID_BTN_ADD_PROC,   g_hBtnAdd);
    mkBtn(L"Remove Selected", ID_BTN_DEL_PROC,   g_hBtnDel);
    mkBtn(L"Scenario A",      ID_BTN_SCENARIO_A, g_hBtnA);
    mkBtn(L"Scenario B",      ID_BTN_SCENARIO_B, g_hBtnB);
    mkBtn(L"Scenario C",      ID_BTN_SCENARIO_C, g_hBtnC);
    mkBtn(L"Scenario D",      ID_BTN_SCENARIO_D, g_hBtnD);
    mkBtn(L"Scenario E",      ID_BTN_SCENARIO_E, g_hBtnE);

    g_hGanttRR    = CreateWindow(L"GanttRRWnd",     L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)ID_GANTT_RR,    hInst, NULL);
    g_hGanttSRTF  = CreateWindow(L"GanttSRTFWnd",   L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)ID_GANTT_SRTF,  hInst, NULL);
    g_hResRR      = CreateWindow(L"ResultsRRWnd",   L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)ID_RESULTS_RR,  hInst, NULL);
    g_hResSRTF    = CreateWindow(L"ResultsSRTFWnd", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)ID_RESULTS_SRTF, hInst, NULL);
    g_hSummary    = CreateWindow(L"SummaryWnd",     L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)ID_SUMMARY,     hInst, NULL);
    g_hConclusion = CreateWindow(L"ConclusionWnd",  L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)ID_CONCLUSION,  hInst, NULL);
}

static void LayoutControls(HWND hWnd)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    int W = rc.right;
    int H = rc.bottom;
    int pad = 10;

    int leftW = 300;
    int rightX = leftW + pad * 2;
    int rightW = W - rightX - pad;

    int y = 80;
    int lvH = 200;
    SetWindowPos(g_hListView, NULL, pad, y, leftW - pad, lvH, SWP_NOZORDER);
    y += lvH + 8;

    int btnW2 = (leftW - pad - 6) / 2;
    SetWindowPos(g_hBtnAdd, NULL, pad,             y, btnW2, 30, SWP_NOZORDER);
    SetWindowPos(g_hBtnDel, NULL, pad + btnW2 + 6, y, btnW2, 30, SWP_NOZORDER);
    y += 38;

    y += 4;
    SetWindowPos(g_hEditQ, NULL, pad + 160, y, 80, 30, SWP_NOZORDER);
    y += 38;

    int btnW = (leftW - pad - 6) / 2;
    SetWindowPos(g_hBtnRun,   NULL, pad,            y, btnW, 38, SWP_NOZORDER);
    SetWindowPos(g_hBtnClear, NULL, pad + btnW + 6, y, btnW, 38, SWP_NOZORDER);
    y += 50;

    int sbW = (leftW - pad - 6) / 2;
    int sbH = 36;
    SetWindowPos(g_hBtnA, NULL, pad,           y, sbW, sbH, SWP_NOZORDER);
    SetWindowPos(g_hBtnB, NULL, pad + sbW + 6, y, sbW, sbH, SWP_NOZORDER);
    y += sbH + 6;
    SetWindowPos(g_hBtnC, NULL, pad,           y, sbW, sbH, SWP_NOZORDER);
    SetWindowPos(g_hBtnD, NULL, pad + sbW + 6, y, sbW, sbH, SWP_NOZORDER);
    y += sbH + 6;
    SetWindowPos(g_hBtnE, NULL, pad,           y, leftW - pad, sbH, SWP_NOZORDER);

    int ganttH = 130;
    int halfW = (rightW - pad) / 2;

    SetWindowPos(g_hGanttRR,   NULL, rightX,                 60, halfW, ganttH, SWP_NOZORDER);
    SetWindowPos(g_hGanttSRTF, NULL, rightX + halfW + pad,   60, halfW, ganttH, SWP_NOZORDER);

    int resH = 200;
    int resY = 60 + ganttH + 10;
    SetWindowPos(g_hResRR,   NULL, rightX,               resY, halfW, resH, SWP_NOZORDER);
    SetWindowPos(g_hResSRTF, NULL, rightX + halfW + pad, resY, halfW, resH, SWP_NOZORDER);

    int sumY = resY + resH + 10;
    int sumH = H - sumY - pad;
    if (sumH < 60) sumH = 60;
    int sumW = (rightW - pad) / 2;
    SetWindowPos(g_hSummary,    NULL, rightX,              sumY, sumW,                sumH, SWP_NOZORDER);
    SetWindowPos(g_hConclusion, NULL, rightX + sumW + pad, sumY, rightW - sumW - pad, sumH, SWP_NOZORDER);
}

static void PaintGantt(HWND hWnd, HDC hdc, const SimResult& res, COLORREF accentColor, const wchar_t* label)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    int W = rc.right;
    int H = rc.bottom;

    HBRUSH hbrBg = CreateSolidBrush(C_PANEL);
    FillRect(hdc, &rc, hbrBg);
    DeleteObject(hbrBg);

    HPEN hpBorder = CreatePen(PS_SOLID, 1, C_BORDER);
    HPEN hpOld = (HPEN)SelectObject(hdc, hpBorder);
    HBRUSH hbOld = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 0, 0, W, H);
    SelectObject(hdc, hpOld);
    SelectObject(hdc, hbOld);
    DeleteObject(hpBorder);

    RECT titleR = {0, 0, W, 28};
    HBRUSH hbrAccent = CreateSolidBrush(accentColor);
    FillRect(hdc, &titleR, hbrAccent);
    DeleteObject(hbrAccent);

    RECT trText = {8, 4, W - 8, 28};
    DrawText2(hdc, label, trText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, C_BG, g_hFontHeader);

    if (!g_hasResult || res.gantt.empty()) {
        RECT msg = {10, 40, W - 10, H - 10};
        DrawText2(hdc, L"No simulation results yet. Run a simulation to see the Gantt chart.",
            msg, DT_CENTER | DT_WORDBREAK, C_TEXT3, g_hFontNormal);
        return;
    }

    int tEnd = 0;
    for (auto& b : res.gantt) tEnd = std::max(tEnd, b.end);
    if (tEnd == 0) return;

    int chartTop = 38, chartH = 50;
    int chartLeft = 10, chartRight = W - 10;
    int chartW = chartRight - chartLeft;
    float scale = (float)chartW / tEnd;

    for (auto& b : res.gantt) {
        int x1 = chartLeft + (int)(b.start * scale);
        int x2 = chartLeft + (int)(b.end   * scale);
        if (x2 <= x1) x2 = x1 + 1;

        COLORREF col = ProcColor(b.pid);
        HBRUSH hbr = CreateSolidBrush(col);
        HPEN hpen = CreatePen(PS_SOLID, 1, RGB(
            std::min(255, (int)GetRValue(col) + 40),
            std::min(255, (int)GetGValue(col) + 40),
            std::min(255, (int)GetBValue(col) + 40)));
        RECT br = {x1, chartTop, x2, chartTop + chartH};
        DrawRoundRect(hdc, br, 4, 4, hbr, hpen);
        DeleteObject(hbr);
        DeleteObject(hpen);

        if (x2 - x1 > 16) {
            wchar_t lbl[16];
            if (b.pid < 0) swprintf(lbl, 16, L"Idle");
            else           swprintf(lbl, 16, L"P%d", b.pid);
            RECT lr = {x1 + 1, chartTop, x2 - 1, chartTop + chartH};
            DrawText2(hdc, lbl, lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
                C_BG, g_hFontSmall);
        }
    }

    int axisY = chartTop + chartH + 4;
    HPEN hpAxis = CreatePen(PS_SOLID, 1, C_TEXT3);
    HPEN hpaOld = (HPEN)SelectObject(hdc, hpAxis);
    MoveToEx(hdc, chartLeft, axisY, NULL);
    LineTo(hdc, chartRight, axisY);
    SelectObject(hdc, hpaOld);
    DeleteObject(hpAxis);

    int maxTicks = chartW / 24;
    if (maxTicks < 1) maxTicks = 1;
    int step = std::max(1, (int)ceil((double)tEnd / maxTicks));
    for (int t = 0; t <= tEnd; t += step) {
        int tx = chartLeft + (int)(t * scale);
        HPEN hpTick = CreatePen(PS_SOLID, 1, C_TEXT3);
        HPEN hptOld = (HPEN)SelectObject(hdc, hpTick);
        MoveToEx(hdc, tx, axisY, NULL);
        LineTo(hdc, tx, axisY + 4);
        SelectObject(hdc, hptOld);
        DeleteObject(hpTick);

        wchar_t tstr[16];
        swprintf(tstr, 16, L"%d", t);
        RECT tr = {tx - 15, axisY + 5, tx + 15, axisY + 20};
        DrawText2(hdc, tstr, tr, DT_CENTER | DT_SINGLELINE, C_TEXT2, g_hFontSmall);
    }
}

static void PaintResults(HWND hWnd, HDC hdc, const SimResult& res, COLORREF accentColor, const wchar_t* label)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    int W = rc.right;
    int H = rc.bottom;

    HBRUSH hbrBg = CreateSolidBrush(C_PANEL);
    FillRect(hdc, &rc, hbrBg);
    DeleteObject(hbrBg);

    HPEN hpB = CreatePen(PS_SOLID, 1, C_BORDER);
    HPEN hpOld = (HPEN)SelectObject(hdc, hpB);
    HBRUSH hbOld = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 0, 0, W, H);
    SelectObject(hdc, hpOld);
    SelectObject(hdc, hbOld);
    DeleteObject(hpB);

    RECT titleR = {0, 0, W, 28};
    HBRUSH hbrAcc = CreateSolidBrush(accentColor);
    FillRect(hdc, &titleR, hbrAcc);
    DeleteObject(hbrAcc);
    RECT trText = {8, 4, W - 8, 28};
    DrawText2(hdc, label, trText, DT_LEFT | DT_VCENTER | DT_SINGLELINE, C_BG, g_hFontHeader);

    if (!g_hasResult || res.results.empty()) {
        RECT msg = {10, 40, W - 10, H - 10};
        DrawText2(hdc, L"Results will appear here after simulation.",
            msg, DT_CENTER | DT_WORDBREAK, C_TEXT3, g_hFontNormal);
        return;
    }

    const wchar_t* cols[] = {L"PID", L"AT", L"BT", L"CT", L"TAT", L"WT", L"RT"};
    int nCols = 7;
    int colW  = (W - 20) / nCols;
    int rowH  = 24;
    int tableTop = 32;

    HBRUSH hbrHead = CreateSolidBrush(C_PANEL2);
    RECT hr = {10, tableTop, W - 10, tableTop + rowH};
    FillRect(hdc, &hr, hbrHead);
    DeleteObject(hbrHead);

    for (int c = 0; c < nCols; c++) {
        RECT cr = {10 + c * colW, tableTop, 10 + (c + 1) * colW, tableTop + rowH};
        DrawText2(hdc, cols[c], cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
            accentColor, g_hFontSmall);
    }

    int y = tableTop + rowH;
    for (int i = 0; i < (int)res.results.size() && y + rowH < H - 40; i++) {
        const ProcessResult& r = res.results[i];
        COLORREF rowCol = (i % 2 == 0) ? C_PANEL : C_PANEL2;
        HBRUSH hbrRow = CreateSolidBrush(rowCol);
        RECT rr = {10, y, W - 10, y + rowH};
        FillRect(hdc, &rr, hbrRow);
        DeleteObject(hbrRow);

        COLORREF pcol = ProcColor(r.pid);
        HBRUSH hbrDot = CreateSolidBrush(pcol);
        HPEN   hpDot  = CreatePen(PS_SOLID, 1, pcol);
        HBRUSH hbDotOld = (HBRUSH)SelectObject(hdc, hbrDot);
        HPEN   hpDotOld = (HPEN)  SelectObject(hdc, hpDot);
        Ellipse(hdc, 14, y + 8, 22, y + 16);
        SelectObject(hdc, hbDotOld);
        SelectObject(hdc, hpDotOld);
        DeleteObject(hbrDot);
        DeleteObject(hpDot);

        wchar_t vals[7][32];
        swprintf(vals[0], 32, L"P%d", r.pid);
        swprintf(vals[1], 32, L"%d",  r.arrivalTime);
        swprintf(vals[2], 32, L"%d",  r.burstTime);
        swprintf(vals[3], 32, L"%d",  r.completionTime);
        swprintf(vals[4], 32, L"%d",  r.turnaroundTime);
        swprintf(vals[5], 32, L"%d",  r.waitingTime);
        swprintf(vals[6], 32, L"%d",  r.responseTime);

        for (int c = 0; c < nCols; c++) {
            RECT cr = {10 + c * colW, y, 10 + (c + 1) * colW, y + rowH};
            DrawText2(hdc, vals[c], cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
                C_TEXT, g_hFontSmall);
        }
        y += rowH;
    }

    if (y + rowH < H - 4) {
        HBRUSH hbrAvg = CreateSolidBrush(RGB(30, 40, 60));
        RECT ar = {10, y, W - 10, y + rowH};
        FillRect(hdc, &ar, hbrAvg);
        DeleteObject(hbrAvg);

        HPEN hpLine = CreatePen(PS_SOLID, 1, accentColor);
        HPEN hpLineOld = (HPEN)SelectObject(hdc, hpLine);
        MoveToEx(hdc, 10, y, NULL);
        LineTo(hdc, W - 10, y);
        SelectObject(hdc, hpLineOld);
        DeleteObject(hpLine);

        wchar_t avgWT[32], avgTAT[32], avgRT[32];
        swprintf(avgWT,  32, L"%.2f", res.avgWT);
        swprintf(avgTAT, 32, L"%.2f", res.avgTAT);
        swprintf(avgRT,  32, L"%.2f", res.avgRT);

        RECT lblR = {10, y, 10 + colW * 3, y + rowH};
        DrawText2(hdc, L"Averages", lblR,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, accentColor, g_hFontSmall);

        const wchar_t* avgVals[] = {L"", L"", L"", L"", avgTAT, avgWT, avgRT};
        for (int c = 4; c < nCols; c++) {
            RECT cr = {10 + c * colW, y, 10 + (c + 1) * colW, y + rowH};
            DrawText2(hdc, avgVals[c], cr,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE, C_YELLOW, g_hFontSmall);
        }
    }
}

static void PaintSummary(HWND hWnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    int W = rc.right;
    int H = rc.bottom;

    HBRUSH hbrBg = CreateSolidBrush(C_PANEL);
    FillRect(hdc, &rc, hbrBg);
    DeleteObject(hbrBg);

    HPEN hpB = CreatePen(PS_SOLID, 1, C_BORDER);
    HPEN hpOld = (HPEN)SelectObject(hdc, hpB);
    HBRUSH hbOld = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 0, 0, W, H);
    SelectObject(hdc, hpOld);
    SelectObject(hdc, hbOld);
    DeleteObject(hpB);

    RECT tr = {0, 0, W, 28};
    HBRUSH hbrT = CreateSolidBrush(RGB(45, 55, 85));
    FillRect(hdc, &tr, hbrT);
    DeleteObject(hbrT);
    RECT trText = {8, 4, W, 28};
    DrawText2(hdc, L"Comparison Summary", trText,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE, C_ACCENT, g_hFontHeader);

    if (!g_hasResult) {
        RECT msg = {10, 40, W - 10, H - 10};
        DrawText2(hdc, L"Summary will appear after simulation.",
            msg, DT_CENTER | DT_WORDBREAK, C_TEXT3, g_hFontNormal);
        return;
    }

    struct Metric { const wchar_t* name; double rr; double srtf; };
    Metric metrics[] = {
        {L"Avg Waiting Time",    g_rrResult.avgWT,  g_srtfResult.avgWT},
        {L"Avg Turnaround Time", g_rrResult.avgTAT, g_srtfResult.avgTAT},
        {L"Avg Response Time",   g_rrResult.avgRT,  g_srtfResult.avgRT},
    };

    int y = 34, rowH = 32;
    int col1 = 10, col3 = W - 10;
    int third = W / 3;

    RECT hhr = {col1, y, col3, y + 22};
    HBRUSH hbrHh = CreateSolidBrush(C_PANEL2);
    FillRect(hdc, &hhr, hbrHh);
    DeleteObject(hbrHh);

    RECT lblR = {col1 + 4, y, col1 + third, y + 22};
    DrawText2(hdc, L"Metric", lblR,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE, C_TEXT2, g_hFontSmall);
    RECT rrR = {col1 + third, y, col1 + 2 * third, y + 22};
    DrawText2(hdc, L"Round Robin", rrR,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE, C_RR, g_hFontSmall);
    RECT srR = {col1 + 2 * third, y, col3, y + 22};
    DrawText2(hdc, L"SRTF", srR,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE, C_SRTF, g_hFontSmall);
    y += 22;

    for (int i = 0; i < 3; i++) {
        Metric& m = metrics[i];
        COLORREF rowC = (i % 2 == 0) ? C_PANEL : C_PANEL2;
        HBRUSH hbrR = CreateSolidBrush(rowC);
        RECT rrf = {col1, y, col3, y + rowH};
        FillRect(hdc, &rrf, hbrR);
        DeleteObject(hbrR);

        RECT nameR = {col1 + 4, y, col1 + third, y + rowH};
        DrawText2(hdc, m.name, nameR,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, C_TEXT, g_hFontSmall);

        bool rrBetter   = m.rr   < m.srtf;
        bool srtfBetter = m.srtf < m.rr;

        wchar_t rrVal[32], srtfVal[32];
        swprintf(rrVal,   32, L"%.2f%ls", m.rr,   rrBetter   ? L"  *" : L"");
        swprintf(srtfVal, 32, L"%.2f%ls", m.srtf, srtfBetter ? L"  *" : L"");

        RECT rrV = {col1 + third, y, col1 + 2 * third, y + rowH};
        DrawText2(hdc, rrVal, rrV,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE,
            rrBetter ? C_GREEN : C_TEXT, g_hFontSmall);
        RECT srV = {col1 + 2 * third, y, col3, y + rowH};
        DrawText2(hdc, srtfVal, srV,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE,
            srtfBetter ? C_GREEN : C_TEXT, g_hFontSmall);
        y += rowH;
    }

    y += 8;
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

        RECT fairR = {col1, y, col3, y + 30};
        HBRUSH hbrF = CreateSolidBrush(C_PANEL2);
        FillRect(hdc, &fairR, hbrF);
        DeleteObject(hbrF);

        wchar_t fairStr[256];
        swprintf(fairStr, 256, L"Fairness (CV of WT) - RR: %.2f  SRTF: %.2f  ->  %ls is fairer",
            cvRR, cvSRTF, (cvRR < cvSRTF) ? L"Round Robin" : L"SRTF");
        RECT ft = {col1 + 4, y, col3 - 4, y + 30};
        DrawText2(hdc, fairStr, ft,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, C_YELLOW, g_hFontSmall);
        y += 38;
    }

    wchar_t qstr[128];
    swprintf(qstr, 128, L"Time Quantum: %d  |  Processes: %d",
        g_quantum, (int)g_processes.size());
    RECT qR = {col1 + 4, y, col3, y + 22};
    DrawText2(hdc, qstr, qR,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE, C_TEXT2, g_hFontSmall);
}

static void PaintConclusion(HWND hWnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    int W = rc.right;
    int H = rc.bottom;

    HBRUSH hbrBg = CreateSolidBrush(C_PANEL);
    FillRect(hdc, &rc, hbrBg);
    DeleteObject(hbrBg);

    HPEN hpB = CreatePen(PS_SOLID, 1, C_BORDER);
    HPEN hpOld = (HPEN)SelectObject(hdc, hpB);
    HBRUSH hbOld = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 0, 0, W, H);
    SelectObject(hdc, hpOld);
    SelectObject(hdc, hbOld);
    DeleteObject(hpB);

    RECT tr = {0, 0, W, 28};
    HBRUSH hbrT = CreateSolidBrush(RGB(45, 55, 85));
    FillRect(hdc, &tr, hbrT);
    DeleteObject(hbrT);
    RECT trText = {8, 4, W, 28};
    DrawText2(hdc, L"Analysis & Conclusion", trText,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE, C_ACCENT2, g_hFontHeader);

    if (!g_hasResult) {
        RECT msg = {10, 40, W - 10, H - 10};
        DrawText2(hdc, L"Conclusions will appear after simulation.",
            msg, DT_CENTER | DT_WORDBREAK, C_TEXT3, g_hFontNormal);
        return;
    }

    std::wstring txt;
    txt += L"WAITING TIME: ";
    txt += (g_rrResult.avgWT < g_srtfResult.avgWT) ? L"Round Robin" : L"SRTF";
    txt += L" achieved better avg WT (";
    txt += FormatDouble(std::min(g_rrResult.avgWT, g_srtfResult.avgWT));
    txt += L" vs " + FormatDouble(std::max(g_rrResult.avgWT, g_srtfResult.avgWT)) + L").\r\n\r\n";

    txt += L"RESPONSE TIME: ";
    txt += (g_rrResult.avgRT < g_srtfResult.avgRT) ? L"Round Robin" : L"SRTF";
    txt += L" delivered better avg response time (";
    txt += FormatDouble(std::min(g_rrResult.avgRT, g_srtfResult.avgRT));
    txt += L" vs " + FormatDouble(std::max(g_rrResult.avgRT, g_srtfResult.avgRT)) + L").\r\n\r\n";

    txt += L"FAIRNESS: Round Robin distributes CPU time in fixed quanta (Q=";
    txt += std::to_wstring(g_quantum);
    txt += L"), so no process waits excessively. SRTF favors short jobs, ";
    txt += L"which can starve longer processes.\r\n\r\n";

    txt += L"SHORT JOBS: SRTF strongly favors short jobs - they finish with minimal WT. ";
    txt += L"Round Robin gives equal slices regardless of length.\r\n\r\n";

    txt += L"QUANTUM EFFECT (Q=";
    txt += std::to_wstring(g_quantum);
    txt += L"): ";
    if (g_quantum <= 2)
        txt += L"Small quantum -> high context-switch overhead but very responsive.";
    else if (g_quantum <= 5)
        txt += L"Moderate quantum -> good balance of responsiveness and efficiency.";
    else
        txt += L"Large quantum -> fewer switches but fairness suffers (approaches FCFS).";
    txt += L"\r\n\r\n";

    txt += L"RECOMMENDATION: ";
    bool rrBetter = (g_rrResult.avgWT + g_rrResult.avgRT) < (g_srtfResult.avgWT + g_srtfResult.avgRT);
    if (rrBetter)
        txt += L"For this workload, Round Robin gave better overall performance. Prefer RR for interactive, time-sharing systems.";
    else
        txt += L"For this workload, SRTF gave better efficiency. Prefer SRTF in batch systems where short-job throughput matters.";

    RECT textR = {10, 34, W - 10, H - 8};
    DrawText2(hdc, txt.c_str(), textR, DT_LEFT | DT_WORDBREAK, C_TEXT, g_hFontSmall);
}

static LRESULT CALLBACK GanttRRProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT:
        DoubleBufferedPaint(hWnd, [](HWND h, HDC dc) {
            PaintGantt(h, dc, g_rrResult, C_RR, L"Round Robin - Gantt Chart");
        });
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK GanttSRTFProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT:
        DoubleBufferedPaint(hWnd, [](HWND h, HDC dc) {
            PaintGantt(h, dc, g_srtfResult, C_SRTF, L"SRTF - Gantt Chart");
        });
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK ResultsRRProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT:
        DoubleBufferedPaint(hWnd, [](HWND h, HDC dc) {
            PaintResults(h, dc, g_rrResult, C_RR, L"Round Robin - Results");
        });
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK ResultsSRTFProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT:
        DoubleBufferedPaint(hWnd, [](HWND h, HDC dc) {
            PaintResults(h, dc, g_srtfResult, C_SRTF, L"SRTF - Results");
        });
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK SummaryProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT:
        DoubleBufferedPaint(hWnd, [](HWND h, HDC dc) { PaintSummary(h, dc); });
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK ConclusionProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT:
        DoubleBufferedPaint(hWnd, [](HWND h, HDC dc) { PaintConclusion(h, dc); });
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
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
        RECT rc;
        GetClientRect(hWnd, &rc);
        int W = rc.right;

        FillRect(hdc, &rc, g_hbrBG);

        RECT titleArea = {0, 0, W, 56};
        HBRUSH hbrTitle = CreateSolidBrush(RGB(18, 22, 36));
        FillRect(hdc, &titleArea, hbrTitle);
        DeleteObject(hbrTitle);

        HBRUSH hbrLine = CreateSolidBrush(C_ACCENT);
        RECT line = {0, 0, W, 3};
        FillRect(hdc, &line, hbrLine);
        DeleteObject(hbrLine);

        RECT t1 = {10, 8, W / 2, 50};
        DrawText2(hdc, L"Round Robin  vs  SRTF", t1,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, C_TEXT, g_hFontTitle);

        RECT rrTag = {W / 2, 16, W / 2 + 120, 40};
        HBRUSH hbrRR = CreateSolidBrush(C_RR);
        FillRect(hdc, &rrTag, hbrRR);
        DeleteObject(hbrRR);
        DrawText2(hdc, L"Round Robin", rrTag,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, C_BG, g_hFontNormal);

        RECT srTag = {W / 2 + 128, 16, W / 2 + 228, 40};
        HBRUSH hbrSRTF = CreateSolidBrush(C_SRTF);
        FillRect(hdc, &srTag, hbrSRTF);
        DeleteObject(hbrSRTF);
        DrawText2(hdc, L"SRTF", srTag,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, C_WHITE, g_hFontNormal);

        RECT leftBg = {0, 56, 300, rc.bottom};
        HBRUSH hbrLeft = CreateSolidBrush(C_PANEL);
        FillRect(hdc, &leftBg, hbrLeft);
        DeleteObject(hbrLeft);

        RECT lbl1 = {10, 60, 290, 76};
        DrawText2(hdc, L"PROCESS TABLE", lbl1,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, C_TEXT2, g_hFontSmall);

        int lvH = 200;
        int yQ = 80 + lvH + 8 + 30 + 6;
        RECT lbl2 = {10, yQ + 4, 170, yQ + 32};
        DrawText2(hdc, L"TIME QUANTUM (Q):", lbl2,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, C_ACCENT, g_hFontNormal);

        int ySep = yQ + 38 + 50;
        HPEN hpSep = CreatePen(PS_SOLID, 1, C_BORDER);
        HPEN hpSepOld = (HPEN)SelectObject(hdc, hpSep);
        MoveToEx(hdc, 10, ySep, NULL);
        LineTo(hdc, 290, ySep);
        SelectObject(hdc, hpSepOld);
        DeleteObject(hpSep);

        RECT lbl3 = {10, ySep + 4, 290, ySep + 20};
        DrawText2(hdc, L"TEST SCENARIOS", lbl3,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, C_TEXT2, g_hFontSmall);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, C_PANEL);
        SetTextColor(hdc, C_TEXT);
        return (LRESULT)g_hbrPanel;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, C_PANEL2);
        SetTextColor(hdc, C_TEXT);
        return (LRESULT)g_hbrPanel2;
    }

    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, C_PANEL);
        SetTextColor(hdc, C_TEXT);
        return (LRESULT)g_hbrPanel;
    }

    case WM_NOTIFY: {
        NMHDR* nmh = (NMHDR*)lParam;
        if (nmh->idFrom == ID_LISTVIEW && nmh->code == NM_CUSTOMDRAW) {
            NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)lParam;
            switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT:
                return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT: {
                int idx = (int)cd->nmcd.dwItemSpec;
                cd->clrTextBk = (idx % 2 == 0) ? C_PANEL : C_PANEL2;
                cd->clrText   = C_TEXT;
                return CDRF_NEWFONT;
            }
            }
        }
        return CDRF_DODEFAULT;
    }

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
        mm->ptMinTrackSize.x = 900;
        mm->ptMinTrackSize.y = 600;
        return 0;
    }

    case WM_DESTROY:
        DestroyFontsAndBrushes();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
