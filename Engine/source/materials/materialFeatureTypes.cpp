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

#include "platform/platform.h"
#include "materials/materialFeatureTypes.h"


ImplementFeatureType( MFT_UseInstancing, U32(-1), -1, false );

ImplementFeatureType( MFT_VertTransform, MFG_Transform, 0, true );

ImplementFeatureType( MFT_TexAnim, MFG_PreTexture, 1.0f, true );
ImplementFeatureType( MFT_Parallax, MFG_PreTexture, 2.0f, true );

ImplementFeatureType( MFT_AccuScale, MFG_PreTexture, 4.0f, true );
ImplementFeatureType( MFT_AccuDirection, MFG_PreTexture, 4.0f, true );
ImplementFeatureType( MFT_AccuStrength, MFG_PreTexture, 4.0f, true );
ImplementFeatureType( MFT_AccuCoverage, MFG_PreTexture, 4.0f, true );
ImplementFeatureType( MFT_AccuSpecular, MFG_PreTexture, 4.0f, true );

ImplementFeatureType( MFT_DiffuseMap, MFG_Texture, 2.0f, true );
ImplementFeatureType( MFT_OverlayMap, MFG_Texture, 3.0f, true );
ImplementFeatureType( MFT_DetailMap, MFG_Texture, 4.0f, true );
ImplementFeatureType( MFT_DiffuseColor, MFG_Texture, 5.0f, true );
ImplementFeatureType( MFT_DiffuseVertColor, MFG_Texture, 6.0f, true );
ImplementFeatureType( MFT_AlphaTest, MFG_Texture, 7.0f, true );
ImplementFeatureType( MFT_InvertRoughness, U32(-1), -1, true);
ImplementFeatureType( MFT_OrmMap, MFG_Texture, 8.0f, true);
ImplementFeatureType( MFT_ORMConfigVars, MFG_Texture, 8.0f, true);
ImplementFeatureType( MFT_MatInfoFlags, MFG_Texture, 9.0f, true);
ImplementFeatureType( MFT_NormalMap, MFG_Texture, 11.0f, true );
ImplementFeatureType( MFT_DetailNormalMap, MFG_Texture, 12.0f, true );
ImplementFeatureType( MFT_Imposter, U32(-1), -1, true );

ImplementFeatureType( MFT_AccuMap, MFG_PreLighting, 2.0f, true );

ImplementFeatureType(MFT_ReflectionProbes, MFG_Lighting, 1.0f, true);
ImplementFeatureType( MFT_RTLighting, MFG_Lighting, 2.0f, true );
ImplementFeatureType( MFT_GlowMap, MFG_Lighting, 3.0f, true );
ImplementFeatureType( MFT_GlowMask, MFG_Lighting, 3.1f, true );
ImplementFeatureType( MFT_LightMap, MFG_Lighting, 4.0f, true );
ImplementFeatureType( MFT_ToneMap, MFG_Lighting, 5.0f, true );
ImplementFeatureType( MFT_VertLitTone, MFG_Lighting, 6.0f, false );
ImplementFeatureType( MFT_StaticCubemap, U32(-1), -1.0, true );
ImplementFeatureType( MFT_CubeMap, MFG_Lighting, 7.0f, true );
ImplementFeatureType( MFT_SubSurface, MFG_Lighting, 8.0f, true );
ImplementFeatureType( MFT_VertLit, MFG_Lighting, 9.0f, true );
ImplementFeatureType( MFT_MinnaertShading, MFG_Lighting, 10.0f, true );

ImplementFeatureType( MFT_Visibility, MFG_PostLighting, 2.0f, true );
ImplementFeatureType( MFT_Fog, MFG_PostProcess, 3.0f, true );

ImplementFeatureType(MFT_DebugViz, MFG_PostProcess, 998.0f, true);

ImplementFeatureType( MFT_HDROut, MFG_PostProcess, 999.0f, true );

ImplementFeatureType( MFT_IsBC3nm, U32(-1), -1, true );
ImplementFeatureType( MFT_IsBC5nm, U32(-1), -1, true);
ImplementFeatureType( MFT_IsTranslucent, U32(-1), -1, true );
ImplementFeatureType( MFT_IsTranslucentZWrite, U32(-1), -1, true );
ImplementFeatureType( MFT_DiffuseMapAtlas, U32(-1), -1, true );
ImplementFeatureType( MFT_NormalMapAtlas, U32(-1), -1, true );
ImplementFeatureType( MFT_InterlacedDeferred, U32(-1), -1, true );

ImplementFeatureType( MFT_ParaboloidVertTransform, MFG_Transform, -1, false );
ImplementFeatureType( MFT_IsSinglePassParaboloid, U32(-1), -1, false );
ImplementFeatureType( MFT_EyeSpaceDepthOut, MFG_PostLighting, 2.0f, false );
ImplementFeatureType( MFT_DepthOut, MFG_PostLighting, 3.0f, false );
ImplementFeatureType( MFT_DeferredConditioner, MFG_PostProcess, 1.0f, false );
ImplementFeatureType( MFT_NormalsOut, MFG_PreLighting, 1.0f, false );

ImplementFeatureType( MFT_LightbufferMRT, MFG_PreLighting, 1.0f, false );
ImplementFeatureType( MFT_RenderTarget1_Zero, MFG_PreTexture, 1.0f, false );
ImplementFeatureType( MFT_RenderTarget2_Zero, MFG_PreTexture, 1.0f, false );
ImplementFeatureType( MFT_RenderTarget3_Zero, MFG_PreTexture, 1.0f, false );

ImplementFeatureType( MFT_Foliage, MFG_PreTransform, 1.0f, false );

ImplementFeatureType( MFT_ParticleNormal, MFG_PreTransform, 2.0f, false );

ImplementFeatureType( MFT_ForwardShading, U32(-1), -1, true );

ImplementFeatureType( MFT_ImposterVert, MFG_PreTransform, 1.0, false );

// Deferred Shading
ImplementFeatureType( MFT_isDeferred, U32(-1), -1, true );
ImplementFeatureType( MFT_isBackground, MFG_Transform, 1.0f, false );
ImplementFeatureType( MFT_SkyBox, MFG_Transform, 2.0f, false );
ImplementFeatureType( MFT_HardwareSkinning, MFG_Transform,-2.0, false );

