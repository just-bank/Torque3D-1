//-----------------------------------------------------------------------------
// Copyright (c) 2015 GarageGames, LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#ifndef _GFXD3D11CUBEMAP_H_
#define _GFXD3D11CUBEMAP_H_

#include "gfx/D3D11/gfxD3D11Device.h"
#include "gfx/gfxCubemap.h"
#include "gfx/gfxResource.h"
#include "gfx/gfxTarget.h"

const U32 CubeFaces = 6;
const U32 MaxMipMaps = 13; //todo this needs a proper static value somewhere to sync up with other classes like GBitmap

class GFXD3D11Cubemap : public GFXCubemap
{
public:
   void initStatic( GFXTexHandle *faces ) override;
   void initStatic( DDSFile *dds ) override;
   void initDynamic( U32 texSize, GFXFormat faceFormat = GFXFormatR8G8B8A8, U32 mipLevels = 0) override;
   void generateMipMaps() override;
   void setToTexUnit( U32 tuNum ) override;
   U32 getSize() const override { return mTexSize; }
   GFXFormat getFormat() const override { return mFaceFormat; }

   GFXD3D11Cubemap();
   virtual ~GFXD3D11Cubemap();

   // GFXResource interface
   void zombify() override;
   void resurrect() override;

   bool isInitialized() override { return mTexture ? true : false; }

   // Get functions
   ID3D11ShaderResourceView* getSRView();
   ID3D11RenderTargetView* getRTView(U32 faceIdx);
   ID3D11DepthStencilView* getDSView();
   ID3D11Texture2D* get2DTex();

private:

   friend class GFXD3D11TextureTarget;
   friend class GFXD3D11Device;

   ID3D11Texture2D* mTexture;
   ID3D11ShaderResourceView* mSRView; // for shader resource input
   ID3D11RenderTargetView* mRTView[CubeFaces]; // for render targets, 6 faces of the cubemap
   ID3D11DepthStencilView* mDSView; //render target view for depth stencil

   bool mAutoGenMips;
   bool mDynamic;
   U32  mTexSize;
   GFXFormat mFaceFormat;
   
   void releaseSurfaces();

   /// The callback used to get texture events.
   /// @see GFXTextureManager::addEventDelegate
   void _onTextureEvent(GFXTexCallbackCode code);
};

class GFXD3D11CubemapArray : public GFXCubemapArray
{
public:
   GFXD3D11CubemapArray();
   virtual ~GFXD3D11CubemapArray();
   void init(GFXCubemapHandle *cubemaps, const U32 cubemapCount) override;
   void init(const U32 cubemapCount, const U32 cubemapFaceSize, const GFXFormat format) override;
   void updateTexture(const GFXCubemapHandle &cubemap, const U32 slot) override;
   void copyTo(GFXCubemapArray *pDstCubemap) override;
   void setToTexUnit(U32 tuNum) override;

   ID3D11ShaderResourceView* getSRView() { return mSRView; }
   ID3D11Texture2D* get2DTex() { return mTexture; }

   // GFXResource interface
   void zombify() override;
   void resurrect() override;

private:
   friend class GFXD3D11TextureTarget;
   friend class GFXD3D11Device;

   ID3D11Texture2D *mTexture;
   ID3D11ShaderResourceView* mSRView; // for shader resource input
};

#endif
