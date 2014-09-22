//=========================================================================
//	Module: obj_Vehicle.cpp
//=========================================================================

#include "r3dPCH.h"

#if VEHICLES_ENABLED

#include "r3d.h"
#include "PhysXWorld.h"
#include "ObjManag.h"
#include "GameCommon.h"

#include "../../EclipseStudio/Sources/ObjectsCode/EFFECTS/obj_ParticleSystem.h"
#include "obj_Vehicle.h"
#include "../../EclipseStudio/Sources/UI/HUDActionUI.h"
#include "VehicleDescriptor.h"
#include "../../EclipseStudio/Sources/multiplayer/P2PMessages.h"
#include "../../EclipseStudio/Sources/ObjectsCode/Gameplay/obj_Zombie.h"
#include "../../EclipseStudio/Sources/ObjectsCode/Gameplay/obj_ZombieDummy.h"
#include "../../EclipseStudio/Sources/multiplayer/ClientGameLogic.h"

#include "ObjectsCode/AI/AI_Player.h"

#include "../../EclipseStudio/Sources/Editors/ObjectManipulator3d.h"
#include "../../EclipseStudio/Sources/ObjectsCode/Gameplay/obj_VehicleSpawn.h"
#include "../../EclipseStudio/Sources/ObjectsCode/world/Lamp.h"
#include "..\..\EclipseStudio\Sources\ui\HUDDisplay.h"

extern bool g_bEditMode;
extern ObjectManipulator3d g_Manipulator3d;
extern int CurHUDID;
extern HUDActionUI*	hudActionUI;
extern HUDDisplay*	hudMain;
extern float getWaterDepthAtPos(const r3dPoint3D& pos);

#define EXTENDED_VEHICLE_CONFIG 1

//////////////////////////////////////////////////////////////////////////

namespace
{
	void QuaternionToEulerAngles(PxQuat &q, float &xRot, float &yRot, float &zRot)
	{
		q.normalize();

		void MatrixGetYawPitchRoll ( const D3DXMATRIX & mat, float & fYaw, float & fPitch, float & fRoll );
		PxMat33 mat(q);
		D3DXMATRIX res;
		D3DXMatrixIdentity(&res);
		res._11 = mat.column0.x;
		res._12 = mat.column0.y;
		res._13 = mat.column0.z;

		res._21 = mat.column1.x;
		res._22 = mat.column1.y;
		res._23 = mat.column1.z;

		res._31 = mat.column2.x;
		res._32 = mat.column2.y;
		res._33 = mat.column2.z;

		MatrixGetYawPitchRoll(res, xRot, yRot, zRot);
	}

//////////////////////////////////////////////////////////////////////////

	/** Helper constant transformation factors */
	struct UsefulTransforms
	{
		PxQuat rotY_quat;
		D3DXMATRIX rotY_mat;

		UsefulTransforms()
		{
			D3DXQUATERNION rotY_D3D;
			D3DXQuaternionRotationYawPitchRoll(&rotY_D3D, D3DX_PI / 2, 0, 0);
			rotY_quat = PxQuat(rotY_D3D.x, rotY_D3D.y, rotY_D3D.z, rotY_D3D.w);
			D3DXMatrixRotationQuaternion(&rotY_mat, &rotY_D3D);
		}
	};
}

//////////////////////////////////////////////////////////////////////////

obj_Vehicle::obj_Vehicle()
: vd(0)
,netMover(this, 0.1f, (float)PKT_C2C_MoveSetCell_s::VEHICLE_CELL_RADIUS)
{
	m_bEnablePhysics = false;
	ObjTypeFlags |= OBJTYPE_Vehicle;
	spawner = NULL;
	m_isSerializable = false;
	ExitVehicle=false;
}

//////////////////////////////////////////////////////////////////////////

obj_Vehicle::~obj_Vehicle()
{
	g_pPhysicsWorld->m_VehicleManager->DeleteCar(vd);
	m_VehicleSnd = NULL;
}

//////////////////////////////////////////////////////////////////////////

void obj_Vehicle::UpdatePositionFromPhysx()
{
	R3DPROFILE_FUNCTION("obj_Vehicle::UpdatePositionFromPhysx");
	if (!vd)
		return;

	PxRigidDynamicFlags f = vd->vehicle->getRigidDynamicActor()->getRigidDynamicFlags();
	if ( !(f & PxRigidDynamicFlag::eKINEMATIC) )
	{

		PxTransform t = vd->vehicle->getRigidDynamicActor()->getGlobalPose();
		SetPosition(r3dVector(t.p.x, t.p.y, t.p.z));

		r3dVector angles;
		QuaternionToEulerAngles(t.q, angles.x, angles.y, angles.z);
		//	To degrees
		angles = D3DXToDegree(angles);

		SetRotationVector(angles);
	}
}

//////////////////////////////////////////////////////////////////////////

BOOL obj_Vehicle::Update() 
{

	R3DPROFILE_FUNCTION("obj_Vehicle::Update");

if (!gClientLogic().localPlayer_)
	return TRUE;

#ifndef FINAL_BUILD

	if ( g_bEditMode )
	{
		if( CurHUDID == 0 && d_drive_vehicles->GetBool() == false ) {
			if ( spawner != NULL && g_Manipulator3d.IsSelected( this ) )
			{
				spawner->SetPosition( GetPosition() );
				spawner->SetRotationVector( GetRotationVector() );
			}
		}
	}
#endif

if ( !g_bEditMode )
{
	LocalSounds();
	ExternalSounds();

	Light->SetRotationVector(GetRotationVector());
	Light->SetPosition(GetBBoxWorld().Center()+r3dPoint3D(0,2,0));
	if (Occupants<=0)
		bOn=false;
	LightOnOff();

		if (this->DamageCar>1 && m_SmallFire != NULL)
		{
			this->m_SmallFire->bKill=true;
			this->m_SmallFire=NULL;
			BombTime=0;
		}
		if (this->DamageCar>2.5 && m_ParticleSmoke != NULL)
		{
			this->m_ParticleSmoke->bKill=true;
			this->m_ParticleSmoke=NULL;
			BombTime=0;
		}
		if (!m_ParticleSmoke && this->DamageCar>=1 && this->DamageCar <= 2.5)
		{
			m_ParticleSmoke = (obj_ParticleSystem*)srv_CreateGameObject("obj_ParticleSystem", "vehicle_damage_01", GetPosition() );
			m_ParticleSmoke->RenderScale=0.5f;
		}
		if (!m_SmallFire && this->DamageCar>=0.1 && this->DamageCar<1)
		{
			if (m_ParticleSmoke)
			{
				m_ParticleSmoke->bKill=true;
				m_ParticleSmoke=NULL;
			}
					m_SmallFire = (obj_ParticleSystem*)srv_CreateGameObject("obj_ParticleSystem", "fire_cameradrone", GetPosition()+r3dPoint3D(0,2,0) );
					m_SmallFire->RenderScale=2.0f;
		}

	if (this->DamageCar <= 0.2)
	{
		if (!m_ParticleTracer)
		{
			if (m_SmallFire)
			{
				m_SmallFire->bKill=true;
				m_SmallFire=NULL;
			}
			if (m_ParticleSmoke)
			{
				m_ParticleSmoke->bKill=true;
				m_ParticleSmoke=NULL;
			}

			m_ParticleTracer = (obj_ParticleSystem*)srv_CreateGameObject("obj_ParticleSystem", "Fire_Large_01", GetPosition() );
			m_ParticleTracer->RenderScale=1.5f;
		}
		if (!m_Particlebomb && DamageCar>=0.001 && DamageCar<=0.2)// && this->Occupants>0)
		{
			m_Particlebomb = (obj_ParticleSystem*)srv_CreateGameObject("obj_ParticleSystem", "explosion_artillerybarrage_01", GetPosition() );
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/Effects/Explosions/Bomb01"),GetPosition());
			SetPosition(this->GetPosition()+r3dPoint3D(0,1.8,0));
			SetRotationVector(this->GetRotationVector()+r3dPoint3D(0,45,0));
			BombTime = r3dGetTime();
			this->DamageCar=0;

			if (!DestroyOnWater)
			ExplosionBlast(this->GetPosition());
		}

	}
	if (BombTime != 0 && m_Particlebomb)
	{
		if ((r3dGetTime() - BombTime) > 0.5)
		{
			SetRotationVector(r3dPoint3D(GetRotationVector().x,168,GetRotationVector().z));
			BombTime=0;
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/Vehicles/Crashes/Crash_Rock"),GetPosition());
		}
	}

	if (m_ParticleTracer)
	{
		m_ParticleTracer->SetPosition(GetPosition());
	}
	if (m_SmallFire)
	{
		m_SmallFire->SetPosition(GetPosition()+r3dPoint3D(0,2,0));
	}
	if (m_ParticleSmoke)
	{
		m_ParticleSmoke->SetPosition(this->GetPosition());
	}
	if (m_Particlebomb && this->Occupants>0) {
		m_Particlebomb->SetPosition(GetPosition());
	}

		if (this->DamageCar<1)
		{
			if (gClientLogic().localPlayer_->ActualVehicle != NULL)
			{
				if (gClientLogic().localPlayer_->ActualVehicle == this && this->DamageCar<=0.50)
				{
					if (gClientLogic().localPlayer_->isPassenger())
						gClientLogic().localPlayer_->exitVehicleHowPassenger();
					else if (gClientLogic().localPlayer_->isInVehicle())
						gClientLogic().localPlayer_->exitVehicle();
				}
			}
			this->DamageCar-=0.003f;
		}

	if (NetworkLocal) //Start of NetworkLocal
	{
		if (InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_TOGGLE_FLASHLIGHT))
		{
			bOn = !bOn;
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/NewWeapons/Melee/flashlight"), GetPosition());
		}

		if (CollisionCar!=NULL)
		{
			if (this->Occupants>0)
			{
				GameWorld().DeleteObject( CollisionCar );
				CollisionCar=NULL;
			}
		}

		if (Occupants<=0)
			enablesound=false;

		if (vd->vehicle->isInAir())
			falltime = r3dGetTime() + 1.0f;

		if (oldInAir && !vd->vehicle->isInAir() && Occupants>0 && falltime > 3.0f)
		{	    
		    if (enablesound)
			{
		    SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/Vehicles/Crashes/Crash_Rock"),GetPosition());
			DamageCar=DamageCar-0.05f;
		    falltime = 0.0f;
			}
			enablesound=true;
		}

		oldInAir = vd->vehicle->isInAir();
		float Speed = vd->vehicle->computeForwardSpeed()*2;

		if (oldSpeed > 10)
		{
			if (abs(Speed)<(oldSpeed/2))
			{
				SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/Vehicles/Crashes/Crash_Metal"),GetPosition());
				DamageCar=DamageCar-0.01f;
				Collobject=true; 
			}
		}
		oldSpeed=abs(Speed);
		oldInAir = vd->vehicle->isInAir();

		for( GameObject* obj = GameWorld().GetFirstObject(); obj; obj = GameWorld().GetNextObject(obj) )
		{
			if(obj->isObjType(OBJTYPE_Zombie))
			{
				obj_Zombie* zombie = (obj_Zombie*)obj;

				if (!zombie->bDead)
				{
					r3dPoint3D FrontKillCar=GetPosition();
					r3dPoint3D ReverseKillCar=GetPosition();
					FrontKillCar += r3dPoint3D( 0, ( 1 ), 0 );
					ReverseKillCar += r3dPoint3D( 0, ( 1 ), 0 );
					D3DXMATRIX mr2;
					D3DXMatrixRotationYawPitchRoll(&mr2, R3D_DEG2RAD(GetRotationVector().x), R3D_DEG2RAD(GetRotationVector().y), 0);
					r3dVector KillCarVector = r3dVector(mr2 ._31, mr2 ._32, mr2 ._33);

					FrontKillCar += KillCarVector*3;
					ReverseKillCar += -KillCarVector*3;

					float dist = (FrontKillCar - obj->GetPosition()).Length();
					float dist2 = (ReverseKillCar - obj->GetPosition()).Length();

					if(abs(dist) < 2 && abs(Speed) > 15 || abs(dist2) < 2 && abs(Speed) > 15)
					{
						SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/Vehicles/Crashes/Crash_Zombie"),GetPosition());
						zombie->DoDeath();
						obj_Player* plr = gClientLogic().localPlayer_;
						PKT_C2S_CarKill_s n;
						n.weaponID = 6;
						n.DieForExplosion = false;
						n.targetId = obj->GetNetworkID();
						p2pSendToHost(plr, &n, sizeof(n));
						DamageCar=DamageCar-0.01f;
					}
				}
			}
			else if( obj->isObjType(OBJTYPE_Human) )
			{
				obj_Player* Player= static_cast< obj_Player* > ( obj );

				if (!Player->bDead && Player->IsOnVehicle == false )
				{
					r3dPoint3D FrontKillCar=GetPosition();
					r3dPoint3D ReverseKillCar=GetPosition();
					FrontKillCar += r3dPoint3D( 0, ( 1 ), 0 );
					ReverseKillCar += r3dPoint3D( 0, ( 1 ), 0 );
					D3DXMATRIX mr2;
					D3DXMatrixRotationYawPitchRoll(&mr2, R3D_DEG2RAD(GetRotationVector().x), R3D_DEG2RAD(GetRotationVector().y), 0);
					r3dVector KillCarVector = r3dVector(mr2 ._31, mr2 ._32, mr2 ._33);

					FrontKillCar += KillCarVector*3;
					ReverseKillCar += -KillCarVector*3;

					float dist = (FrontKillCar - Player->GetPosition()).Length();
					float dist2 = (ReverseKillCar - Player->GetPosition()).Length();

					if(abs(dist) < 2 && abs(Speed) > 15 || abs(dist2) < 2 && abs(Speed) > 15)
					{
						SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/BulletHits/Hit_Death"),GetPosition());    
						PKT_C2S_CarKill_s n;
						n.weaponID = 0;
						n.DieForExplosion = false;
						n.targetId = Player->GetNetworkID();
						p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
						DamageCar=DamageCar-0.01f;
					}
				}
			}
			else if (obj->Class->Name == "obj_Barricade")
			{
				float dist = (this->GetPosition() - obj->GetPosition()).Length();
				if (dist<4 && Collobject == true)
				{
				Collobject=false;
				PKT_C2S_Temp_Damage_s n;
				n.targetId = toP2pNetId(obj->GetNetworkID());
				n.wpnIdx = 0;
				n.damagePercentage = 200;
				n.explosion_pos = this->GetPosition();
				p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
				}
			}
		}
		Collobject=false;

		if (this->Occupants>0 && gClientLogic().localPlayer_->ActualVehicle == this)
		{
			if(!hudActionUI->isActive() )
				hudActionUI->Activate();

			if(hudActionUI->isActive())
			{
				int DataDamage = 0;
				if (DamageCar>=4.5f)
					DataDamage=100;
				else if (DamageCar>=3.5f && DamageCar<4.5f)
					DataDamage=80;
				else if (DamageCar>=2.5f && DamageCar<3.5f)
					DataDamage=60;
				else if (DamageCar>=1.5f && DamageCar<2.5f)
					DataDamage=40;
				else if (DamageCar>0.0f && DamageCar<1.5f)
					DataDamage=20;
				else if (DamageCar<=0.0f )
					DataDamage=0;

				int ActualCar = 0;
				if (strcmp(this->FileName.c_str(),"data/objectsdepot/vehicles/drivable_buggy_02.sco") == 0)
				{
					ActualCar=1;
				}
				else if (strcmp(this->FileName.c_str(),"data/objectsdepot/vehicles/zombie_killer_car.sco") == 0)
				{
					ActualCar=2;
				}
				else
				{
					ActualCar=3;
				}
				if (gClientLogic().localPlayer_->isInVehicle())
					hudMain->setCarInfo(DataDamage,(int)abs(vd->vehicle->computeForwardSpeed()*2.8),(int)abs(vd->vehicle->computeForwardSpeed()*3.3),(int)GasolineCar,(int)abs(vd->vehicle->mDriveDynData.getEngineRotationSpeed()/10),ActualCar,true);
				else
					hudMain->setCarInfo(DataDamage,(int)abs(this->RPMPlayer*2.8),(int)abs(this->RPMPlayer*3.3),(int)GasolineCar,(int)abs(RotationSpeed/10),ActualCar,true);
			    hudActionUI->Deactivate();
			}
		}
		if( NetworkLocal || this->ExitVehicle )
		{
			if (GasolineCar<=0.9 || DamageCar<=0)
			      bOn = false;
			if (this->DamageCar>0 || this->ExitVehicle)
			{
				float waterDepth = getWaterDepthAtPos(GetPosition());
				if(waterDepth > 1.3f) // too deep
				{
					DamageCar=0.9f;
					DestroyOnWater=true;
				}
				if (!ExitVehicle)
				{
					if (Occupants>0)
					{
						if(GasolineCar>=0.9)
							GasolineCar=GasolineCar-0.005f;
						else
							GasolineCar=0;
					}
				}
				PKT_S2C_PositionVehicle_s n;
				if (gClientLogic().localPlayer_->ActualVehicle!=NULL)
				{
					n.spawnID = gClientLogic().localPlayer_->ActualVehicle->GetNetworkID();
					n.spawnPos= gClientLogic().localPlayer_->ActualVehicle->GetPosition();
					n.RotationPos = gClientLogic().localPlayer_->ActualVehicle->GetRotationVector();
				}
				else {
					n.spawnID = this->GetNetworkID();
					n.spawnPos= this->GetPosition();
					n.RotationPos = this->GetRotationVector();
				}
				n.OccupantsVehicle=Occupants;
				n.GasolineCar=GasolineCar;
				n.DamageCar=DamageCar;
				n.RespawnCar=false;
				n.bOn=bOn;
				n.controlData=g_pPhysicsWorld->m_VehicleManager->carControlData;
				n.timeStep=g_pPhysicsWorld->m_VehicleManager->timeStepGet;
				if (gClientLogic().localPlayer_->isInVehicle())
				{
					n.RPMPlayer=g_pPhysicsWorld->m_VehicleManager->getRealDrivenVehicle()->vd->vehicle->computeForwardSpeed();
					n.RotationSpeed=g_pPhysicsWorld->m_VehicleManager->getRealDrivenVehicle()->vd->vehicle->mDriveDynData.getEngineRotationSpeed();
				}
				else {
					n.RPMPlayer=0;
					n.RotationSpeed=0;
				}
				p2pSendToHost(this, &n, sizeof(n));
				this->ExitVehicle=false;
			}
		
			if(this->GasolineCar<=0.9 && (vd->vehicle->computeForwardSpeed()*2<1) )
			{
				if(SoundSys.isPlaying(m_VehicleSnd))
				{
					SoundSys.Stop(m_VehicleSnd);
				}
			}
		}
	} // End of NetworkLocal
	else { //Start of !NetworkLocal

			if (CollisionCar==NULL)
			{
				CollisionCar = (obj_Building*)srv_CreateGameObject("obj_Building", "data/objectsdepot/env_collision/CollisionCar.sco", GetPosition () );
				CollisionCar->SetRotationVector(GetRotationVector() + r3dPoint3D(90,0,0));
				if (this->FileName == "data/objectsdepot/vehicles/Drivable_Stryker.sco")
					CollisionCar->SetScale(r3dPoint3D(1.46f,0.61f,1.69f));
				else
					CollisionCar->SetScale(r3dPoint3D(1.31f,0.49f,1.69f));
					CollisionCar->SetObjFlags(OBJFLAG_SkipDraw);
			}

	}//End of !NetworkLocal

	if(Occupants<=0 && SoundSys.isPlaying(m_VehicleSnd))
	{
		if (HaveDriver)
		{
			HaveDriver=false;
			if (DamageCar>0 && GasolineCar>=1)
			{
				if (FileName == "data/objectsdepot/vehicles/drivable_buggy_02.sco")
					SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/DuneBuggyEngine_Stop"), GetPosition());
				else
					SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/StrykerEngine_Stop"), GetPosition());		
			}
		}
		SoundSys.Stop(m_VehicleSnd);
		m_VehicleSnd = 0;
	}
}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////

void obj_Vehicle::ExternalSounds()
{
	R3DPROFILE_FUNCTION("ExternalSounds");
	if (NetworkLocal) return;
	if (Occupants<1) return;

	if (Occupants>0 && HaveDriver==false)
	{
		if (GasolineCar>0)
		{
			if (FileName == "data/objectsdepot/vehicles/drivable_buggy_02.sco")
			{
				SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/DuneBuggyEngine_Start"), GetPosition());
			}
			else {
				SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/StrykerEngine_Start"), GetPosition());
			}
		}
		HaveDriver=true;
	}

	float Speed = RPMPlayer*2;

	if (oldSpeed > 10)
	{
		if (abs(Speed)<(oldSpeed/2))
		{
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/Vehicles/Crashes/Crash_Metal"),GetPosition());
		}
	}
	oldSpeed=abs(Speed);

	if (!m_VehicleSnd)
	{
		if (FileName == "data/objectsdepot/vehicles/drivable_buggy_02.sco")
			m_VehicleSnd = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/DuneBuggyEngineLoop"), GetPosition());
		else 
			m_VehicleSnd = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/StrykerEngineLoop"), GetPosition());
	}

	if (FileName == "data/objectsdepot/vehicles/drivable_buggy_02.sco")
	{
		SoundSys.SetSoundPos(m_VehicleSnd, GetPosition());
		rpm = R3D_MIN(abs(RPMPlayer*170) , 9000.0f);
		SoundSys.SetParamValue(m_VehicleSnd, "rpm", rpm);
	}
	else {
		SoundSys.SetSoundPos(m_VehicleSnd, GetPosition());
		rpm = R3D_MIN(abs(RPMPlayer*250) , 9000.0f);
		SoundSys.SetParamValue(m_VehicleSnd, "rpm", rpm);
	}
}

void obj_Vehicle::LocalSounds()
{
	R3DPROFILE_FUNCTION("LocalSounds");

	if (!NetworkLocal) return;
	if (Occupants<1) return;
	if (!gClientLogic().localPlayer_) return;
	if (!gClientLogic().localPlayer_->isInVehicle()) return;

	if (!m_VehicleSnd)
	{
		if (FileName == "data/objectsdepot/vehicles/drivable_buggy_02.sco")
			m_VehicleSnd = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/DuneBuggyEngineLoop"), GetPosition());
		else 
			m_VehicleSnd = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/StrykerEngineLoop"), GetPosition());
	}

	if (FileName == "data/objectsdepot/vehicles/drivable_buggy_02.sco")
	{
		SoundSys.SetSoundPos(m_VehicleSnd, GetPosition());
		rpm = R3D_MIN(abs(vd->vehicle->computeForwardSpeed()*170) , 9000.0f);
		SoundSys.SetParamValue(m_VehicleSnd,"rpm",rpm);
	}
	else {
		SoundSys.SetSoundPos(m_VehicleSnd, GetPosition());
		rpm = R3D_MIN(abs(vd->vehicle->computeForwardSpeed()*250) , 9000.0f);
		SoundSys.SetParamValue(m_VehicleSnd,"rpm",rpm);
	}
}

void obj_Vehicle::ApplyDamage(int vehicleID, int weaponID)
{
	GameObject* from = GameWorld().GetNetworkObject(vehicleID);
	if(from)
	{
		bool exitboon = false;
		obj_Vehicle* Vehicle = (obj_Vehicle*)from;
		if (Vehicle->DamageCar<1)
			return;

	if (gClientLogic().localPlayer_)
	{
		if (gClientLogic().localPlayer_->ActualVehicle!= NULL)
		{
			if (gClientLogic().localPlayer_->ActualVehicle != Vehicle)
				return;

			if (weaponID == 100999 || weaponID == 101310 )
			{
				if (gClientLogic().localPlayer_->ActualVehicle->DamageCar>2.6)
				{
					gClientLogic().localPlayer_->ActualVehicle->DamageCar=2.5f;
					return;
				}
				else
				{
					gClientLogic().localPlayer_->ActualVehicle->DamageCar=0.99f;
					Vehicle->DamageCar=gClientLogic().localPlayer_->ActualVehicle->DamageCar;
					Vehicle->ExitVehicle=true;
					exitboon=true;
					/*if (gClientLogic().localPlayer_->isPassenger())
						gClientLogic().localPlayer_->exitVehicleHowPassenger();
					else if (gClientLogic().localPlayer_->isInVehicle())
						gClientLogic().localPlayer_->exitVehicle();*/
				}
			}
			else
			{
				if (weaponID == 9999999)
					gClientLogic().localPlayer_->ActualVehicle->DamageCar-=0.001f;
				else
					gClientLogic().localPlayer_->ActualVehicle->DamageCar-=0.1f;
				
				if (gClientLogic().localPlayer_->ActualVehicle->DamageCar<=0.50)
				{
					Vehicle->DamageCar=gClientLogic().localPlayer_->ActualVehicle->DamageCar;
					Vehicle->ExitVehicle=true;
					exitboon=true;
					/*if (gClientLogic().localPlayer_->isPassenger())
						gClientLogic().localPlayer_->exitVehicleHowPassenger();
					else if (gClientLogic().localPlayer_->isInVehicle())
						gClientLogic().localPlayer_->exitVehicle();*/
				}
				else {
					return;
				}
			}
		  return;
		}
	}
	if (Vehicle->Occupants<=0 && exitboon == false)
	{
		if (weaponID == 100999)
		{
			if (Vehicle->DamageCar>2.6)
				Vehicle->DamageCar=2.5f;
			else
				Vehicle->DamageCar=0.99f;
		}
		else
		Vehicle->DamageCar-=0.1f;

		Vehicle->ExitVehicle=true;
		return;
	}
		PKT_C2S_CarKill_s n;
		n.DieForExplosion = false;
		n.targetId = vehicleID;
		n.weaponID = weaponID;
		p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
   }
}
void obj_Vehicle::ExplosionBlast(r3dPoint3D pos)
{
	for( GameObject* obj = GameWorld().GetFirstObject(); obj; obj = GameWorld().GetNextObject(obj) )
	{
		if(obj->isObjType(OBJTYPE_Zombie))
		{
			obj_Zombie* zombie = (obj_Zombie*)obj;

			if (!zombie->bDead)
			{
				float dist = (pos - zombie->GetPosition()).Length();
				if(dist < 9)
				{
					SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/Vehicles/Crashes/Crash_Zombie"),pos);
					obj_Player* plr = gClientLogic().localPlayer_;
					PKT_C2S_CarKill_s n;
					n.weaponID = 0;
					n.DieForExplosion = false;
					n.targetId = zombie->GetNetworkID();
					p2pSendToHost(plr, &n, sizeof(n));
				}
			}
		}		
	}

		float dist = (pos - gClientLogic().localPlayer_->GetPosition()).Length();
		if(dist < 9)
		{
			if (gClientLogic().localPlayer_->isInVehicle() || gClientLogic().localPlayer_->isPassenger())
				return;
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/BulletHits/Hit_Death"),GetPosition());  
			PKT_C2S_CarKill_s n;
			if (!DestroyOnWater)
				n.DieForExplosion = true;
			else
				n.DieForExplosion = false;
			n.weaponID = 0;
			n.targetId = gClientLogic().localPlayer_->GetNetworkID();
			p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
		}

}

void obj_Vehicle::LightOnOff()
{
if ( !g_bEditMode )
{
	if (bOn)
	{
		Light->LT.Intensity = 4.0f;
		Light->innerRadius = 30.0f;
		Light->outerRadius = 40.0f;
	}
	else
	{
		Light->LT.Intensity = 0.0f;
		Light->innerRadius = 0.0f;
		Light->outerRadius = 0.0f;
	}
}
}

BOOL obj_Vehicle::OnCreate()
{
	R3DPROFILE_FUNCTION("obj_Vehicle::OnCreate");

	m_ActionUI_Title = gLangMngr.getString("$Vehicle");
	m_ActionUI_Msg = gLangMngr.getString("$EnterVehicle");

	if (!parent::OnCreate())
		return FALSE;

	r3dMesh *m = MeshLOD[0];
	m_VehicleSnd = 0;
	HaveDriver = false;

	if (!m)
		return FALSE;

	vd = g_pPhysicsWorld->m_VehicleManager->CreateVehicle(m);
	if (vd)
	{
		//	Set position and orientation for car
		SwitchToDrivable(false);
		SyncPhysicsPoseWithObjectPose();
		r3dBoundBox bb = GetBBoxLocal();
		std::swap(bb.Size.x, bb.Size.z);
		std::swap(bb.Org.x, bb.Org.z);
		SetBBoxLocal(bb);
		vd->owner = this;
		othershavesound=false;
		enablesound=false;
		ExitVehicle=false;
		Collobject=false;
		if ( !g_bEditMode )
		{
		CollisionCar = (obj_Building*)srv_CreateGameObject("obj_Building", "data/objectsdepot/env_collision/CollisionCar.sco", GetPosition () );
		CollisionCar->SetRotationVector(GetRotationVector() + r3dPoint3D(90,0,0));
		if (this->FileName == "data/objectsdepot/vehicles/Drivable_Stryker.sco")
			CollisionCar->SetScale(r3dPoint3D(1.46f,0.61f,1.69f));
		else
			CollisionCar->SetScale(r3dPoint3D(1.31f,0.49f,1.69f));
		CollisionCar->SetObjFlags(OBJFLAG_SkipDraw);
		BombTime = 0;
		DestroyOnWater=false;
		m_ParticleTracer = NULL;
		m_Particlebomb = NULL;
		m_ParticleSmoke=NULL;
		m_SmallFire=NULL;
		Light = (obj_LightHelper*)srv_CreateGameObject("obj_LightHelper", "Spot", GetPosition ());
		Light->SetRotationVector(GetRotationVector());
		Light->SetPosition(GetBBoxWorld().Center());
		Light->Color = r3dColor::white;
		Light->LT.Intensity = 0.0f;
		Light->bOn = true;
		bOn=false;
		Light->innerRadius = 0.0f;
		Light->outerRadius = 0.0f;
		Light->bKilled = false;

		}
	}


	return vd != 0;
}

//////////////////////////////////////////////////////////////////////////

BOOL obj_Vehicle::OnDestroy()
{
if ( !g_bEditMode )
{
	if (m_SmallFire)
		m_SmallFire= NULL;
	if (m_ParticleSmoke)
		m_ParticleSmoke = NULL;

	if (m_ParticleTracer)
		m_ParticleTracer = NULL;
	if (Light != NULL)
	{
		Light = NULL;
	}
}
    m_VehicleSnd = NULL;

	if (vd)
		vd->owner = 0;
	return parent::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////

void obj_Vehicle::SetPosition(const r3dPoint3D& pos)
{
	parent::SetPosition(pos);
	SyncPhysicsPoseWithObjectPose();
}

//////////////////////////////////////////////////////////////////////////

void obj_Vehicle::SetRotationVector(const r3dVector& Angles)
{
	parent::SetRotationVector(Angles);
	SyncPhysicsPoseWithObjectPose();
}

//////////////////////////////////////////////////////////////////////////

void obj_Vehicle::SwitchToDrivable(bool doDrive)
{
	if (vd && vd->vehicle->getRigidDynamicActor())
	{
		vd->vehicle->getRigidDynamicActor()->setRigidDynamicFlag(PxRigidDynamicFlag::eKINEMATIC, !doDrive);
		if (doDrive) {
			if (gClientLogic().localPlayer_)
			{
				obj_Player* plr = gClientLogic().localPlayer_;
				if (plr->ActualVehicle != NULL && !plr->isPassenger())
						g_pPhysicsWorld->m_VehicleManager->DriveCar(plr->ActualVehicle->vd);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void obj_Vehicle::SyncPhysicsPoseWithObjectPose()
{
	if (!vd)
		return;

	PxRigidDynamicFlags f = vd->vehicle->getRigidDynamicActor()->getRigidDynamicFlags();
	if (!(f & PxRigidDynamicFlag::eKINEMATIC))
		return;

	r3dPoint3D pos(GetPosition());
	D3DXMATRIX rotM(GetRotationMatrix());
	D3DXQUATERNION q;
	D3DXQuaternionRotationMatrix(&q, &rotM);
	PxQuat quat(q.x, q.y, q.z, q.w);

	PxTransform carPose(PxVec3(pos.x, pos.y, pos.z), quat);
	vd->vehicle->getRigidDynamicActor()->setGlobalPose(carPose);
}

//////////////////////////////////////////////////////////////////////////

void obj_Vehicle::SetBoneMatrices()
{
	if (!vd)
		return;

	PxRigidDynamic *a = vd->vehicle->getRigidDynamicActor();
	D3DXMATRIX boneTransform;

	static const UsefulTransforms T;

	//	Init with identities
	for (int i = 0; i < vd->skl->NumBones; ++i)
	{
		r3dBone &b = vd->skl->Bones[i];
		b.CurrentTM = T.rotY_mat;
	}

	//	Retrieve vehicle wheels positions
	PxShape *shapes[VEHICLE_PARTS_COUNT] = {0};
	PxU32 sn = a->getShapes(shapes, VEHICLE_PARTS_COUNT);
	for (PxU32 i = 0; i < sn; ++i)
	{
		PxShape *s = shapes[i];
		PxU32 boneIndex = reinterpret_cast<PxU32>(s->userData);
		r3dBone &b = vd->skl->Bones[boneIndex];
		PxTransform pose = s->getLocalPose();

		PxVec3 bonePos = PxVec3(b.vRelPlacement.x, b.vRelPlacement.y, b.vRelPlacement.z);
		D3DXMATRIX toOrigin, fromOrigin, suspensionOffset;
		D3DXMatrixTranslation(&toOrigin, -bonePos.x, -bonePos.y, -bonePos.z);

		PxVec3 bonePosNew = T.rotY_quat.rotate(bonePos);
		D3DXMatrixTranslation(&fromOrigin, pose.p.x, pose.p.y, pose.p.z);

		pose.q = pose.q * T.rotY_quat;

		D3DXQUATERNION q(pose.q.x, pose.q.y, pose.q.z, pose.q.w);
		D3DXMatrixRotationQuaternion(&boneTransform, &q);

		D3DXMatrixMultiply(&boneTransform, &toOrigin, &boneTransform);
		D3DXMatrixMultiply(&boneTransform, &boneTransform, &fromOrigin);

		b.CurrentTM = boneTransform;
	}
	vd->skl->SetShaderConstants();
}

void obj_Vehicle::UpdatePositionFromRemote()
{

}
//////////////////////////////////////////////////////////////////////////

extern PxF32 &gVehicleTireGroundFriction;

#ifndef FINAL_BUILD
//#define EXTENDED_VEHICLE_CONFIG
float obj_Vehicle::DrawPropertyEditor(float scrx, float scry, float scrw, float scrh, const AClass* startClass, const GameObjects& selected)
{
	float y = scry + parent::DrawPropertyEditor(scrx, scry, scrw, scrh, startClass, selected );

	if( !IsParentOrEqual( &ClassData, startClass ) || !vd)
		return y;

	y += 10.0f;
	y += imgui_Static(scrx, y, "Vehicle physics configuration");
#ifdef EXTENDED_VEHICLE_CONFIG
	y += imgui_Value_Slider(scrx, y, "Friction", &gVehicleTireGroundFriction, 0.5f, 8.0f, "%-02.2f");

	PxVehicleAckermannGeometryData &ad = vd->ackermannData;
	y += 5.0f;
	y += imgui_Static(scrx, y, "Chassis:");
	y += imgui_Value_Slider(scrx, y, "Mass", &vd->chassisData.mMass, 100.0f, 5000.0f, "%-02.2f");
	y += imgui_Value_Slider(scrx, y, "Ackerman accuracy", &ad.mAccuracy, 0.0f, 1.0f, "%-02.2f");
#endif

	PxVehicleEngineData &ed = vd->engineData;
	y += 5.0f;
	y += imgui_Static(scrx, y, "Engine:");
	y += imgui_Value_Slider(scrx, y, "Peak torque", &ed.mPeakTorque, 100.0f, 5000.0f, "%-02.f");
	float x = ed.mMaxOmega / 0.104719755f;
	y += imgui_Value_Slider(scrx, y, "Max RPM", &x, 0.0f, 15000.0f, "%-02.0f");
	ed.mMaxOmega = x * 0.104719755f;

#ifdef EXTENDED_VEHICLE_CONFIG
	y += 5.0f;
	y += imgui_Static(scrx, y, "Gears:");
	PxVehicleGearsData &gd = vd->gearsData;
	y += imgui_Value_Slider(scrx, y, "Switch time", &gd.mSwitchTime, 0.0f, 3.0f, "%-02.2f");

	PxVehicleDifferential4WData &dd = vd->diffData;
	y += 5.0f;
	y += imgui_Static(scrx, y, "Differential:");
	y += imgui_Value_Slider(scrx, y, "Front-rear split", &dd.mFrontRearSplit, 0.0f, 1.0f, "%-02.3f");
#endif
	y += 10.0f;
	y += imgui_Static(scrx, y, "Select wheel:");

	static int currentWheel = 2;
	if (imgui_Button(scrx, y, 80.0f, 30.0f, "Front-Left", currentWheel == 2))
		currentWheel = 2;

	if (imgui_Button(scrx + 80, y, 80.0f, 30.0f, "Front-Right", currentWheel == 3))
		currentWheel = 3;

	if (imgui_Button(scrx + 160, y, 80.0f, 30.0f, "Rear-Left", currentWheel == 0))
		currentWheel = 0;

	if (imgui_Button(scrx + 240, y, 80.0f, 30.0f, "Rear-Right", currentWheel == 1))
		currentWheel = 1;

	y += 30.0f;

	VehicleDescriptor::WheelData &wd = vd->wheelsData[currentWheel];

	y += 5.0f;
#ifdef EXTENDED_VEHICLE_CONFIG
	y += imgui_Value_Slider(scrx, y, "Mass", &wd.wheelData.mMass, 1.0f, 100.0f, "%-02.3f");
	y += imgui_Value_Slider(scrx, y, "Spring max compression", &wd.suspensionData.mMaxCompression, 0.0f, 2.0f, "%-02.3f");
	y += imgui_Value_Slider(scrx, y, "Spring max droop", &wd.suspensionData.mMaxDroop, 0.0f, 2.0f, "%-02.3f");
#endif
	y += imgui_Value_Slider(scrx, y, "Break torque", &wd.wheelData.mMaxBrakeTorque, 0.0f, 25000.0f, "%-02.0f");
	float f = R3D_RAD2DEG(wd.wheelData.mMaxSteer);
	y += imgui_Value_Slider(scrx, y, "Steer angle", &f, 0.0f, 90.0f, "%-02.2f");
	wd.wheelData.mMaxSteer = R3D_DEG2RAD(f);
	if (currentWheel >= 2)
	{
		vd->wheelsData[(currentWheel + 1) % 2 + 2].wheelData.mMaxSteer = wd.wheelData.mMaxSteer;
	}
	float z = R3D_RAD2DEG(wd.wheelData.mToeAngle);
	wd.wheelData.mToeAngle = R3D_DEG2RAD(z);
#ifdef EXTENDED_VEHICLE_CONFIG
	y += imgui_Value_Slider(scrx, y, "Spring strength", &wd.suspensionData.mSpringStrength, 10000.0f, 50000.0f, "%-05.0f");
#endif
	y += imgui_Value_Slider(scrx, y, "Spring damper", &wd.suspensionData.mSpringDamperRate, 500.0f, 9000.0f, "%-02.0f");

	y += 10.0f;
	if (imgui_Button(scrx, y, 360.0f, 22.0f, "Save Vehicle Data"))
	{
		vd->Save(0);
	}
	y += 22.0f;

	vd->ConfigureVehicleSimulationData();
	return y - scry;
}
#endif

//////////////////////////////////////////////////////////////////////////

const bool obj_Vehicle::getExitSpace( r3dVector& outVector, int exitIndex )
{
	return vd->GetExitIndex( outVector,exitIndex);
}

//////////////////////////////////////////////////////////////////////////

BOOL obj_Vehicle::OnNetReceive(DWORD EventID, const void* packetData, int packetSize)
{
	r3d_assert(!(ObjFlags & OBJFLAG_JustCreated)); // make sure that object was actually created before processing net commands

	return TRUE;
}

#endif // VEHICLES_ENABLED