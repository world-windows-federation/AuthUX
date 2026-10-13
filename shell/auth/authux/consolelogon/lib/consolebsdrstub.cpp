#include "pch.h"
#include "custombsdr.h"

using namespace Microsoft::WRL;

using namespace ABI::Windows::Foundation;
using namespace Windows::Internal::UI::Logon::Controller;
using namespace Windows::Internal::UI::Logon::CredProvData;

//extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Windows_Internal_UI_Logon_Controller_ConsoleBlockedShutdownResolver[] = L"Windows.Internal.UI.Logon.Controller.ConsoleBlockedShutdownResolver";
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Windows_Internal_UI_Logon_Controller_ConsoleBlockedShutdownResolver[] = L"Windows.Internal.UI.Logon.Controller.BlockedShutdownResolverUX";

class ConsoleBSDRStub
	: public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>
	, IBlockedShutdownResolverUX
	, FtmBase
	>
{
	InspectableClass(RuntimeClass_Windows_Internal_UI_Logon_Controller_ConsoleBlockedShutdownResolver, FullTrust);

public:
	ConsoleBSDRStub();
	~ConsoleBSDRStub() override;

	//~ Begin IBlockedShutdownResolverUX Interface
	STDMETHODIMP Start(IUserSettingManager* settingsManager, ILogonUIStateInfo* stateInfo) override;
	STDMETHODIMP get_ScaleFactor(UINT* value) override;
	STDMETHODIMP get_WasClicked(BOOLEAN* value) override;
	STDMETHODIMP AddApplication(IShutdownBlockingApp* blockingApp) override;
	STDMETHODIMP RemoveApplication(UINT appid) override;
	STDMETHODIMP add_Resolved(ITypedEventHandler<IBlockedShutdownResolverUX*, BlockedShutdownResolution>* handler, EventRegistrationToken* token) override;
	STDMETHODIMP remove_Resolved(EventRegistrationToken token) override;
	STDMETHODIMP Hide() override;
	STDMETHODIMP Stop() override;
	//~ End IBlockedShutdownResolverUX Interface
private:
	static EventSource<ITypedEventHandler<IBlockedShutdownResolverUX*, BlockedShutdownResolution>> _Resolved;
	static BOOLEAN _wasClicked;

	static void Resolve(BlockedShutdownResolution resolution);
};

BOOLEAN ConsoleBSDRStub::_wasClicked = false;
EventSource<ITypedEventHandler<IBlockedShutdownResolverUX*, BlockedShutdownResolution>> ConsoleBSDRStub::_Resolved = {};

ConsoleBSDRStub::ConsoleBSDRStub()
{
}

ConsoleBSDRStub::~ConsoleBSDRStub()
{
}

HRESULT ConsoleBSDRStub::Start(IUserSettingManager* settingsManager, ILogonUIStateInfo* stateInfo)
{
	LogonUIState logonUIState = LogonUIState_Start;
	stateInfo->get_CurrentLogonUIState(&logonUIState);
	CustomBSDR::Start(&ConsoleBSDRStub::Resolve, logonUIState);
	return S_OK;
}

HRESULT ConsoleBSDRStub::get_ScaleFactor(UINT* value)
{
	*value = 100;
	return S_OK;
}

HRESULT ConsoleBSDRStub::get_WasClicked(BOOLEAN* value)
{
	*value = _wasClicked;
	return S_OK;
}

HRESULT ConsoleBSDRStub::AddApplication(IShutdownBlockingApp* blockingApp)
{
	CustomBSDR::AddApplication(blockingApp);
	return S_OK;
}

HRESULT ConsoleBSDRStub::RemoveApplication(UINT appid)
{
	CustomBSDR::RemoveApplication(appid);
	return S_OK;
}

HRESULT ConsoleBSDRStub::add_Resolved(
	ITypedEventHandler<IBlockedShutdownResolverUX*, BlockedShutdownResolution>* handler, EventRegistrationToken* token)
{
	token->value = 0;

	return _Resolved.Add(handler, token);
}

HRESULT ConsoleBSDRStub::remove_Resolved(EventRegistrationToken token)
{
	return _Resolved.Remove(token);
}

HRESULT ConsoleBSDRStub::Hide()
{
	CustomBSDR::Hide();
	return S_OK;
}

HRESULT ConsoleBSDRStub::Stop()
{
	CustomBSDR::Stop();
	return S_OK;
}

void ConsoleBSDRStub::Resolve(BlockedShutdownResolution resolution)
{
	_wasClicked = true;
	(void)_Resolved.InvokeAll(nullptr, resolution);
}

ActivatableClass(ConsoleBSDRStub);
