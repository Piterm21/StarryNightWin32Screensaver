#include "Globals.h"

#include <atomic>

#include "resource.h"

#include "CPURenderer.h"
#include "World.h"
#include "FrameTimer.h"

static POINT InitialMousePosition;
static HINSTANCE hMainInstance;
static HWND hMainWindow;
static bool bClosing = false;
static bool bPreviewMode = false;

static const CHAR MaxStarCountSettingLabel[] = "Max star count";
static const CHAR SettingsRegistryPath[] = "SOFTWARE\\Starry night";

static std::atomic<bool> g_Running = false;

struct ScreensaverThreadData
{
    uint32_t WindowWidth;
    uint32_t WindowHeight;
    uint32_t MaxStarCount;
};

struct RunnableThread
{
    static DWORD ThreadMain(LPVOID lpParameter);

    ScreensaverThreadData Data;
    HANDLE ThreadHandle;
private:
    uint32_t Run();
} g_MainUpdateThread;

uint32_t RunnableThread::Run()
{
    //Initialize world
    World WorldObject = { Data.WindowWidth, Data.WindowHeight, Data.MaxStarCount };

    //Initialize renderer
    CPURenderer Renderer = { Data.WindowWidth, Data.WindowHeight };

    FrameTimer FrameTimerObject = { 1.0f / 15.0f };

    //Update and render as long as we are running
    while (g_Running.load())
    {
        Renderer.Clear();
        //Rendering is done as part of world update, ideally should split it, but seems a bit overkill for the context
        WorldObject.Tick(FrameTimerObject.CurrentFrameTime, Renderer);

        FrameTimerObject.WaitUntilFrametime();
        Renderer.Present(hMainWindow);
    }

    return 0;
}

DWORD RunnableThread::ThreadMain(LPVOID lpParameter)
{
    RunnableThread* Thread = (RunnableThread*)lpParameter;
    return Thread->Run();
}

static uint32_t ReadMaxStarCountFromRegistry()
{
    uint32_t Result = World::DefaultStarCount;

    HKEY Key;
    DWORD Disposition;

    //Get or create key for current user and path SettingsRegistryPath
    LSTATUS RegistryResult = RegCreateKeyExA(HKEY_CURRENT_USER, SettingsRegistryPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &Key, &Disposition);
    if (RegistryResult == ERROR_SUCCESS)
    {
        DWORD OutType;
        DWORD SizeOfBuffer = sizeof(Result);
        //Get the value MaxStarCountSettingLabel from key, filtered to DWORD type
        RegGetValueA(Key, NULL, MaxStarCountSettingLabel, RRF_RT_REG_DWORD, &OutType, &Result, &SizeOfBuffer);
    }

    //If we fall outside of range just use the default, value changed in registry by user or similar
    if (Result > World::MaxStarCount || Result < World::MinStarCount)
    {
        Result = World::DefaultStarCount;
    }

    return Result;
}

//Configuration dialog handling
BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hCount;   // handle to scroll bar 
    static HWND hOK;      // handle to OK push button  
    static uint32_t MaxStarCount = World::DefaultStarCount;

    bool Result = false;

    switch (message)
    {
        case WM_INITDIALOG:
        {
            MaxStarCount = ReadMaxStarCountFromRegistry();

            // Initialize the scroll bar control.
            hCount = GetDlgItem(hDlg, ID_COUNT);
            SetScrollRange(hCount, SB_CTL, World::MinStarCount, World::MaxStarCount, FALSE);
            SetScrollPos(hCount, SB_CTL, MaxStarCount, TRUE);

            // Retrieve a handle to the OK push button control.  
            hOK = GetDlgItem(hDlg, ID_OK);

            Result = true;
        } break;

        case WM_HSCROLL:
        {
            // Process scroll bar input
            switch (LOWORD(wParam))
            {
                case SB_PAGEUP:
                case SB_LINEUP:
                {
                    --MaxStarCount;
                } break;

                case SB_PAGEDOWN:
                case SB_LINEDOWN:
                {
                    ++MaxStarCount;
                } break;

                case SB_THUMBPOSITION: 
                {
                    MaxStarCount = HIWORD(wParam);
                } break;

                case SB_BOTTOM: 
                {
                    MaxStarCount = World::MinStarCount;
                } break;

                case SB_TOP: 
                {
                    MaxStarCount = World::MaxStarCount;
                } break;

                case SB_THUMBTRACK:
                case SB_ENDSCROLL:
                {
                    return TRUE;
                } break;
            }

            //We probably don't really need to clamp here
            if (MaxStarCount < World::MinStarCount)
            {
                MaxStarCount = World::MinStarCount;
            } 
            else if (MaxStarCount > World::MaxStarCount)
            {
                MaxStarCount = World::MaxStarCount;
            }

            SetScrollPos(hCount, SB_CTL, MaxStarCount, TRUE);
        } break;

        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case ID_OK: 
                {
                    HKEY Key;
                    DWORD Disposition;

                    //Get or create registry key for current user and path SettingsRegistryPath
                    LSTATUS Result = RegCreateKeyExA(HKEY_CURRENT_USER, SettingsRegistryPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &Key, &Disposition);
                    if (Result == ERROR_SUCCESS)
                    {
                        //Save set value as DWORD in the registry key under MaxStarCountSettingLabel value
                        RegSetKeyValueA(Key, NULL, MaxStarCountSettingLabel, REG_DWORD, &MaxStarCount, sizeof(MaxStarCount));
                    }
                }
                case ID_CANCEL:
                {
                    EndDialog(hDlg, LOWORD(wParam) == ID_OK);

                    return TRUE;
                }
            }
        } break;
    }

    return Result;
}

//Handling for messages we don't care about in preview mode
LRESULT WINAPI DefScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (bPreviewMode || bClosing)
    {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    switch (message)
    {
        case WM_CLOSE:
        {
            //Destroy window only if we are not closing already
            if (!bClosing)
            {
                DestroyWindow(hWnd);
            }

            return 0;
        }

        case WM_SETCURSOR:
        {
            SetCursor(NULL);

            return TRUE;
        }

        case WM_NCACTIVATE:
        case WM_ACTIVATE:
        case WM_ACTIVATEAPP:
        {
            if (wParam != FALSE)
            {
                break;
            }
        }
        case WM_MOUSEMOVE:
        {
            //Do nothing if position didn't change, otherwise fall through to exiting
            POINT CurrentMouseLocation;
            GetCursorPos(&CurrentMouseLocation);
            if (CurrentMouseLocation.x == InitialMousePosition.x && CurrentMouseLocation.y == InitialMousePosition.y)
            {
                break;
            }
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            if (!bClosing)
            {
                PostMessage(hWnd, WM_CLOSE, 0, 0);
            }
        } break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT WINAPI ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT Result = 0;
    static uint32_t MaxCount = World::DefaultStarCount;

    switch (message)
    {
        case WM_CREATE:
        {
            if (!bPreviewMode)
            {
                SetCursor(NULL);
            }
            GetCursorPos(&InitialMousePosition);

            SeedRandom(GetTickCount());
            MaxCount = ReadMaxStarCountFromRegistry();
        } break;

        //We can probably receive WM_ERASEBKGND if one of the monitors gets turned off, or window gets resized for whatever reason
        //We treat it as a full reset, will result in world and renderer being reinitialized
        case WM_ERASEBKGND:
        {
            //Get window size
            RECT Rectangle;
            GetClientRect(hWnd, &Rectangle);
            uint32_t Width = Rectangle.right - Rectangle.left;
            uint32_t Height = Rectangle.bottom - Rectangle.top;

            //If we were running stop running and wait for world/render thread to finish and join
            if (g_Running)
            {
                g_Running = false;
                WaitForSingleObject(g_MainUpdateThread.ThreadHandle, INFINITE);
            }

            g_MainUpdateThread.Data = { Width, Height, MaxCount };

            g_Running = true;
            //Run the logic on separate thread to avoid using window events for timing which may be inaccurate
            g_MainUpdateThread.ThreadHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(&RunnableThread::ThreadMain), &g_MainUpdateThread, 0, NULL);
        } break;

        case WM_DESTROY:
        {
            PostQuitMessage(0);

            //Stop running and wait for world/render thread to finish and join
            g_Running = false;
            WaitForSingleObject(g_MainUpdateThread.ThreadHandle, INFINITE);
        } break;

        case WM_SYSCOMMAND:
        {
            if (!bPreviewMode)
            {
                switch (wParam)
                {
                    case SC_CLOSE:
                    case SC_SCREENSAVE:
                    case SC_NEXTWINDOW:
                    case SC_PREVWINDOW:
                    {
                        return FALSE;
                    }
                }
            }
        } break;
        
        default:
        {
            //There are some messages we don't handle when running as a preview, handle it as a separate function for readability
            Result = DefScreenSaverProc(hWnd, message, wParam, lParam);
        } break;
    }

    return Result;
}

static WPARAM LaunchScreenSaver(HWND hParent)
{
    WPARAM Result = 0;

    char WindowClassName[] = "WindowsScreenSaverClass";

    WNDCLASS WindowClass;
    WindowClass.hCursor = NULL;
    WindowClass.hIcon = LoadIcon(hMainInstance, MAKEINTATOM(ID_APP));
    WindowClass.lpszMenuName = NULL;
    WindowClass.lpszClassName = WindowClassName;
    WindowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    WindowClass.hInstance = hMainInstance;
    WindowClass.style = CS_VREDRAW | CS_HREDRAW | CS_SAVEBITS | CS_PARENTDC;
    WindowClass.lpfnWndProc = (WNDPROC)ScreenSaverProc;
    WindowClass.cbWndExtra = 0;
    WindowClass.cbClsExtra = 0;

    if (RegisterClass(&WindowClass))
    {
        UINT WindowStyle;
        RECT WindowRect;

        //Different handling for preview window
        if (hParent)
        {
            WindowStyle = WS_CHILD;
            GetClientRect(hParent, &WindowRect);
        }
        else
        {
            WindowStyle = WS_POPUP | WS_VISIBLE;

            WindowRect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
            WindowRect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
            WindowRect.right = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            WindowRect.bottom = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        }

        hMainWindow = CreateWindowEx(hParent ? 0 : WS_EX_TOPMOST, WindowClassName, "Starry night", WindowStyle,
            WindowRect.left, WindowRect.top, WindowRect.right, WindowRect.bottom, hParent, NULL, hMainInstance, NULL);

        if (hMainWindow)
        {
            UpdateWindow(hMainWindow);
            ShowWindow(hMainWindow, SW_SHOW);

            MSG Message;
            Message.wParam = Result;

            while (GetMessage(&Message, NULL, 0, 0) == TRUE)
            {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            }

            Result = Message.wParam;
        }
    }

    return Result;
}

static void LaunchConfig(void)
{
    DialogBox(NULL, MAKEINTRESOURCE(DLG_SCRNSAVECONFIGURE), GetForegroundWindow(), (DLGPROC)ScreenSaverConfigureDialog);
}

static uint64_t TextToUInt64(const char* Buffer)
{
    uint64_t Result = 0;

    const char* WorkingBufferPointer = Buffer;
    
    //Skip to the end, nullptr, or not a number character
    while (*WorkingBufferPointer)
    {
        if (*WorkingBufferPointer < '0' || *WorkingBufferPointer > '9')
        {
            break;
        }

        WorkingBufferPointer++;
    }

    WorkingBufferPointer--;

    uint64_t Numerator = 1;
    //Extract characters and put them into result
    while (WorkingBufferPointer >= Buffer)
    {
        Result += (*WorkingBufferPointer - '0') * Numerator;

        WorkingBufferPointer--;
        Numerator *= 10;
    }

    return Result;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR CmdLine, int nCmdShow)
{
    hMainInstance = hInst;

    LPSTR TextBufferPointer;
    for (TextBufferPointer = CmdLine; *TextBufferPointer; TextBufferPointer++)
    {
        switch (*TextBufferPointer)
        {
            //Start standard screen saver logic
            case 'S':
            case 's':
            {
                return (int)LaunchScreenSaver(NULL);
            }

            //Preview mode
            case 'P':
            case 'p':
            {
                HWND hParent;
                bPreviewMode = TRUE;
                TextBufferPointer++;
                while ((*TextBufferPointer == ' ' || *TextBufferPointer == '\t'))
                {
                    TextBufferPointer++;
                }

                hParent = (HWND)TextToUInt64(TextBufferPointer);
                if (hParent && IsWindow(hParent))
                {
                    return (int)LaunchScreenSaver(hParent);
                }

                return 0;
            }

            //Config
            case 'C':
            case 'c':
            {
                LaunchConfig();

                return 0;
            }

            default:
            {
                break;
            }
        }
    }

    LaunchConfig();

    return 0;
}

IMAGE_DOS_HEADER __ImageBase;

typedef void(__cdecl* _PVFV)(void);
typedef int(__cdecl* _PIFV)(void);

//Taken from \tools_toolchain_sdk10_1607\Source\10.0.14393.0\ucrt\startup\initterm.cpp
// Calls each function in [first, last).  [first, last) must be a valid range of
// function pointers.  Each function is called, in order.
void __cdecl _initterm(_PVFV* const First, _PVFV* const Last)
{
    for (_PVFV* Current = First; Current != Last; ++Current)
    {
        if (*Current == nullptr)
        {
            continue;
        }

        (**Current)();
    }
}

//Taken from \tools_toolchain_sdk10_1607\Source\10.0.14393.0\ucrt\startup\initterm.cpp
// Calls each function in [first, last).  [first, last) must be a valid range of
// function pointers.  Each function must return zero on success, nonzero on
// failure.  If any function returns nonzero, iteration stops immediately and
// the nonzero value is returned.  Otherwise all functions are called and zero
// is returned.
//
// If a nonzero value is returned, it is expected to be one of the runtime error
// values (_RT_{NAME}, defined in the internal header files).
int32_t __cdecl _initterm_e(_PIFV* const First, _PIFV* const Last)
{
    for (_PIFV* Current = First; Current != Last; ++Current)
    {
        if (*Current == nullptr)
        {
            continue;
        }

        int32_t const Result = (**Current)();
        if (Result != 0)
        {
            return Result;
        }
    }

    return 0;
}

//Taken from \MSVC\14.41.34120\crt\src\vcruntime\internal_shared.h
#pragma section(".CRT$XCA",    long, read) // First C++ Initializer
#pragma section(".CRT$XCAA",   long, read) // Startup C++ Initializer
#pragma section(".CRT$XCZ",    long, read) // Last C++ Initializer

#pragma section(".CRT$XIA",    long, read) // First C Initializer
#pragma section(".CRT$XIAA",   long, read) // Startup C Initializer
#pragma section(".CRT$XIAB",   long, read) // PGO C Initializer
#pragma section(".CRT$XIAC",   long, read) // Post-PGO C Initializer
#pragma section(".CRT$XIC",    long, read) // CRT C Initializers
#pragma section(".CRT$XIYA",   long, read) // VCCorLib Threading Model Initializer
#pragma section(".CRT$XIYAA",  long, read) // XAML Designer Threading Model Override Initializer
#pragma section(".CRT$XIYB",   long, read) // VCCorLib Main Initializer
#pragma section(".CRT$XIZ",    long, read) // Last C Initializer

#define _CRTALLOC(x) __declspec(allocate(x))

//Taken from \MSVC\14.41.34120\crt\src\vcruntime\initializers.cpp
_CRTALLOC(".CRT$XIA") _PIFV __xi_a[] = { nullptr }; // C initializers (first)
_CRTALLOC(".CRT$XIZ") _PIFV __xi_z[] = { nullptr }; // C initializers (last)
_CRTALLOC(".CRT$XCA") _PVFV __xc_a[] = { nullptr }; // C++ initializers (first)
_CRTALLOC(".CRT$XCZ") _PVFV __xc_z[] = { nullptr }; // C++ initializers (last)

WORD GetShowWindowMode()
{
    STARTUPINFOW StartupInfo{};
    GetStartupInfoW(&StartupInfo);

    return StartupInfo.dwFlags & STARTF_USESHOWWINDOW ? StartupInfo.wShowWindow : SW_SHOWDEFAULT;
}

char* GetCommandLineASCI()
{
    static char* CommandLineStatic = nullptr;
    
    if (CommandLineStatic)
    {
        return CommandLineStatic;
    }

    char* CurrentPointer = GetCommandLineA();
    //Skip to end of quoted text
    if (*CurrentPointer == '"')
    {
        CurrentPointer++;

        while (*CurrentPointer && *CurrentPointer != '"')
        {
            CurrentPointer++;
        }
    }
    //Skip until we hit whitespace
    else
    {
        while (*CurrentPointer && *CurrentPointer != ' ' && *CurrentPointer != '\t')
        {
            CurrentPointer++;
        }
    }

    //Skip until end of whitespace
    while (*CurrentPointer == ' ' || *CurrentPointer == '\t')
    {
        CurrentPointer++;
    }

    return CommandLineStatic = CurrentPointer;
}

//There is a lot of startup logic which technically should be done here, but this is sufficient for the needs of this program
DWORD WinMainCRTStartup(LPVOID)
{
    if (_initterm_e(__xi_a, __xi_z) != 0)
        return 255;

    _initterm(__xc_a, __xc_z);

    int32_t const Result = WinMain(reinterpret_cast<HINSTANCE>(&__ImageBase), nullptr, GetCommandLineASCI(), GetShowWindowMode());
    
    //There might be some cleanup we "should" do here, but it really shouldn't matter as we are shutting down anyway

    TerminateProcess(GetCurrentProcess(), Result);

    return Result;
}