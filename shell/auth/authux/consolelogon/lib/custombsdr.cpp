#include "pch.h"
#include "custombsdr.h"
#include "wicutil.h"
#include <windowsx.h>
#include <CommCtrl.h>
#include <string>
#include <winerror.h>
#include <wincodec.h>
#include <robuffer.h>
#include <Uxtheme.h>
#include <wrl/wrappers/corewrappers.h>
#include <vector>

#pragma	comment(lib, "msimg32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "UxTheme.lib")

#define WM_ADD_APP (WM_USER + 1)
#define WM_REMOVE_APP (WM_USER + 2)

using namespace Microsoft::WRL;

using namespace Windows::Internal::UI::Logon::Controller;

void CustomBSDR::CenterWindow(HWND hWnd)
{
	RECT rcWindow, rcWorkArea;
	GetWindowRect(hWnd, &rcWindow);
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWorkArea, 0);
	int windowWidth = rcWindow.right - rcWindow.left;
	int windowHeight = rcWindow.bottom - rcWindow.top;
	int xPos = rcWorkArea.left + (rcWorkArea.right - rcWorkArea.left - windowWidth) / 2 - GetSystemMetrics(SM_XVIRTUALSCREEN);
	int yPos = rcWorkArea.top + (rcWorkArea.bottom - rcWorkArea.top - windowHeight) / 2 - GetSystemMetrics(SM_YVIRTUALSCREEN);
	SetWindowPos(hWnd, 0, xPos, yPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

bool CustomBSDR::IsHighContrast()
{
	HIGHCONTRASTW highContrast = {};
	highContrast.cbSize = sizeof(HIGHCONTRASTW);
	if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, 0))
	{
		return (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
	}
	return false;
}

bool CustomBSDR::ShouldUseClassicScrollbar()
{
	HKEY result;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Authentication\\LogonUI\\CustomBSDR", 0, KEY_READ, &result) == S_OK)
	{
		DWORD size = sizeof(DWORD);
		DWORD type = REG_DWORD;

		DWORD data;
		if (RegQueryValueExW(result, L"ThemeScrollbar", nullptr, &type, (LPBYTE)&data, &size) == S_OK)
		{
			return data != 1;
		}
	}
	return true;
}

// Load winsrv alpha bitmaps properly
HBITMAP CustomBSDR::LoadAlphaBitmap(UINT resourceId)
{
	HRSRC hResource = FindResourceW(HINST_THISCOMPONENT, MAKEINTRESOURCEW(resourceId), RT_BITMAP);
	if (!hResource)
		return nullptr;

	HGLOBAL hGlobal = LoadResource(HINST_THISCOMPONENT, hResource);
	if (!hGlobal)
		return nullptr;

	void* pResourceData = LockResource(hGlobal);
	if (!pResourceData)
		return nullptr;

	BITMAPINFO* pBitmapInfo = (BITMAPINFO*)pResourceData;
	BITMAPINFOHEADER* pHeader = &pBitmapInfo->bmiHeader;

	if (pHeader->biBitCount != 32)
	{
		return LoadBitmapW(HINST_THISCOMPONENT, MAKEINTRESOURCEW(resourceId));
	}

	int width = pHeader->biWidth;
	int height = abs(pHeader->biHeight);
	bool isTopDown = pHeader->biHeight < 0;

	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	HDC hdc = GetDC(nullptr);
	void* pBits = nullptr;
	HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
	ReleaseDC(nullptr, hdc);

	if (hBitmap && pBits)
	{
		DWORD dwBitsOffset = pHeader->biSize;
		if (pHeader->biCompression == BI_BITFIELDS)
		{
			dwBitsOffset += 12;
		}

		unsigned char* pSrc = (unsigned char*)pResourceData + dwBitsOffset;
		DWORD dwSize = width * height * 4;

		if (isTopDown)
		{
			memcpy(pBits, pSrc, dwSize);
		}
		else
		{
			for (int y = 0; y < height; y++)
			{
				memcpy((unsigned char*)pBits + y * width * 4,
					pSrc + (height - 1 - y) * width * 4,
					width * 4);
			}
		}

		unsigned char* pPixels = (unsigned char*)pBits;
		for (int i = 0; i < width * height; i++)
		{
			unsigned char b = pPixels[i * 4 + 0];
			unsigned char g = pPixels[i * 4 + 1];
			unsigned char r = pPixels[i * 4 + 2];
			unsigned char a = pPixels[i * 4 + 3];

			pPixels[i * 4 + 0] = (b * a) / 255;
			pPixels[i * 4 + 1] = (g * a) / 255;
			pPixels[i * 4 + 2] = (r * a) / 255;
		}
	}

	return hBitmap;
}

void CustomBSDR::DrawSeparator(HDC hdc, LPDRAWITEMSTRUCT pDIS)
{
	if (separatorBitmap && !IsHighContrast())
	{
		BITMAP bm;
		GetObject(separatorBitmap, sizeof(bm), &bm);

		int width = pDIS->rcItem.right - pDIS->rcItem.left;
		int height = pDIS->rcItem.bottom - pDIS->rcItem.top;

		BITMAPINFO bmi = {};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = width;
		bmi.bmiHeader.biHeight = -height;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;

		void* pBits = nullptr;
		HBITMAP hOffscreenBmp = CreateDIBSection(pDIS->hDC, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
		if (hOffscreenBmp)
		{
			HDC hdcOffscreen = CreateCompatibleDC(pDIS->hDC);
			HBITMAP hOldOffscreen = (HBITMAP)SelectObject(hdcOffscreen, hOffscreenBmp);

			// Proper alpha blending with the background bitmap
			if (bgBitmap)
			{
				POINT pt = { pDIS->rcItem.left, pDIS->rcItem.top };
				MapWindowPoints(pDIS->hwndItem, hBgWnd, &pt, 1);
				pt.x -= bgOffsetX;
				pt.y -= bgOffsetY;

				HDC hdcBg = CreateCompatibleDC(pDIS->hDC);
				HBITMAP hOldBg = (HBITMAP)SelectObject(hdcBg, bgBitmap);
				BitBlt(hdcOffscreen, 0, 0, width, height, hdcBg, pt.x, pt.y, SRCCOPY);
				SelectObject(hdcBg, hOldBg);
				DeleteDC(hdcBg);
			}

			HDC hdcSep = CreateCompatibleDC(pDIS->hDC);
			HBITMAP hOldSep = (HBITMAP)SelectObject(hdcSep, separatorBitmap);

			BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

			// Skipped leftmost transparent pixels * 4 + opacity gradient pixels on left * 19 + fully opaque center pixels * 8 + opacity gradient pixels on right * 19 =
			// 4 + 19 + 8 + 19 = 50 (total width of the source bitmap)
			// Original Win7 shutdown resolver somehow decided to shrink the 19 pixels part to just 6 pixels
			// Also somehow the leftmost pixel is skipped and drawing begins from the second one
			const int srcLeftSkip = 4;
			const int srcLeftGradient = 19;
			const int srcCenter = 8;
			const int srcRightGradient = 19;

			const int dstLeftSkip = 1;
			const int dstLeftGradient = 6;
			const int dstRightGradient = 6;
			const int dstCenter = width - dstLeftSkip - dstLeftGradient - dstRightGradient;

			// Left gradient
			AlphaBlend(hdcOffscreen, dstLeftSkip, 0, dstLeftGradient, 1,
				hdcSep, srcLeftSkip, 1, srcLeftGradient, 1, bf);

			// Center
			AlphaBlend(hdcOffscreen, dstLeftSkip + dstLeftGradient, 0, dstCenter, 1,
				hdcSep, srcLeftSkip + srcLeftGradient, 1, srcCenter, 1, bf);

			// Right gradient
			AlphaBlend(hdcOffscreen, dstLeftSkip + dstLeftGradient + dstCenter, 0, dstRightGradient, 1,
				hdcSep, srcLeftSkip + srcLeftGradient + srcCenter, 1, srcRightGradient, 1, bf);

			SelectObject(hdcSep, hOldSep);
			DeleteDC(hdcSep);

			BitBlt(pDIS->hDC, pDIS->rcItem.left, pDIS->rcItem.top, width, height, hdcOffscreen, 0, 0, SRCCOPY);

			SelectObject(hdcOffscreen, hOldOffscreen);
			DeleteDC(hdcOffscreen);
			DeleteObject(hOffscreenBmp);
		}
	}
	else
	{
		HBRUSH hBrush = CreateSolidBrush(GetSysColor(COLOR_WINDOWTEXT));
		RECT lineRect = { pDIS->rcItem.left, pDIS->rcItem.top, pDIS->rcItem.right, pDIS->rcItem.top + 1 };
		FillRect(pDIS->hDC, &lineRect, hBrush);
		DeleteObject(hBrush);
	}
}

void CustomBSDR::DrawButton(HDC hdc, LPDRAWITEMSTRUCT pDIS)
{
	bool isPressed = (pDIS->itemState & ODS_SELECTED);
	bool isFocused = (pDIS->itemState & ODS_FOCUS);
	bool isHighContrast = IsHighContrast();

	RECT rcButton = pDIS->rcItem;

	if (isHighContrast)
	{
		UINT uState = DFCS_BUTTONPUSH;
		if (isPressed)
			uState |= DFCS_PUSHED;
		DrawFrameControl(hdc, &rcButton, DFC_BUTTON, uState);
	}
	else
	{
		HBITMAP hBitmap = nullptr;
		bool isHover = (hHoverButton == pDIS->hwndItem);

		if (isPressed)
		{
			hBitmap = btnPressedBitmap;
		}
		else if (isFocused && isHover)
		{
			hBitmap = btnSelectedHoverBitmap;
		}
		else if (isFocused)
		{
			hBitmap = btnSelectedBitmap;
		}
		else if (isHover)
		{
			hBitmap = btnHoverBitmap;
		}
		else
		{
			hBitmap = btnNormalBitmap;
		}

		if (hBitmap)
		{
			BITMAP bm;
			GetObject(hBitmap, sizeof(bm), &bm);

			int width = rcButton.right - rcButton.left;
			int height = rcButton.bottom - rcButton.top;

			const int borderSize = 4;
			const int borderSizeDraw = MulDiv(borderSize, GetDpiForWindow(hDlg), 96);

			int srcW = bm.bmWidth;
			int srcH = bm.bmHeight;
			int dstW = width;
			int dstH = height;

			int left = rcButton.left;
			int top = rcButton.top;

			BITMAPINFO bmi = {};
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = dstW;
			bmi.bmiHeader.biHeight = -dstH;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			bmi.bmiHeader.biCompression = BI_RGB;

			void* pBits = nullptr;
			HBITMAP hOffscreenBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
			if (hOffscreenBmp)
			{
				HDC hdcOffscreen = CreateCompatibleDC(hdc);
				HBITMAP hOldOffscreen = (HBITMAP)SelectObject(hdcOffscreen, hOffscreenBmp);

				// Proper alpha blending with the background bitmap
				if (bgBitmap)
				{
					POINT pt = { rcButton.left, rcButton.top };
					MapWindowPoints(pDIS->hwndItem, hBgWnd, &pt, 1);
					pt.x -= bgOffsetX;
					pt.y -= bgOffsetY;

					HDC hdcBg = CreateCompatibleDC(hdc);
					HBITMAP hOldBg = (HBITMAP)SelectObject(hdcBg, bgBitmap);
					BitBlt(hdcOffscreen, 0, 0, dstW, dstH, hdcBg, pt.x, pt.y, SRCCOPY);
					SelectObject(hdcBg, hOldBg);
					DeleteDC(hdcBg);
				}

				HDC hdcSrc = CreateCompatibleDC(hdc);
				HBITMAP hOldSrc = (HBITMAP)SelectObject(hdcSrc, hBitmap);

				BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

				// 1 2 3
				// 4 5 6
				// 7 8 9
				// Corners
				AlphaBlend(hdcOffscreen, 0, 0, borderSizeDraw, borderSizeDraw, hdcSrc, 0, 0, borderSize, borderSize, bf); // 1
				AlphaBlend(hdcOffscreen, dstW - borderSizeDraw, 0, borderSizeDraw, borderSizeDraw, hdcSrc, srcW - borderSize, 0, borderSize, borderSize, bf); // 3
				AlphaBlend(hdcOffscreen, 0, dstH - borderSizeDraw, borderSizeDraw, borderSizeDraw, hdcSrc, 0, srcH - borderSize, borderSize, borderSize, bf); // 7
				AlphaBlend(hdcOffscreen, dstW - borderSizeDraw, dstH - borderSizeDraw, borderSizeDraw, borderSizeDraw, hdcSrc, srcW - borderSize, srcH - borderSize, borderSize, borderSize, bf); // 9

				// Edges
				AlphaBlend(hdcOffscreen, borderSizeDraw, 0, dstW - 2 * borderSizeDraw, borderSizeDraw, hdcSrc, borderSize, 0, srcW - 2 * borderSize, borderSize, bf); // 2
				AlphaBlend(hdcOffscreen, borderSizeDraw, dstH - borderSizeDraw, dstW - 2 * borderSizeDraw, borderSizeDraw, hdcSrc, borderSize, srcH - borderSize, srcW - 2 * borderSize, borderSize, bf); // 8
				AlphaBlend(hdcOffscreen, 0, borderSizeDraw, borderSizeDraw, dstH - 2 * borderSizeDraw, hdcSrc, 0, borderSize, borderSize, srcH - 2 * borderSize, bf); // 4
				AlphaBlend(hdcOffscreen, dstW - borderSizeDraw, borderSizeDraw, borderSizeDraw, dstH - 2 * borderSizeDraw, hdcSrc, srcW - borderSize, borderSize, borderSize, srcH - 2 * borderSize, bf); // 6

				// Center
				AlphaBlend(hdcOffscreen, borderSizeDraw, borderSizeDraw, dstW - 2 * borderSizeDraw, dstH - 2 * borderSizeDraw, hdcSrc, borderSize, borderSize, srcW - 2 * borderSize, srcH - 2 * borderSize, bf); // 5

				SelectObject(hdcSrc, hOldSrc);
				DeleteDC(hdcSrc);

				BitBlt(hdc, left, top, dstW, dstH, hdcOffscreen, 0, 0, SRCCOPY);

				SelectObject(hdcOffscreen, hOldOffscreen);
				DeleteDC(hdcOffscreen);
				DeleteObject(hOffscreenBmp);
			}
		}
	}

	wchar_t buttonText[256] = {};
	GetWindowTextW(pDIS->hwndItem, buttonText, _countof(buttonText));

	if (wcslen(buttonText) > 0)
	{
		if (!isHighContrast)
		{
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, RGB(255, 255, 255));
		}
		else
		{
			SetBkMode(hdc, OPAQUE);
			SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
		}
		UINT format = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
		if (GetPropW(pDIS->hwndItem, L"CustomBSDR_HideAccel"))
		{
			format |= DT_HIDEPREFIX;
		}
		DrawTextW(hdc, buttonText, -1, &rcButton, format);
	}

	if (isHighContrast && isFocused && !GetPropW(pDIS->hwndItem, L"CustomBSDR_HideFocus"))
	{
		int cxEdge = 2 * GetSystemMetrics(SM_CXEDGE);
		int cxBorder = GetSystemMetrics(SM_CXBORDER) + cxEdge;
		int cyEdge = 2 * GetSystemMetrics(SM_CYEDGE);
		int cyBorder = GetSystemMetrics(SM_CYBORDER) + cyEdge;

		InflateRect(&rcButton, -cxBorder, -cyBorder);
		DrawFocusRect(hdc, &rcButton);
	}
}

LRESULT CALLBACK CustomBSDR::ButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (uMsg)
	{
	case WM_MOUSEMOVE:
	{
		if (hHoverButton != hWnd)
		{
			if (hHoverButton)
			{
				RedrawWindow(hHoverButton, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
			}
			hHoverButton = hWnd;
			RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		}

		TRACKMOUSEEVENT tme = { sizeof(tme) };
		tme.dwFlags = TME_LEAVE;
		tme.hwndTrack = hWnd;
		TrackMouseEvent(&tme);
		break;
	}
	case WM_MOUSELEAVE:
	{
		if (hHoverButton == hWnd)
		{
			hHoverButton = nullptr;
			RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		}
		break;
	}
	case WM_UPDATEUISTATE:
	{
		switch (HIWORD(wParam))
		{
		case UISF_HIDEFOCUS:
			SetPropW(hWnd, L"CustomBSDR_HideFocus", (HANDLE)(LOWORD(wParam) == UIS_SET));
			break;
		case UISF_HIDEACCEL:
			SetPropW(hWnd, L"CustomBSDR_HideAccel", (HANDLE)(LOWORD(wParam) == UIS_SET));
			break;
		}
		break;
	}
	case WM_NCDESTROY:
	{
		RemoveWindowSubclass(hWnd, ButtonSubclassProc, uIdSubclass);
		break;
	}
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK CustomBSDR::AppListSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (uMsg)
	{
	case WM_CTLCOLORSTATIC:
	{
		if (!IsHighContrast())
		{
			SetBkMode((HDC)wParam, TRANSPARENT);
			SetStretchBltMode((HDC)wParam, HALFTONE);

			HWND hControl = (HWND)lParam;
			bool isBlockReason = false;
			for (auto& tile : appTiles)
			{
				if (tile.hBlockReason == hControl)
				{
					isBlockReason = true;
					break;
				}
			}

			if (isBlockReason)
			{
				SetTextColor((HDC)wParam, RGB(255, 204, 153));
			}
			else
			{
				SetTextColor((HDC)wParam, RGB(255, 255, 255));
			}

			return (INT_PTR)GetStockObject(NULL_BRUSH);
		}
		break;
	}
	case WM_NCDESTROY:
	{
		RemoveWindowSubclass(hWnd, AppListSubclassProc, uIdSubclass);
		break;
	}
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void CustomBSDR::CreateAppTileControls(IShutdownBlockingApp* blockingApp)
{
	if (!hDlg || !blockingApp)
	{
		return;
	}

	int dpi = GetDpiForWindow(hDlg);

	AppTile tile = {};
	blockingApp->get_Id(&tile.appId);
	blockingApp->get_IsBlocking(&tile.isBlocking);
	tile.app = blockingApp;
	tile.hIconBitmap = nullptr;

	HSTRING caption, blockReason;
	std::wstring titleText;
	std::wstring blockReasonText;
	bool hasCustomReason = false;

	if (SUCCEEDED(blockingApp->get_Caption(&caption)))
	{
		const wchar_t* captionStr = WindowsGetStringRawBuffer(caption, nullptr);
		if (tile.isBlocking)
		{
			wchar_t waitingFor[256] = {};
			LoadStringW(HINST_THISCOMPONENT, IDS_BSDR_WAITINGFOR, waitingFor, _countof(waitingFor));
			titleText = std::wstring(waitingFor) + L" " + captionStr;
		}
		else
		{
			titleText = captionStr;
		}
		WindowsDeleteString(caption);
	}

	if (SUCCEEDED(blockingApp->get_BlockReason(&blockReason)))
	{
		const wchar_t* reasonStr = WindowsGetStringRawBuffer(blockReason, nullptr);
		if (reasonStr && wcslen(reasonStr) > 0)
		{
			blockReasonText = reasonStr;
			hasCustomReason = true;
		}
		WindowsDeleteString(blockReason);
	}

	if (blockReasonText.empty() && tile.isBlocking)
	{
		UINT stringId = IDS_BSDR_BLOCKINGAPP_SHUTDOWN;
		if (_logonUIState == LogonUIState_LoggingOff)
			stringId = IDS_BSDR_BLOCKINGAPP_LOGOFF;
		else if (_logonUIState == LogonUIState_Restarting)
			stringId = IDS_BSDR_BLOCKINGAPP_RESTART;

		wchar_t defaultReason[256] = {};
		LoadStringW(HINST_THISCOMPONENT, stringId, defaultReason, _countof(defaultReason));
		blockReasonText = defaultReason;
	}

	int iconSize = MulDiv(32, dpi, 96);

	tile.hIcon = CreateWindowExW(0, L"Static", nullptr, WS_CHILD | WS_VISIBLE | SS_BITMAP, 0, 0, iconSize, iconSize, hAppListScroll, nullptr, HINST_THISCOMPONENT, nullptr);
	tile.hTitle = CreateWindowExW(0, L"Static", titleText.c_str(), WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS, 0, 0, 100, 20, hAppListScroll, nullptr, HINST_THISCOMPONENT, nullptr);

	if (!blockReasonText.empty())
	{
		tile.hBlockReason = CreateWindowExW(0, L"Static", blockReasonText.c_str(), WS_CHILD | WS_VISIBLE | SS_EDITCONTROL, 0, 0, 100, 40, hAppListScroll, nullptr, HINST_THISCOMPONENT, nullptr);
	}
	else
	{
		tile.hBlockReason = nullptr;
	}

	if (tile.hTitle) {
		SendMessageW(tile.hTitle, WM_SETFONT, (WPARAM)hDescFont, FALSE);
		HDC hdc = GetDC(tile.hTitle);
		HFONT hOldFont = (HFONT)SelectObject(hdc, hDescFont);
		SIZE textSize;
		GetTextExtentPoint32W(hdc, titleText.c_str(), (int)titleText.length(), &textSize);
		SelectObject(hdc, hOldFont);
		ReleaseDC(tile.hTitle, hdc);
		SetWindowPos(tile.hTitle, nullptr, 0, 0, textSize.cx, textSize.cy, SWP_NOMOVE | SWP_NOZORDER); // Width is corrected later
	}
	HFONT hFont = (HFONT)SendMessageW(hDlg, WM_GETFONT, 0, 0);
	if (tile.hBlockReason) {
		SendMessageW(tile.hBlockReason, WM_SETFONT, (WPARAM)hFont, FALSE);
		HDC hdc = GetDC(tile.hBlockReason);
		HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
		SIZE textSize;
		GetTextExtentPoint32W(hdc, blockReasonText.c_str(), (int)blockReasonText.length(), &textSize);
		SelectObject(hdc, hOldFont);
		ReleaseDC(tile.hBlockReason, hdc);
		SetWindowPos(tile.hBlockReason, nullptr, 0, 0, textSize.cx, textSize.cy * 2, SWP_NOMOVE | SWP_NOZORDER); // Always two lines
	}

	ABI::Windows::Storage::Streams::IRandomAccessStream* iconStream = nullptr;
	if (SUCCEEDED(blockingApp->get_Icon(&iconStream)) && iconStream)
	{
		GetBitmapFromRandomStream(iconStream, &tile.hIconBitmap);
		if (tile.hIcon && tile.hIconBitmap)
		{
			SendMessageW(tile.hIcon, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)tile.hIconBitmap);
		}
		iconStream->Release();
	}

	if (tile.isBlocking)
	{
		appTiles.insert(appTiles.begin(), tile);
	}
	else
	{
		appTiles.push_back(tile);
	}

	UpdateAppListLayout();
}

void CustomBSDR::RemoveAppTileControls(UINT appId)
{
	for (auto it = appTiles.begin(); it != appTiles.end(); ++it)
	{
		if (it->appId == appId)
		{
			if (it->hIcon)
			{
				DestroyWindow(it->hIcon);
			}
			if (it->hTitle)
			{
				DestroyWindow(it->hTitle);
			}
			if (it->hBlockReason)
			{
				DestroyWindow(it->hBlockReason);
			}
			if (it->hIconBitmap)
			{
				DeleteObject(it->hIconBitmap);
			}

			appTiles.erase(it);

			UpdateAppListLayout();
			return;
		}
	}
}

void CustomBSDR::UpdateAppListLayout()
{
	if (!hDlg)
	{
		return;
	}

	int dpi = GetDpiForWindow(hDlg);

	const int iconSize = MulDiv(32, dpi, 96);
	const int topMargin = MulDiv(11, dpi, 96);
	const int iconTopMargin = MulDiv(6, dpi, 96);
	const int reasonTopMargin = MulDiv(25, dpi, 96);
	const int itemHeight = MulDiv(83, dpi, 96);
	const int itemHeightNoReason = MulDiv(62, dpi, 96);
	const int iconTextGap = MulDiv(8, dpi, 96);
	int maxWidth = MulDiv(700, dpi, 96);
	int visibleHeight = MulDiv(300, dpi, 96);
	if (hAppList)
	{
		RECT rcContainer;
		GetClientRect(hAppList, &rcContainer);
		visibleHeight = rcContainer.bottom - rcContainer.top;
		maxWidth = rcContainer.right - rcContainer.left - iconSize - iconTextGap;
	}

	totalContentHeight = 0;
	for (auto& tile : appTiles)
	{
		totalContentHeight += (tile.hBlockReason != nullptr) ? itemHeight : itemHeightNoReason;
	}

	int maxScroll = totalContentHeight - visibleHeight;
	if (maxScroll < 0) maxScroll = 0;

	if (scrollPos > maxScroll)
	{
		scrollPos = maxScroll;
	}

	if (maxScroll > 0)
	{
		ShowWindow(hScrollBar, SW_SHOW);

		SCROLLINFO si = {};
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		si.nMin = 0;
		si.nMax = totalContentHeight;
		si.nPage = visibleHeight;
		si.nPos = scrollPos;
		SetScrollInfo(hScrollBar, SB_CTL, &si, TRUE);
	}
	else
	{
		ShowWindow(hScrollBar, SW_HIDE);
		scrollPos = 0;
	}

	int yPos = topMargin;

	int index = 0;
	for (auto& tile : appTiles)
	{
		bool hasBlockReason = (tile.hBlockReason != nullptr);
		int height = hasBlockReason ? itemHeight : itemHeightNoReason;

		if (tile.hIcon)
		{
			SetWindowPos(tile.hIcon, nullptr, 0, yPos + iconTopMargin, iconSize, iconSize, SWP_NOZORDER | SWP_SHOWWINDOW);
		}

		if (tile.hTitle)
		{
			RECT rcTitle;
			GetWindowRect(tile.hTitle, &rcTitle);
			int origHeight = rcTitle.bottom - rcTitle.top;

			int titleYOffset = 0;
			if (!hasBlockReason)
			{
				// Vertically center the title when there's no block reason
				titleYOffset = MulDiv(8, dpi, 96);
			}

			SetWindowPos(tile.hTitle, nullptr, iconSize + iconTextGap, yPos + titleYOffset, maxWidth, origHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
		}

		if (tile.hBlockReason)
		{
			RECT rcBlockReason;
			GetWindowRect(tile.hBlockReason, &rcBlockReason);
			int origHeight = rcBlockReason.bottom - rcBlockReason.top;
			SetWindowPos(tile.hBlockReason, nullptr, iconSize + iconTextGap, yPos + reasonTopMargin, maxWidth, origHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
		}

		yPos += height;
		index++;
	}

	SetWindowPos(hAppListScroll, nullptr, 0, -scrollPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

	// Set the title text based on the number of apps on the list
	if (hTitleText)
	{
		wchar_t titleFormat[256] = {};
		wchar_t titleText[256] = {};
		// Redraw the entire title control area to prevent artifacts from previous longer text when the number of apps decreases
		InvalidateRect(hTitleText, nullptr, TRUE);
		if (appTiles.size() == 1)
		{
			LoadStringW(HINST_THISCOMPONENT, IDS_BSDR_BLOCKINGAPPCOUNT_SINGLE, titleFormat, _countof(titleFormat));
		}
		else if (appTiles.size() == 0)
		{
			LoadStringW(HINST_THISCOMPONENT, IDS_BSDR_BLOCKING_BGAPPS, titleFormat, _countof(titleFormat));
		}
		else
		{
			LoadStringW(HINST_THISCOMPONENT, IDS_BSDR_BLOCKINGAPPCOUNT_MULTI, titleFormat, _countof(titleFormat));
		}
		swprintf_s(titleText, _countof(titleText), titleFormat, (int)appTiles.size());
		SetWindowTextW(hTitleText, titleText);
	}
}

INT_PTR CALLBACK CustomBSDR::DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		CustomBSDR::hDlg = hDlg;

		// Hide title bar
		SetWindowLongW(hDlg, GWL_STYLE, GetWindowLongW(hDlg, GWL_STYLE) & ~WS_CAPTION);
		// Use composited style on dialog, not bg window, to prevent the scrollbar control from flickering on drag
		SetWindowLongW(hDlg, GWL_EXSTYLE, GetWindowLongW(hDlg, GWL_EXSTYLE) | WS_EX_COMPOSITED);
		
		hTitleText = GetDlgItem(hDlg, IDC_BSDR_TITLE);
		hAppList = GetDlgItem(hDlg, IDC_BSDR_APPLIST);
		hScrollBar = GetDlgItem(hDlg, IDC_BSDR_SCROLLBAR);
		hWarningText = GetDlgItem(hDlg, IDC_BSDR_WARNING);
		hForceButton = GetDlgItem(hDlg, IDC_BSDR_FORCE_BTN);
		hCancelButton = GetDlgItem(hDlg, IDCANCEL);
		hDescText = GetDlgItem(hDlg, IDC_BSDR_DESC);
		hYesButton = GetDlgItem(hDlg, IDYES);
		hNoButton = GetDlgItem(hDlg, IDNO);

		// Hide the warning message controls
		ShowWindow(hWarningText, SW_HIDE);
		ShowWindow(hYesButton, SW_HIDE);
		ShowWindow(hNoButton, SW_HIDE);

		if (ShouldUseClassicScrollbar())
		{
			SetWindowTheme(hScrollBar, L" ", L"");
		}

		if (hAppList)
		{
			hAppListScroll = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 100, 100, hAppList, nullptr, HINST_THISCOMPONENT, nullptr);
			SetWindowSubclass(hAppList, AppListSubclassProc, 0, 0);
			SetWindowSubclass(hAppListScroll, AppListSubclassProc, 0, 0);

			// Calculate the height of the app list container then resize it, also moving the controls below it
			int dpi = GetDpiForWindow(hDlg);

			int itemHeight = MulDiv(83, dpi, 96);
			int itemHeightNoReason = MulDiv(62, dpi, 96);

			int totalItemsHeight = 0;
			for (auto& app : pendingApps)
			{
				BOOLEAN isBlocking = FALSE;
				if (SUCCEEDED(app->get_IsBlocking(&isBlocking)) && isBlocking)
				{
					totalItemsHeight += itemHeight;
				}
				else
				{
					totalItemsHeight += itemHeightNoReason;
				}
			}

			RECT rcAppList, rcScrollBar;
			GetWindowRect(hAppList, &rcAppList);
			GetWindowRect(hScrollBar, &rcScrollBar);
			int currentWidth = rcAppList.right - rcAppList.left;
			int scrollBarWidth = rcScrollBar.right - rcScrollBar.left;

			int screenHeight = GetSystemMetrics(SM_CYSCREEN);
			int minHeight = rcAppList.bottom - rcAppList.top;
			int maxHeight = screenHeight - MulDiv(335, dpi, 96);

			if (maxHeight < minHeight)
				maxHeight = minHeight;

			int newHeight = totalItemsHeight;
			if (newHeight < minHeight)
				newHeight = minHeight;
			if (newHeight > maxHeight)
				newHeight = maxHeight;

			int heightDiff = newHeight - minHeight;

			SetWindowPos(hAppList, nullptr, 0, 0, currentWidth, newHeight, SWP_NOMOVE | SWP_NOZORDER);
			SetWindowPos(hAppListScroll, nullptr, 0, 0, currentWidth, totalItemsHeight, SWP_NOMOVE | SWP_NOZORDER);
			SetWindowPos(hScrollBar, nullptr, 0, 0, scrollBarWidth, newHeight, SWP_NOMOVE | SWP_NOZORDER);

			for (HWND hwndSibling = GetWindow(hScrollBar, GW_HWNDNEXT); hwndSibling; hwndSibling = GetWindow(hwndSibling, GW_HWNDNEXT))
			{
				RECT rcSibling;
				GetWindowRect(hwndSibling, &rcSibling);
				MapWindowPoints(HWND_DESKTOP, hDlg, (LPPOINT)&rcSibling, 2);
				OffsetRect(&rcSibling, 0, heightDiff);
				SetWindowPos(hwndSibling, nullptr, rcSibling.left, rcSibling.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			}

			RECT rcDialog, rcTitle, rcNo;
			GetWindowRect(hDlg, &rcDialog);
			OffsetRect(&rcDialog, -GetSystemMetrics(SM_XVIRTUALSCREEN), -GetSystemMetrics(SM_YVIRTUALSCREEN));
			GetWindowRect(hTitleText, &rcTitle);
			MapWindowPoints(HWND_DESKTOP, hDlg, (LPPOINT)&rcTitle, 2);
			GetWindowRect(hNoButton, &rcNo);
			MapWindowPoints(HWND_DESKTOP, hDlg, (LPPOINT)&rcNo, 2);
			int topPadding = rcTitle.top - rcDialog.top;
			int dialogWidth = rcTitle.right - rcDialog.left;
			int dialogHeight = rcDialog.bottom - rcDialog.top;
			int newDialogHeight = rcNo.bottom - rcDialog.top + topPadding - 1;
			SetWindowPos(hDlg, nullptr, 0, 0, dialogWidth, newDialogHeight, SWP_NOMOVE | SWP_NOZORDER);
		}

		SetWindowSubclass(hForceButton, ButtonSubclassProc, 0, 0);
		LRESULT forceButtonUIState = SendMessageW(hForceButton, WM_QUERYUISTATE, 0, 0);
		SetPropW(hForceButton, L"CustomBSDR_HideFocus", (HANDLE)(forceButtonUIState & UISF_HIDEFOCUS));
		SetPropW(hForceButton, L"CustomBSDR_HideAccel", (HANDLE)(forceButtonUIState & UISF_HIDEACCEL));

		SetWindowSubclass(hCancelButton, ButtonSubclassProc, 0, 0);
		LRESULT cancelButtonUIState = SendMessageW(hCancelButton, WM_QUERYUISTATE, 0, 0);
		SetPropW(hCancelButton, L"CustomBSDR_HideFocus", (HANDLE)(cancelButtonUIState & UISF_HIDEFOCUS));
		SetPropW(hCancelButton, L"CustomBSDR_HideAccel", (HANDLE)(cancelButtonUIState & UISF_HIDEACCEL));

		SetWindowSubclass(hYesButton, ButtonSubclassProc, 0, 0);
		LRESULT yesButtonUIState = SendMessageW(hYesButton, WM_QUERYUISTATE, 0, 0);
		SetPropW(hYesButton, L"CustomBSDR_HideFocus", (HANDLE)(yesButtonUIState & UISF_HIDEFOCUS));
		SetPropW(hYesButton, L"CustomBSDR_HideAccel", (HANDLE)(yesButtonUIState & UISF_HIDEACCEL));

		SetWindowSubclass(hNoButton, ButtonSubclassProc, 0, 0);
		LRESULT noButtonUIState = SendMessageW(hNoButton, WM_QUERYUISTATE, 0, 0);
		SetPropW(hNoButton, L"CustomBSDR_HideFocus", (HANDLE)(noButtonUIState & UISF_HIDEFOCUS));
		SetPropW(hNoButton, L"CustomBSDR_HideAccel", (HANDLE)(noButtonUIState & UISF_HIDEACCEL));

		// Set font for title and description/warning texts
		HFONT hDialogFont = (HFONT)SendMessageW(hDlg, WM_GETFONT, 0, 0);
		LOGFONTW lf = {};
		if (hDialogFont && GetObjectW(hDialogFont, sizeof(lf), &lf))
		{
			int originalHeight = lf.lfHeight;

			lf.lfHeight = MulDiv(originalHeight, 160, 100);
			hTitleFont = CreateFontIndirectW(&lf);
			if (hTitleFont && hTitleText)
			{
				SendMessageW(hTitleText, WM_SETFONT, (WPARAM)hTitleFont, FALSE);
			}

			lf.lfHeight = MulDiv(originalHeight, 120, 100);
			hDescFont = CreateFontIndirectW(&lf);
			if (hDescFont)
			{
				if (hWarningText)
					SendMessageW(hWarningText, WM_SETFONT, (WPARAM)hDescFont, FALSE);
				if (hDescText)
					SendMessageW(hDescText, WM_SETFONT, (WPARAM)hDescFont, FALSE);
			}
		}

		for (auto& app : pendingApps)
		{
			CreateAppTileControls(app.Get());
		}
		pendingApps.clear();

		// Load and set the appropriate strings based on the current LogonUI state
		wchar_t desc[256] = {}, warning[256] = {}, btnText[256] = {};
		switch (_logonUIState)
		{
		case LogonUIState_LoggingOff:
			LoadStringW(HINST_THISCOMPONENT, IDS_BSDR_DESC_LOGOFF, desc, _countof(desc));
			LoadStringW(HINST_THISCOMPONENT, IDS_BSDR_WARNING_LOGOFF, warning, _countof(warning));
			LoadStringW(HINST_THISCOMPONENT, IDS_BSDR_BTN_LOGOFF, btnText, _countof(btnText));
			break;
		case LogonUIState_Restarting:
			LoadStringW(HINST_THISCOMPONENT, IDS_BSDR_DESC_RESTART, desc, _countof(desc));
			LoadStringW(HINST_THISCOMPONENT, IDS_BSDR_WARNING_RESTART, warning, _countof(warning));
			LoadStringW(HINST_THISCOMPONENT, IDS_BSDR_BTN_RESTART, btnText, _countof(btnText));
			break;
		}
		if (_logonUIState != LogonUIState_ShuttingDown)
		{
			hDescText&& SetWindowTextW(hDescText, desc);
			hWarningText&& SetWindowTextW(hWarningText, warning);
			hForceButton&& SetWindowTextW(hForceButton, btnText);
		}

		CenterWindow(hDlg);
		return TRUE;
	}
	case WM_CTLCOLORDLG: // make dialog transparent
	{
		if (!IsHighContrast())
		{
			return (INT_PTR)GetStockObject(NULL_BRUSH);
		}
		break;
	}
	case WM_CTLCOLORSTATIC: // make (direct) children bg transparent and text white
	{
		if (!IsHighContrast())
		{
			SetBkMode((HDC)wParam, TRANSPARENT);
			SetStretchBltMode((HDC)wParam, HALFTONE);
			SetTextColor((HDC)wParam, RGB(255, 255, 255));
			return (INT_PTR)GetStockObject(NULL_BRUSH);
		}
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hDlg, &ps);
		if (bgBitmap)
		{
			RECT rcDlg;
			GetClientRect(hDlg, &rcDlg);
			MapWindowPoints(hDlg, hBgWnd, (LPPOINT)&rcDlg, 2);
			HDC memDC = CreateCompatibleDC(hdc);
			HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, bgBitmap);
			BitBlt(hdc, 0, 0, rcDlg.right, rcDlg.bottom, memDC, rcDlg.left, rcDlg.top, SRCCOPY);
			SelectObject(memDC, oldBitmap);
			DeleteDC(memDC);
		}
		EndPaint(hDlg, &ps);
		return 0;
	}
	case WM_ERASEBKGND:
	{
		if (!IsHighContrast())
		{
			HDC hdc = (HDC)wParam;
			RECT rcDlg;
			GetClientRect(hDlg, &rcDlg);
			HBRUSH hBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
			FillRect(hdc, &rcDlg, hBrush);
			DeleteObject(hBrush);
			return TRUE;
		}
		break;
	}
	case WM_DRAWITEM:
	{
		LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
		if (pDIS->CtlID == IDC_BSDR_SEPARATOR_TOP || pDIS->CtlID == IDC_BSDR_SEPARATOR_BOTTOM)
		{
			DrawSeparator(pDIS->hDC, pDIS);
			return TRUE;
		}
		else if (pDIS->CtlType == ODT_BUTTON)
		{
			DrawButton(pDIS->hDC, pDIS);
			return TRUE;
		}
		break;
	}
	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case IDCANCEL:
		{
			_resolve(BlockedShutdownResolution_Cancel);
			EndDialog(hDlg, 0);
			DestroyWindow(hBgWnd);
			if (!isOnSecureDesktop)
			{
				// If BSDR is forced to show on the default desktop with the Windhawk mod, LogonUI.exe won't exit for some reason on cancel,
				// causing issues with subsequent session ends, unless it's killed manually or Ctrl+Alt+Del is pressed once
				// so force exit the process as a dirty workaround
				ExitProcess(0);
			}
			return TRUE;
		}
		case IDC_BSDR_FORCE_BTN:
		{
			ShowWindow(hWarningText, SW_SHOW);
			ShowWindow(hYesButton, SW_SHOW);
			ShowWindow(hNoButton, SW_SHOW);
			ShowWindow(hDescText, SW_HIDE);
			ShowWindow(hCancelButton, SW_HIDE);
			ShowWindow(hForceButton, SW_HIDE);
			return TRUE;
		}
		case IDYES:
		{
			_resolve(BlockedShutdownResolution_Force);
			EndDialog(hDlg, 0);
			DestroyWindow(hBgWnd);
			return TRUE;
		}
		case IDNO:
		{
			ShowWindow(hWarningText, SW_HIDE);
			ShowWindow(hYesButton, SW_HIDE);
			ShowWindow(hNoButton, SW_HIDE);
			ShowWindow(hDescText, SW_SHOW);
			ShowWindow(hCancelButton, SW_SHOW);
			ShowWindow(hForceButton, SW_SHOW);
			return TRUE;
		}
		}
		break;
	}
	case WM_ADD_APP:
	{
		if (!addQueue.empty())
		{
			for (auto& app : addQueue)
			{
				CreateAppTileControls(app.Get());
			}
			addQueue.clear();
		}
		return TRUE;
	}
	case WM_REMOVE_APP:
	{
		UINT appId = (UINT)wParam;
		RemoveAppTileControls(appId);
		return TRUE;
	}
	case WM_MOUSEWHEEL:
	{
		int dpi = GetDpiForWindow(hDlg);
		int visibleHeight = MulDiv(300, dpi, 96);

		if (hAppList)
		{
			RECT rcContainer;
			GetClientRect(hAppList, &rcContainer);
			visibleHeight = rcContainer.bottom - rcContainer.top;
		}

		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		int oldPos = scrollPos;
		scrollPos -= delta / 4;
		if (scrollPos < 0) scrollPos = 0;

		int maxScroll = totalContentHeight - visibleHeight;
		if (maxScroll < 0) maxScroll = 0;
		if (scrollPos > maxScroll) scrollPos = maxScroll;

		if (oldPos != scrollPos)
		{
			SCROLLINFO si = {};
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_POS;
			si.nPos = scrollPos;
			SetScrollInfo(hScrollBar, SB_CTL, &si, TRUE);
			SetWindowPos(hAppListScroll, nullptr, 0, -scrollPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOREDRAW);
			RedrawWindow(hAppList, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
		return TRUE;
	}
	case WM_VSCROLL:
	{
		int dpi = GetDpiForWindow(hDlg);
		int visibleHeight = MulDiv(300, dpi, 96);

		if (hAppList)
		{
			RECT rcContainer;
			GetClientRect(hAppList, &rcContainer);
			visibleHeight = rcContainer.bottom - rcContainer.top;
		}

		int oldPos = scrollPos;
		switch (LOWORD(wParam))
		{
		case SB_LINEUP: scrollPos -= MulDiv(20, dpi, 96); break;
		case SB_LINEDOWN: scrollPos += MulDiv(20, dpi, 96); break;
		case SB_PAGEUP: scrollPos -= MulDiv(100, dpi, 96); break;
		case SB_PAGEDOWN: scrollPos += MulDiv(100, dpi, 96); break;
		case SB_THUMBTRACK:
		case SB_THUMBPOSITION: scrollPos = HIWORD(wParam); break;
		}

		if (scrollPos < 0) scrollPos = 0;
		int maxScroll = totalContentHeight - visibleHeight;
		if (maxScroll < 0) maxScroll = 0;
		if (scrollPos > maxScroll) scrollPos = maxScroll;

		if (oldPos != scrollPos)
		{
			SCROLLINFO si = {};
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_POS;
			si.nPos = scrollPos;
			SetScrollInfo(hScrollBar, SB_CTL, &si, FALSE);
			SetWindowPos(hAppListScroll, nullptr, 0, -scrollPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOREDRAW);
			RedrawWindow(hAppList, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
		return TRUE;
	}
	}
	return FALSE;
}

LRESULT CALLBACK CustomBSDR::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		hDlg = CreateDialogParamW(HINST_THISCOMPONENT, MAKEINTRESOURCEW(IDD_BSDR_DLG), hWnd, DlgProc, lParam);
		if (hDlg)
		{
			ShowWindow(hDlg, SW_SHOW);
		}
		if (!IsHighContrast())
		{
			// Screenshot desktop and dim it
			HDC hDC = GetDC(nullptr);
			HDC hMemDC = CreateCompatibleDC(hDC);
			bgBitmap = CreateCompatibleBitmap(hDC, bgWidth, bgHeight);
			HBITMAP oldBitmap = (HBITMAP)SelectObject(hMemDC, bgBitmap);

			RECT rect = { 0, 0, bgWidth, bgHeight };
			HBRUSH hBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
			FillRect(hMemDC, &rect, hBrush);

			AlphaBlend(hMemDC, 0, 0, bgWidth, bgHeight, hDC, bgOffsetX, bgOffsetY, bgWidth, bgHeight, { AC_SRC_OVER, 0, 31, 0 });
			SelectObject(hMemDC, oldBitmap);
			DeleteDC(hMemDC);
			ReleaseDC(nullptr, hDC);

			separatorBitmap = LoadAlphaBitmap(IDB_BSDR_SEPARATOR);
			btnNormalBitmap = LoadAlphaBitmap(IDB_BSDR_BTN_NORMAL);
			btnHoverBitmap = LoadAlphaBitmap(IDB_BSDR_BTN_HOVER);
			btnPressedBitmap = LoadAlphaBitmap(IDB_BSDR_BTN_PRESSED);
			btnSelectedBitmap = LoadAlphaBitmap(IDB_BSDR_BTN_SELECTED);
			btnSelectedHoverBitmap = LoadAlphaBitmap(IDB_BSDR_BTN_SELECTED_HOVER);
		}
		return 0;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		if (bgBitmap)
		{
			HDC memDC = CreateCompatibleDC(hdc);
			HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, bgBitmap);
			BitBlt(hdc, 0, 0, bgWidth, bgHeight, memDC, 0, 0, SRCCOPY);
			SelectObject(memDC, oldBitmap);
			DeleteDC(memDC);
		}
		EndPaint(hWnd, &ps);
		return 0;
	}
	case WM_WINDOWPOSCHANGING:
	{
		// Deny window movement/resize from workarea resize, etc.
		WINDOWPOS* pPos = (WINDOWPOS*)lParam;
		pPos->flags |= SWP_NOMOVE | SWP_NOSIZE;
		return 0;
	}
	case WM_DISPLAYCHANGE:
	{
		int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
		int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
		int cx = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		int cy = GetSystemMetrics(SM_CYVIRTUALSCREEN);
		SetWindowPos(hWnd, nullptr, x, y, cx, cy, SWP_NOSENDCHANGING);
		CenterWindow(hDlg);
		UpdateAppListLayout();
		RedrawWindow(hDlg, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
		return 0;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(hWnd, message, wParam, lParam);
}

DWORD WINAPI CustomBSDR::ThreadProc(LPVOID lpParameter)
{
	// Attempt to create window on the input desktop, as the thread is always running in secure desktop at this point,
	// but the Windhawk mod can force this phase of session end to run in the default desktop
	HDESK hDesktop = OpenInputDesktop(0, FALSE, DESKTOP_CREATEWINDOW | DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS);
	if (hDesktop)
	{
		wchar_t desktopName[256] = {};
		if (GetUserObjectInformationW(hDesktop, UOI_NAME, desktopName, sizeof(desktopName), nullptr))
		{
			isOnSecureDesktop = (_wcsicmp(desktopName, L"winlogon") == 0);
		}
		if (!SetThreadDesktop(hDesktop))
		{
			CloseDesktop(hDesktop);
			return GetLastError();
		}
		CloseDesktop(hDesktop);
	}

	WNDCLASSEXW wndClass = {};

	wndClass.cbSize = sizeof(WNDCLASSEXW);
	wndClass.style = CS_GLOBALCLASS;
	wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndClass.hInstance = HINST_THISCOMPONENT;
	wndClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wndClass.lpfnWndProc = WndProc;
	wndClass.lpszClassName = L"BlockedShutdownResolver";

	if (RegisterClassExW(&wndClass) == 0)
	{
		return GetLastError();
	}

	bgOffsetX = GetSystemMetrics(SM_XVIRTUALSCREEN);
	bgOffsetY = GetSystemMetrics(SM_YVIRTUALSCREEN);
	bgWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	bgHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	hBgWnd = CreateWindowExW(WS_EX_TOPMOST, wndClass.lpszClassName, NULL, WS_POPUP | WS_VISIBLE, bgOffsetX, bgOffsetY, bgWidth, bgHeight, NULL, NULL, NULL, NULL);

	if (!hBgWnd)
	{
		return GetLastError();
	}

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0) > 0)
	{
		if (msg.message == WM_SYSKEYDOWN && msg.wParam == VK_MENU)
		{
			SetPropW(hForceButton, L"CustomBSDR_HideAccel", (HANDLE)FALSE);
			SetPropW(hCancelButton, L"CustomBSDR_HideAccel", (HANDLE)FALSE);
			SetPropW(hYesButton, L"CustomBSDR_HideAccel", (HANDLE)FALSE);
			SetPropW(hNoButton, L"CustomBSDR_HideAccel", (HANDLE)FALSE);
			RedrawWindow(hDlg, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
		}

		if (!IsDialogMessageW(hDlg, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}
	return 0;
}

void CustomBSDR::Start(Resolve_t resolve, LogonUIState state)
{
	_resolve = resolve;
	_logonUIState = state;
	hThread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, nullptr);
}

void CustomBSDR::AddApplication(IShutdownBlockingApp* blockingApp)
{
	if (hDlg && IsWindow(hDlg))
	{
		addQueue.push_back(blockingApp);
		PostMessageW(hDlg, WM_ADD_APP, 0, 0);
	}
	else
	{
		pendingApps.push_back(blockingApp);
	}
}

void CustomBSDR::RemoveApplication(UINT appid)
{
	if (hDlg && IsWindow(hDlg))
	{
		PostMessageW(hDlg, WM_REMOVE_APP, (WPARAM)appid, 0);
	}
}

void CustomBSDR::Hide()
{
	if (hDlg)
	{
		ShowWindow(hDlg, SW_HIDE);
	}
}

void CustomBSDR::Stop()
{
	for (auto& tile : appTiles)
	{
		if (tile.hIconBitmap) DeleteObject(tile.hIconBitmap);
	}
	appTiles.clear();

	if (hDlg)
	{
		DestroyWindow(hDlg);
		hDlg = nullptr;
	}

	if (hBgWnd)
	{
		DestroyWindow(hBgWnd);
		hBgWnd = nullptr;
	}

	if (bgBitmap)
	{
		DeleteObject(bgBitmap);
		bgBitmap = nullptr;
	}
	if (separatorBitmap)
	{
		DeleteObject(separatorBitmap);
		separatorBitmap = nullptr;
	}
	if (btnNormalBitmap)
	{
		DeleteObject(btnNormalBitmap);
		btnNormalBitmap = nullptr;
	}
	if (btnHoverBitmap)
	{
		DeleteObject(btnHoverBitmap);
		btnHoverBitmap = nullptr;
	}
	if (btnPressedBitmap)
	{
		DeleteObject(btnPressedBitmap);
		btnPressedBitmap = nullptr;
	}
	if (btnSelectedBitmap)
	{
		DeleteObject(btnSelectedBitmap);
		btnSelectedBitmap = nullptr;
	}
	if (btnSelectedHoverBitmap)
	{
		DeleteObject(btnSelectedHoverBitmap);
		btnSelectedHoverBitmap = nullptr;
	}
	if (hTitleFont)
	{
		DeleteObject(hTitleFont);
		hTitleFont = nullptr;
	}
	if (hDescFont)
	{
		DeleteObject(hDescFont);
		hDescFont = nullptr;
	}
}
