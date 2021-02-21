/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
 * reserved. Copyright 2013 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if ANDROID_VERSION < 29
#  error "Only ANDROID_VERSION >= 29 devices are supported"
#endif

#include <gui/Surface.h>
#include <gui/IProducerListener.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <hardware/power.h>
#include <suspend/autosuspend.h>

#include "cutils/properties.h"
#include "FramebufferSurface.h"
#include "GonkDisplayP.h"

#ifdef LOG_TAG
#  undef LOG_TAG
#  define LOG_TAG "GonkDisplay"
#endif

#define DEFAULT_XDPI 75.0
// This define should be passed from gonk-misc and depends on device config.
// #define GET_FRAMEBUFFER_FORMAT_FROM_HWC

using namespace android;

/* global variables */
std::mutex hotplugMutex;
std::condition_variable hotplugCv;

typedef mozilla::GonkDisplay::GonkDisplayVsyncCBFun GonkDisplayVsyncCBFun;
typedef mozilla::GonkDisplay::GonkDisplayInvalidateCBFun
    GonkDisplayInvalidateCBFun;

class HWComposerCallback : public HWC2::ComposerCallback {
 public:
  HWComposerCallback(HWC2::Device* device);

  void onVsyncReceived(int32_t sequenceId, hwc2_display_t display,
                       int64_t timestamp) override;

  void onHotplugReceived(int32_t sequenceId, hwc2_display_t display,
                         HWC2::Connection connection) override;

  void onRefreshReceived(int32_t sequenceId, hwc2_display_t display) override;

 private:
  HWC2::Device* hwcDevice;
};

HWComposerCallback::HWComposerCallback(HWC2::Device* device) {
  hwcDevice = device;
}

void HWComposerCallback::onVsyncReceived(int32_t sequenceId,
                                         hwc2_display_t display,
                                         int64_t timestamp) {
  // ALOGI("onVsyncReceived(%d, %" PRIu64 ", %" PRId64 ")",
  //        sequenceId, display,timestamp);
  (void)sequenceId;

  GonkDisplayVsyncCBFun func = mozilla::GetGonkDisplay()->getVsyncCallBack();
  if (func) {
    func(display, timestamp);
  }
}

void HWComposerCallback::onHotplugReceived(int32_t sequenceId,
                                           hwc2_display_t display,
                                           HWC2::Connection connection) {
  {
    std::lock_guard<std::mutex> lock(hotplugMutex);
    ALOGI("HWComposerCallback::onHotplugReceived %d %llu %d", sequenceId,
          (unsigned long long)display, (uint32_t)connection);
    hwcDevice->onHotplug(display, connection);
  }

  hotplugCv.notify_all();
}

void HWComposerCallback::onRefreshReceived(int32_t sequenceId,
                                           hwc2_display_t display) {
  ALOGI("onRefreshReceived(%d, %" PRIu64 ")", sequenceId, display);

  GonkDisplayInvalidateCBFun func =
      mozilla::GetGonkDisplay()->getInvalidateCallBack();
  if (func) {
    func();
  }
}

std::shared_ptr<const HWC2::Display::Config> getActiveConfig(
    HWC2::Display* hwcDisplay, int32_t displayId) {
  std::shared_ptr<const HWC2::Display::Config> config;
  auto error = hwcDisplay->getActiveConfig(&config);
  if (error == HWC2::Error::BadConfig) {
    fprintf(stderr, "getActiveConfig: No config active, returning null");
    return nullptr;
  } else if (error != HWC2::Error::None) {
    fprintf(stderr, "getActiveConfig failed for display %d: %s (%d)", displayId,
            to_string(error).c_str(), static_cast<int32_t>(error));
    return nullptr;
  } else if (!config) {
    fprintf(stderr, "getActiveConfig returned an unknown config for display %d",
            displayId);
    return nullptr;
  }

  return config;
}

// ----------------------------------------------------------------------------
namespace mozilla {
// ----------------------------------------------------------------------------

static GonkDisplayP* sGonkDisplay = nullptr;
static android::Mutex sMutex;

GonkDisplayP::GonkDisplayP()
    : mHwc(nullptr),
      mFBDevice(nullptr),
      mExtFBDevice(nullptr),
      mPowerModule(nullptr),
      mList(nullptr),
      mEnabledCallback(nullptr),
      mEnableHWCPower(false),
      mFBEnabled(
          true)  // Initial value should sync with hal::GetScreenEnabled()
      ,
      mExtFBEnabled(
          true)  // Initial value should sync with hal::GetExtScreenEnabled()
      ,
      mHwcDisplay(nullptr) {
  std::string serviceName = "default";
  mHwc = std::make_unique<HWC2::Device>(
      std::make_unique<Hwc2::impl::Composer>(serviceName));
  assert(mHwc);
  mHwc->registerCallback(1);

  std::unique_lock<std::mutex> lock(hotplugMutex);
  HWC2::Display* hwcDisplay;
  while (!(hwcDisplay = mHwc->getDisplayById(1))) {
    /* Wait at most 5s for hotplug events */
    hotplugCv.wait_for(lock, std::chrono::seconds(5));
  }
  hotplugMutex.unlock();
  assert(hwcDisplay);

  (void)hwcDisplay->setPowerMode(HWC2::PowerMode::On);
  mHwcDisplay = hwcDisplay;

  std::shared_ptr<const HWC2::Display::Config> config;
  config = getActiveConfig(hwcDisplay, 0);

  char lcd_density_str[PROPERTY_VALUE_MAX] = "0";
  property_get("ro.sf.lcd_density", lcd_density_str, "0");
  int lcd_dnsity = atoi(lcd_density_str);

  mEnableHWCPower = property_get_bool("persist.hwc.powermode", false);

  ALOGI("width: %i, height: %i, dpi: %f, lcd: %d\n", config->getWidth(),
        config->getHeight(), config->getDpiX(), lcd_dnsity);

  DisplayNativeData& dispData =
      mDispNativeData[(uint32_t)DisplayType::DISPLAY_PRIMARY];
  if (config->getWidth() > 0) {
    dispData.mWidth = config->getWidth();
    dispData.mHeight = config->getHeight();
    dispData.mXdpi = (lcd_dnsity > 0) ? lcd_dnsity : config->getDpiX();
    /* The emulator actually reports RGBA_8888, but EGL doesn't return
     * any matching configuration. We force RGBX here to fix it. */
    /*TODO: need to discuss with vendor to check this format issue.*/
    dispData.mSurfaceformat = HAL_PIXEL_FORMAT_RGBA_8888;
  }
  (void)hwcDisplay->createLayer(&mlayer);

  Rect r = {0, 0, config->getWidth(), config->getHeight()};
  (void)mlayer->setCompositionType(HWC2::Composition::Client);
  (void)mlayer->setBlendMode(HWC2::BlendMode::None);
  (void)mlayer->setSourceCrop(
      FloatRect(0.0f, 0.0f, config->getWidth(), config->getHeight()));
  (void)mlayer->setDisplayFrame(r);
  (void)mlayer->setVisibleRegion(Region(r));
  (void)mPowerModule;

  ALOGI("created native window\n");
  native_gralloc_initialize(0);

  mPower = IPower::getService();
  if (mPower == nullptr) {
    ALOGE("Can't find IPower service...");
  }

  CreateFramebufferSurface(mSTClient, mDispSurface, config->getWidth(),
                           config->getHeight(), dispData.mSurfaceformat,
                           hwcDisplay, mlayer, nullptr);

  (void)hwcDisplay->createLayer(&mlayerBootAnim);
  (void)mlayerBootAnim->setCompositionType(HWC2::Composition::Client);
  (void)mlayerBootAnim->setBlendMode(HWC2::BlendMode::None);
  (void)mlayerBootAnim->setSourceCrop(
      FloatRect(0.0f, 0.0f, config->getWidth(), config->getHeight()));
  (void)mlayerBootAnim->setDisplayFrame(r);
  (void)mlayerBootAnim->setVisibleRegion(Region(r));

  CreateFramebufferSurface(mBootAnimSTClient, mBootAnimDispSurface,
                           config->getWidth(), config->getHeight(),
                           dispData.mSurfaceformat, hwcDisplay, mlayerBootAnim,
                           nullptr);

  {
    // mBootAnimSTClient is used by CPU directly via GonkDisplayP's
    // dequeueBuffer() / queueBuffer(). We connect it here for use
    // later or it will be failed to queue buffers.
    Surface* surface = static_cast<Surface*>(mBootAnimSTClient.get());
    static sp<IProducerListener> listener = new DummyProducerListener();
    surface->connect(NATIVE_WINDOW_API_CPU, listener);
  }

  uint32_t usage =
      GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_COMPOSER;

  // Set this prop to turn on native framebuffer support for fb1,
  // such as external screen of flip phone.
  // ro.h5.display.fb1_backlightdev=__full_backlight_device_path__
  //
  // ex: Octans will set this prop in device/t2m/octans/octans.mk
  DisplayNativeData& extDispData =
      mDispNativeData[(uint32_t)DisplayType::DISPLAY_EXTERNAL];
  mExtFBDevice = NativeFramebufferDevice::Create();

  if (mExtFBDevice) {
    if (mExtFBDevice->Open()) {
      extDispData.mWidth = mExtFBDevice->mWidth;
      extDispData.mHeight = mExtFBDevice->mHeight;
      extDispData.mSurfaceformat = mExtFBDevice->mSurfaceformat;
      extDispData.mXdpi = mExtFBDevice->mXdpi;

      mExtFBDevice->EnableScreen(true);
      CreateFramebufferSurface(mExtSTClient, mExtDispSurface,
                               extDispData.mWidth, extDispData.mHeight,
                               extDispData.mSurfaceformat, nullptr, nullptr,
                               mExtFBDevice);
      mExtSTClient->perform(mExtSTClient.get(), NATIVE_WINDOW_SET_BUFFER_COUNT,
                            2);
      mExtSTClient->perform(mExtSTClient.get(), NATIVE_WINDOW_SET_USAGE, usage);

      {
        // mExtSTClient is used by CPU directly via GonkDisplayP's
        // dequeueBuffer() / queueBuffer(). We connect it here for use
        // later or it will be failed to queue buffers.
        Surface* surface = static_cast<Surface*>(mExtSTClient.get());
        static sp<IProducerListener> listener = new DummyProducerListener();
        surface->connect(NATIVE_WINDOW_API_CPU, listener);
      }
    } else {
      delete mExtFBDevice;
      mExtFBDevice = nullptr;
    }
  }

  if (!mExtFBDevice) {
    // Set mExtFBEnabled to false if no support externl screen.
    mExtFBEnabled = false;
  }
}

GonkDisplayP::~GonkDisplayP() {
  if (mList) {
    free(mList);
  }
}

void GonkDisplayP::CreateFramebufferSurface(
    sp<ANativeWindow>& aNativeWindow, sp<DisplaySurface>& aDisplaySurface,
    uint32_t aWidth, uint32_t aHeight, unsigned int format,
    HWC2::Display* display, HWC2::Layer* layer,
    NativeFramebufferDevice* ExtFBDevice) {
  sp<IGraphicBufferProducer> producer;
  sp<IGraphicBufferConsumer> consumer;
  BufferQueue::createBufferQueue(&producer, &consumer);

  aDisplaySurface = new FramebufferSurface(aWidth, aHeight, format, consumer,
                                           display, layer, ExtFBDevice);
  aNativeWindow = new android::Surface(producer, true);
}

void GonkDisplayP::CreateVirtualDisplaySurface(
    IGraphicBufferProducer* aSink, sp<ANativeWindow>& aNativeWindow,
    sp<DisplaySurface>& aDisplaySurface) {
  /* TODO: implement VirtualDisplay*/
  (void)aSink;
  (void)aNativeWindow;
  (void)aDisplaySurface;

  /* FIXME: bug 4036, fix the build error in libdisplay
  #if ANDROID_VERSION >= 19
      sp<VirtualDisplaySurface> virtualDisplay;
      virtualDisplay = new VirtualDisplaySurface(-1, aSink, producer, consumer,
  String8("VirtualDisplaySurface")); aDisplaySurface = virtualDisplay;
      aNativeWindow = new Surface(virtualDisplay);
  #endif*/
}

void GonkDisplayP::SetEnabled(bool enabled) {
  android::Mutex::Autolock lock(mPrimaryScreenLock);
  if (enabled) {
    if (!mExtFBEnabled) {
      autosuspend_disable();
      mPower->setInteractive(true);

      if (mHwc && mEnableHWCPower) {
        SetHwcPowerMode(enabled);
      } else if (mFBDevice && mFBDevice->enableScreen) {
        mFBDevice->enableScreen(mFBDevice, enabled);
      }
    }
    mFBEnabled = enabled;

    // enable vsync
    if (mEnabledCallback && !mExtFBEnabled) {
      mEnabledCallback(enabled);
    }
  } else {
    if (mEnabledCallback && !mExtFBEnabled) {
      mEnabledCallback(enabled);
    }

    if (!mExtFBEnabled) {
      if (mHwc && mEnableHWCPower) {
        SetHwcPowerMode(enabled);
      } else if (mFBDevice && mFBDevice->enableScreen) {
        mFBDevice->enableScreen(mFBDevice, enabled);
      }

      autosuspend_enable();
      mPower->setInteractive(false);
    }
    mFBEnabled = enabled;
  }
}

int GonkDisplayP::TryLockScreen() {
  int ret = mPrimaryScreenLock.tryLock();
  return ret;
}

void GonkDisplayP::UnlockScreen() { mPrimaryScreenLock.unlock(); }

void GonkDisplayP::SetExtEnabled(bool enabled) {
  android::Mutex::Autolock lock(mPrimaryScreenLock);
  if (!mExtFBDevice) {
    return;
  }

  if (enabled) {
    if (!mFBEnabled) {
      autosuspend_disable();
      mPower->setInteractive(true);

      SetHwcPowerMode(enabled);
    }
    mExtFBDevice->EnableScreen(enabled);
    mExtFBEnabled = enabled;

    if (mEnabledCallback && !mFBEnabled) {
      mEnabledCallback(enabled);
    }
  } else {
    if (mEnabledCallback && !mFBEnabled) {
      mEnabledCallback(enabled);
    }

    mExtFBDevice->EnableScreen(enabled);
    mExtFBEnabled = enabled;
    if (!mFBEnabled) {
      SetHwcPowerMode(enabled);

      autosuspend_enable();
      mPower->setInteractive(false);
    }
  }
}

HWC2::Error GonkDisplayP::SetHwcPowerMode(bool enabled) {
    HWC2::PowerMode mode =
        (enabled ? HWC2::PowerMode::On : HWC2::PowerMode::Off);
    HWC2::Display* hwcDisplay = mHwc->getDisplayById(HWC_DISPLAY_PRIMARY);

    auto error = hwcDisplay->setPowerMode(mode);
    if (error != HWC2::Error::None) {
      ALOGE(
          "SetHwcPowerMode: Unable to set power mode %s for "
          "display %d: %s (%d)",
          to_string(mode).c_str(), HWC_DISPLAY_PRIMARY,
          to_string(error).c_str(), static_cast<int32_t>(error));
    }

    return error;
}

void GonkDisplayP::OnEnabled(OnEnabledCallbackType callback) {
  mEnabledCallback = callback;
}

void* GonkDisplayP::GetHWCDevice() { return mHwc.get(); }

bool GonkDisplayP::IsExtFBDeviceEnabled() { return !!mExtFBDevice; }

bool GonkDisplayP::SwapBuffers(DisplayType aDisplayType) {
  if (aDisplayType == DisplayType::DISPLAY_PRIMARY) {
    // Should be called when composition rendering is complete for a frame.
    // Only HWC v1.0 needs this call.
    // HWC > v1.0 case, do not call compositionComplete().
    // mFBDevice is present only when HWC is v1.0.

    return Post(mDispSurface->lastHandle, mDispSurface->GetPrevDispAcquireFd(),
                DisplayType::DISPLAY_PRIMARY);
  } else if (aDisplayType == DisplayType::DISPLAY_EXTERNAL) {
    if (mExtFBDevice) {
      return Post(mExtDispSurface->lastHandle,
                  mExtDispSurface->GetPrevDispAcquireFd(),
                  DisplayType::DISPLAY_EXTERNAL);
    }

    return false;
  }

  return false;
}

bool GonkDisplayP::Post(buffer_handle_t buf, int fence,
                        DisplayType aDisplayType) {
  sp<Fence> fenceObj = new Fence(fence);
  if (aDisplayType == DisplayType::DISPLAY_PRIMARY) {
    // UpdateDispSurface(0, EGL_NO_SURFACE);
    return true;
  } else if (aDisplayType == DisplayType::DISPLAY_EXTERNAL) {
    // Only support fb1 for certain device, use hwc to control
    // external screen in general case.
    // update buffer on onFrameAvailable.
    return true;
  }

  return false;
}

ANativeWindowBuffer* GonkDisplayP::DequeueBuffer(DisplayType aDisplayType) {
  // Check for bootAnim or normal display flow.
  sp<ANativeWindow> nativeWindow;
  if (aDisplayType == DisplayType::DISPLAY_PRIMARY) {
    nativeWindow = !mBootAnimSTClient.get() ? mSTClient : mBootAnimSTClient;
  } else if (aDisplayType == DisplayType::DISPLAY_EXTERNAL) {
    if (mExtFBDevice) {
      nativeWindow = mExtSTClient;
    }
  }

  if (!nativeWindow.get()) {
    return nullptr;
  }

  ANativeWindowBuffer* buf;
  int fenceFd = -1;
  nativeWindow->dequeueBuffer(nativeWindow.get(), &buf, &fenceFd);
  sp<Fence> fence(new Fence(fenceFd));
  fence->waitForever("GonkDisplay::DequeueBuffer");
  return buf;
}

bool GonkDisplayP::QueueBuffer(ANativeWindowBuffer* buf,
                               DisplayType aDisplayType) {
  bool success = false;
  int error = DoQueueBuffer(buf, aDisplayType);

  sp<DisplaySurface> displaySurface;
  if (aDisplayType == DisplayType::DISPLAY_PRIMARY) {
    displaySurface =
        !mBootAnimSTClient.get() ? mDispSurface : mBootAnimDispSurface;
  } else if (aDisplayType == DisplayType::DISPLAY_EXTERNAL) {
    if (mExtFBDevice) {
      displaySurface = mExtDispSurface;
    }
  }

  if (!displaySurface.get()) {
    return false;
  }

  success = Post(displaySurface->lastHandle,
                 displaySurface->GetPrevDispAcquireFd(), aDisplayType);

  return error == 0 && success;
}

int GonkDisplayP::DoQueueBuffer(ANativeWindowBuffer* buf,
                                DisplayType aDisplayType) {
  int error = 0;
  sp<ANativeWindow> nativeWindow;
  if (aDisplayType == DisplayType::DISPLAY_PRIMARY) {
    nativeWindow = !mBootAnimSTClient.get() ? mSTClient : mBootAnimSTClient;
  } else if (aDisplayType == DisplayType::DISPLAY_EXTERNAL) {
    if (mExtFBDevice) {
      nativeWindow = mExtSTClient;
    }
  }

  if (!nativeWindow.get()) {
    return error;
  }

  error = nativeWindow->queueBuffer(nativeWindow.get(), buf, -1);

  return error;
}

void GonkDisplayP::UpdateDispSurface(EGLDisplay dpy, EGLSurface sur) {
  // not used now.
}

void GonkDisplayP::NotifyBootAnimationStopped() {
  if (mBootAnimSTClient.get()) {
    ALOGI("[%s] NotifyBootAnimationStopped \n", __func__);
    if (mlayerBootAnim) {
      (void)mHwcDisplay->destroyLayer(mlayerBootAnim);
      mlayerBootAnim = nullptr;
    }

    mBootAnimSTClient = nullptr;
    mBootAnimDispSurface = nullptr;
  }
  if (mExtSTClient.get()) {
    Surface* surface = static_cast<Surface*>(mExtSTClient.get());
    surface->disconnect(NATIVE_WINDOW_API_CPU);
  }
}

void GonkDisplayP::PowerOnDisplay(int displayId) {
  if (mHwc) {
    HWC2::Display* hwcDisplay = mHwc->getDisplayById(displayId);
    auto error = hwcDisplay->setPowerMode(HWC2::PowerMode::On);
    if (error != HWC2::Error::None) {
      ALOGE(
          "setPowerMode: Unable to set power mode %s for "
          "display %d: %s (%d)",
          to_string(HWC2::PowerMode::On).c_str(), displayId,
          to_string(error).c_str(), static_cast<int32_t>(error));
    }
  }
}

GonkDisplay::NativeData GonkDisplayP::GetNativeData(
    DisplayType aDisplayType, IGraphicBufferProducer* aSink) {
  NativeData data;

  if (aDisplayType == DisplayType::DISPLAY_PRIMARY) {
    data.mNativeWindow = mSTClient;
    data.mDisplaySurface = mDispSurface;
    data.mXdpi = mDispNativeData[(uint32_t)DisplayType::DISPLAY_PRIMARY].mXdpi;
    data.mComposer2DSupported = true;
    data.mVsyncSupported = true;
  } else if (aDisplayType == DisplayType::DISPLAY_EXTERNAL) {
    if (mExtFBDevice) {
      data.mNativeWindow = mExtSTClient;
      data.mDisplaySurface = mExtDispSurface;
      data.mXdpi =
          mDispNativeData[(uint32_t)DisplayType::DISPLAY_EXTERNAL].mXdpi;
      data.mComposer2DSupported = false;
      data.mVsyncSupported = false;
    }
  } else if (aDisplayType == DisplayType::DISPLAY_VIRTUAL) {
    data.mXdpi = mDispNativeData[(uint32_t)DisplayType::DISPLAY_PRIMARY].mXdpi;
    CreateVirtualDisplaySurface(aSink, data.mNativeWindow,
                                data.mDisplaySurface);
  }

  return data;
}

android::sp<ANativeWindow> GonkDisplayP::GetSurface(
    DisplayType aDisplayType) {
  if (aDisplayType == DisplayType::DISPLAY_PRIMARY) {
    return mSTClient? mSTClient : nullptr;
  } else if (aDisplayType == DisplayType::DISPLAY_EXTERNAL) {
    return mExtSTClient? mExtSTClient : nullptr;
  }

  return nullptr;
}

android::sp<GraphicBuffer> GonkDisplayP::GetFrameBuffer(
    DisplayType aDisplayType) {
  if (aDisplayType == DisplayType::DISPLAY_PRIMARY) {
    return mDispSurface? mDispSurface->GetCurrentFrameBuffer() : nullptr;
  } else if (aDisplayType == DisplayType::DISPLAY_EXTERNAL) {
    return mExtDispSurface? mExtDispSurface->GetCurrentFrameBuffer() : nullptr;
  }

  return nullptr;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
__attribute__((visibility("weak"))) GonkDisplay* GetGonkDisplay() {
  android::Mutex::Autolock _l(sMutex);
  if (!sGonkDisplay) {
    sGonkDisplay = new GonkDisplayP();
  }
  return sGonkDisplay;
}
#pragma clang diagnostic pop

// ----------------------------------------------------------------------------
}  // namespace mozilla
// ----------------------------------------------------------------------------
