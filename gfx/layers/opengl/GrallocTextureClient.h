/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GRALLOCTEXTURECLIENT_H
#define MOZILLA_GFX_GRALLOCTEXTURECLIENT_H
#ifdef MOZ_WIDGET_GONK

#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/FenceUtils.h" // for FenceHandle
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include <ui/GraphicBuffer.h>


namespace android {
class MediaBuffer;
};

namespace mozilla {
namespace gl {
class SharedSurface;
}

namespace layers {

/// A TextureData implementation based on android::GraphicBuffer (also referred to
/// as "gralloc").
///
/// Gralloc lets us map texture data in memory (accessible through pointers)
/// and also use it directly as an OpenGL texture without the cost of texture
/// uploads.
/// Gralloc buffers can also be shared accros processes.
///
/// More info about Gralloc here: https://wiki.mozilla.org/Platform/GFX/Gralloc
///
/// This is only used in Firefox OS
class GrallocTextureData : public TextureData {
public:
  typedef uint32_t AndroidFormat;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  // FIXME
  //virtual bool Lock(OpenMode aMode, FenceHandle* aFence) override;
  virtual bool Lock(OpenMode aMode) override;

  virtual void Unlock() override;

  virtual void FillInfo(TextureData::Info& aInfo) const override;

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  virtual bool BorrowMappedData(MappedTextureData& aMap) override;

  virtual void Deallocate(LayersIPCChannel*) override;

  virtual void Forget(LayersIPCChannel*) override;

  static GrallocTextureData* CreateForDrawing(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                              gfx::BackendType aMoz2dBackend,
                                              LayersIPCChannel* aAllocator,
                                              TextureAllocationFlags aAllocFlags);

  static GrallocTextureData* CreateForYCbCr(gfx::IntSize aYSize, gfx::IntSize aCbCrSize,
                                            LayersIPCChannel* aAllocator);

  static GrallocTextureData* CreateForGLRendering(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                                  LayersIPCChannel* aAllocator);

  static GrallocTextureData* Create(gfx::IntSize aSize, AndroidFormat aFormat,
                                    gfx::BackendType aMoz2DBackend, uint32_t aUsage,
                                    LayersIPCChannel* aAllocator);

  virtual TextureData* CreateSimilar(
      LayersIPCChannel* aAllocator, LayersBackend aLayersBackend,
      TextureFlags aFlags = TextureFlags::DEFAULT,
      TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

  // use TextureClient's default implementation
  virtual bool UpdateFromSurface(gfx::SourceSurface* aSurface) override;

  /// Hold android::MediaBuffer.
  /// MediaBuffer needs to be add refed to keep MediaBuffer alive while the texture
  /// is in use.
  ///
  /// TODO - ideally we should be able to put the MediaBuffer in the texture's
  /// constructor and not expose these methods.
  void SetMediaBuffer(android::MediaBuffer* aMediaBuffer) { mMediaBuffer = aMediaBuffer; }
  android::MediaBuffer* GetMediaBuffer() { return mMediaBuffer; }

  android::sp<android::GraphicBuffer> GetGraphicBuffer() { return mGraphicBuffer; }

  void WaitForBufferOwnership();

  void WaitForFence(FenceHandle* aFence);

  ~GrallocTextureData();

  virtual TextureFlags GetTextureFlags() const override;

  virtual GrallocTextureData* AsGrallocTextureData() override { return this; }

  void SetReleaseFenceHandle(const FenceHandle& aReleaseFenceHandle)
  {
    mReleaseFenceHandle.Merge(aReleaseFenceHandle);
  }

  FenceHandle GetAndResetReleaseFenceHandle()
  {
    FenceHandle fence;
    mReleaseFenceHandle.TransferToAnotherFenceHandle(fence);
    return fence;
  }

  void SetAcquireFenceHandle(const FenceHandle& aAcquireFenceHandle)
  {
    mAcquireFenceHandle = aAcquireFenceHandle;
  }

  const FenceHandle& GetAcquireFenceHandle() const
  {
    return mAcquireFenceHandle;
  }

protected:
  GrallocTextureData(MaybeMagicGrallocBufferHandle aGrallocHandle,
                     gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                     gfx::BackendType aMoz2DBackend);

  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  gfx::BackendType mMoz2DBackend;

  MaybeMagicGrallocBufferHandle mGrallocHandle;
  android::sp<android::GraphicBuffer> mGraphicBuffer;

  FenceHandle mReleaseFenceHandle;
  FenceHandle mAcquireFenceHandle;

  // Points to a mapped gralloc buffer between calls to lock and unlock.
  // Should be null outside of the lock-unlock pair.
  uint8_t* mMappedBuffer;

  android::MediaBuffer* mMediaBuffer;
};

gfx::SurfaceFormat SurfaceFormatForPixelFormat(android::PixelFormat aFormat);

} // namespace layers
} // namespace mozilla

#endif // MOZ_WIDGET_GONK
#endif
