#include "pch.h"

#include <initguid.h>
#include "logonviewmanager.h"

#include <WtsApi32.h>

#include "EventDispatcher.h"
#include "optionaldependencyprovider.h"
#include "logonframe.h"
#include "logonguids.h"

using namespace Microsoft::WRL;

LogonViewManager::LogonViewManager()
	: m_currentViewType(LogonView::None)
	, m_serializationCompleteToken()
	, m_bioFeedbackStateChangeToken()
	, m_usersChangedToken()
	, m_selectedUserChangeToken()
	, m_credentialsChangedToken()
	, m_selectedCredentialChangedToken()
	, m_webDialogVisibilityChangedToken()
	, m_isCredentialResetRequired(false)
	, m_credProvInitialized(false)
	, m_showCredentialViewOnInitComplete(false)
	, m_currentReason(LC::LogonUIRequestReason_LogonUILogon)
{
}

HRESULT LogonViewManager::RuntimeClassInitialize()
{
	RETURN_IF_FAILED(Initialize()); // 46
	return S_OK;
}

HRESULT LogonViewManager::Invoke(LCPD::ICredentialGroup* sender, LCPD::ICredential* args)
{
	if (m_webDialogDismissTrigger.Get())
	{
		RETURN_IF_FAILED(m_webDialogDismissTrigger->DismissWebDialog());
	}

	LOG_IF_FAILED_MSG(E_FAIL,"INVOKE 1");
	if (m_currentViewType == LogonView::UserSelection
		|| m_currentViewType == LogonView::CredProvSelection
		|| m_currentViewType == LogonView::SelectedCredential
		|| m_currentViewType == LogonView::ComboBox)
	{
		LOG_IF_FAILED_MSG(E_FAIL, "INVOKE 1A");
		RETURN_IF_FAILED(ShowCredentialView()); // 235
	}

	return S_OK;
}

HRESULT LogonViewManager::Invoke(WFC::IObservableVector<LCPD::Credential*>* sender, WFC::IVectorChangedEventArgs* args)
{
	LOG_IF_FAILED_MSG(E_FAIL, "INVOKE 2");
	if (m_currentViewType == LogonView::CredProvSelection)
	{
		LOG_IF_FAILED_MSG(E_FAIL, "INVOKE 2A");
		RETURN_IF_FAILED(ShowCredentialView()); // 222
	}

	return S_OK;
}

HRESULT LogonViewManager::Invoke(WFC::IObservableVector<IInspectable*>* sender, WFC::IVectorChangedEventArgs* args)
{
	LOG_IF_FAILED_MSG(E_FAIL, "INVOKE 3");
	if (m_currentViewType == LogonView::UserSelection)
	{
		LOG_IF_FAILED_MSG(E_FAIL, "INVOKE 3A");
		RETURN_IF_FAILED(ShowCredentialView()); // 205
	}

	return S_OK;
}

HRESULT LogonViewManager::Invoke(LCPD::ICredProvDataModel* sender, IInspectable* args)
{
	if (m_webDialogDismissTrigger.Get())
	{
		RETURN_IF_FAILED(m_webDialogDismissTrigger->DismissWebDialog());
	}

	LOG_IF_FAILED_MSG(E_FAIL, "INVOKE 4");
	if (m_currentViewType == LogonView::UserSelection
		|| m_currentViewType == LogonView::CredProvSelection
		|| m_currentViewType == LogonView::SelectedCredential
		|| m_currentViewType == LogonView::ComboBox)
	{
		LOG_IF_FAILED_MSG(E_FAIL, "INVOKE 4A");
		RETURN_IF_FAILED(ShowCredentialView()); // 196
	}

	return S_OK;
}

HRESULT LogonViewManager::Invoke(LCPD::ICredProvDataModel* sender, LCPD::BioFeedbackState args)
{
	LOG_IF_FAILED_MSG(E_FAIL, "INVOKE 5");
	if (m_bioFeedbackListener.Get())
	{
		LCPD::BioFeedbackState state;
		RETURN_IF_FAILED(m_credProvDataModel->get_CurrentBioFeedbackState(&state)); // 178

		Wrappers::HString label;
		RETURN_IF_FAILED(m_credProvDataModel->get_BioFeedbackLabel(label.ReleaseAndGetAddressOf())); // 181

		RETURN_IF_FAILED(m_bioFeedbackListener->OnBioFeedbackUpdate(state, label.Get())); // 183
	}

	return S_OK;
}

HRESULT LogonViewManager::Invoke(LCPD::ICredProvDataModel* sender, LCPD::ICredentialSerialization* args)
{
	LOG_IF_FAILED_MSG(E_FAIL, "INVOKE 6");
	if (m_requestCredentialsComplete)
	{
		LCPD::SerializationResponse response;
		RETURN_IF_FAILED(args->get_SerializationResponse(&response)); // 55

		if (response == LCPD::SerializationResponse_ReturnCredentialComplete)
		{
			wil::unique_cotaskmem_ptr<BYTE> aaa = wil::make_unique_cotaskmem_nothrow<BYTE>(20 * 20);
			ComPtr<LC::IRequestCredentialsDataFactory> factory;
			RETURN_IF_FAILED(WF::GetActivationFactory(
				Wrappers::HStringReference(RuntimeClass_Windows_Internal_UI_Logon_Controller_RequestCredentialsData).Get(), &factory)); // 60

			ComPtr<LC::IRequestCredentialsData> data;
			RETURN_IF_FAILED(factory->CreateRequestCredentialsData(args, LC::LogonUIShutdownChoice_None, nullptr, &data)); // 63

			RETURN_IF_FAILED(m_requestCredentialsComplete->GetResult().Set(data.Get())); // 65

			m_requestCredentialsComplete->Complete(S_OK);
			m_requestCredentialsComplete.reset();
		}
		else
		{
			bool b = true;

			if (response != LCPD::SerializationResponse_ReturnNoCredentialComplete)
			{
				Wrappers::HString statusMessage;
				RETURN_IF_FAILED(args->get_StatusMessage(statusMessage.ReleaseAndGetAddressOf())); // 75
				if (statusMessage.Get())
				{
					UINT uType = 0;
					if (response == LCPD::SerializationResponse_NoCredentialIncomplete)
					{
						uType = 3;
					}
					else if (response == LCPD::SerializationResponse_NoCredentialComplete)
					{
						uType = 4;
					}

					LCPD::CredentialProviderStatusIcon statusIcon;
					RETURN_IF_FAILED(args->get_SerializationIcon(&statusIcon)); // 92

					UINT logonMessageMode;
					if (statusIcon == LCPD::CredentialProviderStatusIcon_Error)
					{
						logonMessageMode = 0x10;
					}
					else if (statusIcon == LCPD::CredentialProviderStatusIcon_Warning)
					{
						logonMessageMode = 0x10 | 0x20;
					}
					else
					{
						logonMessageMode = 0x40;
					}

					CoTaskMemNativeString caption;
					RETURN_IF_FAILED(caption.Initialize(HINST_THISCOMPONENT, IDS_WINSECURITY)); // 108

					UINT redirectResult;
					LC::LogonErrorRedirectorResponse errorResponse;
					RETURN_IF_FAILED(m_redirectionManager->RedirectMessage(Wrappers::HStringReference(caption.Get()).Get(), statusMessage.Get(), logonMessageMode, &redirectResult, &errorResponse)); // 112
					if (errorResponse == LC::LogonErrorRedirectorResponse_HandledDoNotShowLocally && uType == 3)
					{
						errorResponse = LC::LogonErrorRedirectorResponse_HandledShowLocally;
					}

					if (errorResponse != LC::LogonErrorRedirectorResponse_HandledDoNotShowLocally
						&& errorResponse != LC::LogonErrorRedirectorResponse_HandledDoNotShowLocallyStartOver)
					{
						b = false;
						RETURN_IF_FAILED(m_redirectionManager->OnBeginPainting()); // 132
						RETURN_IF_FAILED(ShowSerializationFailedView(nullptr, statusMessage.Get())); // 133
					}
				}
				else
				{
					if (response == LCPD::SerializationResponse_NoCredentialIncomplete
						|| response == LCPD::SerializationResponse_NoCredentialComplete)
					{
						RETURN_IF_FAILED(m_redirectionManager->OnBeginPainting()); // 142
						RETURN_IF_FAILED(ShowCredentialView()); // 143
						b = false;
					}
				}
			}

			if (b)
			{
				ComPtr<LC::IRequestCredentialsDataFactory> factory;
				RETURN_IF_FAILED(WF::GetActivationFactory(
					Wrappers::HStringReference(RuntimeClass_Windows_Internal_UI_Logon_Controller_RequestCredentialsData).Get(), &factory)); // 152
				ComPtr<LC::IRequestCredentialsData> data;
				RETURN_IF_FAILED(factory->CreateRequestCredentialsData(nullptr, LC::LogonUIShutdownChoice_None, nullptr, &data)); // 155

				RETURN_IF_FAILED(m_requestCredentialsComplete->GetResult().Set(data.Get())); // 157

				m_requestCredentialsComplete->Complete(S_OK);
			}
		}
	}
	else if (m_unlockTrigger.Get())
	{
		m_cachedSerialization = args;
		RETURN_IF_FAILED(m_unlockTrigger->TriggerUnlock()); // 165
		m_unlockTrigger.Reset();
	}

	return S_OK;
}

HRESULT LogonViewManager::Invoke(LCPD::ICredential* sender, IInspectable* args)
{
	BOOLEAN webDialogVisible;
	RETURN_IF_FAILED(sender->get_IsWebDialogVisible(&webDialogVisible));

	if (webDialogVisible)
	{
		if (m_requestCredentialsComplete)
		{
			Wrappers::HString webDialogUrl;
			RETURN_IF_FAILED(sender->get_WebDialogUrl(webDialogUrl.ReleaseAndGetAddressOf()));
			ComPtr<LC::IRequestCredentialsDataFactory> factory;
			RETURN_IF_FAILED(WF::GetActivationFactory(Wrappers::HStringReference(RuntimeClass_Windows_Internal_UI_Logon_Controller_RequestCredentialsData).Get(), &factory));

			ComPtr<LC::IRequestCredentialsData> data;
			RETURN_IF_FAILED(factory->CreateRequestCredentialsData(nullptr, LC::LogonUIShutdownChoice_None, webDialogUrl.Get(), &data));

			RETURN_IF_FAILED(m_requestCredentialsComplete->GetResult().Set(data.Get()));
			m_requestCredentialsComplete->Complete(S_OK);
		}
	}
	else
	{
		if (m_webDialogDismissTrigger.Get())
		{
			RETURN_IF_FAILED(m_webDialogDismissTrigger->DismissWebDialog());
		}
	}

	return S_OK;
}

HRESULT LogonViewManager::SetContext(
	IInspectable* autoLogonManager, LC::IUserSettingManager* userSettingManager,
	LC::IRedirectionManager* redirectionManager, LCPD::IDisplayStateProvider* displayStateProvider,
	LC::IBioFeedbackListener* bioFeedbackListener)
{
	RETURN_IF_FAILED(EnsureUIStarted()); // 328

	ComPtr<IInspectable> autoLogonManagerRef = autoLogonManager;
	ComPtr<LC::IUserSettingManager> userSettingManagerRef = userSettingManager;
	ComPtr<LC::IRedirectionManager> redirectionManagerRef = redirectionManager;
	ComPtr<LCPD::IDisplayStateProvider> displayStateProviderRef = displayStateProvider;
	ComPtr<LC::IBioFeedbackListener> bioFeedbackListenerRef = bioFeedbackListener;
	ComPtr<LogonViewManager> thisRef = this;

	wil::unique_event_nothrow setContextCompleteEvent;
	RETURN_IF_FAILED(setContextCompleteEvent.create(wil::EventOptions::ManualReset)); // 339

	HRESULT hr = BeginInvoke(m_Dispatcher.Get(), [thisRef, this, autoLogonManagerRef, userSettingManagerRef, redirectionManagerRef, displayStateProviderRef, bioFeedbackListenerRef, &setContextCompleteEvent]() -> void
	{
		UNREFERENCED_PARAMETER(thisRef);
		SetContextUIThread(
			autoLogonManagerRef.Get(), userSettingManagerRef.Get(), redirectionManagerRef.Get(),
			displayStateProviderRef.Get(), bioFeedbackListenerRef.Get());
		setContextCompleteEvent.SetEvent();
	});
	RETURN_IF_FAILED(hr); // 344
	WaitForSingleObject(setContextCompleteEvent.get(), INFINITE);
	return S_OK;
}

HRESULT LogonViewManager::Lock(
	LC::LogonUIRequestReason reason, BOOLEAN allowDirectUserSwitching, HSTRING unk,
	LC::IUnlockTrigger* unlockTrigger)
{
	RETURN_IF_FAILED(EnsureUIStarted()); // 353

	ComPtr<CRefCountedObject<Wrappers::HString>> unkRef = CreateRefCountedObj<Wrappers::HString>();
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
	RETURN_IF_FAILED(unkRef->Set(unk));
#endif

	ComPtr<LC::IUnlockTrigger> unlockTriggerRef = unlockTrigger;
	ComPtr<LogonViewManager> thisRef = this;

	HRESULT hr = BeginInvoke(m_Dispatcher.Get(), [=]() -> void
	{
		UNREFERENCED_PARAMETER(thisRef);
		HRESULT hrInner = LockUIThread(reason, allowDirectUserSwitching, unkRef->Get(), unlockTriggerRef.Get());
		if (FAILED(hrInner))
		{
			unlockTriggerRef->TriggerUnlock();
		}
	});
	RETURN_IF_FAILED(hr); // 364
	return S_OK;
}

HRESULT LogonViewManager::RequestCredentials(
	LC::LogonUIRequestReason reason, LC::LogonUIFlags flags, HSTRING unk,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IRequestCredentialsData>> completion)
{
	RETURN_IF_FAILED(EnsureUIStarted()); // 370

	ComPtr<CRefCountedObject<Wrappers::HString>> unkRef = CreateRefCountedObj<Wrappers::HString>();
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
	RETURN_IF_FAILED(unkRef->Set(unk));
#endif

	ComPtr<LogonViewManager> thisRef = this;

	HRESULT hr = BeginInvoke(m_Dispatcher.Get(), [thisRef, this, reason, flags, unkRef, completion]() -> void
	{
		UNREFERENCED_PARAMETER(thisRef);
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IRequestCredentialsData>> deferral = completion;
		HRESULT hrInner = RequestCredentialsUIThread(reason, flags, unkRef->Get(), deferral);
		if (FAILED(hrInner))
		{
			deferral.Complete(hrInner);
		}
	});
	RETURN_IF_FAILED(hr); // 382
	return S_OK;
}

HRESULT LogonViewManager::ReportResult(
	LC::LogonUIRequestReason reason, NTSTATUS ntStatus, NTSTATUS ntSubStatus, HSTRING samCompatibleUserName,
	HSTRING displayName, HSTRING userSid,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IReportCredentialsData>> completion)
{
	RETURN_IF_FAILED(EnsureUIStarted()); // 388

	ComPtr<CRefCountedObject<Wrappers::HString>> samCompatibleUserNameRef = CreateRefCountedObj<Wrappers::HString>();
	RETURN_IF_FAILED(samCompatibleUserNameRef->Set(samCompatibleUserName)); // 391

	ComPtr<CRefCountedObject<Wrappers::HString>> displayNameRef = CreateRefCountedObj<Wrappers::HString>();
	RETURN_IF_FAILED(displayNameRef->Set(displayName)); // 394

	ComPtr<CRefCountedObject<Wrappers::HString>> userSidRef = CreateRefCountedObj<Wrappers::HString>();
	RETURN_IF_FAILED(userSidRef->Set(userSid)); // 397

	ComPtr<LogonViewManager> thisRef = this;

	HRESULT hr = BeginInvoke(m_Dispatcher.Get(), [thisRef, this, reason, ntStatus, ntSubStatus, samCompatibleUserNameRef, displayNameRef, userSidRef, completion]() -> void
	{
		UNREFERENCED_PARAMETER(thisRef);
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IReportCredentialsData>> deferral = completion;
		HRESULT hrInner = ReportResultUIThread(reason, ntStatus, ntSubStatus, samCompatibleUserNameRef->Get(), displayNameRef->Get(), userSidRef->Get(), deferral);
		if (FAILED(hrInner))
		{
			deferral.Complete(hrInner);
		}
	});
	RETURN_IF_FAILED(hr); // 409
	return S_OK;
}

HRESULT LogonViewManager::ClearCredentialState()
{
	RETURN_IF_FAILED(EnsureUIStarted()); // 415

	ComPtr<LogonViewManager> thisRef = this;

	HRESULT hr = BeginInvoke(m_Dispatcher.Get(), [=]() -> void
	{
		UNREFERENCED_PARAMETER(thisRef);
		ClearCredentialStateUIThread();
	});
	RETURN_IF_FAILED(hr); // 421
	return S_OK;
}

HRESULT LogonViewManager::DisplayStatus(LC::LogonUIState state, HSTRING status, WI::AsyncDeferral<WI::CNoResult> completion)
{
	RETURN_IF_FAILED(EnsureUIStarted()); // 445

	ComPtr<CRefCountedObject<Wrappers::HString>> statusRef = CreateRefCountedObj<Wrappers::HString>();

	if (status)
	{
		RETURN_IF_FAILED(statusRef->Set(status)); // 451
	}

	ComPtr<LogonViewManager> thisRef = this;

	HRESULT hr = BeginInvoke(m_Dispatcher.Get(), [thisRef, this, state, statusRef, completion]() -> void
	{
		UNREFERENCED_PARAMETER(thisRef);
		WI::AsyncDeferral<WI::CNoResult> deferral = completion;
		HRESULT hrInner = DisplayStatusUIThread(state, statusRef->Get(), deferral);
		if (FAILED(hrInner))
		{
			deferral.Complete(hrInner);
		}
	});
	RETURN_IF_FAILED(hr); // 464
	return S_OK;
}

HRESULT LogonViewManager::DisplayMessage(
	LC::LogonMessageMode messageMode, UINT messageBoxFlags, HSTRING caption, HSTRING message,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion)
{
	RETURN_IF_FAILED(EnsureUIStarted()); // 470

	ComPtr<CRefCountedObject<Wrappers::HString>> captionRef = CreateRefCountedObj<Wrappers::HString>();
	if (caption)
	{
		RETURN_IF_FAILED(captionRef->Set(caption)); // 475
	}

	ComPtr<CRefCountedObject<Wrappers::HString>> messageRef = CreateRefCountedObj<Wrappers::HString>();
	if (message)
	{
		RETURN_IF_FAILED(messageRef->Set(message)); // 481
	}

	ComPtr<LogonViewManager> thisRef = this;

	HRESULT hr = BeginInvoke(m_Dispatcher.Get(), [thisRef, this, messageMode, messageBoxFlags, captionRef, messageRef, completion]() -> void
	{
		UNREFERENCED_PARAMETER(thisRef);
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> deferral = completion;
		HRESULT hrInner = DisplayMessageUIThread(messageMode, messageBoxFlags, captionRef->Get(), messageRef->Get(), deferral);
		if (FAILED(hrInner))
		{
			deferral.Complete(hrInner);
		}
	});
	RETURN_IF_FAILED(hr); // 494
	return S_OK;
}

HRESULT LogonViewManager::DisplayCredentialError(
	NTSTATUS ntsStatus, NTSTATUS ntsSubStatus, UINT messageBoxFlags, HSTRING caption, HSTRING message,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion)
{
	RETURN_IF_FAILED(EnsureUIStarted()); // 500

	ComPtr<CRefCountedObject<Wrappers::HString>> captionRef = CreateRefCountedObj<Wrappers::HString>();
	if (caption)
	{
		RETURN_IF_FAILED(captionRef->Set(caption)); // 505
	}

	ComPtr<CRefCountedObject<Wrappers::HString>> messageRef = CreateRefCountedObj<Wrappers::HString>();
	if (message)
	{
		RETURN_IF_FAILED(messageRef->Set(message)); // 511
	}

	ComPtr<LogonViewManager> thisRef = this;

	HRESULT hr = BeginInvoke(m_Dispatcher.Get(), [thisRef, this, ntsStatus, ntsSubStatus, messageBoxFlags, captionRef, messageRef, completion]() -> void
	{
		UNREFERENCED_PARAMETER(thisRef);
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> deferral = completion;
		HRESULT hrInner = DisplayCredentialErrorUIThread(ntsStatus, ntsSubStatus, messageBoxFlags, captionRef->Get(), messageRef->Get(), deferral);
		if (FAILED(hrInner))
		{
			deferral.Complete(hrInner);
		}
	});
	RETURN_IF_FAILED(hr); // 524
	return S_OK;
}

HRESULT LogonViewManager::ShowSecurityOptions(
	LC::LogonUISecurityOptions options,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>> completion)
{
	RETURN_IF_FAILED(EnsureUIStarted()); // 427

	ComPtr<LogonViewManager> thisRef = this;

	HRESULT hr = BeginInvoke(m_Dispatcher.Get(), [thisRef, this, options, completion]() -> void
	{
		UNREFERENCED_PARAMETER(thisRef);
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>> deferral = completion;
		HRESULT hrInner = ShowSecurityOptionsUIThread(options, deferral);
		if (FAILED(hrInner))
		{
			deferral.Complete(hrInner);
		}
	});
	RETURN_IF_FAILED(hr); // 439
	return S_OK;
}

HRESULT LogonViewManager::WebDialogDisplayed(LC::IWebDialogDismissTrigger* dismissTrigger)
{
	RETURN_IF_FAILED(EnsureUIStarted());

	ComPtr<LC::IWebDialogDismissTrigger> webDialogDismissTriggerRef = dismissTrigger;
	ComPtr<LogonViewManager> thisRef = this;

	HRESULT hr = BeginInvoke(m_Dispatcher.Get(), [thisRef, this, webDialogDismissTriggerRef]() -> void
	{
		UNREFERENCED_PARAMETER(thisRef);
		WebDialogDisplayedUIThread(webDialogDismissTriggerRef.Get());
	});
	RETURN_IF_FAILED(hr);
	return S_OK;
}

HRESULT LogonViewManager::Cleanup(WI::AsyncDeferral<WI::CNoResult> completion)
{
	RETURN_IF_FAILED(EnsureUIStarted()); // 530

	ComPtr<LogonViewManager> thisRef = this;

	HRESULT hr = BeginInvoke(m_Dispatcher.Get(), [thisRef, this, completion]() -> void
	{
		UNREFERENCED_PARAMETER(thisRef);
		WI::AsyncDeferral<WI::CNoResult> deferral = completion;
		HRESULT hrInner = CleanupUIThread(deferral);
		if (FAILED(hrInner))
		{
			deferral.Complete(hrInner);
		}
	});
	RETURN_IF_FAILED(hr); // 542
	return S_OK;
}

HRESULT LogonViewManager::SetContextUIThread(
	IInspectable* autoLogonManager, LC::IUserSettingManager* userSettingManager,
	LC::IRedirectionManager* redirectionManager, LCPD::IDisplayStateProvider* displayStateProvider,
	LC::IBioFeedbackListener* bioFeedbackListener)
{
	m_autoLogonManager = autoLogonManager;
	m_userSettingManager = userSettingManager;
	m_redirectionManager = redirectionManager;
	m_displayStateProvider = displayStateProvider;
	m_bioFeedbackListener = bioFeedbackListener;

	LANGID langID = 0;
	RETURN_IF_FAILED(m_userSettingManager->get_LangID(&langID)); // 556

	if (langID)
	{
		SetThreadUILanguage(langID);
	}

	RETURN_IF_FAILED(CoCreateInstance(CLSID_InputSwitchControl, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_inputSwitchControl))); // 563
	RETURN_IF_FAILED(m_inputSwitchControl->Init(ISCT_IDL_LOGONUI)); // 564
	m_inputSwitchControl->RegisterHotkeys();
	return S_OK;
}

HRESULT LogonViewManager::LockUIThread(
	LC::LogonUIRequestReason reason, BOOLEAN allowDirectUserSwitching, HSTRING unk, LC::IUnlockTrigger* unlockTrigger)
{
	m_requestCredentialsComplete.reset();

	m_currentReason = reason;
	CLogonFrame::GetSingleton()->m_currentReason = reason;
	CLogonFrame::GetSingleton()->m_consoleUIManager = this;
	m_unlockTrigger = unlockTrigger;
	m_webDialogDismissTrigger.Reset();

	CLogonFrame::GetSingleton()->ShowLockedScreen();

	//ComPtr<LockedView> lockView;
	//RETURN_IF_FAILED(MakeAndInitialize<LockedView>(&lockView)); // 578
	//
	//RETURN_IF_FAILED(lockView->Advise(this)); // 580
	//
	//RETURN_IF_FAILED(SetActiveView(lockView.Get())); // 582

	//m_currentView.Swap(lockView.Get());

	m_currentViewType = LogonView::Locked;
	m_showCredentialViewOnInitComplete = false;

	RETURN_IF_FAILED(StartCredProvsIfNecessary(reason, allowDirectUserSwitching, unk)); // 588

	return S_OK;
}

HRESULT LogonViewManager::RequestCredentialsUIThread(
	LC::LogonUIRequestReason reason, LC::LogonUIFlags flags, HSTRING unk,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IRequestCredentialsData>> completion)
{
	auto completeOnFailure = wil::scope_exit([this]() -> void { m_requestCredentialsComplete->Complete(E_UNEXPECTED); });

	//if (m_unlockTrigger.Get())
	//	m_unlockTrigger->TriggerUnlock();

	m_unlockTrigger.Reset();
	m_currentReason = reason;
	CLogonFrame::GetSingleton()->m_currentReason = reason;
	CLogonFrame::GetSingleton()->m_consoleUIManager = this;
	m_requestCredentialsComplete = wil::make_unique_nothrow<WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IRequestCredentialsData>>>(completion);
	RETURN_IF_NULL_ALLOC(m_requestCredentialsComplete); // 598

	if (m_cachedSerialization.Get())
	{
		ComPtr<LC::IRequestCredentialsDataFactory> factory;
		RETURN_IF_FAILED(WF::GetActivationFactory(
			Wrappers::HStringReference(RuntimeClass_Windows_Internal_UI_Logon_Controller_RequestCredentialsData).Get(),
			&factory)); // 610

		ComPtr<LC::IRequestCredentialsData> data;
		RETURN_IF_FAILED(factory->CreateRequestCredentialsData(m_cachedSerialization.Get(), LC::LogonUIShutdownChoice_None, nullptr, &data)); // 613

		RETURN_IF_FAILED(m_requestCredentialsComplete->GetResult().Set(data.Get())); // 615

		m_requestCredentialsComplete->Complete(S_OK);
		m_requestCredentialsComplete.reset();
		m_cachedSerialization.Reset();
	}
	else
	{
		m_showCredentialViewOnInitComplete = true;
		RETURN_IF_FAILED(StartCredProvsIfNecessary(reason, (flags & LC::LogonUIFlags_AllowDirectUserSwitching) != 0, unk)); // 623
	}

	completeOnFailure.release();
	return S_OK;
}

template <typename TDelegateInterface, typename TOperation, typename TFunc>
HRESULT StartOperationAndThen(TOperation* pOperation, TFunc&& func)
{
	ComPtr<TDelegateInterface> spCallback = Callback<TDelegateInterface>([func](TOperation* pOperation, AsyncStatus status)
	{
		HRESULT hr = S_OK;
		if (status != AsyncStatus::Completed)
		{
			ComPtr<IAsyncInfo> spAsyncInfo;
			hr = pOperation->QueryInterface(IID_PPV_ARGS(&spAsyncInfo));
			if (SUCCEEDED(hr))
			{
				spAsyncInfo->get_ErrorCode(&hr);
			}
		}
		func(hr, pOperation);
		return S_OK;
	});

	HRESULT hr;
	if (spCallback.Get())
	{
		hr = pOperation->put_Completed(spCallback.Get());
	}
	else
	{
		hr = E_OUTOFMEMORY;
	}

	return hr;
}

HRESULT LogonViewManager::ReportResultUIThread(
	LC::LogonUIRequestReason reason, NTSTATUS ntStatus, NTSTATUS ntSubStatus, HSTRING samCompatibleUserName,
	HSTRING displayName, HSTRING userSid,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IReportCredentialsData>> completion)
{
	//if (m_unlockTrigger.Get())
	//	m_unlockTrigger->TriggerUnlock();

	m_unlockTrigger.Reset();
	m_requestCredentialsComplete.reset();

	ComPtr<WF::IAsyncOperation<LCPD::ReportResultInfo*>> asyncOp;
	RETURN_IF_FAILED(m_credProvDataModel->ReportResultAsync(ntStatus, ntSubStatus, userSid, &asyncOp)); // 635

	ComPtr<LogonViewManager> thisRef = this;

	HRESULT hr = StartOperationAndThen<WF::IAsyncOperationCompletedHandler<LCPD::ReportResultInfo*>>(asyncOp.Get(), [completion, thisRef, this](HRESULT hrAction, WF::IAsyncOperation<LCPD::ReportResultInfo*>* asyncOp) -> HRESULT
	{
		UNREFERENCED_PARAMETER(thisRef);
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IReportCredentialsData>> completionRef = completion;
		auto completeOnFailure = wil::scope_exit([&completionRef]() -> void { completionRef.Complete(E_UNEXPECTED); });
		RETURN_IF_FAILED(hrAction);

		ComPtr<LCPD::IReportResultInfo> resultInfo;
		RETURN_IF_FAILED(asyncOp->GetResults(&resultInfo));

		Wrappers::HString statusMessage;
		RETURN_IF_FAILED(resultInfo->get_StatusMessage(statusMessage.ReleaseAndGetAddressOf()));

		ComPtr<LC::IReportCredentialsDataFactory> factory;
		RETURN_IF_FAILED(WF::GetActivationFactory(
			Wrappers::HStringReference(RuntimeClass_Windows_Internal_UI_Logon_Controller_ReportCredentialsData).Get(), &factory));

		ComPtr<LC::IReportCredentialsData> resultData;
		RETURN_IF_FAILED(factory->CreateReportCredentialsData(LC::LogonUICredProvResponse_LogonUIResponseDefault, statusMessage.Get(), &resultData)); // 667

		m_lastReportResultInfo = resultInfo;
		completionRef.GetResult().Set(resultData.Get());
		completionRef.Complete(S_OK);
		completeOnFailure.release();
		return S_OK;
	});
	RETURN_IF_FAILED(hr); // 667
	return S_OK;
}

HRESULT LogonViewManager::ShowSecurityOptionsUIThread(
	LC::LogonUISecurityOptions options,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>> completion)
{
	CLogonFrame::GetSingleton()->ShowSecurityOptions(options,completion);

	m_currentViewType = LogonView::SecurityOptions;
	return S_OK;
}

HRESULT LogonViewManager::DisplayStatusUIThread(LC::LogonUIState state, HSTRING status, WI::AsyncDeferral<WI::CNoResult> completion)
{
	if (status)
	{
		LC::LogonErrorRedirectorResponse errorResponse;
		RETURN_IF_FAILED(m_redirectionManager->RedirectStatus(status, &errorResponse)); // 706

		if (errorResponse != LC::LogonErrorRedirectorResponse_HandledDoNotShowLocally
			&& errorResponse != LC::LogonErrorRedirectorResponse_HandledDoNotShowLocallyStartOver)
		{
			RETURN_IF_FAILED(m_redirectionManager->OnBeginPainting()); // 716
			RETURN_IF_FAILED(ShowStatusView(status)); // 717
		}
	}

	completion.Complete(S_OK);
	return S_OK;
}

HRESULT LogonViewManager::DisplayMessageUIThread(
	LC::LogonMessageMode messageMode, UINT messageBoxFlags, HSTRING caption, HSTRING message,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion)
{
	auto completeOnFailure = wil::scope_exit([&completion]() -> void { completion.Complete(E_UNEXPECTED); });

	UINT redirectResult;
	LC::LogonErrorRedirectorResponse errorResponse;
	RETURN_IF_FAILED(m_redirectionManager->RedirectMessage(caption, message, messageBoxFlags, &redirectResult, &errorResponse)); // 733

	if (errorResponse == LC::LogonErrorRedirectorResponse_HandledDoNotShowLocally
		|| errorResponse == LC::LogonErrorRedirectorResponse_HandledDoNotShowLocallyStartOver)	
	{
		ComPtr<LC::IMessageDisplayResultFactory> factory;
		RETURN_IF_FAILED(WF::GetActivationFactory<ComPtr<LC::IMessageDisplayResultFactory>>(
			Wrappers::HStringReference(RuntimeClass_Windows_Internal_UI_Logon_Controller_MessageDisplayResult).Get(), &factory)); // 741

		ComPtr<LC::IMessageDisplayResult> messageResult;
		RETURN_IF_FAILED(factory->CreateMessageDisplayResult(redirectResult, &messageResult)); // 744

		// WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult> result = completion.GetResult(); // @Note: This is a copy of `completion`, not a reference, and is set below???
		RETURN_IF_FAILED(completion.GetResult().Set(messageResult.Get())); // 747

		completion.Complete(S_OK);
	}
	else
	{
		RETURN_IF_FAILED(m_redirectionManager->OnBeginPainting()); // 754
		RETURN_IF_FAILED(ShowMessageView(caption, message, messageBoxFlags, completion)); // 755
	}

	completeOnFailure.release();
	return S_OK;
}

HRESULT LogonViewManager::DisplayCredentialErrorUIThread(
	NTSTATUS ntsStatus, NTSTATUS ntsSubStatus, UINT messageBoxFlags, HSTRING caption, HSTRING message,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion)
{
	auto completeOnFailure = wil::scope_exit([&completion]() -> void { completion.Complete(E_UNEXPECTED); });

	if (m_lastReportResultInfo.Get())
	{
		LCPD::CredentialProviderStatusIcon statusIcon;
		RETURN_IF_FAILED(m_lastReportResultInfo->get_StatusIcon(&statusIcon)); // 775
	}

	m_lastReportResultInfo.Reset();

	UINT redirectResult;
	LC::LogonErrorRedirectorResponse errorResponse;
	RETURN_IF_FAILED(m_redirectionManager->RedirectLogonError(
		ntsStatus, ntsSubStatus, caption, message, messageBoxFlags, &redirectResult, &errorResponse)); // 796

	if (errorResponse == LC::LogonErrorRedirectorResponse_HandledDoNotShowLocally
		|| errorResponse == LC::LogonErrorRedirectorResponse_HandledDoNotShowLocallyStartOver)
	{
		ComPtr<LC::IMessageDisplayResultFactory> factory;
		RETURN_IF_FAILED(WF::GetActivationFactory(
			Wrappers::HStringReference(RuntimeClass_Windows_Internal_UI_Logon_Controller_MessageDisplayResult).Get(), &factory)); // 804

		ComPtr<LC::IMessageDisplayResult> messageResult;
		RETURN_IF_FAILED(factory->CreateMessageDisplayResult(redirectResult, &messageResult)); // 807

		// WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult> result = completion.GetResult(); // @Note: This is a copy of `completion`, not a reference, and is set below???
		RETURN_IF_FAILED(completion.GetResult().Set(messageResult.Get())); // 810

		completion.Complete(S_OK);
	}
	else
	{
		RETURN_IF_FAILED(m_redirectionManager->OnBeginPainting()); // 816
		RETURN_IF_FAILED(ShowMessageView(caption, message, messageBoxFlags, completion)); // 817
	}

	completeOnFailure.release();
	return S_OK;
}

HRESULT LogonViewManager::ClearCredentialStateUIThread()
{
	if (m_credProvDataModel.Get())
	{
		RETURN_IF_FAILED(m_credProvDataModel->ClearState()); // 676
		m_isCredentialResetRequired = true;
		m_cachedSerialization.Reset();
		m_showCredentialViewOnInitComplete = false;
	}

	return S_OK;
}

HRESULT LogonViewManager::WebDialogDisplayedUIThread(LC::IWebDialogDismissTrigger* dismissTrigger)
{
	m_webDialogDismissTrigger = dismissTrigger;

	if (m_selectedGroup.Get())
	{
		ComPtr<LCPD::ICredential> credential;
		RETURN_IF_FAILED(m_selectedGroup->get_SelectedCredential(&credential)); // 907

		if (credential.Get())
		{
			RETURN_IF_FAILED(credential->put_IsWebDialogVisible(true)); // 911
		}
	}

	return S_OK;
}

HRESULT LogonViewManager::CleanupUIThread(WI::AsyncDeferral<WI::CNoResult> completion)
{
	m_bioFeedbackListener.Reset();

	if (m_credProvDataModel.Get())
	{
		RETURN_IF_FAILED(m_credProvDataModel->remove_BioFeedbackStateChange(m_bioFeedbackStateChangeToken)); // 830
		RETURN_IF_FAILED(m_credProvDataModel->remove_SerializationComplete(m_serializationCompleteToken)); // 831

		ComPtr<WFC::IObservableVector<IInspectable*>> usersAndV1Creds;
		RETURN_IF_FAILED(m_credProvDataModel->get_UsersAndV1Credentials(&usersAndV1Creds)); // 834
		RETURN_IF_FAILED(usersAndV1Creds->remove_VectorChanged(m_usersChangedToken)); // 835
		RETURN_IF_FAILED(m_credProvDataModel->remove_SelectedUserChanged(m_selectedUserChangeToken)); // 836
	}

	m_inputSwitchControl.Reset();

	completion.Complete(S_OK);
	return S_OK;
}

HRESULT LogonViewManager::ShowCredentialView()
{
	if (m_selectedCredential.Get() && m_webDialogVisibilityChangedToken.value)
	{
		RETURN_IF_FAILED(m_selectedCredential->remove_WebDialogVisibilityChanged(m_webDialogVisibilityChangedToken));
		m_webDialogVisibilityChangedToken.value = 0;
	}

	//if (m_unlockTrigger.Get())
	//	m_unlockTrigger->TriggerUnlock();

	auto zoomedTile = CLogonFrame::GetSingleton()->m_LogonUserList->GetZoomedTile();
	if (zoomedTile)
	{
		CLogonFrame::GetSingleton()->m_LogonUserList->UnzoomList(zoomedTile);
	}

	CLogonFrame::GetSingleton()->m_LogonUserList->DestroyAllTiles();

	//if (!GetSystemMetrics(SM_REMOTESESSION) && m_currentReason == LC::LogonUIRequestReason_LogonUIUnlock)
	//{
	//	RETURN_IF_FAILED(m_userSettingManager->put_IsLockScreenAllowed(FALSE));
	//	WTSDisconnectSession(nullptr, WTS_CURRENT_SESSION, FALSE);
	//}

	if (m_credentialsChangedToken.value)
	{
		ComPtr<WFC::IObservableVector<LCPD::Credential*>> credentials;
		RETURN_IF_FAILED(m_selectedGroup->get_Credentials(&credentials)); // 852
		RETURN_IF_FAILED(credentials->remove_VectorChanged(m_credentialsChangedToken)); // 853
		m_credentialsChangedToken.value = 0;
	}

	if (m_selectedCredentialChangedToken.value)
	{
		RETURN_IF_FAILED(m_selectedGroup->remove_SelectedCredentialChanged(m_selectedCredentialChangedToken)); // 859
		m_selectedCredentialChangedToken.value = 0;
	}

	ComPtr<WFC::IObservableVector<IInspectable*>> observableUsersAndCreds;
	RETURN_IF_FAILED(m_credProvDataModel->get_UsersAndV1Credentials(&observableUsersAndCreds));

	ComPtr<WFC::IVector<IInspectable*>> usersAndCreds;
	RETURN_IF_FAILED(observableUsersAndCreds.As(&usersAndCreds));

	UINT size = 0;
	RETURN_IF_FAILED(usersAndCreds->get_Size(&size));

	for (UINT i = 0; i < size; i++)
	{
		ComPtr<IInspectable> data;
		RETURN_IF_FAILED(usersAndCreds->GetAt(i, &data));

		//ComPtr<LCPD::ICredential> selectedCred;
		if (SUCCEEDED(data.As(&m_selectedCredential)))
		{
			Wrappers::HString label;
			RETURN_IF_FAILED(m_selectedCredential->get_LogoLabel(label.ReleaseAndGetAddressOf()));

			LOG_HR_MSG(E_FAIL,"added credential %s\n", label.GetRawBuffer(nullptr));
			CLogonFrame::GetSingleton()->m_LogonUserList->AddTileFromData(m_selectedCredential,nullptr,label);

			continue;
		}
		m_selectedCredential.Reset();

		ComPtr<LCPD::IUser> user;
		RETURN_IF_FAILED(data.As(&user));

		Wrappers::HString userName;
		RETURN_IF_FAILED(user->get_DisplayName(userName.ReleaseAndGetAddressOf()));

		ComPtr<LCPD::ICredentialGroup> credGroup;
		RETURN_IF_FAILED(user.As(&credGroup)); // 875


		RETURN_IF_FAILED(credGroup->get_SelectedCredential(&m_selectedCredential));
		if (m_selectedCredential.Get() == nullptr)
		{
			ComPtr<WFC::IObservableVector<LCPD::Credential*>> obcredentials;
			RETURN_IF_FAILED(credGroup->get_Credentials(&obcredentials));

			ComPtr<WFC::IVector<LCPD::Credential*>> credentials;
			RETURN_IF_FAILED(obcredentials.As(&credentials));

			UINT credsize;
			RETURN_IF_FAILED(credentials->get_Size(&credsize));

			for (UINT x = 0; x < credsize; x++)
			{
				ComPtr<LCPD::ICredential> cred;
				RETURN_IF_FAILED(credentials->GetAt(i, &cred));

				GUID guid;
				RETURN_IF_FAILED(cred->get_ProviderId(&guid));

				if (guid == CLSID_PasswordCredentialProvider)
				{
					m_selectedCredential = cred;
					credGroup->put_SelectedCredential(m_selectedCredential.Get());
					break;
				}
			}
			LOG_HR_MSG(E_FAIL,"DEFAULTING TO PASSWORD CRED for %s\n", userName.GetRawBuffer(nullptr));
		}

		RETURN_HR_IF_NULL_MSG(E_FAIL,m_selectedCredential.Get(),"FAILED TO GET CREDENTIAL FOR USER");

		LOG_HR_MSG(E_FAIL,"selected Cred for %s\n", userName.GetRawBuffer(nullptr));
		CLogonFrame::GetSingleton()->m_LogonUserList->AddTileFromData(m_selectedCredential,user,userName);

	}

	ComPtr<IInspectable> selectedUserOrCred;
	RETURN_IF_FAILED(m_credProvDataModel->get_SelectedUserOrV1Credential(&selectedUserOrCred));

	CLogonFrame::GetSingleton()->SwitchToUserList(CLogonFrame::GetSingleton()->m_LogonUserList);

	if (selectedUserOrCred.Get())
	{
		ComPtr<LCPD::IUser> selectedUser;
		//ComPtr<LCPD::ICredential> selectedCredential;
		Wrappers::HString userName;
		if (SUCCEEDED(selectedUserOrCred.As(&selectedUser)))
		{
			Wrappers::HString qualifiedUserName;
			RETURN_IF_FAILED(selectedUser->get_QualifiedUsername(qualifiedUserName.ReleaseAndGetAddressOf()));
			RETURN_IF_FAILED(m_userSettingManager->put_UserSid(qualifiedUserName.Get()));

			Wrappers::HString currentInputProfile;
			RETURN_IF_FAILED(m_userSettingManager->get_CurrentInputProfile(currentInputProfile.ReleaseAndGetAddressOf()));
			if (currentInputProfile.Get())
			{
				LOG_IF_FAILED(m_inputSwitchControl->ActivateInputProfile(currentInputProfile.GetRawBuffer(nullptr)));
			}

			RETURN_IF_FAILED(selectedUser->get_DisplayName(userName.ReleaseAndGetAddressOf())); // 873

			RETURN_IF_FAILED(selectedUser.As(&m_selectedGroup)); // 875

			ComPtr<WFC::IObservableVector<LCPD::Credential*>> credentials;
			RETURN_IF_FAILED(m_selectedGroup->get_Credentials(&credentials)); // 878
			RETURN_IF_FAILED(credentials->add_VectorChanged(this, &m_credentialsChangedToken)); // 879

			RETURN_IF_FAILED(m_selectedGroup->add_SelectedCredentialChanged(this, &m_selectedCredentialChangedToken)); // 881
			LOG_HR_MSG(E_FAIL,"called add_SelectedCredentialChanged");

			RETURN_IF_FAILED(m_selectedGroup->get_SelectedCredential(&m_selectedCredential)); // 883
		}
		else
		{
			m_selectedGroup.Reset();

			RETURN_IF_FAILED(selectedUserOrCred.As(&m_selectedCredential)); // 889
			RETURN_IF_FAILED(m_selectedCredential->get_LogoLabel(userName.ReleaseAndGetAddressOf())); // 890

			if (!userName.Get())
			{
				CoTaskMemNativeString defaultV1Label;
				RETURN_IF_FAILED(defaultV1Label.Initialize(HINST_THISCOMPONENT, IDS_USER)); // 895
				RETURN_IF_FAILED(defaultV1Label.Get() ? userName.Set(defaultV1Label.Get()) : E_POINTER); // 896
			}
		}

		LOG_HR_MSG(E_FAIL,"there is a selectedUserOrCred %s , we should zoom it!\n", userName.GetRawBuffer(nullptr));

		auto tileToZoom = CLogonFrame::GetSingleton()->m_LogonUserList->FindTileByCredential(m_selectedCredential);

		RETURN_HR_IF_NULL_MSG(E_FAIL,tileToZoom,"FAILED TO FIND TILE FOR SELECTEDCREDENTIAL");

		CLogonFrame::GetSingleton()->m_LogonUserList->ZoomTile(tileToZoom);

		ComPtr<LCPD::IOptionalDependencyProvider> optionalDependencyProvider;
		RETURN_IF_FAILED(MakeAndInitialize<OptionalDependencyProvider>(&optionalDependencyProvider, m_currentReason, m_autoLogonManager.Get(), m_userSettingManager.Get(), m_displayStateProvider.Get())); // 1084

		ComPtr<LCPD::ICredProvDefaultSelector> defaultSelector;
		RETURN_IF_FAILED(optionalDependencyProvider->GetOptionalDependency(LCPD::OptionalDependencyKind_DefaultSelector,&defaultSelector));

		BOOLEAN bautosubmit = false;
		if (selectedUser)
			RETURN_IF_FAILED(defaultSelector->AllowAutoSubmitOnSelection(selectedUser.Get(),&bautosubmit));

		BOOLEAN bIsLocalNoPassword = false;
		if (selectedUser)
			RETURN_IF_FAILED(selectedUser->get_IsLocalNoPasswordUser(&bIsLocalNoPassword));

		if (bautosubmit && bIsLocalNoPassword && m_currentReason != LC::LogonUIRequestReason_LogonUIChange)
			m_selectedCredential->Submit();

		m_currentViewType = LogonView::SelectedCredential;

		return S_OK;
	}

	m_currentViewType = LogonView::UserSelection;

	return S_OK;
}

HRESULT LogonViewManager::ShowStatusView(HSTRING status)
{
	CLogonFrame::GetSingleton()->ShowStatusMessage(WindowsGetStringRawBuffer(status, nullptr));
	
	m_currentViewType = LogonView::Status;
	return S_OK;
}

HRESULT LogonViewManager::ShowMessageView(
	HSTRING caption, HSTRING message, UINT messageBoxFlags,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion)
{
	CLogonFrame::GetSingleton()->DisplayLogonDialog(WindowsGetStringRawBuffer(caption,nullptr),WindowsGetStringRawBuffer(message,nullptr),messageBoxFlags,completion);

	m_currentViewType = LogonView::Message;
	return S_OK;
}

HRESULT LogonViewManager::ShowSerializationFailedView(HSTRING caption, HSTRING message)
{
	CLogonFrame::GetSingleton()->DisplayLogonDialog(WindowsGetStringRawBuffer(caption,nullptr),WindowsGetStringRawBuffer(message,nullptr),16 | (int)MessageOptionFlag::Ok);

	m_currentViewType = LogonView::SerializationFailed;
	return S_OK;
}

HRESULT LogonViewManager::StartCredProvsIfNecessary(LC::LogonUIRequestReason reason, BOOLEAN allowDirectUserSwitching, HSTRING unk)
{
	LCPD::CredProvScenario scenario = LCPD::CredProvScenario_Logon;
	if (reason == LC::LogonUIRequestReason_LogonUIUnlock)
	{
		scenario = allowDirectUserSwitching ? LCPD::CredProvScenario_Logon : LCPD::CredProvScenario_Unlock;
	}
	else if (reason == LC::LogonUIRequestReason_LogonUIChange)
	{
		scenario = LCPD::CredProvScenario_ChangePassword;
	}

	if (m_credProvDataModel.Get())
	{
		if (m_isCredentialResetRequired)
		{
			ComPtr<WF::IAsyncAction> resetAction;
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
			RETURN_IF_FAILED(m_credProvDataModel->ResetAsync(scenario,LCPD::SupportedFeatureFlags_0, nullptr, &resetAction)); // 1124
#else
			RETURN_IF_FAILED(m_credProvDataModel->ResetAsync(scenario, LCPD::SupportedFeatureFlags_0, &resetAction)); // 1124
#endif
			m_credProvInitialized = false;

			ComPtr<LogonViewManager> thisRef = this;

			HRESULT hr = StartOperationAndThen<WF::IAsyncActionCompletedHandler>(resetAction.Get(), [thisRef, this](HRESULT hrAction, WF::IAsyncAction* asyncOp) -> HRESULT
			{
				UNREFERENCED_PARAMETER(thisRef);
				auto completeOnFailure = wil::scope_exit([this]() -> void
				{
					if (m_requestCredentialsComplete)
						m_requestCredentialsComplete->Complete(E_UNEXPECTED);
				});

				if (SUCCEEDED(hrAction))
				{
					m_credProvInitialized = true;
					hrAction = ShowCredentialView();
				}

				RETURN_IF_FAILED(hrAction); // 1147
				m_isCredentialResetRequired = false;
				completeOnFailure.release();
				return S_OK;
			});
			RETURN_IF_FAILED(hr); // 1147
		}
		else if (m_credProvInitialized)
		{
			if (m_showCredentialViewOnInitComplete)
			{
				RETURN_IF_FAILED(ShowCredentialView()); // 1153
			}

			ComPtr<LCPD::IUser> selectedUser;
			RETURN_IF_FAILED(m_credProvDataModel->get_SelectedUser(&selectedUser)); // 1157

			if (selectedUser.Get())
			{
				ComPtr<LCPD::ICredentialGroup> selectedCredentialGroup;
				RETURN_IF_FAILED(selectedUser.As(&selectedCredentialGroup)); // 1166
				RETURN_IF_FAILED(selectedCredentialGroup->RefreshSelection()); // 1167
			}
		}

		return S_OK;
	}

	ComPtr<LCPD::IUIThreadEventDispatcher> eventDispatcher;
	RETURN_IF_FAILED(MakeAndInitialize<EventDispatcher>(&eventDispatcher, m_Dispatcher.Get())); // 1081

	ComPtr<LCPD::IOptionalDependencyProvider> optionalDependencyProvider;
	RETURN_IF_FAILED(MakeAndInitialize<OptionalDependencyProvider>(&optionalDependencyProvider, reason, m_autoLogonManager.Get(), m_userSettingManager.Get(), m_displayStateProvider.Get())); // 1084

	//@MOD, dont think this is needed
	ComPtr<LCPD::ITelemetryDataProvider> telemetryProvider;
	RETURN_IF_FAILED(m_userSettingManager->get_TelemetryDataProvider(&telemetryProvider)); // 1087;

	ComPtr<LCPD::ICredProvDataModelFactory> credProvDataModelFactory;
	RETURN_IF_FAILED(WF::GetActivationFactory(Wrappers::HStringReference(RuntimeClass_Windows_Internal_UI_Logon_CredProvData_CredProvDataModel).Get(), &credProvDataModelFactory)); // 1090
	RETURN_IF_FAILED(credProvDataModelFactory->CreateCredProvDataModel(LCPD::SelectionMode_PLAP, eventDispatcher.Get(), optionalDependencyProvider.Get(), &m_credProvDataModel)); // 1091

	RETURN_IF_FAILED(m_credProvDataModel->add_SerializationComplete(this, &m_serializationCompleteToken)); // 1093
	RETURN_IF_FAILED(m_credProvDataModel->add_BioFeedbackStateChange(this, &m_bioFeedbackStateChangeToken)); // 1094

	ComPtr<WF::IAsyncAction> initAction;
	LANGID langID = 0;
	RETURN_IF_FAILED(m_userSettingManager->get_LangID(&langID)); // 1098
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
	RETURN_IF_FAILED(m_credProvDataModel->InitializeAsync(scenario, LCPD::SupportedFeatureFlags_0, langID, unk, &initAction)); // 1099
#else
	RETURN_IF_FAILED(m_credProvDataModel->InitializeAsync(scenario, LCPD::SupportedFeatureFlags_0, langID, &initAction)); // 1099
#endif

	ComPtr<LogonViewManager> thisRef = this;

	HRESULT hr = StartOperationAndThen<WF::IAsyncActionCompletedHandler>(initAction.Get(), [thisRef, this](HRESULT hrAction, WF::IAsyncAction* asyncOp) -> HRESULT
	{
		UNREFERENCED_PARAMETER(thisRef);
		auto completeOnFailure = wil::scope_exit([this]() -> void
		{
			if (m_requestCredentialsComplete)
				m_requestCredentialsComplete->Complete(E_UNEXPECTED);
		});

		if (SUCCEEDED(hrAction))
		{
			m_credProvInitialized = true;
			hrAction = OnCredProvInitComplete();
		}

		RETURN_IF_FAILED(hrAction); // 1119
		completeOnFailure.release();
		return S_OK;
	});
	RETURN_IF_FAILED(hr); // 1119
	return S_OK;
}

HRESULT LogonViewManager::OnCredProvInitComplete()
{
	ComPtr<WFC::IObservableVector<IInspectable*>> usersAndV1Creds;
	RETURN_IF_FAILED(m_credProvDataModel->get_UsersAndV1Credentials(&usersAndV1Creds)); // 1177
	RETURN_IF_FAILED(usersAndV1Creds->add_VectorChanged(this, &m_usersChangedToken)); // 1178
	RETURN_IF_FAILED(m_credProvDataModel->add_SelectedUserOrV1CredentialChanged(this, &m_selectedUserChangeToken)); // 1179
	if (m_showCredentialViewOnInitComplete)
	{
		RETURN_IF_FAILED(ShowCredentialView()); // 1182
	}

	return S_OK;
}
