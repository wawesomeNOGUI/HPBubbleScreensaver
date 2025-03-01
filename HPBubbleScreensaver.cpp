#ifndef UNICODE
#define UNICODE
#endif 

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

const COLORREF TRANSPARENT_COLOR = RGB(0, 0, 0);
const COLORREF BACKGROUND_COLOR = RGB(1, 1, 1);
const int TIME_TILL_IDLE = 10000; // time in milliseconds

HANDLE idleCheckHandle;

int myWidth, myHeight;
int monitorWidth, monitorHeight;
HDC hDesktopDC, hMyDC, hdcMemDC;
HBITMAP hMyBmp;

//=======================Bubble Stuff=====================
const int NUMBER_OF_BUBBLES = 10;
int BUBBLE_RADIUS = 120; // default 120 but will scale based on screen size

struct BUBBLE {
    float x;
    float y;
    float r;
    float xVel;
    float yVel;
    float mass;
    bool doGrav;
};

BUBBLE bubbles[NUMBER_OF_BUBBLES];

void initializeBubbles(); // populates bubbles array
void wallCheck(BUBBLE* b);
void collisionCheck(BUBBLE* b);
void bubbleUpdate(BUBBLE* b);
void DrawBubbles();
//======================================================

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
DWORD WINAPI CheckUserInteractionLoop(LPVOID lpParam);

// drawing routines
void DrawBackground();

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
    
    printf("myWidth: %d myHeight: %d\n", myWidth, myHeight);

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

    // set window to not be included in screen capture
    // WDA_EXCLUDEFROMCAPTURE requires at least win 10
    SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE); 

    // Setup for drawing
    // https://learn.microsoft.com/en-us/windows/win32/gdi/capturing-an-image
	hMyDC = GetDC(hwnd);
    hDesktopDC = GetDC(NULL);

    // Create a compatible DC, which is used as a drawing buffer and then BitBlt to the window DC.
    hdcMemDC = CreateCompatibleDC(hMyDC);

    // "Before an application can use a memory DC for drawing operations, 
    // it must select a bitmap of the correct width and height into the DC. 
    // To select a bitmap into a DC, use the CreateCompatibleBitmap function, 
    // specifying the height, width, and color organization required."
    // https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createcompatibledc
    hMyBmp = CreateCompatibleBitmap(hMyDC, myWidth, myHeight);
    SelectObject(hdcMemDC, hMyBmp);

    // This is the best stretch mode. (need for stretching screenshot into bubble window??)
    SetStretchBltMode(hMyDC, HALFTONE);
    SetStretchBltMode(hdcMemDC, HALFTONE);

    // Select DC_PEN so you can change the color of the pen with
    // COLORREF SetDCPenColor(HDC hdc, COLORREF color)
    SelectObject(hdcMemDC, GetStockObject(DC_PEN));

    // Select DC_BRUSH so you can change the brush color from the 
    // default WHITE_BRUSH to any other color
    SelectObject(hdcMemDC, GetStockObject(DC_BRUSH));
    
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

    // initialize bubbles
    // scale radius based on screen size
    BUBBLE_RADIUS = (int) myWidth * myHeight / (NUMBER_OF_BUBBLES * 1000);
    printf("R: %d\n", BUBBLE_RADIUS);
    initializeBubbles();

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
    L"Bubble Screensaver",
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
            BeginPaint(hwnd, &ps); 

            // draw to hdcMemDC buffer device context
            DrawBackground();
            DrawBubbles();

            // Bit block transfer onto window dc
            if(!BitBlt(hMyDC, 0, 0, myWidth, myHeight, hdcMemDC, 0, 0, SRCCOPY)) {
                printf("Oh no");
            }

            EndPaint(hwnd, &ps); 

            Sleep(33); // thread sleep in milliseconds (33msec is about 30fps)

            // mark window for redrawing if not minimized
            if (!IsIconic(hwnd))
                InvalidateRect(hwnd, NULL, true);
        }
        return 0;

    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// populates bubbles array
void initializeBubbles()
{
    for (int i = 0; i < NUMBER_OF_BUBBLES; i++)
    {
        bubbles[i].x = rand() % myWidth;
        bubbles[i].y = rand() % myHeight;
        bubbles[i].r = BUBBLE_RADIUS;
        bubbles[i].mass = 10;
        bubbles[i].xVel = 0.5;
        bubbles[i].yVel = 0;

            // printf("X: %f, Y: %f, R: %f", bubbles[i].xVel, bubbles[i].y, bubbles[i].r);
    }
}

const float friction = 1; // no energy loss if == 1
// checks if bubble hitting wall
void wallCheck(BUBBLE* b) {
    // bottom & top
    if (b->y + b->r > myHeight) {
        b->y = myHeight - b->r;
        b->xVel *= friction;
        b->yVel *= -1 * friction;        
    } else if (b->y - b->r < 0) {
        b->y = b->r;
        b->xVel *= friction;
        b->yVel *= -1 * friction;
    }

    // sides
    if (b->x + b->r > myWidth) {
        b->x = myWidth - b->r;
        b->xVel *= -1 * friction;
        b->yVel *= friction;
    } else if (b->x - b->r < 0) {
        b->x = b->r;
        b->xVel *= -1 * friction;
        b->yVel *= friction;
    }
}

const float ballFriction = 1;
const float ballEnergyTransfer = 0.2;
// checks if bubble collided with other bubble
void collisionCheck(BUBBLE* b) {
    for (int i = 0; i < NUMBER_OF_BUBBLES; i++)
    {
        // skip self
        if (b == &bubbles[i])
            continue;

        float velLength = sqrt(pow(b->xVel, 2) + pow(b->yVel, 2));

        // first check if close enough to hit
        if ( sqrt(pow((b->x - bubbles[i].x), 2) + pow((b->y - bubbles[i].y), 2)) < (b->r + bubbles[i].r + velLength) ) {
            // find line between balls' centers 
            // this will be the line we reflect the angle of bounce around
            float normX = b->x - bubbles[i].x;
            float normY = b->y - bubbles[i].y;
            float normMagnitude = sqrt(pow(normX, 2) + pow(normY,2));
            
            // make normal vector length 1 (normalize vector)
            normX /= normMagnitude;
            normY /= normMagnitude;

            // reflect velocity vector over normal vector to find new velcoity after bounce
            float dotProduct = b->xVel * normX + b->yVel * normY;
            // var ang = Math.acos(dotProduct / velLength);

            // https://math.stackexchange.com/questions/13261/how-to-get-a-reflection-vector
            // derive by setting angle of current velcoity with normal equal to
            // angle of new velocity (reflection) with normal and
            // solve the dot product equation
            float newXVel = b->xVel - 2 * dotProduct * normX;
            float newYVel = b->yVel - 2 * dotProduct * normY;

            // move balls to just touching and update velocity
            b->x += normX * velLength; 
            b->y += normY * velLength; 
            
            b->xVel = newXVel * ballFriction;
            b->yVel = newYVel * ballFriction;

            // transfer some energy to other ball
            bubbles[i].xVel -= newXVel * ballFriction * ballEnergyTransfer / bubbles[i].mass;
            bubbles[i].yVel -= newYVel * ballFriction * ballEnergyTransfer / bubbles[i].mass;
        }
    }
}

// run in loop to update each bubble individually in bubbles array
void bubbleUpdate(BUBBLE* b) {
    b->x += b->xVel;
    b->y += b->yVel;

    wallCheck(b);
    collisionCheck(b);
}

// draws background (either slid color or intersting stuff into hdcMemDC)
// call before drawing bubbles
void DrawBackground()
{
    // fill background
    // SetBkColor(hdcMemDC, BACKGROUND_COLOR);
    SetDCPenColor(hdcMemDC, BACKGROUND_COLOR);
    SetDCBrushColor(hdcMemDC, RGB(25, 25, 25));
    Rectangle(hdcMemDC, 0, 0, myWidth, myHeight);

    // The source DC is the whole screen, and the destination DC is buffer dc (hdcMemDC).
    if (!StretchBlt(hdcMemDC,
        0, 0,
        myWidth, myHeight,
        hDesktopDC,
        0, 0,
        monitorWidth, monitorHeight,
        MERGECOPY))
    {
        printf("StretchBlt failed.\n");
        return;
    }
}

void DrawBubbles()
{
    // draw to buffer DC and when done bit blt to window

    // SetTextColor(hdcMemDC, TRANSPARENT_COLOR);
    // SetDCPenColor(hdcMemDC, RGB(255, 0, 0));
    SetDCPenColor(hdcMemDC, TRANSPARENT_COLOR);
    SetDCBrushColor(hdcMemDC, TRANSPARENT_COLOR);

    for (int i = 0; i < NUMBER_OF_BUBBLES; i++) {
        bubbleUpdate(&bubbles[i]);
        Ellipse(
            hdcMemDC,
            bubbles[i].x - bubbles[i].r, // top left bounding corner x
            bubbles[i].y - bubbles[i].r, // top left bounding corner y
            bubbles[i].x + bubbles[i].r, // bottom right bounding corner x
            bubbles[i].y + bubbles[i].r // bottom right bounding corner y
        );

        // if (i == 0)
        //     printf("X: %f, Y: %f, R: %f \n", bubbles[i].x, bubbles[i].y, bubbles[i].r);
    }
}

// used for checking if user presses bound key to exit program
// or other specified function
// only call this function once
DWORD WINAPI CheckUserInteractionLoop(LPVOID lpParam)
{
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
        // disable bubbles if window is not minimized and user action recently
        if (!IsIconic(hwnd) && GetTickCount() - plii.dwTime < 250)
        {
            ShowWindow(hwnd, SW_MINIMIZE);
        } else if (IsIconic(hwnd) && GetTickCount() - plii.dwTime > TIME_TILL_IDLE) {
            ShowWindow(hwnd, SW_MAXIMIZE);
        }
    }
}