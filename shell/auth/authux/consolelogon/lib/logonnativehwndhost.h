#pragma once

#include "pch.h"
#include "DirectUI/DirectUI.h"

class CLogonNativeHWNDHost : public DirectUI::NativeHWNDHost
{
public:

	CLogonNativeHWNDHost();

	~CLogonNativeHWNDHost() override
	{
		UnregisterClassW(L"AUTHUI.DLL: LogonUI Logon Window", nullptr);
		DirectUI::NativeHWNDHost::~NativeHWNDHost();
	}

	static HRESULT Create(int dx, int dy, int dwidth, int dheight, CLogonNativeHWNDHost** out);

	HRESULT OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plRet) override;

	void FrameResize(class CLogonFrame* logonFrame, int X, int Y, int width, int height);

	LCPD::CredProvScenario m_scenario;
};
