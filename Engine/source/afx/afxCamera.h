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

//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//
// Arcane-FX for MIT Licensed Open Source version of Torque 3D from GarageGames
// Copyright (C) 2015 Faust Logic, Inc.
//
// afxCamera implements a modified camera for demonstrating a third person camera style
// which is more common to RPG games than the standard FPS style camera. For the most part,
// it is a hybrid of the standard TGE camera and the third person mode of the Advanced Camera
// resource, authored by Thomas "Man of Ice" Lund. This camera implements the bare minimum
// required for demonstrating an RPG style camera and leaves tons of room for improvement. 
// It should be replaced with a better camera if possible.
//
// Advanced Camera Resource by Thomas "Man of Ice" Lund:
//   http://www.garagegames.com/index.php?sec=mg&mod=resource&page=view&qid=5471
//
//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//

#ifndef _AFX_CAMERA_H_
#define _AFX_CAMERA_H_

#ifndef _SHAPEBASE_H_
#include "game/shapeBase.h"
#endif

//----------------------------------------------------------------------------
struct afxCameraData: public ShapeBaseData {
  typedef ShapeBaseData Parent;

  static U32    sCameraCollisionMask;

  //
  DECLARE_CONOBJECT(afxCameraData);
  static void initPersistFields();
  void packData(BitStream* stream) override;
  void unpackData(BitStream* stream) override;
};


//----------------------------------------------------------------------------
// Implements a basic camera object.
class afxCamera: public ShapeBase
{
  typedef ShapeBase Parent;

  enum MaskBits {
    MoveMask     = Parent::NextFreeMask,
    SubjectMask  = Parent::NextFreeMask << 1,
    NextFreeMask = Parent::NextFreeMask << 2
  };

  struct StateDelta {
    Point3F pos;
    Point3F rot;
    VectorF posVec;
    VectorF rotVec;
  };

  enum 
  {
    ThirdPersonMode = 1,
    FlyMode         = 2,
    OrbitObjectMode = 3,
    OrbitPointMode  = 4,
    CameraFirstMode = 0,
    CameraLastMode  = 4
  };

private:
  int             mMode;
  Point3F         mRot;
  StateDelta      mDelta;

  SimObjectPtr<GameBase> mOrbitObject;
  F32             mMinOrbitDist;
  F32             mMaxOrbitDist;
  F32             mCurOrbitDist;
  Point3F         mPosition;
  bool            mObservingClientObject;

  SceneObject*    mCam_subject;
  Point3F         mCam_offset;
  Point3F         mCoi_offset;
  F32             mCam_distance;
  F32             mCam_angle;
  bool            mCam_dirty;

  bool            mFlymode_saved;
  Point3F         mFlymode_saved_pos;
  S8              mThird_person_snap_c;
  S8              mThird_person_snap_s;

  void            set_cam_pos(const Point3F& pos, const Point3F& viewRot);
  void            cam_update(F32 dt, bool on_server);

public:
  /*C*/           afxCamera();
  /*D*/           ~afxCamera();

  Point3F&        getPosition();
  void            setFlyMode();
  void            setOrbitMode(GameBase* obj, Point3F& pos, AngAxisF& rot, F32 minDist, F32 maxDist, F32 curDist, bool ownClientObject);
  void            validateEyePoint(F32 pos, MatrixF *mat);

  GameBase*       getOrbitObject() { return(mOrbitObject); }
  bool            isObservingClientObject() { return(mObservingClientObject); }

  void            snapToPosition(const Point3F& pos);
  void            setCameraSubject(SceneObject* subject);
  void            setThirdPersonOffset(const Point3F& offset);
  void            setThirdPersonOffset(const Point3F& offset, const Point3F& coi_offset);
  const Point3F&  getThirdPersonOffset() const { return mCam_offset; }
  const Point3F&  getThirdPersonCOIOffset() const { return mCoi_offset; }
  void            setThirdPersonDistance(F32 distance);
  F32             getThirdPersonDistance();
  void            setThirdPersonAngle(F32 angle);
  F32             getThirdPersonAngle();
  void            setThirdPersonMode();
  void            setThirdPersonSnap();
  void            setThirdPersonSnapClient();
  const char*     getMode();

  bool            isCamera() const override { return true; }

  DECLARE_CONOBJECT(afxCamera);
  DECLARE_CATEGORY("UNLISTED");

private:          // 3POV SECTION
  void            cam_update_3pov(F32 dt, bool on_server);
  bool            avoid_blocked_view(const Point3F& start, const Point3F& end, Point3F& newpos);
  bool            test_blocked_line(const Point3F& start, const Point3F& end);

public:           // STD OVERRIDES SECTION
  bool    onAdd() override;
  void    onRemove() override;
  void    onDeleteNotify(SimObject *obj) override;

  void    advanceTime(F32 dt) override;
  void    processTick(const Move* move) override;
  void    interpolateTick(F32 delta) override;

  void    writePacketData(GameConnection *conn, BitStream *stream) override;
  void    readPacketData(GameConnection *conn, BitStream *stream) override;
  U32     packUpdate(NetConnection *conn, U32 mask, BitStream *stream) override;
  void    unpackUpdate(NetConnection *conn, BitStream *stream) override;

  void    onCameraScopeQuery(NetConnection* cr, CameraScopeQuery*) override;
  void    getCameraTransform(F32* pos,MatrixF* mat) override;
  void    setTransform(const MatrixF& mat) override;

  void    onEditorEnable() override;
  void    onEditorDisable() override;

  F32     getCameraFov() override;
  F32     getDefaultCameraFov() override;
  bool    isValidCameraFov(F32 fov) override;
  void    setCameraFov(F32 fov) override;

  F32     getDamageFlash() const override;
  F32     getWhiteOut() const override;

  void setControllingClient( GameConnection* connection ) override;
};


#endif // _AFX_CAMERA_H_
