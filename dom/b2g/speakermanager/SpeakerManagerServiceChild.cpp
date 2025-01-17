/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeakerManagerServiceChild.h"

#include "AudioChannelService.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/StaticPtr.h"
#include "nsThreadUtils.h"

#include <cutils/properties.h>

using namespace mozilla;
using namespace mozilla::dom;

StaticRefPtr<SpeakerManagerServiceChild> gSpeakerManagerServiceChild;

/* static */
SpeakerManagerService*
SpeakerManagerServiceChild::GetOrCreateSpeakerManagerService() {
  MOZ_ASSERT(NS_IsMainThread());

  // If we already exist, exit early
  if (gSpeakerManagerServiceChild) {
    return gSpeakerManagerServiceChild;
  }

  // Create new instance, register, return
  RefPtr<SpeakerManagerServiceChild> service = new SpeakerManagerServiceChild();

  gSpeakerManagerServiceChild = service;

  return gSpeakerManagerServiceChild;
}

/* static */
SpeakerManagerService* SpeakerManagerServiceChild::GetSpeakerManagerService() {
  MOZ_ASSERT(NS_IsMainThread());

  return gSpeakerManagerServiceChild;
}

void SpeakerManagerServiceChild::ForceSpeaker(bool aEnable, bool aVisible,
                                              bool aChannelActive,
                                              uint64_t aWindowID,
                                              uint64_t aChildID) {
  mOrgSpeakerStatus = aEnable;
  ContentChild* cc = ContentChild::GetSingleton();
  if (cc) {
    cc->SendSpeakerManagerForceSpeaker(aEnable, aVisible, aChannelActive,
                                       aWindowID);
  }
}

bool SpeakerManagerServiceChild::GetSpeakerStatus() {
  ContentChild* cc = ContentChild::GetSingleton();
  bool status = false;
  if (cc) {
    cc->SendSpeakerManagerGetSpeakerStatus(&status);
  }
  char propQemu[PROPERTY_VALUE_MAX];
  property_get("ro.kernel.qemu", propQemu, "");
  if (!strncmp(propQemu, "1", 1)) {
    return mOrgSpeakerStatus;
  }
  return status;
}

void SpeakerManagerServiceChild::Shutdown() {
  if (gSpeakerManagerServiceChild) {
    gSpeakerManagerServiceChild = nullptr;
  }
}

void SpeakerManagerServiceChild::SetAudioChannelActive(bool aIsActive) {
  // Content process and switch to background with no audio and speaker forced.
  // Then disable speaker
  for (auto iter = mRegisteredSpeakerManagers.Iter(); !iter.Done();
       iter.Next()) {
    RefPtr<SpeakerManager> sm = iter.Data();
    sm->SetAudioChannelActive(aIsActive);
  }
}

SpeakerManagerServiceChild::SpeakerManagerServiceChild() {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<AudioChannelService> audioChannelService =
      AudioChannelService::GetOrCreate();
  if (audioChannelService) {
    audioChannelService->RegisterSpeakerManager(this);
  }
}

SpeakerManagerServiceChild::~SpeakerManagerServiceChild() {
  RefPtr<AudioChannelService> audioChannelService =
      AudioChannelService::GetOrCreate();
  if (audioChannelService) {
    audioChannelService->UnregisterSpeakerManager(this);
  }
}

void SpeakerManagerServiceChild::Notify() {
  for (auto iter = mRegisteredSpeakerManagers.Iter(); !iter.Done();
       iter.Next()) {
    RefPtr<SpeakerManager> sm = iter.Data();
    sm->DispatchSimpleEvent(u"speakerforcedchange"_ns);
  }
}
