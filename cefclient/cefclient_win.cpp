// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "include/cef_runnable.h"
#include "cefclient.h"
#include "client_handler.h"
#include "resource.h"
#include <commdlg.h>
#include <direct.h>
#include <sstream>
#include <shellapi.h>

#define MAX_LOADSTRING 100
#define MAX_URL_LENGTH  255
#define BUTTON_WIDTH 72
#define URLBAR_HEIGHT  24

// Global Variables:
HINSTANCE hInst;								// current instance
char szWorkingDir[MAX_PATH];   // The current working directory
UINT uFindMsg;  // Message identifier for find events.
HWND hFindDlg = NULL; // Handle for the find dialog.

// Forward declarations of functions included in this code module:
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);

// The global ClientHandler reference.
extern CefRefPtr<ClientHandler> g_handler;
extern CefRefPtr<CefCommandLine> g_command_line;

#if defined(OS_WIN)
// Add Common Controls to the application manifest because it's required to
// support the default tooltip implementation.
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

// Program entry point function.
int APIENTRY wWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  // Retrieve the current working directory.
  if(_getcwd(szWorkingDir, MAX_PATH) == NULL)
    szWorkingDir[0] = 0;

  // Parse command line arguments. The passed in values are ignored on Windows.
  AppInitCommandLine(0, NULL);

  CefSettings settings;
  CefRefPtr<CefApp> app;

  // Populate the settings based on command line arguments.
  AppGetSettings(settings, app);

  // Initialize CEF.
  CefInitialize(settings, app);

  HACCEL hAccelTable;

  std::wstring optName(L"CefClient");
  int optWidth = 800;
  int optHeight = 600;
  if (g_command_line->HasSwitch("name"))
    optName = g_command_line->GetSwitchValue("name").ToWString();
  if (g_command_line->HasSwitch("width"))
    optWidth = _wtoi(g_command_line->GetSwitchValue("width").c_str());
  if (g_command_line->HasSwitch("height"))
    optHeight = _wtoi(g_command_line->GetSwitchValue("height").c_str());

  WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CEFCLIENT));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_CEFCLIENT);
	wcex.lpszClassName	= optName.c_str();
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	if (!RegisterClassEx(&wcex)) return FALSE;

  // Perform window initialization
  HWND hWnd;
  hInst = hInstance; // Store instance handle in our global variable
  hWnd = CreateWindow(
    optName.c_str(),
    optName.c_str(),
    WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
    CW_USEDEFAULT, 0,
    optWidth,
    optHeight,
    NULL, NULL, hInstance, NULL);

  if (!hWnd) return FALSE;

  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CEFCLIENT));

  // Register the find event message.
  uFindMsg = RegisterWindowMessage(FINDMSGSTRING);

  int result = 0;

  if (!settings.multi_threaded_message_loop) {
    // Run the CEF message loop. This function will block until the application
    // recieves a WM_QUIT message.
    CefRunMessageLoop();
  } else {
    MSG msg;
  
    // Run the application message loop.
    while (GetMessage(&msg, NULL, 0, 0)) {
      // Allow processing of find dialog messages.
      if (hFindDlg && IsDialogMessage(hFindDlg, &msg))
        continue;

      if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

    result = (int)msg.wParam;
  }

  // Shut down CEF.
  CefShutdown();

  return result;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  static HWND backWnd = NULL, forwardWnd = NULL, reloadWnd = NULL,
      stopWnd = NULL, editWnd = NULL;
  static WNDPROC editWndOldProc = NULL;
  
  // Static members used for the find dialog.
  static FINDREPLACE fr;
  static WCHAR szFindWhat[80] = {0};
  static WCHAR szLastFindWhat[80] = {0};
  static bool findNext = false;
  static bool lastMatchCase = false;
  
  int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

    // Callback for the main window
	  switch (message)
	  {
    case WM_CREATE:
      {
        // Create the single static handler class instance
        g_handler = new ClientHandler();
        g_handler->SetMainHwnd(hWnd);

        // Create the child windows used for navigation
        RECT rect;
        int x = 0;
        
        GetClientRect(hWnd, &rect);
        CefWindowInfo info;
        CefBrowserSettings settings;

        // Populate the settings based on command line arguments.
        AppGetBrowserSettings(settings);

        // Initialize window info to the defaults for a child window
        info.SetAsChild(hWnd, rect);

        // Creat the new child browser window
        CefString url = CefString("http://google.com");
        if (g_command_line->HasSwitch("url"))
          url = g_command_line->GetSwitchValue("url");
        CefBrowser::CreateBrowser(info,
          static_cast<CefRefPtr<CefClient> >(g_handler),
		      url, settings);

        // Set window icon if --ico flag is set.
        if (g_command_line->HasSwitch("ico")) {
          std::wstring ico;
          ico = g_command_line->GetSwitchValue("ico").ToWString();
          HANDLE hIconBig = LoadImage(NULL, ico.c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
          HANDLE hIconSmall = LoadImage(NULL, ico.c_str(), IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
          SendMessage(hWnd, WM_SETICON, ICON_BIG, LPARAM(hIconBig));
          SendMessage(hWnd, WM_SETICON, ICON_SMALL, LPARAM(hIconSmall));
        }
      }
      return 0;

    case WM_COMMAND:
      {
        CefRefPtr<CefBrowser> browser;
        if(g_handler.get())
          browser = g_handler->GetBrowser();

        wmId    = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_EXIT:
          DestroyWindow(hWnd);
          return 0;
        case ID_WARN_DOWNLOADCOMPLETE:
        case ID_WARN_DOWNLOADERROR:
          if(g_handler.get()) {
            std::wstringstream ss;
            ss << L"File \"" <<
                std::wstring(CefString(g_handler->GetLastDownloadFile())) <<
                L"\" ";

            if(wmId == ID_WARN_DOWNLOADCOMPLETE)
              ss << L"downloaded successfully.";
            else
              ss << L"failed to download.";

            MessageBox(hWnd, ss.str().c_str(), L"File Download",
                MB_OK | MB_ICONINFORMATION);
          }
          return 0;
        }
      }
      break;

    case WM_PAINT:
      hdc = BeginPaint(hWnd, &ps);
      EndPaint(hWnd, &ps);
      return 0;

    case WM_SETFOCUS:
      if(g_handler.get() && g_handler->GetBrowserHwnd())
      {
        // Pass focus to the browser window
        PostMessage(g_handler->GetBrowserHwnd(), WM_SETFOCUS, wParam, NULL);
      }
      return 0;

    case WM_SIZE:
      if(g_handler.get() && g_handler->GetBrowserHwnd())
      {
        // Resize the browser window to match the new frame window size
        RECT rect;
        GetClientRect(hWnd, &rect);
        HDWP hdwp = BeginDeferWindowPos(1);
        hdwp = DeferWindowPos(hdwp, g_handler->GetBrowserHwnd(), NULL,
          rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
          SWP_NOZORDER);
        EndDeferWindowPos(hdwp);
      }
      break;

    case WM_ERASEBKGND:
      if(g_handler.get() && g_handler->GetBrowserHwnd())
      {
        // Dont erase the background if the browser window has been loaded
        // (this avoids flashing)
        return 0;
      }
      break;

    case WM_CLOSE:
      if (g_handler.get()) {
        CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
        if (browser.get()) {
          // Let the browser window know we are about to destroy it.
          browser->ParentWindowWillClose();
        }
      }
      break;

    case WM_DESTROY:
      // The frame window has exited
      PostQuitMessage(0);
	  return 0;

	// Restricts window resizing smaller than the values below.
	case WM_GETMINMAXINFO:
    int minwidth = 600;
    int minheight = 400;
    if (g_command_line->HasSwitch("minwidth"))
      minwidth = _wtoi(g_command_line->GetSwitchValue("minwidth").c_str());
    if (g_command_line->HasSwitch("minheight"))
      minheight = _wtoi(g_command_line->GetSwitchValue("minheight").c_str());

    LPMINMAXINFO pMMI = (LPMINMAXINFO)lParam;
      pMMI->ptMinTrackSize.x = minwidth;
      pMMI->ptMinTrackSize.y = minheight;
    return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Global functions

std::string AppGetWorkingDirectory()
{
	return szWorkingDir;
}
