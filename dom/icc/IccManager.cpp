/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IccManager.h"
#include "mozilla/dom/IccManagerBinding.h"
#include "Icc.h"
#include "IccListener.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/IccChangeEvent.h"
#include "mozilla/Preferences.h"
#include "nsIIccInfo.h"
// Service instantiation
#include "ipc/IccIPCService.h"
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
#  include "nsIGonkIccService.h"
#endif
#include "nsXULAppAPI.h"  // For XRE_GetProcessType()

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(IccManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IccManager,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IccManager,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

// QueryInterface implementation for IccManager
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IccManager)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IccManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IccManager, DOMEventTargetHelper)

IccManager::IccManager(nsIGlobalObject* aGlobal)
    : DOMEventTargetHelper(aGlobal) {
  uint32_t numberOfServices =
      mozilla::Preferences::GetUint("ril.numRadioInterfaces", 1);

  for (uint32_t i = 0; i < numberOfServices; i++) {
    RefPtr<IccListener> iccListener = new IccListener(this, i);
    mIccListeners.AppendElement(iccListener);
  }
}

IccManager::~IccManager() { Shutdown(); }

JSObject* IccManager::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return IccManager_Binding::Wrap(aCx, this, aGivenProto);
}

void IccManager::Shutdown() {
  for (uint32_t i = 0; i < mIccListeners.Length(); i++) {
    mIccListeners[i]->Shutdown();
    mIccListeners[i] = nullptr;
  }
  mIccListeners.Clear();
}

nsresult IccManager::NotifyIccAdd(const nsAString& aIccId) {
  IccManager_Binding::ClearCachedIccIdsValue(this);

  IccChangeEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mIccId = aIccId;

  RefPtr<IccChangeEvent> event =
      IccChangeEvent::Constructor(this, u"iccdetected"_ns, init);
  event->SetTrusted(true);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, event);

  return asyncDispatcher->PostDOMEvent();
}

nsresult IccManager::NotifyIccRemove(const nsAString& aIccId) {
  IccManager_Binding::ClearCachedIccIdsValue(this);

  IccChangeEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mIccId = aIccId;

  RefPtr<IccChangeEvent> event =
      IccChangeEvent::Constructor(this, u"iccundetected"_ns, init);
  event->SetTrusted(true);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, event);

  return asyncDispatcher->PostDOMEvent();
}

// IccManager

void IccManager::GetIccIds(nsTArray<nsString>& aIccIds) {
  nsTArray<RefPtr<IccListener>>::size_type i;
  for (i = 0; i < mIccListeners.Length(); ++i) {
    Icc* icc = mIccListeners[i]->GetIcc();
    if (icc) {
      aIccIds.AppendElement(icc->GetIccId());
    }
  }
}

Icc* IccManager::GetIccById(const nsAString& aIccId) const {
  nsTArray<RefPtr<IccListener>>::size_type i;
  for (i = 0; i < mIccListeners.Length(); ++i) {
    Icc* icc = mIccListeners[i]->GetIcc();
    if (icc && aIccId == icc->GetIccId()) {
      return icc;
    }
  }
  return nullptr;
}

already_AddRefed<nsIIccService> NS_CreateIccService() {
  nsCOMPtr<nsIIccService> service;

  if (XRE_IsContentProcess()) {
    service = new mozilla::dom::icc::IccIPCService();
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
  } else {
    service = do_GetService(GONK_ICC_SERVICE_CONTRACTID);
#endif
  }

  return service.forget();
}
