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
#include "gfx/Null/gfxNullDevice.h"

#include "core/strings/stringFunctions.h"
#include "gfx/gfxCubemap.h"
#include "gfx/screenshot.h"
#include "gfx/gfxPrimitiveBuffer.h"
#include "gfx/gfxCardProfile.h"
#include "gfx/gfxTextureManager.h"
#include "gfx/bitmap/gBitmap.h"
#include "core/util/safeDelete.h"


GFXAdapter::CreateDeviceInstanceDelegate GFXNullDevice::mCreateDeviceInstance(GFXNullDevice::createInstance); 

class GFXNullCardProfiler: public GFXCardProfiler
{
private:
   typedef GFXCardProfiler Parent;
public:

   ///
   const String &getRendererString() const override { static String sRS("GFX Null Device Renderer"); return sRS; }

protected:

   void setupCardCapabilities() override { };

   bool _queryCardCap(const String &query, U32 &foundResult) override{ return false; }
   bool _queryFormat(const GFXFormat fmt, const GFXTextureProfile *profile, bool &inOutAutogenMips) override { inOutAutogenMips = false; return false; }
   
public:
   void init() override
   {
      mCardDescription = "GFX Null Device Card";
      mChipSet = "NULL Device";
      mVersionString = "0";

      Parent::init(); // other code notes that not calling this is "BAD".
   };
};

class GFXNullTextureObject : public GFXTextureObject 
{
public:
   GFXNullTextureObject(GFXDevice * aDevice, GFXTextureProfile *profile); 
   ~GFXNullTextureObject() { kill(); };

#ifdef TORQUE_DEBUG
   void pureVirtualCrash() override {}
#endif

   GFXLockedRect * lock( U32 mipLevel = 0, RectI *inRect = NULL ) override { return NULL; };
   void unlock( U32 mipLevel = 0) override {};
   bool copyToBmp(GBitmap *) override { return false; };

   void zombify() override {}
   void resurrect() override {}
};

GFXNullTextureObject::GFXNullTextureObject(GFXDevice * aDevice, GFXTextureProfile *profile) :
   GFXTextureObject(aDevice, profile) 
{
   mProfile = profile;
   mTextureSize.set( 0, 0, 0 );
}

class GFXNullTextureManager : public GFXTextureManager
{
public:
   GFXTextureObject* createTexture(GBitmap* bmp, const String& resourceName, GFXTextureProfile* profile, bool deleteBmp) override { return nullptr; } // _createNullTextureObject();}
   GFXTextureObject* createTexture(DDSFile* dds, GFXTextureProfile* profile, bool deleteDDS) override { return nullptr; }
   GFXTextureObject* createTexture(const Torque::Path& path, GFXTextureProfile* profile) override { return nullptr; }
   GFXTextureObject* createTexture(U32 width, U32 height, void* pixels, GFXFormat format, GFXTextureProfile* profile) override { return nullptr; }
   GFXTextureObject* createTexture(U32 width, U32 height, U32 depth, GFXFormat format, GFXTextureProfile* profile, U32 numMipLevels = 1) override { return nullptr; }
   GFXTextureObject* createTexture(U32 width, U32 height, GFXFormat format, GFXTextureProfile* profile, U32 numMipLevels, S32 antialiasLevel) override { return nullptr; }
   GFXTextureObject* createCompositeTexture(GBitmap* bmp[4], U32 inputKey[4], const String& resourceName, GFXTextureProfile* profile, bool deleteBmp) override { return nullptr; }
protected:
      GFXTextureObject *_createTextureObject( U32 height, 
                                                      U32 width, 
                                                      U32 depth, 
                                                      GFXFormat format, 
                                                      GFXTextureProfile *profile, 
                                                      U32 numMipLevels, 
                                                      bool forceMips = false, 
                                                      S32 antialiasLevel = 0, 
                                                      GFXTextureObject *inTex = NULL ) override
      { 
         GFXNullTextureObject *retTex;
         if ( inTex )
         {
            AssertFatal( dynamic_cast<GFXNullTextureObject*>( inTex ), "GFXNullTextureManager::_createTexture() - Bad inTex type!" );
            retTex = static_cast<GFXNullTextureObject*>( inTex );
         }      
         else
         {
            retTex = new GFXNullTextureObject( GFX, profile );
            retTex->registerResourceWithDevice( GFX );
         }

         SAFE_DELETE( retTex->mBitmap );
         retTex->mBitmap = new GBitmap(width, height);
         return retTex;
      };

      /// Load a texture from a proper DDSFile instance.
      bool _loadTexture(GFXTextureObject *texture, DDSFile *dds) override{ return true; };

      /// Load data into a texture from a GBitmap using the internal API.
      bool _loadTexture(GFXTextureObject *texture, GBitmap *bmp) override{ return true; };

      /// Load data into a texture from a raw buffer using the internal API.
      ///
      /// Note that the size of the buffer is assumed from the parameters used
      /// for this GFXTextureObject's _createTexture call.
      bool _loadTexture(GFXTextureObject *texture, void *raw) override{ return true; };

      /// Refresh a texture using the internal API.
      bool _refreshTexture(GFXTextureObject *texture) override{ return true; };

      /// Free a texture (but do not delete the GFXTextureObject) using the internal
      /// API.
      ///
      /// This is only called during zombification for textures which need it, so you
      /// don't need to do any internal safety checks.
      bool _freeTexture(GFXTextureObject *texture, bool zombify=false) override { return true; };

      virtual U32 _getTotalVideoMemory() { return 0; };
      virtual U32 _getFreeVideoMemory() { return 0; };
};

class GFXNullCubemap : public GFXCubemap
{
   friend class GFXDevice;
private:
   // should only be called by GFXDevice
   void setToTexUnit( U32 tuNum ) override { };

public:
   void initStatic( GFXTexHandle *faces ) override { };
   void initStatic( DDSFile *dds ) override { };
   void initDynamic( U32 texSize, GFXFormat faceFormat = GFXFormatR8G8B8A8, U32 mipLevels = 0) override { };
   void generateMipMaps() override {}
   U32 getSize() const override { return 0; }
   GFXFormat getFormat() const override { return GFXFormatR8G8B8A8; }

   virtual ~GFXNullCubemap(){};

   void zombify() override {}
   void resurrect() override {}
};

class GFXNullCubemapArray : public GFXCubemapArray
{
   friend class GFXDevice;
private:
   // should only be called by GFXDevice
   void setToTexUnit(U32 tuNum) override { };

public:
   void init(GFXCubemapHandle *cubemaps, const U32 cubemapCount) override { };
   void init(const U32 cubemapCount, const U32 cubemapFaceSize, const GFXFormat format) override { };
   void updateTexture(const GFXCubemapHandle &cubemap, const U32 slot) override { };
   void copyTo(GFXCubemapArray *pDstCubemap) override { }
   virtual ~GFXNullCubemapArray() {};
   void zombify() override {}
   void resurrect() override {}
};

class GFXNullTextureArray : public GFXTextureArray
{
public:
   void zombify() override {}
   void resurrect() override {}
   void Release() override {}
   void setToTexUnit(U32 tuNum) override { }
   void init() override { }

protected:
   void _setTexture(const GFXTexHandle& texture, U32 slot) override { }
};

class GFXNullVertexBuffer : public GFXVertexBuffer 
{
   unsigned char* tempBuf;
public:
   GFXNullVertexBuffer( GFXDevice *device, 
                        U32 numVerts, 
                        const GFXVertexFormat *vertexFormat, 
                        U32 vertexSize, 
                        GFXBufferType bufferType ) :
      GFXVertexBuffer(device, numVerts, vertexFormat, vertexSize, bufferType) {tempBuf =NULL;};
   void lock(U32 vertexStart, U32 vertexEnd, void **vertexPtr) override;
   void unlock() override;
   void prepare() override;

   void zombify() override {}
   void resurrect() override {}
};

void GFXNullVertexBuffer::lock(U32 vertexStart, U32 vertexEnd, void **vertexPtr) 
{
   tempBuf = new unsigned char[(vertexEnd - vertexStart) * mVertexSize];
   *vertexPtr = (void*) tempBuf;
   lockedVertexStart = vertexStart;
   lockedVertexEnd   = vertexEnd;
}

void GFXNullVertexBuffer::unlock() 
{
   delete[] tempBuf;
   tempBuf = NULL;
}

void GFXNullVertexBuffer::prepare() 
{
}

class GFXNullPrimitiveBuffer : public GFXPrimitiveBuffer
{
private:
   U16* temp;
public:
   GFXNullPrimitiveBuffer( GFXDevice *device, 
                           U32 indexCount, 
                           U32 primitiveCount, 
                           GFXBufferType bufferType ) :
      GFXPrimitiveBuffer(device, indexCount, primitiveCount, bufferType), temp( NULL ) {};

   void lock(U32 indexStart, U32 indexEnd, void **indexPtr) override; ///< locks this primitive buffer for writing into
   void unlock() override; ///< unlocks this primitive buffer.
   void prepare() override { };  ///< prepares this primitive buffer for use on the device it was allocated on

   void zombify() override {}
   void resurrect() override {}
};

void GFXNullPrimitiveBuffer::lock(U32 indexStart, U32 indexEnd, void **indexPtr)
{
   temp = new U16[indexEnd - indexStart];
   *indexPtr = temp;
}

void GFXNullPrimitiveBuffer::unlock() 
{
   delete[] temp;
   temp = NULL;
}

//
// GFXNullStateBlock
//
class GFXNullStateBlock : public GFXStateBlock
{
public:
   /// Returns the hash value of the desc that created this block
   U32 getHashValue() const override { return 0; };

   /// Returns a GFXStateBlockDesc that this block represents
   const GFXStateBlockDesc& getDesc() const override { return mDefaultDesc; }

   //
   // GFXResource
   //
   void zombify() override { }
   /// When called the resource should restore all device sensitive information destroyed by zombify()
   void resurrect() override { }
private:
   GFXStateBlockDesc mDefaultDesc;
};

//
// GFXNullDevice
//

GFXDevice *GFXNullDevice::createInstance( U32 adapterIndex )
{
   return new GFXNullDevice();
}

GFXNullDevice::GFXNullDevice()
{
   clip.set(0, 0, 800, 800);

   mTextureManager = new GFXNullTextureManager();
   gScreenShot = new ScreenShot();
   mCardProfiler = new GFXNullCardProfiler();
   mCardProfiler->init();
}

GFXNullDevice::~GFXNullDevice()
{
}

GFXVertexBuffer *GFXNullDevice::allocVertexBuffer( U32 numVerts, 
                                                   const GFXVertexFormat *vertexFormat,
                                                   U32 vertSize, 
                                                   GFXBufferType bufferType,
                                                   void* data ) 
{
   return new GFXNullVertexBuffer(GFX, numVerts, vertexFormat, vertSize, bufferType);
}

GFXPrimitiveBuffer *GFXNullDevice::allocPrimitiveBuffer( U32 numIndices, 
                                                         U32 numPrimitives, 
                                                         GFXBufferType bufferType,
                                                         void* data ) 
{
   return new GFXNullPrimitiveBuffer(GFX, numIndices, numPrimitives, bufferType);
}

GFXCubemap* GFXNullDevice::createCubemap()
{ 
   return new GFXNullCubemap(); 
};

GFXCubemapArray* GFXNullDevice::createCubemapArray()
{
   return new GFXNullCubemapArray();
};

GFXTextureArray* GFXNullDevice::createTextureArray()
{
   return new GFXNullTextureArray();
};

void GFXNullDevice::enumerateAdapters( Vector<GFXAdapter*> &adapterList )
{
   // Add the NULL renderer
   GFXAdapter *toAdd = new GFXAdapter();

   toAdd->mIndex = 0;
   toAdd->mType  = NullDevice;
   toAdd->mCreateDeviceInstanceDelegate = mCreateDeviceInstance;

   GFXVideoMode vm;
   vm.bitDepth = 32;
   vm.resolution.set(800,600);
   toAdd->mAvailableModes.push_back(vm);

   dStrcpy(toAdd->mName, "GFX Null Device", GFXAdapter::MaxAdapterNameLen);

   adapterList.push_back(toAdd);
}

void GFXNullDevice::init( const GFXVideoMode &mode, PlatformWindow *window )
{
   mCardProfiler = new GFXNullCardProfiler();
   mCardProfiler->init();
}

GFXStateBlockRef GFXNullDevice::createStateBlockInternal(const GFXStateBlockDesc& desc)
{
   return new GFXNullStateBlock();
}

//
// Register this device with GFXInit
//
class GFXNullRegisterDevice
{
public:
   GFXNullRegisterDevice()
   {
      GFXInit::getRegisterDeviceSignal().notify(&GFXNullDevice::enumerateAdapters);
   }
};

static GFXNullRegisterDevice pNullRegisterDevice;
