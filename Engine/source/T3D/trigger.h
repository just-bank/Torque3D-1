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

#ifndef _H_TRIGGER
#define _H_TRIGGER

#ifndef _GAMEBASE_H_
#include "T3D/gameBase/gameBase.h"
#endif
#ifndef _MBOX_H_
#include "math/mBox.h"
#endif
#ifndef _EARLYOUTPOLYLIST_H_
#include "collision/earlyOutPolyList.h"
#endif
#ifndef _MPOLYHEDRON_H_
#include "math/mPolyhedron.h"
#endif

class Convex;
class PhysicsBody;
class Trigger;

DefineConsoleType( TypeTriggerPolyhedron, Polyhedron )


struct TriggerData: public GameBaseData {
   typedef GameBaseData Parent;

  public:
   S32  tickPeriodMS;
   bool isClientSide;

   TriggerData();
   
   DECLARE_CONOBJECT(TriggerData);

   DECLARE_CALLBACK( void, onEnterTrigger, ( Trigger* trigger, GameBase* obj ) );
   DECLARE_CALLBACK( void, onTickTrigger, ( Trigger* trigger ) );
   DECLARE_CALLBACK( void, onLeaveTrigger, ( Trigger* trigger, GameBase* obj ) );

   bool onAdd() override;
   static void initPersistFields();
   void packData  (BitStream* stream) override;
   void unpackData(BitStream* stream) override;
};

class Trigger : public GameBase
{
   typedef GameBase Parent;

   /// Trigger polyhedron with *outward* facing normals and CCW ordered
   /// vertices.
   Polyhedron mTriggerPolyhedron;

   EarlyOutPolyList  mClippedList;
   Vector<GameBase*> mObjects;

   PhysicsBody      *mPhysicsRep;

   TriggerData*      mDataBlock;

   U32               mLastThink;
   U32               mCurrTick;
   Convex            *mConvexList;

   bool              mTripOnce;
   bool              mTripped;
   S32               mTrippedBy;

   String            mTripIf;
   String            mEnterCommand;
   String            mLeaveCommand;
   String            mTickCommand;

   static const U32 CMD_SIZE = 1024;

   void onUnmount(SceneObject* obj,S32 node) override;

  protected:
   
   enum TriggerUpdateBits
   {
      TransformMask = Parent::NextFreeMask << 0,
      PolyMask      = Parent::NextFreeMask << 1,
      EnterCmdMask  = Parent::NextFreeMask << 2,
      LeaveCmdMask  = Parent::NextFreeMask << 3,
      TickCmdMask   = Parent::NextFreeMask << 4,
      NextFreeMask  = Parent::NextFreeMask << 5,
   };

   static bool smRenderTriggers;
   bool testObject(GameBase* enter);
   bool testTrippable();
   bool testCondition();
   bool evalCmD(String*);
   void processTick(const Move *move) override;
   void interpolateTick(F32 delta) override;

   void buildConvex(const Box3F& box, Convex* convex) override;

   static bool setEnterCmd(void *object, const char *index, const char *data);
   static bool setLeaveCmd(void *object, const char *index, const char *data);
   static bool setTickCmd(void *object, const char *index, const char *data);
  public:
   Trigger();
   ~Trigger();

   // SimObject
   DECLARE_CONOBJECT(Trigger);
   DECLARE_CATEGORY("Volume");

   DECLARE_CALLBACK( void, onAdd, ( U32 objectId ) );
   DECLARE_CALLBACK( void, onRemove, ( U32 objectId ) );

   static void consoleInit();
   static void initPersistFields();
   void testObjects();
   bool onAdd() override;
   void onRemove() override;
   void onDeleteNotify(SimObject*) override;
   void inspectPostApply() override;

   // NetObject
   U32  packUpdate  (NetConnection *conn, U32 mask, BitStream* stream) override;
   void unpackUpdate(NetConnection *conn,           BitStream* stream) override;

   // SceneObject
   void setTransform(const MatrixF &mat) override;
   void prepRenderImage( SceneRenderState* state ) override;

   // GameBase
   bool onNewDataBlock( GameBaseData *dptr, bool reload ) override;

   // Trigger
   void setTriggerPolyhedron(const Polyhedron&);

   virtual void potentialEnterObject(GameBase*);
   U32       getNumTriggeringObjects() const;
   GameBase* getObject(const U32);   
   const Vector<GameBase*>& getObjects() const { return mObjects; }

   void renderObject( ObjectRenderInst *ri, SceneRenderState *state, BaseMatInstance *overrideMat );

   bool castRay(const Point3F &start, const Point3F &end, RayInfo* info) override;
};

inline U32 Trigger::getNumTriggeringObjects() const
{
   return mObjects.size();
}

inline GameBase* Trigger::getObject(const U32 index)
{
   AssertFatal(index < getNumTriggeringObjects(), "Error, out of range object index");

   return mObjects[index];
}

#endif // _H_TRIGGER

