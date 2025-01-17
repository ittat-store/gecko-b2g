/*
 * Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights reserved.
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_SF_FRAMEBUFFER_SURFACE_H
#define ANDROID_SF_FRAMEBUFFER_SURFACE_H

#include <stdint.h>
#include <sys/types.h>

#include "DisplaySurface.h"
#include "HwcHAL.h"  // for HWC2
#include "NativeFramebufferDevice.h"

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

class Rect;
class String8;

// ---------------------------------------------------------------------------

class FramebufferSurface : public DisplaySurface {
public:
    FramebufferSurface(
        uint32_t width, uint32_t height, uint32_t format,
        const sp<IGraphicBufferConsumer>& consumer,
        HWC2::Display *aHwcDisplay, HWC2::Layer *aLayer,
        mozilla::NativeFramebufferDevice *ExtFBDevice);

    // From DisplaySurface
    virtual status_t beginFrame(bool mustRecompose);
    virtual status_t prepareFrame(CompositionType compositionType);
    virtual status_t advanceFrame();
    virtual void onFrameCommitted();
    // Cannot resize a buffers in a FramebufferSurface. Only works with virtual
    // displays.
    virtual void resizeBuffers(const uint32_t width, const uint32_t height);

    // setReleaseFenceFd stores a fence file descriptor that will signal when the
    // current buffer is no longer being read. This fence will be returned to
    // the producer when the current buffer is released by updateTexImage().
    // Multiple fences can be set for a given buffer; they will be merged into
    // a single union fence. The SurfaceTexture will close the file descriptor
    // when finished with it.
    status_t setReleaseFenceFd(int fenceFd);

    virtual int GetPrevDispAcquireFd();

    virtual sp<GraphicBuffer> GetCurrentFrameBuffer() { return mCurrentBuffer; }

private:
    virtual ~FramebufferSurface() { }; // this class cannot be overloaded

    virtual void onFrameAvailable(const BufferItem &item);

    virtual void freeBufferLocked(int slotIndex);

    // nextBuffer waits for and then latches the next buffer from the
    // BufferQueue and releases the previously latched buffer to the
    // BufferQueue.  The new buffer is returned in the 'buffer' argument.
    status_t nextBuffer(sp<GraphicBuffer>& outBuffer, sp<Fence>& outFence);

	void presentLocked(
        const int slot,
        const sp<GraphicBuffer>& buffer,
        const sp<Fence>& acquireFence);

    // mCurrentBufferIndex is the slot index of the current buffer or
    // INVALID_BUFFER_SLOT to indicate that either there is no current buffer
    // or the buffer is not associated with a slot.
    int mCurrentSlot;

    // mCurrentBuffer is the current buffer or NULL to indicate that there is
    // no current buffer.
    sp<GraphicBuffer> mCurrentBuffer;

    sp<Fence> mPrevFBAcquireFence;

    // Previous buffer to release after getting an updated retire fence
    bool mHasPendingRelease;
    int mPreviousBufferSlot;
    sp<GraphicBuffer> mPreviousBuffer;

    HWC2::Display* hwcDisplay;
    HWC2::Layer* layer;
    mozilla::NativeFramebufferDevice* mExtFBDevice;

    sp<Fence> mLastPresentFence;
};

// ---------------------------------------------------------------------------
}; // namespace android
// ---------------------------------------------------------------------------

#endif // ANDROID_SF_FRAMEBUFFER_SURFACE_H

