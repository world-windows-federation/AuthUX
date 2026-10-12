#include "pch.h"
#include "logonnativehwndhost.h"

#include "duiutil.h"
#include "logonframe.h"

CLogonNativeHWNDHost::CLogonNativeHWNDHost()
	: m_scenario(LCPD::CredProvScenario_Logon)
{

}

HRESULT CLogonNativeHWNDHost::Create(int dx, int dy, int dwidth, int dheight, CLogonNativeHWNDHost** out)
{
	CLogonNativeHWNDHost* host = DirectUI::HNewAndZero<CLogonNativeHWNDHost>();
	if (!host)
		return E_OUTOFMEMORY;

	HRESULT er = S_OK;

	WNDCLASSEXW wndClass = {};

	wndClass.cbSize = sizeof(WNDCLASSEXW);
	wndClass.style = CS_GLOBALCLASS;
	wndClass.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wndClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wndClass.lpfnWndProc = DefWindowProcW;
	wndClass.lpszClassName = L"AUTHUI.DLL: LogonUI Logon Window";
	if (!RegisterClassExW(&wndClass))
	{
		er = HRESULTFromLastErrorError();
		if (FAILED(er))
		{
			host->Destroy();
			return er;
		}
	}

	er = host->Initialize(L"AUTHUI.DLL: LogonUI Logon Window", L"Windows Logon",nullptr,nullptr,dx,dy,dwidth,dheight, 0, 0x80000000 | WS_VISIBLE,nullptr,3);
	if (SUCCEEDED(er))
	{
		*out = host;
	}
	else
	{
		*out = nullptr;
		host->Destroy();
	}
	return er;
}

static bool gWindowPosChanging = false;

//@TODO: WM_SETFOCUS, WM_ENABLE, WM_POWERBROADCAST, WM_TABLET_ADDED, WM_APPCOMMAND
HRESULT CLogonNativeHWNDHost::OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plRet)
{
	bool isMainFrame = false;
	CLogonFrame* logonFrame = nullptr;
	if ( auto elm = GetElement() )
	{
		isMainFrame = IsElementOfClass(elm, L"MainFrame");
		logonFrame = (CLogonFrame*)elm;
	}

	switch (uMsg)
	{
	case WM_CLOSE:
		{
			return FALSE;
		}
	case WM_DISPLAYCHANGE:
		{
			if (!isMainFrame)
				break;

			gWindowPosChanging = true;
			int width  = LOWORD(lParam);
			int height = HIWORD(lParam);
			FrameResize(logonFrame,0,0,width,height);

			break;
		}

	case WM_SETTINGCHANGE:
	{
		if (wParam == SPI_SETWORKAREA && isMainFrame)
		{
			RECT rect;
			if (SystemParametersInfoW(/*uiAction=*/SPI_GETWORKAREA, 0, &rect, 0))
			{
				FrameResize(logonFrame,rect.left, rect.top,rect.right - rect.left,rect.bottom - rect.top);
			}
		}
		return TRUE;
	}



	case WM_WINDOWPOSCHANGING:
	{
		auto winPos = reinterpret_cast<PWINDOWPOS>(lParam);
		if (isMainFrame
			&& !gWindowPosChanging && winPos && ((winPos->flags & SWP_NOMOVE) != 0 || !winPos->x && !winPos->y) && ((winPos->flags & SWP_NOSIZE) != 0 ||
				winPos->cx == GetSystemMetrics(SM_CXSCREEN) && winPos->cy == GetSystemMetrics(SM_CYSCREEN)))
		{
			gWindowPosChanging = false;
			if (winPos)
			{
				winPos->x = 0;
				winPos->y = 0;
				winPos->cx = GetSystemMetrics(SM_CXSCREEN);
				winPos->cy = GetSystemMetrics(SM_CYSCREEN);
			}
			int width = GetSystemMetrics(SM_CXSCREEN);
			int height = GetSystemMetrics(SM_CYSCREEN);
			FrameResize(logonFrame, 0, 0, width, height);
			return TRUE;
		}
	}


	default:
		break;
	}
	return TRUE;
}

void CLogonNativeHWNDHost::FrameResize(class CLogonFrame* logonFrame, int X, int Y, int width, int height)
{
	logonFrame->RemoveBackgroundGraphics();
	if ( width <= 0 || height <= 0 )
	{
		HWND hwnd = GetHWND();
		int cy = GetSystemMetrics(SM_CYSCREEN);
		int cx = GetSystemMetrics(SM_CXSCREEN);
		SetWindowPos(
		  hwnd,
		  nullptr,
		  0,
		  0,
		  cx,
		  cy,
		  SWP_NOACTIVATE | SWP_NOZORDER);
	}
	else
	{
		SetWindowPos(
		  GetHWND(),
		  nullptr,
		  X,
		  Y,
		  width,
		  height,
		  SWP_NOACTIVATE | SWP_NOZORDER);
	}
	logonFrame->SetBackgroundGraphics();

	//@TODO: handle secondary monitor frame stuff when its eventually done
}
