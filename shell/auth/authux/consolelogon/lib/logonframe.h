#pragma once

#include "pch.h"

#include "logonnativehwndhost.h"
#include "logonviewmanager.h"
#include "DirectUI/DirectUI.h"
#include "userlist.h"

class CLogonFrame : public DirectUI::HWNDElement
{
public:

	~CLogonFrame() override;

	static DirectUI::IClassInfo* Class;
	DirectUI::IClassInfo* GetClassInfoW() override;
	static DirectUI::IClassInfo* GetClassInfoPtr();

	static HRESULT Create(HWND hParent, bool fDblBuffer, UINT nCreate, Element* pParent, DWORD* pdwDeferCookie, DirectUI::Element** ppElement);

	static HRESULT Create(CLogonNativeHWNDHost* host);

	static HRESULT Register();

	static CLogonFrame* GetSingleton();

	HRESULT CreateStyleParser(DirectUI::DUIXmlParser** outParser) override;
	void OnEvent(DirectUI::Event* pEvent) override;

	void SetOptions(MessageOptionFlag optionsFlag);

	void SetBackgroundGraphics();
	void RemoveBackgroundGraphics();
	void ShowSecurityOptions(LC::LogonUISecurityOptions SecurityOptsFlag, WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>> completion);
	HRESULT OnSecurityOptionSelected(LC::LogonUISecurityOptions SecurityOpt);
	HRESULT ConfirmEmergencyShutdown();

	void ShowStatusMessage(const wchar_t* message);

	void SwitchToUserList(class UserList* userList);
	void DisplayLogonDialog(const wchar_t* messageCaptionContent, const wchar_t* messageContent, WORD flags, WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion);
	void DisplayLogonDialog(const wchar_t* messageCaptionContent, const wchar_t* messageContent, WORD flags);
	HRESULT OnMessageOptionPressed(MessageOptionFlag flag);

	void ShowLockedScreen();

	CLogonNativeHWNDHost* m_nativeHost;
	DirectUI::Element* m_CurrentWindow = nullptr;
	DirectUI::DUIXmlParser* m_xmlParser;

	DirectUI::Element* m_Window = nullptr;
	DirectUI::Element* m_Options = nullptr;
	DirectUI::Element* m_SwitchUser = nullptr;
	DirectUI::Element* m_OtherTiles = nullptr;
	DirectUI::Element* m_Ok = nullptr;
	DirectUI::Element* m_Yes = nullptr;
	DirectUI::Element* m_No = nullptr;
	DirectUI::Element* m_Cancel = nullptr;
	DirectUI::Element* m_ShutDownFrame = nullptr;
	DirectUI::Element* m_ShowPLAP = nullptr;
	DirectUI::Element* m_DisconnectPLAP = nullptr;
	DirectUI::Element* m_Accessibility = nullptr;
	DirectUI::Element* m_MessageFrame = nullptr;
	DirectUI::Element* m_FullMessageFrame = nullptr;
	DirectUI::Element* m_ShortMessageFrame = nullptr;
	DirectUI::Element* m_ConnectMessageFrame = nullptr;
	DirectUI::Element* m_Status = nullptr;
	DirectUI::Element* m_StatusText = nullptr;
	DirectUI::Element* m_SecurityOptions = nullptr;
	DirectUI::Element* m_Locked = nullptr;
	DirectUI::Element* m_WaitAnimation = nullptr;

	UserList* m_activeUserList;
	UserList* m_LogonUserList;
	UserList* m_PLAPUserList;

	Microsoft::WRL::ComPtr<LogonViewManager> m_consoleUIManager;
	Microsoft::WRL::ComPtr<IShutdownChoices> m_shutdownChoices;
	LC::LogonUIRequestReason m_currentReason;

	bool isHighContrast = false;

private:
	HRESULT _Initialize(CLogonNativeHWNDHost* Host, DirectUI::Element* pParent, DWORD* DeferCookie);
	HRESULT _InitializeUserLists();
	bool _IsInstallUpdatesAndShutdownAllowed();
	bool _ShowBackgroundBitmap();
	bool _IsSwitchUserAllowed();
	void _SetSoftKeyboardAllowed(bool allowed);
	void _SetBrandingGraphic();
	void _SelectMode(DirectUI::Element* elementToHost, bool isVisible);
	void _ShowCursor(bool bShow);
	void _DisplayStatusMessage(const wchar_t* message, bool showSpinner);
	void _DisplayLogonDialog(const wchar_t* messageCaptionContent, const wchar_t* messageContent, UINT flags);
	void _OnEmergencyRestart();
	void _HandleShutdownChoices();
	void _ShutdownCommon(DWORD choice);

	static CLogonFrame* _pSingleton;

	wistd::unique_ptr<WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>>> m_SecurityOptionsCompletion;
	wistd::unique_ptr<WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>>> m_MessageDisplayResultCompletion;
	bool m_bIsInEmergencyRestartDialog = false;
};
