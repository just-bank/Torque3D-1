//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
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

#ifndef _CUBEMAPDATA_H_
#define _CUBEMAPDATA_H_

#ifndef _SIMOBJECT_H_
#include "console/simObject.h"
#endif

#ifndef _GFXCUBEMAP_H_
#include "gfx/gfxCubemap.h"
#endif

#ifndef _GFXTARGET_H_
#include "gfx/gfxTarget.h"
#endif

#ifndef _SCENEMANAGER_H_
#include "scene/sceneManager.h"
#endif

#include "T3D/assets/ImageAsset.h"

/// A script interface for creating static or dynamic cubemaps.
class CubemapData : public SimObject
{
   typedef SimObject Parent;   

public:   

   GFXCubemapHandle  mCubemap;

   CubemapData();
   ~CubemapData();

   bool onAdd() override;
   static void initPersistFields();

   DECLARE_CONOBJECT(CubemapData);

   // Force creation of cubemap
   void createMap();   

   // Update a static cubemap @ pos
	void updateFaces();

   void setCubemapFile(FileName newCubemapFile);

   void setCubeFaceFile(U32 index, FileName newFaceFile);

   void setCubeFaceTexture(U32 index, GFXTexHandle newFaceTexture);

   GFXTexHandle* getCubeFaceTexture(U32 faceIdx) { return &mCubeMapFace[faceIdx]; }

protected:
   DECLARE_IMAGEASSET(CubemapData, CubeMap, onCubemapChanged, GFXStaticTextureSRGBProfile);
   DECLARE_ASSET_SETGET(CubemapData, CubeMap);

   DECLARE_IMAGEASSET_ARRAY(CubemapData, CubeMapFace, 6, onCubeMapFaceChanged);
   DECLARE_IMAGEASSET_ARRAY_SETGET(CubemapData, CubeMapFace);

   void onCubeMapFaceChanged() {}
   GFXTexHandle mDepthBuff;
   GFXTextureTargetRef mRenderTarget;

   void onCubemapChanged() {}
};

#endif // CUBEMAPDATA
