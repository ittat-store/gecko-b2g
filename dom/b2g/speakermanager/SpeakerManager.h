/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeakerManager_h
#define mozilla_dom_SpeakerManager_h

#include "nsIDOMEventListener.h"
#include "mozilla/dom/MozSpeakerManagerBinding.h"
#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {
/* This class is used for UA to control devices's speaker status.
 * After UA set the speaker status, the UA should handle the
 * forcespeakerchange event and change the speaker status in UI.
 * The device's speaker status would set back to normal when UA close the
 * application.
 */
class SpeakerManager final : public DOMEventTargetHelper,
                             public nsIDOMEventListener {
  friend class SpeakerManagerService;
  friend class SpeakerManagerServiceChild;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMEVENTLISTENER

 public:
  nsresult Init(nsPIDOMWindowInner* aWindow);

  nsPIDOMWindowInner* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  /**
   * WebIDL Interface
   */
  // Get this api's force speaker setting.
  bool Forcespeaker();
  // Force acoustic sound go through speaker. Don't force to speaker if
  // application stay in the background and re-force when application go to
  // foreground
  void SetForcespeaker(bool aEnable);
  // Get the device's speaker forced setting.
  bool Speakerforced();

  void SetAudioChannelActive(bool aIsActive);

  uint64_t WindowID() { return mWindow ? mWindow->WindowID() : 0; }

  IMPL_EVENT_HANDLER(speakerforcedchange)

  static already_AddRefed<SpeakerManager> Constructor(
      const GlobalObject& aGlobal, const Optional<SpeakerPolicy>& aPolicy,
      ErrorResult& aRv);

 protected:
  explicit SpeakerManager(SpeakerPolicy aPolicy);
  ~SpeakerManager();
  void DispatchSimpleEvent(const nsAString& aStr);
  void UpdateStatus();
  static bool HasPermission(nsPIDOMWindowInner* aWindow);

  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
  // This api's force speaker setting
  bool mForcespeaker;
  bool mVisible;
  bool mAudioChannelActive;
  SpeakerPolicy mPolicy;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SpeakerManager_h
