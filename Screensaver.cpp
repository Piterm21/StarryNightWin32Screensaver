#define WIN32_LEAN_AND_MEAN 1
#include "Windows.h"
#undef WIN32_LEAN_AND_MEAN

#include <stdint.h>
#include <atomic>
#include <thread>
#include <Scrnsave.h>

#include "resource.h"

#include "CPURenderer.h"
#include "World.h"
#include "FrameTimer.h"

static const CHAR MaxStarCountSettingLabel[] = "Max star count";
static const CHAR SettingsRegistryPath[] = "SOFTWARE\\Starry night";

static std::atomic<bool> g_Running = false;
static std::thread g_WorldAndRenderThread;

static void MainUpdate(HWND WindowHandle, uint32_t WindowWidth, uint32_t WindowHeight, uint32_t MaxStarCount)
{
    //Initialize world
    World WorldObject = { WindowWidth, WindowHeight, MaxStarCount };

    //Initialize renderer
    CPURenderer Renderer = { WindowWidth, WindowHeight };

    FrameTimer FrameTimerObject = { 1.0f / 15.0f };

    //Update and render as long as we are running
    while (g_Running.load())
    {
        Renderer.Clear();
        //Rendering is done as part of world update, ideally should split it, but seems a bit overkill for the context
        WorldObject.Tick(FrameTimerObject.CurrentFrameTime, Renderer);

        FrameTimerObject.WaitUntilFrametime();
        Renderer.Present(WindowHandle);
    }
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
    static HWND hSpeed;   // handle to speed scroll bar 
    static HWND hOK;      // handle to OK push button  
    static uint32_t MaxStarCount = World::DefaultStarCount;

    bool Result = false;

    switch (message)
    {
        case WM_INITDIALOG:
        {
            MaxStarCount = ReadMaxStarCountFromRegistry();

            // Initialize the redraw speed scroll bar control.
            hSpeed = GetDlgItem(hDlg, ID_COUNT);
            SetScrollRange(hSpeed, SB_CTL, World::MinStarCount, World::MaxStarCount, FALSE);
            SetScrollPos(hSpeed, SB_CTL, MaxStarCount, TRUE);

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

            SetScrollPos(hSpeed, SB_CTL, MaxStarCount, TRUE);
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

BOOL WINAPI RegisterDialogClasses(HANDLE hInst)
{
    return TRUE;
}

//Window proc for screen saver window created in Scrnsave main function
LRESULT WINAPI ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int64_t Result = 0;
    static uint32_t MaxCount = World::DefaultStarCount;

    switch (message)
    {
        case WM_CREATE:
        {
            srand(GetTickCount());
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
                g_WorldAndRenderThread.join();
            }

            g_Running = true;
            //Run the logic on separate thread to avoid using window events for timing which may be inaccurate
            g_WorldAndRenderThread = std::thread(MainUpdate, hWnd, Width, Height, MaxCount);
        } break;

        case WM_DESTROY:
        {
            //Stop running and wait for world/render thread to finish and join
            g_Running = false;
            g_WorldAndRenderThread.join();
        } break;

        default:
        {
            //Let Scrnsave handle all the other messages
            Result = DefScreenSaverProc(hWnd, message, wParam, lParam);
        } break;
    }

    return Result;
}