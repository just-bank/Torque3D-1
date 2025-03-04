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
#include "materials/processedMaterial.h"

#include "materials/sceneData.h"
#include "materials/materialParameters.h"
#include "materials/matTextureTarget.h"
#include "materials/materialFeatureTypes.h"
#include "materials/materialManager.h"
#include "scene/sceneRenderState.h"
#include "gfx/gfxPrimitiveBuffer.h"
#include "gfx/gfxTextureManager.h"
#include "gfx/sim/cubemapData.h"

RenderPassData::RenderPassData()
{
   reset();
}

void RenderPassData::reset()
{
   for( U32 i = 0; i < Material::MAX_TEX_PER_PASS; ++ i )
   {
      destructInPlace( &mTexSlot[ i ] );
      mSamplerNames[ i ].clear();
   }

   dMemset( &mTexSlot, 0, sizeof(mTexSlot) );
   dMemset( &mTexType, 0, sizeof(mTexType) );

   mCubeMap = NULL;
   mNumTex = mNumTexReg = mStageNum = 0;
   mGlow = false;
   mBlendOp = Material::None;

   mFeatureData.clear();

   for (U32 i = 0; i < STATE_MAX; i++)
      mRenderStates[i] = NULL;
}

String RenderPassData::describeSelf() const
{
   String desc;

   // Now write all the textures.
   String texName;
   for ( U32 i=0; i < Material::MAX_TEX_PER_PASS; i++ )
   {
      if ( mTexType[i] == Material::TexTarget )
         texName = ( mTexSlot[i].texTarget ) ? mTexSlot[i].texTarget->getName() : String("null_texTarget");
      else if ( mTexType[i] == Material::Cube && mCubeMap )
         texName = mCubeMap->getPath();
      else if ( mTexSlot[i].texObject )
         texName = mTexSlot[i].texObject->getPath();
      else
         continue;

      desc += String::ToString( "TexSlot %d: %d, %s\n", i, mTexType[i], texName.c_str() );
   }

   // Write out the first render state which is the
   // basis for all the other states and shoud be
   // enough to define the pass uniquely.
   desc += mRenderStates[0]->getDesc().describeSelf();

   return desc;
}

ProcessedMaterial::ProcessedMaterial()
:  mMaterial( NULL ),
   mCurrentParams( NULL ),
   mHasSetStageData( false ),
   mHasGlow( false ),   
   mHasAccumulation( false ),   
   mMaxStages( 0 ),
   mVertexFormat( NULL ),
   mUserObject( NULL )
{
   VECTOR_SET_ASSOCIATION( mPasses );
}

ProcessedMaterial::~ProcessedMaterial()
{
	T3D::for_each( mPasses.begin(), mPasses.end(), T3D::delete_pointer() );
}

void ProcessedMaterial::_setBlendState(Material::BlendOp blendOp, GFXStateBlockDesc& desc )
{
   switch( blendOp )
   {
   case Material::Add:
      {
         desc.blendSrc = GFXBlendOne;
         desc.blendDest = GFXBlendOne;
         break;
      }
   case Material::AddAlpha:
      {
         desc.blendSrc = GFXBlendSrcAlpha;
         desc.blendDest = GFXBlendOne;
         break;
      }
   case Material::Mul:
      {
         desc.blendSrc = GFXBlendDestColor;
         desc.blendDest = GFXBlendInvSrcAlpha;
         break;
      }
   case Material::PreMul:
      {
         desc.blendSrc = GFXBlendOne;
         desc.blendDest = GFXBlendInvSrcAlpha;
         break;
      }
   case Material::LerpAlpha:
      {
         desc.blendSrc = GFXBlendSrcAlpha;
         desc.blendDest = GFXBlendInvSrcAlpha;
         break;
      }
   case Material::Sub:
      {
         desc.blendOp = GFXBlendOpSubtract;
         desc.blendSrc = GFXBlendOne;
         desc.blendDest = GFXBlendOne;
         break;
      }

   default:
      {
         // default to LerpAlpha
         desc.blendSrc = GFXBlendSrcAlpha;
         desc.blendDest = GFXBlendInvSrcAlpha;
         break;
      }
   }
}

void ProcessedMaterial::setBuffers(GFXVertexBufferHandleBase* vertBuffer, GFXPrimitiveBufferHandle* primBuffer)
{
   GFX->setVertexBuffer( *vertBuffer );
   GFX->setPrimitiveBuffer( *primBuffer );
}

bool ProcessedMaterial::stepInstance()
{
   AssertFatal( false, "ProcessedMaterial::stepInstance() - This type of material doesn't support instancing!" );
   return false;
}

String ProcessedMaterial::_getTexturePath(const String& filename)
{
   // if '/', then path is specified, use it.
   if( filename.find('/') != String::NPos )
   {
      return filename;
   }

   // otherwise, construct path
   return mMaterial->getPath() + filename;
}

GFXTexHandle ProcessedMaterial::_createTexture( const char* filename, GFXTextureProfile *profile)
{
   return GFXTexHandle( _getTexturePath(filename), profile, avar("%s() - NA (line %d)", __FUNCTION__, __LINE__) );
}

GFXTexHandle ProcessedMaterial::_createCompositeTexture(const char *filenameR, const char *filenameG, const char *filenameB, const char *filenameA, U32 inputKey[4], GFXTextureProfile *profile)
{
   return GFXTexHandle(_getTexturePath(filenameR), _getTexturePath(filenameG), _getTexturePath(filenameB), _getTexturePath(filenameA), inputKey, profile, avar("%s() - NA (line %d)", __FUNCTION__, __LINE__));
}

void ProcessedMaterial::addStateBlockDesc(const GFXStateBlockDesc& sb)
{
   mUserDefined = sb;
}

void ProcessedMaterial::_initStateBlockTemplates(GFXStateBlockDesc& stateTranslucent, GFXStateBlockDesc& stateReflect)
{
   // Translucency   
   stateTranslucent.blendDefined = true;
   stateTranslucent.blendEnable = mMaterial->mTranslucentBlendOp != Material::None;
   _setBlendState(mMaterial->mTranslucentBlendOp, stateTranslucent);
   stateTranslucent.zDefined = true;
   stateTranslucent.zWriteEnable = mMaterial->mTranslucentZWrite;   
   stateTranslucent.alphaDefined = true;
   stateTranslucent.alphaTestEnable = mMaterial->mAlphaTest;
   stateTranslucent.alphaTestRef = mMaterial->mAlphaRef;
   stateTranslucent.alphaTestFunc = GFXCmpGreaterEqual;
   stateTranslucent.samplersDefined = true;

   // Reflect   
   stateReflect.cullDefined = true;
   stateReflect.cullMode = mMaterial->mDoubleSided ? GFXCullNone : GFXCullCW;
}

void ProcessedMaterial::_initRenderPassDataStateBlocks()
{
   for (U32 pass = 0; pass < mPasses.size(); pass++)
      _initRenderStateStateBlocks( mPasses[pass] );
}

void ProcessedMaterial::_initPassStateBlock( RenderPassData *rpd, GFXStateBlockDesc &result )
{
   if ( rpd->mBlendOp != Material::None )
   {
      result.blendDefined = true;
      result.blendEnable = true;
      _setBlendState( rpd->mBlendOp, result );
   }

   if (mMaterial && mMaterial->isDoubleSided())
   {
      result.cullDefined = true;
      result.cullMode = GFXCullNone;         
   }

   if(mMaterial && mMaterial->mAlphaTest)
   {
      result.alphaDefined = true;
      result.alphaTestEnable = mMaterial->mAlphaTest;
      result.alphaTestRef = mMaterial->mAlphaRef;
      result.alphaTestFunc = GFXCmpGreaterEqual;
   }

   result.samplersDefined = true;
   NamedTexTarget *texTarget;

   U32 maxAnisotropy = 1;
   if (mMaterial &&  mMaterial->mUseAnisotropic[ rpd->mStageNum ] )
      maxAnisotropy = MATMGR->getDefaultAnisotropy();

   for( U32 i=0; i < rpd->mNumTex; i++ )
   {      
      U32 currTexFlag = rpd->mTexType[i];

      switch( currTexFlag )
      {
         default:
         {
            result.samplers[i].addressModeU = GFXAddressWrap;
            result.samplers[i].addressModeV = GFXAddressWrap;

            if ( maxAnisotropy > 1 )
            {
               result.samplers[i].minFilter = GFXTextureFilterAnisotropic;
               result.samplers[i].magFilter = GFXTextureFilterAnisotropic;
               result.samplers[i].maxAnisotropy = maxAnisotropy;
            }
            else
            {
               result.samplers[i].minFilter = GFXTextureFilterLinear;
               result.samplers[i].magFilter = GFXTextureFilterLinear;
            }
            break;
         }

         case Material::Cube:
         case Material::SGCube:
         case Material::NormalizeCube:
         {
            result.samplers[i].addressModeU = GFXAddressClamp;
            result.samplers[i].addressModeV = GFXAddressClamp;
            result.samplers[i].addressModeW = GFXAddressClamp;
            result.samplers[i].minFilter = GFXTextureFilterLinear;
            result.samplers[i].magFilter = GFXTextureFilterLinear;
            break;
         }

         case Material::TexTarget:
         {
            texTarget = mPasses[0]->mTexSlot[i].texTarget;
            if ( texTarget )
               texTarget->setupSamplerState( &result.samplers[i] );
            break;
         }
      }
   }

   // The deferred will take care of writing to the 
   // zbuffer, so we don't have to by default.
   if (  MATMGR->getDeferredEnabled() && 
         !mFeatures.hasFeature(MFT_ForwardShading))
      result.setZReadWrite( result.zEnable, false );

   result.addDesc(mUserDefined);
}

/// Creates the default state blocks for a list of render states
void ProcessedMaterial::_initRenderStateStateBlocks( RenderPassData *rpd )
{
   GFXStateBlockDesc stateTranslucent;
   GFXStateBlockDesc stateReflect;
   GFXStateBlockDesc statePass;

   _initStateBlockTemplates( stateTranslucent, stateReflect );
   _initPassStateBlock( rpd, statePass );

   // Ok, we've got our templates set up, let's combine them together based on state and
   // create our state blocks.
   for (U32 i = 0; i < RenderPassData::STATE_MAX; i++)
   {
      GFXStateBlockDesc stateFinal;

      if (i & RenderPassData::STATE_REFLECT)
         stateFinal.addDesc(stateReflect);
      if (i & RenderPassData::STATE_TRANSLUCENT)
         stateFinal.addDesc(stateTranslucent);

      stateFinal.addDesc(statePass);

      if (i & RenderPassData::STATE_WIREFRAME)
         stateFinal.fillMode = GFXFillWireframe;

      GFXStateBlockRef sb = GFX->createStateBlock(stateFinal);
      rpd->mRenderStates[i] = sb;
   }   
}

U32 ProcessedMaterial::_getRenderStateIndex( const SceneRenderState *sceneState, 
                                             const SceneData &sgData )
{
   // Based on what the state of the world is, get our render state block
   U32 currState = 0;

   // NOTE: We should only use per-material or per-pass hints to
   // change the render state.  This is importaint because we 
   // only change the state blocks between material passes.
   //
   // For example sgData.visibility would be bad to use
   // in here without changing how RenderMeshMgr works.

   if ( sceneState && sceneState->isReflectPass() )
      currState |= RenderPassData::STATE_REFLECT;

   if ( sgData.binType != SceneData::DeferredBin &&
        mMaterial->isTranslucent() )
      currState |= RenderPassData::STATE_TRANSLUCENT;

   if ( sgData.wireframe )
      currState |= RenderPassData::STATE_WIREFRAME;

   return currState;
}

void ProcessedMaterial::_setRenderState(  const SceneRenderState *state, 
                                          const SceneData& sgData, 
                                          U32 pass )
{   
   // Make sure we have the pass
   if ( pass >= mPasses.size() )
      return;

   U32 currState = _getRenderStateIndex( state, sgData );

   GFX->setStateBlock(mPasses[pass]->mRenderStates[currState]);   
}


void ProcessedMaterial::_setStageData()
{
   // Only do this once
   if (mHasSetStageData)
      return;
   mHasSetStageData = true;

   U32 i;

   // Load up all the textures for every possible stage
   for (i = 0; i < Material::MAX_STAGES; i++)
   {
      // DiffuseMap
      if (mMaterial->mDiffuseMapAsset[i] && !mMaterial->mDiffuseMapAsset[i].isNull())
      {
         mStages[i].setTex(MFT_DiffuseMap, mMaterial->getDiffuseMapResource(i));
         if (!mStages[i].getTex(MFT_DiffuseMap))
         {
            // If we start with a #, we're probably actually attempting to hit a named target and it may not get a hit on the first pass.
            if (!String(mMaterial->mDiffuseMapAsset[i]->getImageFileName()).startsWith("#") && !String(mMaterial->mDiffuseMapAsset[i]->getImageFileName()).startsWith("$"))
               mMaterial->logError("Failed to load diffuse map %s for stage %i", mMaterial->mDiffuseMapAsset[i]->getImageFileName(), i);

            mStages[i].setTex(MFT_DiffuseMap, _createTexture(GFXTextureManager::getMissingTexturePath().c_str(), &GFXStaticTextureSRGBProfile));
         }
      }
      else if (mMaterial->mDiffuseMapName[i] != StringTable->EmptyString())
      {
         mStages[i].setTex(MFT_DiffuseMap, _createTexture(mMaterial->mDiffuseMapName[i], &GFXStaticTextureSRGBProfile));
         if (!mStages[i].getTex(MFT_DiffuseMap))
         {
            //If we start with a #, we're probably actually attempting to hit a named target and it may not get a hit on the first pass.
            if (!String(mMaterial->mDiffuseMapName[i]).startsWith("#") && !String(mMaterial->mDiffuseMapName[i]).startsWith("$"))
               mMaterial->logError("Failed to load diffuse map %s for stage %i", mMaterial->mDiffuseMapName[i], i);

            // Load a debug texture to make it clear to the user 
            // that the texture for this stage was missing.
            mStages[i].setTex(MFT_DiffuseMap, _createTexture(GFXTextureManager::getMissingTexturePath().c_str(), &GFXStaticTextureSRGBProfile));
         }
      }
      // OverlayMap
      if (mMaterial->getOverlayMap(i) != StringTable->EmptyString())
      {
         mStages[i].setTex(MFT_OverlayMap, mMaterial->getOverlayMapResource(i));
         if (!mStages[i].getTex(MFT_OverlayMap))
            mMaterial->logError("Failed to load overlay map %s for stage %i", mMaterial->getOverlayMap(i), i);
      }

      // LightMap
      if (mMaterial->getLightMap(i) != StringTable->EmptyString())
      {
         mStages[i].setTex(MFT_LightMap, mMaterial->getLightMapResource(i));
         if (!mStages[i].getTex(MFT_LightMap))
            mMaterial->logError("Failed to load light map %s for stage %i", mMaterial->getLightMap(i), i);
      }

      // ToneMap
      if (mMaterial->getToneMap(i) != StringTable->EmptyString())
      {
         mStages[i].setTex(MFT_ToneMap, mMaterial->getToneMapResource(i));
         if (!mStages[i].getTex(MFT_ToneMap))
            mMaterial->logError("Failed to load tone map %s for stage %i", mMaterial->getToneMap(i), i);
      }

      // DetailMap
      if (mMaterial->getDetailMap(i) != StringTable->EmptyString())
      {
         mStages[i].setTex(MFT_DetailMap, mMaterial->getDetailMapResource(i));
         if (!mStages[i].getTex(MFT_DetailMap))
            mMaterial->logError("Failed to load detail map %s for stage %i", mMaterial->getDetailMap(i), i);
      }

      // NormalMap
      if (mMaterial->mNormalMapAsset[i] && !mMaterial->mNormalMapAsset[i].isNull())
      {
         mStages[i].setTex(MFT_NormalMap, mMaterial->getNormalMapResource(i));
         //mStages[i].setTex(MFT_DiffuseMap, _createTexture(mMaterial->getDiffuseMap(i), &GFXStaticTextureSRGBProfile));
         if (!mStages[i].getTex(MFT_NormalMap))
         {
            // Load a debug texture to make it clear to the user 
            // that the texture for this stage was missing.
            mStages[i].setTex(MFT_NormalMap, _createTexture(GFXTextureManager::getMissingTexturePath().c_str(), &GFXNormalMapProfile));
         }
      }
      else if (mMaterial->mNormalMapName[i] != StringTable->EmptyString())
      {
         mStages[i].setTex(MFT_NormalMap, _createTexture(mMaterial->mNormalMapName[i], &GFXNormalMapProfile));
         if (!mStages[i].getTex(MFT_NormalMap))
         {
            //If we start with a #, we're probably actually attempting to hit a named target and it may not get a hit on the first pass. So we'll
            //pass on the error rather than spamming the console
            if (!String(mMaterial->mNormalMapName[i]).startsWith("#"))
               mMaterial->logError("Failed to load normal map %s for stage %i", mMaterial->mNormalMapName[i], i);
         }
      }

      // Detail Normal Map
      if (mMaterial->getDetailNormalMap(i) != StringTable->EmptyString())
      {
         mStages[i].setTex(MFT_DetailNormalMap, mMaterial->getDetailNormalMapResource(i));
         if (!mStages[i].getTex(MFT_DetailNormalMap))
            mMaterial->logError("Failed to load normal map %s for stage %i", mMaterial->getDetailNormalMap(i), i);
      }

      //depending on creation method this may or may not have been shoved into srgb space eroneously
      GFXTextureProfile* profile = &GFXStaticTextureProfile;
      if (mMaterial->mIsSRGb[i])
         profile = &GFXStaticTextureSRGBProfile;

      // ORMConfig
      if (mMaterial->getORMConfigMap(i) != StringTable->EmptyString())
      {
         mStages[i].setTex(MFT_OrmMap, _createTexture(mMaterial->getORMConfigMap(i), profile));
         if (!mStages[i].getTex(MFT_OrmMap))
            mMaterial->logError("Failed to load PBR Config map %s for stage %i", mMaterial->getORMConfigMap(i), i);
      }
      else
      {
         if ((mMaterial->getAOMap(i) != StringTable->EmptyString()) || (mMaterial->getRoughMap(i) != StringTable->EmptyString()) || (mMaterial->getMetalMap(i) != StringTable->EmptyString()))
         {
            U32 inputKey[4];
            inputKey[0] = mMaterial->mAOChan[i];
            inputKey[1] = mMaterial->mRoughnessChan[i];
            inputKey[2] = mMaterial->mMetalChan[i];
            inputKey[3] = 0;
            mStages[i].setTex(MFT_OrmMap, _createCompositeTexture( mMaterial->getAOMap(i), mMaterial->getRoughMap(i),
               mMaterial->getMetalMap(i), "",
               inputKey, profile));
            if (!mStages[i].getTex(MFT_OrmMap))
               mMaterial->logError("Failed to dynamically create ORM Config map for stage %i", i);
         }
      }
      if (mMaterial->getGlowMap(i) != StringTable->EmptyString())
      {
         mStages[i].setTex(MFT_GlowMap, mMaterial->getGlowMapResource(i));
         if (!mStages[i].getTex(MFT_GlowMap))
            mMaterial->logError("Failed to load glow map %s for stage %i", mMaterial->getGlowMap(i), i);
      }
   }

   mMaterial->mCubemapData = dynamic_cast<CubemapData*>(Sim::findObject(mMaterial->mCubemapName));
   if (!mMaterial->mCubemapData)
      mMaterial->mCubemapData = NULL;


   // If we have a cubemap put it on stage 0 (cubemaps only supported on stage 0)
   if (mMaterial->mCubemapData)
   {
      mMaterial->mCubemapData->createMap();
      mStages[0].setCubemap(mMaterial->mCubemapData->mCubemap);
      if (!mStages[0].getCubemap())
         mMaterial->logError("Failed to load cubemap");
   }
}

