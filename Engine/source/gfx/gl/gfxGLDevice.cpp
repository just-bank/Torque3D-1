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
#include "gfx/gl/gfxGLDevice.h"

#include "gfxGLTextureArray.h"
#include "platform/platformGL.h"

#include "gfx/gfxCubemap.h"
#include "gfx/gl/screenshotGL.h"
#include "gfx/gfxDrawUtil.h"

#include "gfx/gl/gfxGLEnumTranslate.h"
#include "gfx/gl/gfxGLVertexBuffer.h"
#include "gfx/gl/gfxGLPrimitiveBuffer.h"
#include "gfx/gl/gfxGLTextureTarget.h"
#include "gfx/gl/gfxGLTextureManager.h"
#include "gfx/gl/gfxGLTextureObject.h"
#include "gfx/gl/gfxGLCubemap.h"
#include "gfx/gl/gfxGLCardProfiler.h"
#include "gfx/gl/gfxGLWindowTarget.h"
#include "platform/platformDlibrary.h"
#include "gfx/gl/gfxGLShader.h"
#include "gfx/primBuilder.h"
#include "console/console.h"
#include "gfx/gl/gfxGLOcclusionQuery.h"
#include "materials/shaderData.h"
#include "gfx/gl/gfxGLStateCache.h"
#include "gfx/gl/gfxGLVertexAttribLocation.h"
#include "gfx/gl/gfxGLVertexDecl.h"
#include "shaderGen/shaderGen.h"
#include "gfxGLUtils.h"

#if defined(TORQUE_OS_WIN)
#include "gfx/gl/tGL/tWGL.h"
#elif defined(TORQUE_OS_LINUX)
#include "gfx/gl/tGL/tXGL.h"
#endif

GFXAdapter::CreateDeviceInstanceDelegate GFXGLDevice::mCreateDeviceInstance(GFXGLDevice::createInstance);

GFXDevice *GFXGLDevice::createInstance( U32 adapterIndex )
{
   return new GFXGLDevice(adapterIndex);
}

namespace GL
{
   extern void gglPerformBinds();
   extern void gglPerformExtensionBinds(void *context);
}

void loadGLCore()
{
   static bool coreLoaded = false; // Guess what this is for.
   if(coreLoaded)
      return;
   coreLoaded = true;

   // Make sure we've got our GL bindings.
   GL::gglPerformBinds();
}

void loadGLExtensions(void *context)
{
   static bool extensionsLoaded = false;
   if(extensionsLoaded)
      return;
   extensionsLoaded = true;

   GL::gglPerformExtensionBinds(context);
}

void STDCALL glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
	const GLchar *message, const void *userParam)
{
    // JTH [11/24/2016]: This is a temporary fix so that we do not get spammed for redundant fbo changes.
    // This only happens on Intel cards. This should be looked into sometime in the near future.
    if (dStrStartsWith(message, "API_ID_REDUNDANT_FBO"))
        return;
    if (severity == GL_DEBUG_SEVERITY_HIGH)
        Con::errorf("OPENGL: %s", message);
    else if (severity == GL_DEBUG_SEVERITY_MEDIUM)
        Con::warnf("OPENGL: %s", message);
    else if (severity == GL_DEBUG_SEVERITY_LOW)
        Con::printf("OPENGL: %s", message);
}

void STDCALL glAmdDebugCallback(GLuint id, GLenum category, GLenum severity, GLsizei length,
    const GLchar* message, GLvoid* userParam)
{
    if (severity == GL_DEBUG_SEVERITY_HIGH)
        Con::errorf("AMDOPENGL: %s", message);
    else if (severity == GL_DEBUG_SEVERITY_MEDIUM)
        Con::warnf("AMDOPENGL: %s", message);
    else if (severity == GL_DEBUG_SEVERITY_LOW)
        Con::printf("AMDOPENGL: %s", message);
}

void GFXGLDevice::initGLState()
{
   // We don't currently need to sync device state with a known good place because we are
   // going to set everything in GFXGLStateBlock, but if we change our GFXGLStateBlock strategy, this may
   // need to happen.

   // Deal with the card profiler here when we know we have a valid context.
   mCardProfiler = new GFXGLCardProfiler();
   mCardProfiler->init();
   glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, (GLint*)&mMaxShaderTextures);
   // JTH: Needs removed, ffp
   //glGetIntegerv(GL_MAX_TEXTURE_UNITS, (GLint*)&mMaxFFTextures);
   glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, (GLint*)&mMaxTRColors);
   mMaxTRColors = getMin( mMaxTRColors, (U32)(GFXTextureTarget::MaxRenderSlotId-1) );

   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   // [JTH 5/6/2016] GLSL 1.50 is really SM 4.0
   // Setting mPixelShaderVersion to 3.0 will allow Advanced Lighting to run.
   mPixelShaderVersion = 3.0;

	// Set capability extensions.
   mCapabilities.anisotropicFiltering = mCardProfiler->queryProfile("GL_EXT_texture_filter_anisotropic");
   mCapabilities.bufferStorage = mCardProfiler->queryProfile("GL_ARB_buffer_storage");
   mCapabilities.textureStorage = mCardProfiler->queryProfile("GL_ARB_texture_storage");
   mCapabilities.copyImage = mCardProfiler->queryProfile("GL_ARB_copy_image");
   mCapabilities.vertexAttributeBinding = mCardProfiler->queryProfile("GL_ARB_vertex_attrib_binding");
   mCapabilities.khrDebug = mCardProfiler->queryProfile("GL_KHR_debug");
   mCapabilities.extDebugMarker = mCardProfiler->queryProfile("GL_EXT_debug_marker");

   String vendorStr = (const char*)glGetString( GL_VENDOR );
   if( vendorStr.find("NVIDIA", 0, String::NoCase | String::Left) != String::NPos)
      mUseGlMap = false;

   // Workaround for all Mac's, has a problem using glMap* with volatile buffers
#ifdef TORQUE_OS_MAC
   mUseGlMap = false;
#endif

#if TORQUE_DEBUG
   if( gglHasExtension(ARB_debug_output) )
   {
      glEnable(GL_DEBUG_OUTPUT);
      glDebugMessageCallbackARB(glDebugCallback, NULL);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
      GLuint unusedIds = 0;
      glDebugMessageControlARB(GL_DONT_CARE,
            GL_DONT_CARE,
            GL_DONT_CARE,
            0,
            &unusedIds,
            GL_TRUE);
   }
   else if(gglHasExtension(AMD_debug_output))
   {
      glEnable(GL_DEBUG_OUTPUT);
      glDebugMessageCallbackAMD(glAmdDebugCallback, NULL);
      //glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
      GLuint unusedIds = 0;
      glDebugMessageEnableAMD(GL_DONT_CARE, GL_DONT_CARE, 0,&unusedIds, GL_TRUE);
   }
#endif

   PlatformGL::setVSync(smEnableVSync);

   //install vsync callback
   Con::NotifyDelegate clbk(this, &GFXGLDevice::vsyncCallback);
   Con::addVariableNotify("$pref::Video::enableVerticalSync", clbk);

   //OpenGL 3 need a binded VAO for render
   GLuint vao;
   glGenVertexArrays(1, &vao);
   glBindVertexArray(vao);

   // MacOS uses OGL 4.1. This workaround is functional, but will not provide the improvied depth performance.
#if defined(__MACOSX__)
   glDepthRangef(0.0, 1.0);
#else
   glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
#endif
   //enable sRGB
   glEnable(GL_FRAMEBUFFER_SRGB);

   //enable seamless cubemapping
   glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

void GFXGLDevice::vsyncCallback()
{
   PlatformGL::setVSync(smEnableVSync);
}

GFXGLDevice::GFXGLDevice(U32 adapterIndex) :
   mAdapterIndex(adapterIndex),
   mNeedUpdateVertexAttrib(false),
   mCurrentPB(NULL),
   mDrawInstancesCount(0),
   mCurrentShader( NULL ),
   m_mCurrentWorld(true),
   m_mCurrentView(true),
   mContext(NULL),
   mPixelFormat(NULL),
   mPixelShaderVersion(0.0f),
   mMaxShaderTextures(2),
   mMaxFFTextures(2),
   mMaxTRColors(1),
   mClip(0, 0, 0, 0),
   mWindowRT(NULL),
   mUseGlMap(true)
{
   for(int i = 0; i < VERTEX_STREAM_COUNT; ++i)
   {
      mCurrentVB[i] = NULL;
      mCurrentVB_Divisor[i] = 0;
   }

   // Initiailize capabilities to false.
   memset(&mCapabilities, 0, sizeof(GLCapabilities));

   loadGLCore();

   GFXGLEnumTranslate::init();

   GFXVertexColor::setSwizzle( &Swizzles::rgba );

   // OpenGL have native RGB, no need swizzle
   mDeviceSwizzle32 = &Swizzles::rgba;
   mDeviceSwizzle24 = &Swizzles::rgb;

   mTextureManager = new GFXGLTextureManager();
   gScreenShot = new ScreenShotGL();

   for(U32 i = 0; i < GFX_TEXTURE_STAGE_COUNT; i++)
      mActiveTextureType[i] = GL_ZERO;

   mNumVertexStream = 2;

   for(int i = 0; i < GS_COUNT; ++i)
      mModelViewProjSC[i] = NULL;

   mOpenglStateCache = new GFXGLStateCache;
   mCurrentConstBuffer = NULL;
}

GFXGLDevice::~GFXGLDevice()
{
   mCurrentStateBlock = NULL;

   for(int i = 0; i < VERTEX_STREAM_COUNT; ++i)
      mCurrentVB[i] = NULL;
   mCurrentPB = NULL;

   for(U32 i = 0; i < mVolatileVBs.size(); i++)
      mVolatileVBs[i] = NULL;
   for(U32 i = 0; i < mVolatilePBs.size(); i++)
      mVolatilePBs[i] = NULL;

   // Clear out our current texture references
   for (U32 i = 0; i < GFX_TEXTURE_STAGE_COUNT; i++)
   {
      mCurrentTexture[i] = NULL;
      mNewTexture[i] = NULL;
      mCurrentCubemap[i] = NULL;
      mNewCubemap[i] = NULL;
   }

   mRTStack.clear();
   mCurrentRT = NULL;

   if( mTextureManager )
   {
      mTextureManager->zombify();
      mTextureManager->kill();
   }

   // Free device buffers
   DeviceBufferMap::Iterator bufferIter = mDeviceBufferMap.begin();
   for (; bufferIter != mDeviceBufferMap.end(); ++bufferIter)
      glDeleteBuffers(1, &bufferIter->value);

   GFXResource* walk = mResourceListHead;
   while(walk)
   {
      walk->zombify();
      walk = walk->getNextResource();
   }

   if( mCardProfiler )
      SAFE_DELETE( mCardProfiler );

   SAFE_DELETE( gScreenShot );

   SAFE_DELETE( mOpenglStateCache );
}

GLuint GFXGLDevice::getDeviceBuffer(const GFXShaderConstDesc desc)
{
   String name(desc.name + "_" + String::ToString(desc.size));
   DeviceBufferMap::Iterator buf = mDeviceBufferMap.find(name);
   if (buf != mDeviceBufferMap.end())
   {
      return mDeviceBufferMap[name];
   }

   GLuint uboHandle;
   glGenBuffers(1, &uboHandle);

   mDeviceBufferMap[name] = uboHandle;

   return uboHandle;
}

void GFXGLDevice::zombify()
{
   mTextureManager->zombify();

   for(int i = 0; i < VERTEX_STREAM_COUNT; ++i)
      if(mCurrentVB[i])
         mCurrentVB[i]->finish();
   if(mCurrentPB)
         mCurrentPB->finish();

   //mVolatileVBs.clear();
   //mVolatilePBs.clear();
   GFXResource* walk = mResourceListHead;
   while(walk)
   {
      walk->zombify();
      walk = walk->getNextResource();
   }
}

void GFXGLDevice::resurrect()
{
   GFXResource* walk = mResourceListHead;
   while(walk)
   {
      walk->resurrect();
      walk = walk->getNextResource();
   }
   for(int i = 0; i < VERTEX_STREAM_COUNT; ++i)
      if(mCurrentVB[i])
         mCurrentVB[i]->prepare();
   if(mCurrentPB)
      mCurrentPB->prepare();

   mTextureManager->resurrect();
}

GFXVertexBuffer* GFXGLDevice::findVolatileVBO(U32 numVerts, const GFXVertexFormat *vertexFormat, U32 vertSize)
{
   PROFILE_SCOPE(GFXGLDevice_findVBPool);
   for(U32 i = 0; i < mVolatileVBs.size(); i++)
      if (  mVolatileVBs[i]->mNumVerts >= numVerts &&
            mVolatileVBs[i]->mVertexFormat.isEqual( *vertexFormat ) &&
            mVolatileVBs[i]->mVertexSize == vertSize &&
            mVolatileVBs[i]->getRefCount() == 1 )
         return mVolatileVBs[i];

   // No existing VB, so create one
   PROFILE_SCOPE(GFXGLDevice_createVBPool);
   StrongRefPtr<GFXGLVertexBuffer> buf(new GFXGLVertexBuffer(GFX, numVerts, vertexFormat, vertSize, GFXBufferTypeVolatile));
   buf->registerResourceWithDevice(this);
   mVolatileVBs.push_back(buf);
   return buf.getPointer();
}

GFXPrimitiveBuffer* GFXGLDevice::findVolatilePBO(U32 numIndices, U32 numPrimitives)
{
   for(U32 i = 0; i < mVolatilePBs.size(); i++)
      if((mVolatilePBs[i]->mIndexCount >= numIndices) && (mVolatilePBs[i]->getRefCount() == 1))
         return mVolatilePBs[i];

   // No existing PB, so create one
   StrongRefPtr<GFXGLPrimitiveBuffer> buf(new GFXGLPrimitiveBuffer(GFX, numIndices, numPrimitives, GFXBufferTypeVolatile));
   buf->registerResourceWithDevice(this);
   mVolatilePBs.push_back(buf);
   return buf.getPointer();
}

GFXVertexBuffer *GFXGLDevice::allocVertexBuffer(   U32 numVerts,
                                                   const GFXVertexFormat *vertexFormat,
                                                   U32 vertSize,
                                                   GFXBufferType bufferType,
                                                   void* data )
{
   PROFILE_SCOPE(GFXGLDevice_allocVertexBuffer);
   if(bufferType == GFXBufferTypeVolatile)
      return findVolatileVBO(numVerts, vertexFormat, vertSize);

   GFXGLVertexBuffer* buf = new GFXGLVertexBuffer( GFX, numVerts, vertexFormat, vertSize, bufferType );
   buf->registerResourceWithDevice(this);

   if(data)
   {
      void* dest;
      buf->lock(0, numVerts, &dest);
      dMemcpy(dest, data, vertSize * numVerts);
      buf->unlock();
   }

   return buf;
}

GFXPrimitiveBuffer *GFXGLDevice::allocPrimitiveBuffer( U32 numIndices, U32 numPrimitives, GFXBufferType bufferType, void* data )
{
   GFXPrimitiveBuffer* buf;

   if(bufferType == GFXBufferTypeVolatile)
   {
      buf = findVolatilePBO(numIndices, numPrimitives);
   }
   else
   {
      buf = new GFXGLPrimitiveBuffer(GFX, numIndices, numPrimitives, bufferType);
      buf->registerResourceWithDevice(this);
   }

   if(data)
   {
      void* dest;
      buf->lock(0, numIndices, &dest);
      dMemcpy(dest, data, sizeof(U16) * numIndices);
      buf->unlock();
   }
   return buf;
}

void GFXGLDevice::setVertexStream( U32 stream, GFXVertexBuffer *buffer )
{
   AssertFatal(stream <= 1, "GFXGLDevice::setVertexStream only support 2 stream (0: data, 1: instancing)");

   //if(mCurrentVB[stream] != buffer)
   {
      // Reset the state the old VB required, then set the state the new VB requires.
      if( mCurrentVB[stream] )
      {
         mCurrentVB[stream]->finish();
      }

      mCurrentVB[stream] = static_cast<GFXGLVertexBuffer*>( buffer );

      mNeedUpdateVertexAttrib = true;
   }
}

void GFXGLDevice::setVertexStreamFrequency( U32 stream, U32 frequency )
{
   if( stream == 0 )
   {
      mCurrentVB_Divisor[stream] = 0; // non instanced, is vertex buffer
      mDrawInstancesCount = frequency; // instances count
   }
   else
   {
      AssertFatal(frequency <= 1, "GFXGLDevice::setVertexStreamFrequency only support 0/1 for this stream" );
      if( stream == 1 && frequency == 1 )
         mCurrentVB_Divisor[stream] = 1; // instances data need a frequency of 1
      else
         mCurrentVB_Divisor[stream] = 0;
   }

   mNeedUpdateVertexAttrib = true;
}

GFXCubemap* GFXGLDevice::createCubemap()
{
   GFXGLCubemap* cube = new GFXGLCubemap();
   cube->registerResourceWithDevice(this);
   return cube;
};

GFXCubemapArray *GFXGLDevice::createCubemapArray()
{
   GFXGLCubemapArray* cubeArray = new GFXGLCubemapArray();
   cubeArray->registerResourceWithDevice(this);
   return cubeArray;
}

GFXTextureArray* GFXGLDevice::createTextureArray()
{
   GFXGLTextureArray* textureArray = new GFXGLTextureArray();
   textureArray->registerResourceWithDevice(this);
   return textureArray;
}

void GFXGLDevice::endSceneInternal()
{
   // nothing to do for opengl
   mCanCurrentlyRender = false;
}

void GFXGLDevice::copyResource(GFXTextureObject* pDst, GFXCubemap* pSrc, const U32 face)
{
   AssertFatal(pDst, "GFXGLDevice::copyResource: Destination texture is null");
   AssertFatal(pSrc, "GFXGLDevice::copyResource: Source cubemap is null");

   GFXGLTextureObject* gGLDst = static_cast<GFXGLTextureObject*>(pDst);
   GFXGLCubemap* pGLSrc = static_cast<GFXGLCubemap*>(pSrc);

   const GFXFormat format = pGLSrc->getFormat();
   const bool isCompressed = ImageUtil::isCompressedFormat(format);
   const U32 mipLevels = pGLSrc->getMipMapLevels();
   const U32 texSize = pGLSrc->getSize();

   //set up pbo if we don't have copyImage support
   if (!GFXGL->mCapabilities.copyImage)
   {
      const GLuint pbo = gGLDst->getBuffer();
      glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
      //allocate data
      glBufferData(GL_PIXEL_PACK_BUFFER, texSize * texSize * GFXFormat_getByteSize(format), NULL, GL_STREAM_COPY);
   }

   for (U32 mip = 0; mip < mipLevels; mip++)
   {
      const U32 mipSize =  texSize >> mip;
      if (GFXGL->mCapabilities.copyImage)
      {
         glCopyImageSubData(pGLSrc->mCubemap, GL_TEXTURE_CUBE_MAP, mip, 0, 0, face, gGLDst->getHandle(), GL_TEXTURE_2D, mip, 0, 0, 0, mipSize, mipSize, 1);
      }
      else
      {
         //pbo id
         const GLuint pbo = gGLDst->getBuffer();

         //copy source texture data to pbo
         glBindTexture(GL_TEXTURE_CUBE_MAP, pGLSrc->mCubemap);
         glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);

         if (isCompressed)
            glGetCompressedTexImage(GFXGLFaceType[face], mip, NULL);
         else
            glGetTexImage(GFXGLFaceType[face], mip, GFXGLTextureFormat[format], GFXGLTextureType[format], NULL);

         glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
         glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

         //copy data from pbo to destination
         glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
         glBindTexture(gGLDst->getBinding(), gGLDst->getHandle());

         if (isCompressed)
         {
            const U32 mipDataSize = getCompressedSurfaceSize(format, pGLSrc->getSize(), pGLSrc->getSize(), 0);
            glCompressedTexSubImage2D(gGLDst->getBinding(), mip, 0, 0, mipSize, mipSize, GFXGLTextureFormat[format], mipDataSize, NULL);
         }
         else
         {
            glTexSubImage2D(gGLDst->getBinding(), mip, 0, 0, mipSize, mipSize, GFXGLTextureFormat[format], GFXGLTextureType[format], NULL);
         }

         glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
         glBindTexture(gGLDst->getBinding(), 0);
      }
   }
}

void GFXGLDevice::clear(U32 flags, const LinearColorF& color, F32 z, U32 stencil)
{
   // Make sure we have flushed our render target state.
   _updateRenderTargets();

   bool writeAllColors = true;
   bool zwrite = true;
   bool writeAllStencil = true;
   const GFXStateBlockDesc *desc = NULL;
   if (mCurrentGLStateBlock)
   {
      desc = &mCurrentGLStateBlock->getDesc();
      zwrite = desc->zWriteEnable;
      writeAllColors = desc->colorWriteRed && desc->colorWriteGreen && desc->colorWriteBlue && desc->colorWriteAlpha;
      writeAllStencil = desc->stencilWriteMask == 0xFFFFFFFF;
   }

   glColorMask(true, true, true, true);
   glDepthMask(true);
   glStencilMask(0xFFFFFFFF);
   glClearColor(color.red, color.green, color.blue, color.alpha);
   glClearDepth(z);
   glClearStencil(stencil);

   GLbitfield clearflags = 0;
   clearflags |= (flags & GFXClearTarget)   ? GL_COLOR_BUFFER_BIT : 0;
   clearflags |= (flags & GFXClearZBuffer)  ? GL_DEPTH_BUFFER_BIT : 0;
   clearflags |= (flags & GFXClearStencil)  ? GL_STENCIL_BUFFER_BIT : 0;

   glClear(clearflags);

   if(!writeAllColors)
      glColorMask(desc->colorWriteRed, desc->colorWriteGreen, desc->colorWriteBlue, desc->colorWriteAlpha);

   if(!zwrite)
      glDepthMask(false);

   if(!writeAllStencil)
      glStencilMask(desc->stencilWriteMask);
}

void GFXGLDevice::clearColorAttachment(const U32 attachment, const LinearColorF& color)
{
   const GLfloat clearColor[4] = { color.red, color.green, color.blue, color.alpha };
   glClearBufferfv(GL_COLOR, attachment, clearColor);
}

// Given a primitive type and a number of primitives, return the number of indexes/vertexes used.
inline GLsizei GFXGLDevice::primCountToIndexCount(GFXPrimitiveType primType, U32 primitiveCount)
{
   switch (primType)
   {
      case GFXPointList :
         return primitiveCount;
         break;
      case GFXLineList :
         return primitiveCount * 2;
         break;
      case GFXLineStrip :
         return primitiveCount + 1;
         break;
      case GFXTriangleList :
         return primitiveCount * 3;
         break;
      case GFXTriangleStrip :
         return 2 + primitiveCount;
         break;
      default:
         AssertFatal(false, "GFXGLDevice::primCountToIndexCount - unrecognized prim type");
         break;
   }

   return 0;
}

GFXVertexDecl* GFXGLDevice::allocVertexDecl( const GFXVertexFormat *vertexFormat )
{
   PROFILE_SCOPE(GFXGLDevice_allocVertexDecl);
   typedef Map<void*, GFXGLVertexDecl> GFXGLVertexDeclMap;
   static GFXGLVertexDeclMap declMap;
   GFXGLVertexDeclMap::Iterator itr = declMap.find( (void*)vertexFormat->getDescription().c_str() ); // description string are interned, safe to use c_str()
   if(itr != declMap.end())
      return &itr->value;

   GFXGLVertexDecl &decl = declMap[(void*)vertexFormat->getDescription().c_str()];
   decl.init(vertexFormat);
   return &decl;
}

void GFXGLDevice::setVertexDecl( const GFXVertexDecl *decl )
{
   static_cast<const GFXGLVertexDecl*>(decl)->prepareVertexFormat();
}

inline void GFXGLDevice::preDrawPrimitive()
{
   if( mStateDirty )
   {
      updateStates();
   }

   if(mCurrentShaderConstBuffer)
      setShaderConstBufferInternal(mCurrentShaderConstBuffer);

   if( mNeedUpdateVertexAttrib )
   {
      AssertFatal(mCurrVertexDecl, "");
      const GFXGLVertexDecl* decl = static_cast<const GFXGLVertexDecl*>(mCurrVertexDecl);

      for(int i = 0; i < getNumVertexStreams(); ++i)
      {
         if(mCurrentVB[i])
         {
            mCurrentVB[i]->prepare(i, mCurrentVB_Divisor[i]);    // GL_ARB_vertex_attrib_binding
            decl->prepareBuffer_old( i, mCurrentVB[i]->mBuffer, mCurrentVB_Divisor[i] ); // old vertex buffer/format
         }
      }

      decl->updateActiveVertexAttrib( GFXGL->getOpenglCache()->getCacheVertexAttribActive() );
   }

   mNeedUpdateVertexAttrib = false;
}

inline void GFXGLDevice::postDrawPrimitive(U32 primitiveCount)
{
   mDeviceStatistics.mDrawCalls++;
   mDeviceStatistics.mPolyCount += primitiveCount;
}

void GFXGLDevice::drawPrimitive( GFXPrimitiveType primType, U32 vertexStart, U32 primitiveCount )
{
   preDrawPrimitive();

   if(mCurrentVB[0])
      vertexStart += mCurrentVB[0]->mBufferVertexOffset;

   if(mDrawInstancesCount)
      glDrawArraysInstanced(GFXGLPrimType[primType], vertexStart, primCountToIndexCount(primType, primitiveCount), mDrawInstancesCount);
   else
      glDrawArrays(GFXGLPrimType[primType], vertexStart, primCountToIndexCount(primType, primitiveCount));

   postDrawPrimitive(primitiveCount);
}

void GFXGLDevice::drawIndexedPrimitive(   GFXPrimitiveType primType,
                                          U32 startVertex,
                                          U32 minIndex,
                                          U32 numVerts,
                                          U32 startIndex,
                                          U32 primitiveCount )
{
   preDrawPrimitive();

   U16* buf = (U16*)static_cast<GFXGLPrimitiveBuffer*>(mCurrentPrimitiveBuffer.getPointer())->getBuffer() + startIndex + mCurrentPrimitiveBuffer->mVolatileStart;

   const U32 baseVertex = mCurrentVB[0]->mBufferVertexOffset + startVertex;

   if(mDrawInstancesCount)
      glDrawElementsInstancedBaseVertex(GFXGLPrimType[primType], primCountToIndexCount(primType, primitiveCount), GL_UNSIGNED_SHORT, buf, mDrawInstancesCount, baseVertex);
   else
      glDrawElementsBaseVertex(GFXGLPrimType[primType], primCountToIndexCount(primType, primitiveCount), GL_UNSIGNED_SHORT, buf, baseVertex);

   postDrawPrimitive(primitiveCount);
}

void GFXGLDevice::setPB(GFXGLPrimitiveBuffer* pb)
{
   if(mCurrentPB)
      mCurrentPB->finish();
   mCurrentPB = pb;
}

void GFXGLDevice::setTextureInternal(U32 textureUnit, const GFXTextureObject*texture)
{
   GFXGLTextureObject *tex = static_cast<GFXGLTextureObject*>(const_cast<GFXTextureObject*>(texture));
   if (tex)
   {
      mActiveTextureType[textureUnit] = tex->getBinding();
      tex->bind(textureUnit);
   }
   else if(mActiveTextureType[textureUnit] != GL_ZERO)
   {
      glActiveTexture(GL_TEXTURE0 + textureUnit);
      glBindTexture(mActiveTextureType[textureUnit], 0);
      getOpenglCache()->setCacheBindedTex(textureUnit, mActiveTextureType[textureUnit], 0);
      mActiveTextureType[textureUnit] = GL_ZERO;
   }
}

void GFXGLDevice::setCubemapInternal(U32 textureUnit, const GFXGLCubemap* texture)
{
   if(texture)
   {
      mActiveTextureType[textureUnit] = GL_TEXTURE_CUBE_MAP;
      texture->bind(textureUnit);
   }
   else if(mActiveTextureType[textureUnit] != GL_ZERO)
   {
      glActiveTexture(GL_TEXTURE0 + textureUnit);
      glBindTexture(mActiveTextureType[textureUnit], 0);
      getOpenglCache()->setCacheBindedTex(textureUnit, mActiveTextureType[textureUnit], 0);
      mActiveTextureType[textureUnit] = GL_ZERO;
   }
}

void GFXGLDevice::setCubemapArrayInternal(U32 textureUnit, const GFXGLCubemapArray* texture)
{
   if (texture)
   {
      mActiveTextureType[textureUnit] = GL_TEXTURE_CUBE_MAP_ARRAY_ARB;
      texture->bind(textureUnit);
   }
   else if (mActiveTextureType[textureUnit] != GL_ZERO)
   {
      glActiveTexture(GL_TEXTURE0 + textureUnit);
      glBindTexture(mActiveTextureType[textureUnit], 0);
      getOpenglCache()->setCacheBindedTex(textureUnit, mActiveTextureType[textureUnit], 0);
      mActiveTextureType[textureUnit] = GL_ZERO;
   }
}

void GFXGLDevice::setTextureArrayInternal(U32 textureUnit, const GFXGLTextureArray* texture)
{
   if (texture)
   {
      mActiveTextureType[textureUnit] = GL_TEXTURE_2D_ARRAY;
      texture->bind(textureUnit);
   }
   else if (mActiveTextureType[textureUnit] != GL_ZERO)
   {
      glActiveTexture(GL_TEXTURE0 + textureUnit);
      glBindTexture(mActiveTextureType[textureUnit], 0);
      getOpenglCache()->setCacheBindedTex(textureUnit, mActiveTextureType[textureUnit], 0);
      mActiveTextureType[textureUnit] = GL_ZERO;
   }
}

void GFXGLDevice::setClipRect( const RectI &inRect )
{
   AssertFatal(mCurrentRT.isValid(), "GFXGLDevice::setClipRect - must have a render target set to do any rendering operations!");

   // Clip the rect against the renderable size.
   Point2I size = mCurrentRT->getSize();
   RectI maxRect(Point2I(0,0), size);
   mClip = inRect;
   mClip.intersect(maxRect);

   static Point4F pt;
   F32 l = F32(mClip.point.x);
   F32 r = F32(mClip.point.x + mClip.extent.x);
   F32 b = F32(mClip.point.y + mClip.extent.y);
   F32 t = F32(mClip.point.y);

   // Set up projection matrix,
   //static Point4F pt;
   pt.set(2.0f / (r - l), 0.0f, 0.0f, 0.0f);
   mProjectionMatrix.setColumn(0, pt);

   pt.set(0.0f, 2.0f / (t - b), 0.0f, 0.0f);
   mProjectionMatrix.setColumn(1, pt);

   pt.set(0.0f, 0.0f, 1.0f, 0.0f);
   mProjectionMatrix.setColumn(2, pt);

   pt.set((l + r) / (l - r), (t + b) / (b - t), 1.0f, 1.0f);
   mProjectionMatrix.setColumn(3, pt);

   MatrixF mTempMatrix(true);
   setViewMatrix( mTempMatrix );
   setWorldMatrix( mTempMatrix );

   // Set the viewport to the clip rect
   RectI viewport(mClip.point.x, mClip.point.y, mClip.extent.x, mClip.extent.y);
   setViewport(viewport);
}

/// Creates a state block object based on the desc passed in.  This object
/// represents an immutable state.
GFXStateBlockRef GFXGLDevice::createStateBlockInternal(const GFXStateBlockDesc& desc)
{
   return GFXStateBlockRef(new GFXGLStateBlock(desc));
}

/// Activates a stateblock
void GFXGLDevice::setStateBlockInternal(GFXStateBlock* block, bool force)
{
   AssertFatal(dynamic_cast<GFXGLStateBlock*>(block), "GFXGLDevice::setStateBlockInternal - Incorrect stateblock type for this device!");
   GFXGLStateBlock* glBlock = static_cast<GFXGLStateBlock*>(block);
   GFXGLStateBlock* glCurrent = static_cast<GFXGLStateBlock*>(mCurrentStateBlock.getPointer());
   if (force)
      glCurrent = NULL;

   glBlock->activate(glCurrent); // Doesn't use current yet.
   mCurrentGLStateBlock = glBlock;
}

//------------------------------------------------------------------------------

GFXTextureTarget * GFXGLDevice::allocRenderToTextureTarget(bool genMips)
{
   GFXGLTextureTarget *targ = new GFXGLTextureTarget(genMips);
   targ->registerResourceWithDevice(this);
   return targ;
}

GFXFence * GFXGLDevice::createFence()
{
   GFXFence* fence = _createPlatformSpecificFence();
   if(!fence)
      fence = new GFXGeneralFence( this );

   fence->registerResourceWithDevice(this);
   return fence;
}

GFXOcclusionQuery* GFXGLDevice::createOcclusionQuery()
{
   GFXOcclusionQuery *query = new GFXGLOcclusionQuery( this );
   query->registerResourceWithDevice(this);
   return query;
}

void GFXGLDevice::setupGenericShaders( GenericShaderType type )
{
   AssertFatal(type != GSTargetRestore, "");

   if( mGenericShader[GSColor] == NULL )
   {
      ShaderData *shaderData;

      shaderData = new ShaderData();
      shaderData->setField("OGLVertexShaderFile", ShaderGen::smCommonShaderPath + String("/fixedFunction/gl/colorV.glsl"));
      shaderData->setField("OGLPixelShaderFile", ShaderGen::smCommonShaderPath + String("/fixedFunction/gl/colorP.glsl"));
      shaderData->setField("pixVersion", "2.0");
      shaderData->registerObject();
      mGenericShader[GSColor] =  shaderData->getShader();
      mGenericShaderBuffer[GSColor] = mGenericShader[GSColor]->allocConstBuffer();
      mModelViewProjSC[GSColor] = mGenericShader[GSColor]->getShaderConstHandle( "$modelView" );
      Sim::getRootGroup()->addObject(shaderData);

      shaderData = new ShaderData();
      shaderData->setField("OGLVertexShaderFile", ShaderGen::smCommonShaderPath + String("/fixedFunction/gl/modColorTextureV.glsl"));
      shaderData->setField("OGLPixelShaderFile", ShaderGen::smCommonShaderPath + String("/fixedFunction/gl/modColorTextureP.glsl"));
      shaderData->setSamplerName("$diffuseMap", 0);
      shaderData->setField("pixVersion", "2.0");
      shaderData->registerObject();
      mGenericShader[GSModColorTexture] = shaderData->getShader();
      mGenericShaderBuffer[GSModColorTexture] = mGenericShader[GSModColorTexture]->allocConstBuffer();
      mModelViewProjSC[GSModColorTexture] = mGenericShader[GSModColorTexture]->getShaderConstHandle( "$modelView" );
      Sim::getRootGroup()->addObject(shaderData);

      shaderData = new ShaderData();
      shaderData->setField("OGLVertexShaderFile", ShaderGen::smCommonShaderPath + String("/fixedFunction/gl/addColorTextureV.glsl"));
      shaderData->setField("OGLPixelShaderFile", ShaderGen::smCommonShaderPath + String("/fixedFunction/gl/addColorTextureP.glsl"));
      shaderData->setSamplerName("$diffuseMap", 0);
      shaderData->setField("pixVersion", "2.0");
      shaderData->registerObject();
      mGenericShader[GSAddColorTexture] = shaderData->getShader();
      mGenericShaderBuffer[GSAddColorTexture] = mGenericShader[GSAddColorTexture]->allocConstBuffer();
      mModelViewProjSC[GSAddColorTexture] = mGenericShader[GSAddColorTexture]->getShaderConstHandle( "$modelView" );
      Sim::getRootGroup()->addObject(shaderData);

      shaderData = new ShaderData();
      shaderData->setField("OGLVertexShaderFile", ShaderGen::smCommonShaderPath + String("/fixedFunction/gl/textureV.glsl"));
      shaderData->setField("OGLPixelShaderFile", ShaderGen::smCommonShaderPath + String("/fixedFunction/gl/textureP.glsl"));
      shaderData->setSamplerName("$diffuseMap", 0);
      shaderData->setField("pixVersion", "2.0");
      shaderData->registerObject();
      mGenericShader[GSTexture] = shaderData->getShader();
      mGenericShaderBuffer[GSTexture] = mGenericShader[GSTexture]->allocConstBuffer();
      mModelViewProjSC[GSTexture] = mGenericShader[GSTexture]->getShaderConstHandle( "$modelView" );
      Sim::getRootGroup()->addObject(shaderData);
   }

   MatrixF tempMatrix =  mProjectionMatrix * mViewMatrix * mWorldMatrix[mWorldStackSize];
   mGenericShaderBuffer[type]->setSafe(mModelViewProjSC[type], tempMatrix);

   setShader( mGenericShader[type] );
   setShaderConstBuffer( mGenericShaderBuffer[type] );
}
GFXShader* GFXGLDevice::createShader()
{
   GFXGLShader* shader = new GFXGLShader(this);
   shader->registerResourceWithDevice( this );
   return shader;
}

void GFXGLDevice::setShader(GFXShader *shader, bool force)
{
   if(mCurrentShader == shader && !force)
      return;

   if ( shader )
   {
      GFXGLShader *glShader = static_cast<GFXGLShader*>( shader );
      glShader->useProgram();
      mCurrentShader = shader;
   }
   else
   {
      setupGenericShaders();
   }
}

void GFXGLDevice::setShaderConstBufferInternal(GFXShaderConstBuffer* buffer)
{
   if (buffer)
   {
      PROFILE_SCOPE(GFXGLDevice_setShaderConstBufferInternal);
      AssertFatal(static_cast<GFXGLShaderConstBuffer*>(buffer), "Incorrect shader const buffer type for this device!");
      GFXGLShaderConstBuffer* oglBuffer = static_cast<GFXGLShaderConstBuffer*>(buffer);

      oglBuffer->activate(mCurrentConstBuffer);
      mCurrentConstBuffer = oglBuffer;
   }
   else
   {
      mCurrentConstBuffer = NULL;
   }
}

U32 GFXGLDevice::getNumSamplers() const
{
   return getMin((U32)GFX_TEXTURE_STAGE_COUNT,mPixelShaderVersion > 0.001f ? mMaxShaderTextures : mMaxFFTextures);
}

GFXTextureObject* GFXGLDevice::getDefaultDepthTex() const
{
   if(mWindowRT && mWindowRT->getPointer())
      return static_cast<GFXGLWindowTarget*>( mWindowRT->getPointer() )->mBackBufferDepthTex.getPointer();

   return NULL;
}

U32 GFXGLDevice::getNumRenderTargets() const
{
   return mMaxTRColors;
}

void GFXGLDevice::_updateRenderTargets()
{
   if ( mRTDirty || mCurrentRT->isPendingState() )
   {
      if ( mRTDeactivate )
      {
         mRTDeactivate->deactivate();
         mRTDeactivate = NULL;
      }

      // NOTE: The render target changes is not really accurate
      // as the GFXTextureTarget supports MRT internally.  So when
      // we activate a GFXTarget it could result in multiple calls
      // to SetRenderTarget on the actual device.
      mDeviceStatistics.mRenderTargetChanges++;

      GFXGLTextureTarget *tex = dynamic_cast<GFXGLTextureTarget*>( mCurrentRT.getPointer() );
      if ( tex )
      {
         tex->applyState();
         tex->makeActive();
      }
      else
      {
         GFXGLWindowTarget *win = dynamic_cast<GFXGLWindowTarget*>( mCurrentRT.getPointer() );
         AssertFatal( win != NULL,
                     "GFXGLDevice::_updateRenderTargets() - invalid target subclass passed!" );

         win->makeActive();

         if( win->mContext != static_cast<GFXGLDevice*>(GFX)->mContext )
         {
            mRTDirty = false;
            GFX->updateStates(true);
         }
      }

      mRTDirty = false;
   }

   if ( mViewportDirty )
   {
      glViewport( mViewport.point.x, mViewport.point.y, mViewport.extent.x, mViewport.extent.y );
      mViewportDirty = false;
   }
}

GFXFormat GFXGLDevice::selectSupportedFormat(   GFXTextureProfile* profile,
                                                const Vector<GFXFormat>& formats,
                                                bool texture,
                                                bool mustblend,
                                                bool mustfilter )
{
   for(U32 i = 0; i < formats.size(); i++)
   {
      // Single channel textures are not supported by FBOs.
      if(profile->testFlag(GFXTextureProfile::RenderTarget) && (formats[i] == GFXFormatA8 || formats[i] == GFXFormatL8 || formats[i] == GFXFormatL16))
         continue;
      if(GFXGLTextureInternalFormat[formats[i]] == GL_ZERO)
         continue;

      return formats[i];
   }

   return GFXFormatR8G8B8A8;
}

U32 GFXGLDevice::getTotalVideoMemory_GL_EXT()
{
   // Source: http://www.opengl.org/registry/specs/ATI/meminfo.txt
   if( gglHasExtension(ATI_meminfo) )
   {
      GLint mem[4] = {0};
      glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, mem);  // Retrieve the texture pool

      /* With mem[0] i get only the total memory free in the pool in KB
      *
      * mem[0] - total memory free in the pool
      * mem[1] - largest available free block in the pool
      * mem[2] - total auxiliary memory free
      * mem[3] - largest auxiliary free block
      */

      return  mem[0] / 1024;
   }

   //source http://www.opengl.org/registry/specs/NVX/gpu_memory_info.txt
   else if( gglHasExtension(NVX_gpu_memory_info) )
   {
      GLint mem = 0;
      glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &mem);
      return mem / 1024;
   }


#if defined(TORQUE_OS_WIN)
   else if( (gglHasWExtension(AMD_gpu_association)) )
   {
      // Just assume 1 AMD gpu. Who uses crossfire anyways now? And, crossfire doesn't double
      // vram anyways, so does it really matter?
      UINT id;
      if (wglGetGPUIDsAMD(1, &id) != 0)
      {
         S32 memorySize;
         if (wglGetGPUInfoAMD(id, WGL_GPU_RAM_AMD, GL_INT, 1, &memorySize) != -1)
         {
            // memory size is returned in MB
            return memorySize;
         }
      }
   }
#endif

#if defined(TORQUE_OS_LINUX)
   else if ( (gglHasXExtension(NULL, NULL, MESA_query_renderer)) )
   {
      // memory size is in mb
      U32 memorySize;
      glXQueryCurrentRendererIntegerMESA(GLX_RENDERER_VIDEO_MEMORY_MESA, &memorySize);
      return memorySize;
   }
#endif

   // No other way, sad. Probably windows Intel.
   return 0;
}

//
// Register this device with GFXInit
//
class GFXGLRegisterDevice
{
public:
   GFXGLRegisterDevice()
   {
      GFXInit::getRegisterDeviceSignal().notify(&GFXGLDevice::enumerateAdapters);
   }
};

static GFXGLRegisterDevice pGLRegisterDevice;

DefineEngineFunction(cycleResources, void, (),, "")
{
   static_cast<GFXGLDevice*>(GFX)->zombify();
   static_cast<GFXGLDevice*>(GFX)->resurrect();
}
