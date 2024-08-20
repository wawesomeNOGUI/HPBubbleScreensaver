#ifndef UNICODE
#define UNICODE
#endif 

#include <windows.h>
#include <stdio.h>
#include <time.h>

const COLORREF TRANSPARENT_COLOR = RGB(0, 0, 0);
const COLORREF BACKGROUND_COLOR = RGB(1, 1, 1);

const int TIME_TILL_IDLE = 10000; // time in milliseconds

HANDLE idleCheckHandle;

int myWidth, myHeight;
int monitorWidth, monitorHeight;
HDC hMyDC;

// Function prototypes (forward declarations)
void GetMonitorRealResolution(HMONITOR hmon, int* pixelsWidth, int* pixelsHeight, MONITORINFOEX *info);
HWND CreateFullscreenWindow(HMONITOR hmon, HINSTANCE *hInstance, MONITORINFOEX *info);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void Wineventproc(
  HWINEVENTHOOK hWinEventHook,
  DWORD event,
  HWND hwnd,
  LONG idObject,
  LONG idChild,
  DWORD idEventThread,
  DWORD dwmsEventTime
);
LRESULT CALLBACK KeyboardProc(
  int    code,
  WPARAM wParam,
  LPARAM lParam
);
void DrawAscii();
DWORD WINAPI CheckUserInteractionLoop(LPVOID lpParam);

int main()
{
    srand(time(0));

    HINSTANCE hInstance;

    // Register the window class.
    const wchar_t CLASS_NAME[]  = L"Sample Window Class";
    
    WNDCLASS wc = { };

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HMONITOR hmon = MonitorFromWindow(GetForegroundWindow(),
                        MONITOR_DEFAULTTONEAREST);

    MONITORINFOEX info = { sizeof(MONITORINFOEX) };

    // get this monitor's width and height for drawing
    GetMonitorRealResolution(hmon, &monitorWidth, &monitorHeight, &info);
    printf("%d  %d\n", monitorWidth, monitorHeight);

    HWND hwnd = CreateFullscreenWindow(hmon, &hInstance, &info);

    // record this window's width and height
    // in my testing the windows width was sometimes different
    // than the monitor resolution even when window was fullscreen 
    // (presumably due to some windows scaling)
    RECT rect;
    GetClientRect(hwnd, &rect);
    myWidth = rect.right - rect.left;
    myHeight = rect.bottom - rect.top;

    // ShowWindow(hwnd, SW_NORMAL);

    // set window to be clickable through
    // https://stackoverflow.com/questions/13069717/letting-the-mouse-pass-through-windows-c
    // https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles?redirectedfrom=MSDN
    // WS_EX_APPWINDOW - forces taskbar icon to be shown, but also allows this window to be foreground?
    // WS_EX_NOACTIVATE - window cannot be foreground
    // WS_EX_TRANSPARENT | WS_EX_LAYERED
    // LONG cur_style = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_LAYERED);

    // Transparency settings for window
    SetLayeredWindowAttributes(hwnd, 
        TRANSPARENT_COLOR, // color that will be rendered fully transparent
        255,    // 0 - 255 controls overall trnasparency of window (alpha value)
        LWA_COLORKEY | LWA_ALPHA
    );

    // set window to always on top
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // Setup for drawing
	HDC hMyDC = GetDC(hwnd);
    
    // Start keypress/user interaction control thread for getting out of screensaver mode
    idleCheckHandle = CreateThread(
        NULL,           // default security attributes
        0,              // use default stack size  
        CheckUserInteractionLoop,  // function to run in new thread
        hwnd,   // thread function parameters
        0,      // thread runs immediately after creation
        NULL    // pointer to variable to receive thread id
    );

    // begin with the window minimized
    ShowWindow(hwnd, SW_MINIMIZE);

    // Run the message and update loop.
    // https://learn.microsoft.com/en-us/windows/win32/learnwin32/window-messages
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// https://stackoverflow.com/questions/4631292/how-to-detect-the-current-screen-resolution
void GetMonitorRealResolution(HMONITOR hmon, int* pixelsWidth, int* pixelsHeight, MONITORINFOEX *info)
{
    GetMonitorInfo(hmon, info);
    DEVMODE devmode = {};
    devmode.dmSize = sizeof(DEVMODE);
    EnumDisplaySettings(info->szDevice, ENUM_CURRENT_SETTINGS, &devmode);
    *pixelsWidth = devmode.dmPelsWidth;
    *pixelsHeight = devmode.dmPelsHeight;
}

// https://stackoverflow.com/questions/2382464/win32-full-screen-and-hiding-taskbar
HWND CreateFullscreenWindow(HMONITOR hmon, HINSTANCE *hInstance, MONITORINFOEX *info)
{
    const wchar_t CLASS_NAME[]  = L"Sample Window Class";
    
    return CreateWindowEx(
    0,
    CLASS_NAME,
    L"ASCII Screen",
    WS_POPUP | WS_MAXIMIZE | WS_VISIBLE,
    info->rcMonitor.left,
    info->rcMonitor.top,
    0,
    0,
    NULL,       // Parent window    
    NULL,       // Menu
    *hInstance,  // Instance handle
    NULL        // Additional application data
    );
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        {
            printf("Goodbye!");
            CloseHandle(idleCheckHandle);
            PostQuitMessage(0);
        }
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            hMyDC = BeginPaint(hwnd, &ps); 

            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hMyDC, &rect, CreateSolidBrush(BACKGROUND_COLOR));

            DrawAscii();

            EndPaint(hwnd, &ps); 

            Sleep(10.5); // thread sleep in milliseconds

            // mark window for redrawing if not minimized
            if (!IsIconic(hwnd))
                InvalidateRect(hwnd, NULL, true);
        }
        return 0;

    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void DrawAscii()
{
    SetTextColor(hMyDC, TRANSPARENT_COLOR);
    SetBkColor(hMyDC, BACKGROUND_COLOR);

    int i = 0;
    wchar_t myChar = 'H';

    for(int y = 0; y < myHeight; y += 13) {
        for(int x = 0; x < myWidth; x += 12){
            // printable ascii range is 32 (space) through 126 (~)
            // myChar = (int) rand() % (126 - 32 + 1) + 32;
            // myChar = charString[(int) rand() % (CHARS)];
            // myChar = charString[i++ % (CHARS)]; // print character string in a row
            ExtTextOutW(hMyDC, x, y, ETO_OPAQUE, NULL, &myChar, 1, NULL);
        }
    }
}

// used for checking if user presses bound key to exit program
// or other specified function
// only call this function once
DWORD WINAPI CheckUserInteractionLoop(LPVOID lpParam)
{
    bool idle = false;

    HWND hwnd = (HWND) lpParam;

    while(true)
    {
        Sleep(100); // thread sleep in milliseconds

        // check last input time
        // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getlastinputinfo
        LASTINPUTINFO plii;
        plii.cbSize = sizeof(LASTINPUTINFO);

        GetLastInputInfo(&plii);

        // printf("%d\n", GetTickCount() - plii.dwTime);

        // tick count in milliseconds
        if (idle && GetTickCount() - plii.dwTime < 250)
        {
            ShowWindow(hwnd, SW_MINIMIZE);
            idle = false;
        } else if (!idle && GetTickCount() - plii.dwTime > TIME_TILL_IDLE) {
            ShowWindow(hwnd, SW_MAXIMIZE);
            idle = true;
        }
    }
}