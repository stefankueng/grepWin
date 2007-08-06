#include "stdafx.h"
#include "BaseWindow.h"


ResString::ResString (HINSTANCE hInst, int resId)
{
	if (!::LoadString (hInst, resId, _buf, MAX_RESSTRING + 1))
	{
		ZeroMemory(_buf, sizeof(_buf));
	}
}


bool CWindow::RegisterWindow(UINT style, HICON hIcon, HCURSOR hCursor, HBRUSH hbrBackground, 
									LPCTSTR lpszMenuName, LPCTSTR lpszClassName, HICON hIconSm)
{
	WNDCLASSEX wcx; 
 
	// Fill in the window class structure with default parameters 
 
	wcx.cbSize = sizeof(WNDCLASSEX);				// size of structure 
	wcx.style = style;								// redraw if size changes 
	wcx.lpfnWndProc = CWindow::stWinMsgHandler;		// points to window procedure 
	wcx.cbClsExtra = 0;								// no extra class memory 
	wcx.cbWndExtra = 0;								// no extra window memory 
	wcx.hInstance = hResource;						// handle to instance 
	wcx.hIcon = hIcon;								// predefined app. icon 
	wcx.hCursor = hCursor;							// predefined arrow 
	wcx.hbrBackground = hbrBackground;				// white background brush 
	wcx.lpszMenuName = lpszMenuName;				// name of menu resource 
	wcx.lpszClassName = lpszClassName;				// name of window class 
	wcx.hIconSm = hIconSm;							// small class icon 
 
	// Register the window class. 
	return RegisterWindow(&wcx); 
}

bool CWindow::RegisterWindow(CONST WNDCLASSEX* wcx)
{
	// Register the window class. 
	sClassName = std::wstring(wcx->lpszClassName);

	if (RegisterClassEx(wcx) == 0)
	{
		if (GetLastError() == ERROR_CLASS_ALREADY_EXISTS)
			return TRUE;
		return FALSE;
	}
	else
		return TRUE;
}

LRESULT CALLBACK CWindow::stWinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CWindow* pWnd;

	if (uMsg == WM_NCCREATE)
	{
		// get the pointer to the window from lpCreateParams which was set in CreateWindow
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (long)((LPCREATESTRUCT(lParam))->lpCreateParams));
	}

	// get the pointer to the window
	pWnd = GetObjectFromWindow(hwnd);

	// if we have the pointer, go to the message handler of the window
	// else, use DefWindowProc
	if (pWnd)
		return pWnd->WinMsgHandler(hwnd, uMsg, wParam, lParam);
	else
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool CWindow::Create()
{ 
	// Create the window
	RECT rect;

	rect.top = 0;
	rect.left = 0;
	rect.right = 600;
	rect.bottom = 400;

	return Create(WS_OVERLAPPEDWINDOW | WS_VISIBLE, NULL, &rect);
}

bool CWindow::Create(DWORD dwStyles, HWND hParent /* = NULL */, RECT* rect /* = NULL */)
{ 
	return CreateEx(0, dwStyles, hParent, rect);	
}

bool CWindow::CreateEx(DWORD dwExStyles, DWORD dwStyles, HWND hParent /* = NULL */, RECT* rect /* = NULL */)
{
	// send the this pointer as the window creation parameter
	if (rect == NULL)
		m_hwnd = CreateWindowEx(dwExStyles, sClassName.c_str(), sWindowTitle.c_str(), dwStyles, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hParent, NULL, hResource, (void *)this);
	else
	{
		m_hwnd = CreateWindowEx(dwExStyles, sClassName.c_str(), sWindowTitle.c_str(), dwStyles, rect->left, rect->top, 
			rect->right - rect->left, rect->bottom - rect->top, hParent, NULL, hResource, 
			(void *)this);
	}
	return (m_hwnd != NULL);
}

void CWindow::SetTransparency(BYTE alpha, COLORREF color /* = 0xFF000000 */)
{
	if (alpha == 255)
	{
		LONG exstyle = GetWindowLongPtr(*this, GWL_EXSTYLE);
		exstyle &= ~WS_EX_LAYERED;
		SetWindowLongPtr(*this, GWL_EXSTYLE, exstyle);
	}
	else
	{
		LONG exstyle = GetWindowLongPtr(*this, GWL_EXSTYLE);
		exstyle |= WS_EX_LAYERED;
		SetWindowLongPtr(*this, GWL_EXSTYLE, exstyle);
	}
	COLORREF col = color;
	DWORD flags = LWA_ALPHA;
	if (col & 0xFF000000)
	{
		col = RGB(255, 255, 255);
		flags = LWA_ALPHA;
	}
	else
	{
		flags = LWA_COLORKEY;
	}
	SetLayeredWindowAttributes(*this, col, alpha, flags);
}