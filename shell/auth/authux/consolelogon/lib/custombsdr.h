#pragma once

#include "pch.h"
#include <vector>

using namespace Windows::Internal::UI::Logon::Controller;

typedef void(*Resolve_t)(BlockedShutdownResolution);

namespace CustomBSDR
{
	void Start(Resolve_t resolve, LogonUIState state);
	void Hide();
	void Stop();
	void AddApplication(IShutdownBlockingApp* app);
	void RemoveApplication(UINT appId);

	static Resolve_t _resolve = nullptr;
	static LogonUIState _logonUIState = LogonUIState::LogonUIState_LoggingOff;

	// handles
	static HANDLE hThread = nullptr;
	static HWND hDlg = nullptr;
	static HWND hBgWnd = nullptr;
	static HWND hTitleText = nullptr;
	static HWND hAppList = nullptr;
	static HWND hAppListScroll = nullptr;
	static HWND hScrollBar = nullptr;
	static HWND hWarningText = nullptr;
	static HWND hForceButton = nullptr;
	static HWND hCancelButton = nullptr;
	static HWND hDescText = nullptr;
	static HWND hYesButton = nullptr;
	static HWND hNoButton = nullptr;
	static HWND hHoverButton = nullptr;
	static HFONT hTitleFont = nullptr;
	static HFONT hDescFont = nullptr;

	// functions
	void CenterWindow(HWND hWnd);
	bool IsHighContrast();
	bool ShouldUseClassicScrollbar();
	HBITMAP LoadAlphaBitmap(UINT resourceId);
	void DrawSeparator(HDC hdc, LPDRAWITEMSTRUCT pDIS);
	void DrawButton(HDC hdc, LPDRAWITEMSTRUCT pDIS);
	void CreateAppTileControls(IShutdownBlockingApp* blockingApp);
	void RemoveAppTileControls(UINT appId);
	void UpdateAppListLayout();
	LRESULT CALLBACK ButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK AppListSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	INT_PTR CALLBACK DlgProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	DWORD WINAPI ThreadProc(LPVOID lpParameter);

	// bitmaps
	static HBITMAP bgBitmap = nullptr;
	static HBITMAP separatorBitmap = nullptr;
	static HBITMAP btnNormalBitmap = nullptr;
	static HBITMAP btnHoverBitmap = nullptr;
	static HBITMAP btnPressedBitmap = nullptr;
	static HBITMAP btnSelectedBitmap = nullptr;
	static HBITMAP btnSelectedHoverBitmap = nullptr;

	// variables
	static int bgOffsetX = 0;
	static int bgOffsetY = 0;
	static int bgWidth = 0;
	static int bgHeight = 0;
	static int scrollPos = 0;
	static int totalContentHeight = 0;
	static bool isOnSecureDesktop = true;
	
	// app list data stuff
	struct AppTile
	{
		UINT appId;
		BOOLEAN isBlocking;
		HWND hIcon;
		HWND hTitle;
		HWND hBlockReason;
		HBITMAP hIconBitmap;
		Microsoft::WRL::ComPtr<IShutdownBlockingApp> app;
	};
	static std::vector<AppTile> appTiles;
	static std::vector<Microsoft::WRL::ComPtr<IShutdownBlockingApp>> pendingApps;
	static std::vector<Microsoft::WRL::ComPtr<IShutdownBlockingApp>> addQueue;
};
