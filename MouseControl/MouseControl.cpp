
#include "framework.h"
#include "MouseControl.h"
#include <shellapi.h>
#include <windows.h>
#include <stdlib.h>
#include <string>
#include <stdio.h>
#include <cstdlib>
#include <ctime>
#include <vector>

#define MAX_LOADSTRING 100
#define TRAY_ICONUID 100
#define WM_TRAYMESSAGE (WM_USER + 1)
#define IDM_SHOW (IDM_EXIT + 1)

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
wchar_t mousemessage[256];      // this is the string we draw to the screen for the first mouse 
wchar_t debugInfo[256];     // this is the string we draw to the screen for the second mouse 
wchar_t rawinputdevices[256];   // string with number of raw input devices
int state;   // string with number of raw input devices
int mainMouseID;
POINT lastMousePos;
int lastDisplay;
std::clock_t lastTime;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


struct MonitorRects
{
    std::vector<RECT>   rcMonitors;
    RECT                rcCombined;

    static BOOL CALLBACK MonitorEnum(HMONITOR hMon, HDC hdc, LPRECT lprcMonitor, LPARAM pData)
    {
        MonitorRects* pThis = reinterpret_cast<MonitorRects*>(pData);
        pThis->rcMonitors.push_back(*lprcMonitor);
        UnionRect(&pThis->rcCombined, &pThis->rcCombined, lprcMonitor);
        return TRUE;
    }

    MonitorRects()
    {
        SetRectEmpty(&rcCombined);
        EnumDisplayMonitors(0, 0, MonitorEnum, (LPARAM)this);
    }
};

MonitorRects* monitors;


void InitRawInput(HWND hWnd)
{
    printf("Initializing...");
    RAWINPUTDEVICE Rid[50]; // allocate storage for 50 device (not going to need this many :) )
    UINT nDevices;
    PRAWINPUTDEVICELIST pRawInputDeviceList;

    if (GetRawInputDeviceList(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST)) != 0) {
        return;
    }

    pRawInputDeviceList = (RAWINPUTDEVICELIST*)malloc(sizeof(RAWINPUTDEVICELIST) * nDevices);

    GetRawInputDeviceList(pRawInputDeviceList, &nDevices, sizeof(RAWINPUTDEVICELIST));

    // do the job...
    wsprintf(rawinputdevices, L"Number of raw input devices: %i\n\n", nDevices);
    printf("Number of raw input devices: %i\n\n", nDevices);
    // after the job, free the RAWINPUTDEVICELIST
    free(pRawInputDeviceList);

    Rid[0].usUsagePage = 0x01;
    Rid[0].usUsage = 0x02;
    Rid[0].dwFlags = RIDEV_INPUTSINK;// RIDEV_NOLEGACY;   // adds HID mouse and also ignores legacy mouse messages
    Rid[0].hwndTarget = hWnd;

    if (RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])) == FALSE) {
        wsprintf(mousemessage, L"RawInput init failed:\n");
        printf("RawInput init failed:\n");
        //registration failed. 
    }

    state = 0;

    return;
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MOUSECONTROL, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MOUSECONTROL));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MOUSECONTROL));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_MOUSECONTROL);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }
    InitRawInput(hWnd);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//


int WhichDisplay(POINT p)
{
    for (int i = 0; i < monitors->rcMonitors.size(); i++) {
        RECT displayRect = monitors->rcMonitors[i];
        if (p.x >= displayRect.left && p.x <= displayRect.right && p.y >= displayRect.top && p.y <= displayRect.bottom)
            return i;
    }
    return  monitors->rcMonitors.size();
}


void ControlMouse(int deviceID)
{
    POINT p;
    GetCursorPos(&p);
    int display = WhichDisplay(p);
    std::clock_t time = std::clock();

    switch (state) {
    case 0:
        lastMousePos = p;
        lastTime = time;
        lastDisplay = display;
        mainMouseID = deviceID;
        state = 1;
        break;

    case 1:
        if (deviceID == mainMouseID) {
            float idleTime = (time - lastTime) / (float)CLOCKS_PER_SEC;
            if (idleTime >= 0.5 && display != lastDisplay)
                SetCursorPos(lastMousePos.x, lastMousePos.y);
            else {
                lastMousePos = p;
                lastTime = time;
                lastDisplay = display;
            }
        }
        break;
    }
}


void TrayDrawIcon(HWND hWnd) {
    NOTIFYICONDATA nid;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICONUID;
    nid.uVersion = NOTIFYICON_VERSION;
    nid.uCallbackMessage = WM_TRAYMESSAGE;
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SMALL));
    LoadString(hInst, IDS_APP_TITLE, nid.szTip, 128);
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void TrayDeleteIcon(HWND hWnd) {
    NOTIFYICONDATA nid;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICONUID;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hwndButton = 0;
    static int cx, cy;          /* Height and width of our button. */
    HDC hdc;             /* A device context used for drawing */
    PAINTSTRUCT ps;              /* Also used during window drawing */
    RECT rc;              /* A rectangle used during drawing */
    LPBYTE lpb;// = new BYTE[dwSize];//LPBYTE lpb = new BYTE[dwSize];
    UINT dwSize;
    RAWINPUT* raw;
    long tmpx, tmpy;
    POINT p;
    static long maxx = 0;
    int deviceID;

    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            TrayDeleteIcon(hWnd);
            DestroyWindow(hWnd);
            break;
        case IDM_SHOW:
            SetForegroundWindow(hWnd);
            ShowWindow(hWnd, SW_RESTORE);
            //TrayDeleteIcon(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_PAINT:
    {
        wchar_t stateText[10];
        switch (state)
        {
        case 0: wsprintf(stateText, L"Initial"); break;
        case 1: wsprintf(stateText, L"Active"); break;
        case 2: wsprintf(stateText, L"Idle"); break;
        }
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: Add any drawing code that uses hdc here...
        GetClientRect(hWnd, &rc);
        DrawText(hdc, mousemessage, wcslen(mousemessage), &rc, DT_CENTER);
        OffsetRect(&rc, 0, 100); // move the draw position down a bit
        DrawText(hdc, stateText, wcslen(stateText), &rc, DT_CENTER);
        OffsetRect(&rc, 0, 200); // move the draw position down a bit
        DrawText(hdc, debugInfo, wcslen(debugInfo), &rc, DT_CENTER);
        EndPaint(hWnd, &ps);
    }
    break;

    case WM_DESTROY:
    {
        TrayDeleteIcon(hWnd);
        PostQuitMessage(0);
        break;
    }

    case WM_INPUT:
    {
        monitors = new MonitorRects;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
        lpb = (LPBYTE)malloc(sizeof(LPBYTE) * dwSize);
        if (lpb == NULL)
            return 0;

        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
            OutputDebugString(TEXT("GetRawInputData doesn't return correct size !\n"));

        raw = (RAWINPUT*)lpb;

        if (raw->header.dwType == RIM_TYPEMOUSE)
        {
            tmpx = raw->data.mouse.lLastX;
            tmpy = raw->data.mouse.lLastY;
            deviceID = (int)raw->header.hDevice;
            ControlMouse(deviceID);
        }
        else
        {
            tmpx = 0;
            tmpy = 0;
            deviceID = -1;
        }

        if (!GetCursorPos(&p))
        {
            p.x = 0;
            p.y = 0;
        }
        wsprintf(mousemessage, L"Mouse:hDevice %d \nlLastX=%ld \nlLastY=%ld \nPosition: x%d-y%d \nInside Display: %d/%d",
            deviceID,
            tmpx, //raw->data.mouse.lLastX, 
            tmpy, //raw->data.mouse.lLastY, 
            p.x,
            p.y,
            WhichDisplay(p) + 1,
            monitors->rcMonitors.size());

        InvalidateRect(hWnd, 0, TRUE);
        SendMessage(hWnd, WM_PAINT, 0, 0);
        free(lpb);
        break;
    }

    case WM_SIZE:
    {
        if (wParam == SIZE_MINIMIZED) {
            TrayDrawIcon(hWnd);
            ShowWindow(hWnd, SW_HIDE);
        }
        break;
    }

    case WM_TRAYMESSAGE:
    {
        HMENU hMenuPopup;
        switch (lParam) {
        case WM_LBUTTONDBLCLK:
            SetForegroundWindow(hWnd);
            ShowWindow(hWnd, SW_RESTORE);
            //TrayDeleteIcon(hWnd);
            break;
        case WM_RBUTTONUP:
            POINT p;
            GetCursorPos(&p);
            hMenuPopup = CreatePopupMenu();
            AppendMenu(hMenuPopup, MF_STRING, IDM_SHOW, L"Show window");
            AppendMenu(hMenuPopup, MF_STRING, IDM_EXIT, L"Exit");
            TrackPopupMenu(hMenuPopup, TPM_LEFTALIGN, p.x, p.y, 0, hWnd, NULL);
            DestroyMenu(hMenuPopup);
            break;
        }
        break;
    }

    /* Junk
    case WM_TOUCH:
    {
        wsprintf(mousemessage, L"Touch");
        UINT cInputs = LOWORD(wParam);
        PTOUCHINPUT pInputs = new TOUCHINPUT[cInputs];
        if (NULL != pInputs)
        {
            if (GetTouchInputInfo((HTOUCHINPUT)lParam,
                cInputs,
                pInputs,
                sizeof(TOUCHINPUT)))
            {
                // process pInputs
                wsprintf(mousemessage, L"Touch");
                if (!CloseTouchInputHandle((HTOUCHINPUT)lParam))
                {
                    // error handling
                }
            }
            else
            {
                // GetLastError() and error handling
            }
            delete[] pInputs;
        }
        break;
    }*/

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
