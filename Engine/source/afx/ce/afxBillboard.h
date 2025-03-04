
//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//
// Arcane-FX for MIT Licensed Open Source version of Torque 3D from GarageGames
// Copyright (C) 2015 Faust Logic, Inc.
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
//
//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//

#ifndef _AFX_BILLBOARD_H_
#define _AFX_BILLBOARD_H_

#include "afx/afxEffectDefs.h"

#define BLEND_UNDEFINED GFXBlend_COUNT

class afxBillboardData : public GameBaseData, public afxEffectDefs
{
  typedef GameBaseData  Parent;

public:
   // This enum specifies common blend settings with predefined values
   // for src/dst blend factors. 
   enum BlendStyle {
     BlendUndefined,
     BlendNormal,
     BlendAdditive,
     BlendSubtractive,
     BlendPremultAlpha,
     BlendUser,
   };

public:
   DECLARE_IMAGEASSET(afxBillboardData, Texture, onChangeTexture, GFXStaticTextureSRGBProfile);
   DECLARE_ASSET_SETGET(afxBillboardData, Texture);


  LinearColorF            color;
  Point2F           texCoords[4];
  Point2F           dimensions;
  S32               blendStyle; 
  GFXBlend          srcBlendFactor;
  GFXBlend          dstBlendFactor;

public:
  /*C*/             afxBillboardData();
  /*C*/             afxBillboardData(const afxBillboardData&, bool = false);

  void      packData(BitStream*) override;
  void      unpackData(BitStream*) override;

  bool              preload(bool server, String &errorStr) override;

  bool      allowSubstitutions() const override { return true; }

  static void       initPersistFields();

  void onChangeTexture() {}

  DECLARE_CONOBJECT(afxBillboardData);
};

typedef afxBillboardData::BlendStyle afxBillboard_BlendStyle;
DefineEnumType( afxBillboard_BlendStyle );


//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//
// afxBillboard

class afxBillboard : public GameBase, public afxEffectDefs
{
  typedef GameBase Parent;
  friend class afxEA_Billboard;

private:
  afxBillboardData* mDataBlock;

  F32               fade_amt;
  bool              is_visible;
  S8                sort_priority;
  LinearColorF            live_color;

  GFXStateBlockRef  normal_sb;
  GFXStateBlockRef  reflected_sb;

public:
  /*C*/             afxBillboard();
  /*D*/             ~afxBillboard();

  bool      onNewDataBlock(GameBaseData* dptr, bool reload) override;
  bool      onAdd() override;
  void      onRemove() override;

  void              setFadeAmount(F32 amt) { fade_amt = amt; }
  void              setSortPriority(S8 priority) { sort_priority = priority; }
  void              setVisibility(bool flag) { is_visible = flag; }

  void      prepRenderImage(SceneRenderState*) override;

  void              _renderBillboard(ObjectRenderInst*, SceneRenderState*, BaseMatInstance*);

  DECLARE_CONOBJECT(afxBillboard);
  DECLARE_CATEGORY("UNLISTED");
};

//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//

#endif // _AFX_BILLBOARD_H_
