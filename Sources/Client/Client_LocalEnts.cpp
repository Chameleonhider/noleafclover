/*
 Copyright (c) 2013 yvt
 based on code of pysnip (c) Mathias Kaerlev 2011-2012.
 
 This file is part of OpenSpades.
 
 OpenSpades is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 OpenSpades is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with OpenSpades.  If not, see <http://www.gnu.org/licenses/>.
 
 */

#include "Client.h"
#include <cstdlib>
#include <cmath>

#include <Core/ConcurrentDispatch.h>
#include <Core/Settings.h>
#include <Core/Strings.h>

#include "IAudioChunk.h"
#include "IAudioDevice.h"

#include "ClientUI.h"
#include "PaletteView.h"
#include "LimboView.h"
#include "MapView.h"
#include "Corpse.h"
#include "ClientPlayer.h"
#include "ILocalEntity.h"
#include "ChatWindow.h"
#include "CenterMessageView.h"
#include "Tracer.h"
#include "FallingBlock.h"
#include "HurtRingView.h"
#include "ParticleSpriteEntity.h"
//#include "SmokeSpriteEntity.h"

#include "World.h"
#include "Weapon.h"
#include "GameMap.h"
#include "Grenade.h"

#include "NetClient.h"

SPADES_SETTING(cg_blood, "1");
SPADES_SETTING(cg_reduceSmoke, "0");
SPADES_SETTING(cg_waterImpact, "1");
SPADES_SETTING(cg_manualFocus, "0");
SPADES_SETTING(cg_autoFocusSpeed, "0.4");
//Chameleon: I added these for configuration
//Maximum distance of particles
SPADES_SETTING(opt_particleMaxDist, "125");
//Maximum distance for detailed particles
SPADES_SETTING(opt_particleNiceDist, "1.0");
//Scales the amount of particles produced
SPADES_SETTING(opt_particleNumScale, "1.0");
//Sets the muzzleflash on or off, 0 off, 1 on cheap, 2 on default
SPADES_SETTING(opt_muzzleFlash, "1");
//Sets the muzzleflash on or off
SPADES_SETTING(r_dlights, "0");
//switches between default fire vibration and Chameleon's
//SPADES_SETTING(v_defaultFireVibration, "0"); //transfered to Client_Update
//sets block hit vibration on/off
SPADES_SETTING(v_hitVibration, "1");
//Chameleon: absolute maximum sound distance. footstep&other sound limit is 0.75 times this
SPADES_SETTING(snd_maxDistance, "");

namespace spades {
	namespace client {
		
		
#pragma mark - Local Entities / Effects
		

		void Client::RemoveAllCorpses()
		{
			SPADES_MARK_FUNCTION();
			
			corpses.clear();
			lastMyCorpse = nullptr;
		}
		
		
		void Client::RemoveAllLocalEntities()
		{
			SPADES_MARK_FUNCTION();
			
			localEntities.clear();
		}
		
		void Client::RemoveInvisibleCorpses(){
			SPADES_MARK_FUNCTION();
			
			decltype(corpses)::iterator it;
			std::vector<decltype(it)> its;
			int cnt = (int)corpses.size() - corpseSoftLimit;
			for(it = corpses.begin(); it != corpses.end(); it++){
				if(cnt <= 0)
					break;
				auto& c = *it;
				if(!c->IsVisibleFrom(lastSceneDef.viewOrigin)){
					if(c.get() == lastMyCorpse)
						lastMyCorpse = nullptr;
					its.push_back(it);
				}
				cnt--;
			}
			
			for(size_t i = 0; i < its.size(); i++)
				corpses.erase(its[i]);
			
		}
		
		
		Player *Client::HotTrackedPlayer( hitTag_t* hitFlag )
		{
			if(!world)
				return nullptr;
			Player *p = world->GetLocalPlayer();
			if(!p || !p->IsAlive())
				return nullptr; //same as ShouldRenderInThirdPersonView()
			if(ShouldRenderInThirdPersonView())
				return nullptr;
			Vector3 origin = p->GetEye();
			Vector3 dir = p->GetFront();
//NEED TO LOOK INTO THIS LATER
			World::WeaponRayCastResult result = world->WeaponRayCast(origin, dir, p);
			
			if(result.hit == false || result.player == nullptr)
				return nullptr;
			
			// don't hot track enemies (non-spectator only)
			//if(result.player->GetTeamId() != p->GetTeamId() && p->GetTeamId() < 2)
			if (result.player->GetTeamId() != p->GetTeamId() && p->GetTeamId() < 2)
				return nullptr;
			if( hitFlag ) {
				*hitFlag = result.hitFlag;
			}
			return result.player;
		}
		
		bool Client::IsMuted() {
			// prevent to play loud sound at connection
			// caused by saved packets
			return time < worldSetTime + .05f;
		}
		
//PLAYER LEAK------------------------------------------------------------------------------------------
		void Client::Leak(spades::Vector3 p, spades::Vector3 v){
			SPADES_MARK_FUNCTION();

			//basic 1x1 pixel (>64 blocks)
			Handle<IImage> img = renderer->RegisterImage("Gfx/WhitePixel.tga");

			Vector4 color = { 0.9f, 0.9f, 0.4f, 0.4f };
			//limit for particles opt_particleNumScale
			{
				ParticleSpriteEntity *ent = new ParticleSpriteEntity(this, img, color);

				v.x *= GetRandom() + 4;
				v.y *= GetRandom() + 4;
				v.z *= GetRandom() + 4;

				ent->SetTrajectory(p, v*3.f, 1.f, 0.75f);
				ent->SetRotation(GetRandom() * 6.48f);
				ent->SetRadius(0.1f+GetRandom()*0.1f, 0.01f);
				ent->SetLifeTime(3.f+3.f*GetRandom(), 0.1f, 3.f);
				ent->SetBlockHitAction(ParticleSpriteEntity::Stick);
				localEntities.emplace_back(ent);
			}
		}

//PLAYER HIT------------------------------------------------------------------------------------------
		void Client::Bleed(spades::Vector3 v, spades::Vector3 dir){
			SPADES_MARK_FUNCTION();
			
			if(!cg_blood)
				return;
			
			//Chameleon: I added these two so I can use them later.
			int distance = (v - lastSceneDef.viewOrigin).GetLength();

			//
			int distLimit = int(opt_particleMaxDist);

			//distance cull
			if (distance > distLimit)
				return;
			
			//the farther it is, the more it merges into fog
			float transparency = (distLimit - distance) / (distLimit*1.0f);

			//basic 1x1 pixel (>64 blocks)
			Handle<IImage> img = renderer->RegisterImage("Gfx/WhitePixel.tga");
			//8x8 round blob (<64 blocks)
			Handle<IImage> img2 = renderer->RegisterImage("Gfx/WhiteDisk.tga");
			//16x16 round blob (5<x<96 blocks) NOT USING IT
			//Handle<IImage> img3 = renderer->RegisterImage("Gfx/WhiteSmoke.tga");

			if (transparency < 0.1f)
				transparency = 0.1;

			Vector4 color = { 0.6f, 0.1f, 0.1f, transparency*0.8f};
			
			Vector3 vDir;
			//limit for particles opt_particleNumScale
			if (distance <= distLimit*0.5f)
			{
				for (int i = 0; i < 6 * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
					new ParticleSpriteEntity(this, img2, color);

					if (dir.GetPoweredLength() != 0)
						vDir = MakeVector3(GetRandom() - GetRandom(), GetRandom() - GetRandom(), GetRandom() - GetRandom())*4.f + dir*4.f;
					else
						vDir = MakeVector3(GetRandom() - GetRandom(), GetRandom() - GetRandom(), GetRandom() - GetRandom())*8.f;
					
					ent->SetTrajectory(v, vDir, 0.5f, 0.7f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(0.2f + GetRandom()*0.2f, 0.05f);
					ent->SetLifeTime(6.f, 0.f, 4.f);
					ent->SetBlockHitAction(ParticleSpriteEntity::Stick);
					localEntities.emplace_back(ent);
				}
			}
			else
			{
				for (int i = 0; i < 3 * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
					new ParticleSpriteEntity(this, img, color);

					if (dir.GetPoweredLength() != 0)
						vDir = MakeVector3(GetRandom() - GetRandom(), GetRandom() - GetRandom(), GetRandom() - GetRandom())*4.f + dir*4.f;
					else
						vDir = MakeVector3(GetRandom() - GetRandom(), GetRandom() - GetRandom(), GetRandom() - GetRandom())*8.f;

					ent->SetTrajectory(v, vDir, 0.5f, 0.7f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(0.4f);
					ent->SetLifeTime(4.f, 0.f, 2.f);
					ent->SetBlockHitAction(ParticleSpriteEntity::Stick);
					localEntities.emplace_back(ent);
				}
			}			
		}

//BLOCK HIT-------------------------------------------------------------------------------------------	
		void Client::EmitBlockFragments(Vector3 origin,
										IntVector3 c, bool local){
			SPADES_MARK_FUNCTION();
			
			// distance cull
			float distance = (origin - lastSceneDef.viewOrigin).GetLength();
			//Chameleon: I added this so distance is configure-able.
			int distLimit = int(opt_particleMaxDist);
			if(distance > distLimit)
				return;
			
			//the farther it is, the more it merges into fog
			float transparency = (distLimit - distance) / (distLimit*1.0f);

			if (transparency < 0.01f)
				transparency = 0.01;

			//grenade vibration
			//old code
			//grenadeVibration += 2.f / (distance + 10.f);
			if (distance <= 8.f && v_hitVibration)
			{
				float temp = (8.f - distance) / 160.f;

				if (temp > 0.03f)
					grenadeVibration += 0.03f;
				else if (temp > 0)
					grenadeVibration += temp;

				if (grenadeVibration > 0.25f)
					grenadeVibration = 0.25f;
			}

			//determines free space around for SetTrajectory
			Vector3 velBias = { 0, 0, 0 };
			if (!map->ClipBox(origin.x, origin.y, origin.z))
			{
				if (map->ClipBox(origin.x + 1.f, origin.y, origin.z)){
					velBias.x -= 1.f;
				}
				if (map->ClipBox(origin.x - 1.f, origin.y, origin.z)){
					velBias.x += 1.f;
				}
				if (map->ClipBox(origin.x, origin.y + 1.f, origin.z)){
					velBias.y -= 1.f;
				}
				if (map->ClipBox(origin.x, origin.y - 1.f, origin.z)){
					velBias.y += 1.f;
				}
				if (map->ClipBox(origin.x, origin.y, origin.z + 1.f)){
					velBias.z -= 1.f;
				}
				if (map->ClipBox(origin.x, origin.y, origin.z - 1.f)){
					velBias.z += 1.f;
				}
			}

			//idea is that for long range (> 64) use WhitePixel - fast moving
			//then move onto WhiteDisk (< 64) - half the speed of WPixel
			//for "smoke" - WhiteSmoke (5 < x < 96) - ejecting from block and slowing down 1-2 blocks away, start disappearing while slowly going down to ground, ~halfway to ground disappear

			//basic 1x1 pixel (>64 blocks)
			Handle<IImage> img = renderer->RegisterImage("Gfx/WhitePixel.tga");
			//8x8 round blob (<64 blocks)
			Handle<IImage> img2 = renderer->RegisterImage("Gfx/WhiteDisk.tga");
			//16x16 round blob (5<x<96 blocks)
			Handle<IImage> img3 = renderer->RegisterImage("Gfx/WhiteSmoke.tga");

			//gray color, so particles stand out more
			float clrGr = (c.x + c.y + c.z);
			Vector4 color = { c.x / 255.f,
				c.y / 255.f, c.z / 255.f, transparency};

			//if distLimit is 128, then distance is 64
			//closer particles //get thrown everywhere
			if (distance <= distLimit*0.5f)
			{
				for (int i = 0; i < 3.f * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, img2, color);

					//the starting value of velfac is velocity factor for more distant stuff
					//it gets decreased if you are close, so it doesn't throw it at your face
					float velfac = 1.f;
					if (distance < 3)
						velfac = velfac*0.5f;
					if (distance < 6)
						velfac = velfac*0.75f;

					ent->SetTrajectory(origin,
						(MakeVector3(GetRandom() - GetRandom(),
						GetRandom() - GetRandom(),
						GetRandom() - GetRandom())) * (10.f*velfac),
						0.5f, 0.9f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(0.2f + GetRandom()*0.1f);
					ent->SetLifeTime(2.0f, 0.f, 1.0f);
					if (distance < 32.f * float(opt_particleNiceDist))
						ent->SetBlockHitAction(ParticleSpriteEntity::BounceWeak);
					else
						ent->SetBlockHitAction(ParticleSpriteEntity::Delete);
					localEntities.emplace_back(ent);

					if (local)
						break;
				}
			}
			//farther particles //get thrown upwards, so they are more noticeable
			else //if (distance > distLimit*0.5f)
			{
				for (int i = 0; i < 3.f * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, img, color);

					ent->SetTrajectory(origin,
										(MakeVector3(GetRandom()-GetRandom(),
													GetRandom()-GetRandom(),
													GetRandom()-GetRandom())+(velBias*2.f)) * 5.f,
										0.5f, 0.9f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(0.2f + GetRandom()*0.1f);
					ent->SetLifeTime(2.0f, 0.f, 1.0f);
					//if (distance > 96.f * float(opt_particleNiceDist))
					ent->SetBlockHitAction(ParticleSpriteEntity::Delete); //it's better delete than ignore, right?
					localEntities.emplace_back(ent);
				}
			}
			
			
			//"smoke" particles //get thrown kind of upwards
			if (distance < distLimit * 0.75f * float(opt_particleNiceDist) && !cg_reduceSmoke && !local)
			{
				//color += (MakeVector4(1, 1, 1, 1) - color) * 0.2f;
				//color.w *= 0.5f;
				//for (int i = 0; i < 2 * float(opt_particleNumScale); i++)
				//{
				//	ParticleSpriteEntity *ent =
				//		new SmokeSpriteEntity(this, color, 0.f);
				//	//ADDED velBias
				//	ent->SetTrajectory(origin,
				//						(MakeVector3(GetRandom() - GetRandom(),
				//									GetRandom() - GetRandom(),
				//									GetRandom() - GetRandom()) + velBias) * 5.0f,
				//						0.25f, 0.2f);
				//	ent->SetRotation(GetRandom() * 6.48f);
				//	ent->SetRadius(0.25f + GetRandom()*0.25f, 1.0f, 0.0f);
				//	ent->SetLifeTime(0.75f + GetRandom() * 0.25f, 0.1f, 0.5f);
				//	ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
				//	localEntities.emplace_back(ent);
				//}
				//color += (MakeVector4(1, 1, 1, 1) - color) * 0.5f;

				//the starting value of velfac is velocity factor for more distant stuff
				//it gets decreased if you are close, so it doesn't throw it at your face
				float velfac = 1.f;
				if (distance < 5)
					velfac = velfac*0.75f;
				if (distance < 10)
					velfac = velfac*0.75f;

				//color = MakeVector4(clrGr/255.f, clrGr/255.f, clrGr/255.f, transparency*0.5f);
				color = MakeVector4((c.x + clrGr) / 1020.f, (c.y + clrGr) / 1020.f, (c.z + clrGr) / 1020.f, transparency*0.75f);
				for (int i = 0; i < 2.f * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, img3, color);

					ent->SetTrajectory(origin,
										(MakeVector3(GetRandom() - GetRandom(),
													GetRandom() - GetRandom(),
													GetRandom() - GetRandom()) + velBias*(velfac)) * (4.f*velfac),
										0.01f, 0.1f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(0.1f + GetRandom()*0.1f, 0.5f);
					ent->SetLifeTime(2.0f, 0.f, 1.5f);
					ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
					localEntities.emplace_back(ent);
				}
			}
			
		}

//BLOCK DESTRUCTION-----------------------------------------------------------------------------------
		void Client::EmitBlockDestroyFragments(IntVector3 blk,
											   IntVector3 c){
			SPADES_MARK_FUNCTION();
			
			Vector3 origin = {blk.x + .5f, blk.y + .5f, blk.z + .5f};
			
			//Chameleon: distance cull
			int distance = (origin - lastSceneDef.viewOrigin).GetLength();

			//Chameleon: distance is configure-able.
			int distLimit = int(opt_particleMaxDist);

			// distance cull
			if (distance > distLimit)
				return;

			//Chameleon: the farther it is, the more it merges into fog
			float transparency = (distLimit - distance) / (distLimit*1.0f);

			if (transparency < 0.01f)
				transparency = 0.01;

			//basic 1x1 pixel (>64 blocks)
			//Handle<IImage> img = renderer->RegisterImage("Gfx/WhitePixel.tga");
			//8x8 round blob (<64 blocks)
			Handle<IImage> img2 = renderer->RegisterImage("Gfx/WhiteDisk.tga");
			//16x16 round blob (5<x<96 blocks)
			Handle<IImage> img3 = renderer->RegisterImage("Gfx/WhiteSmoke.tga");

			Vector4 color = {c.x / 255.f,
				c.y / 255.f, c.z / 255.f, transparency};
			if (float(opt_particleNumScale) > 0.24f)
			{
				ParticleSpriteEntity *ent =
					new ParticleSpriteEntity(this, img2, color);

				ent->SetTrajectory(origin,
					MakeVector3(GetRandom() - GetRandom(),
					GetRandom() - GetRandom(),
					GetRandom() - GetRandom()),
					1.f, 1.f);
				ent->SetRotation(GetRandom() * 6.48f);
				ent->SetRadius(0.5f);
				ent->SetLifeTime(1.f, 0.f, 1.f);
				if (distance < 32.f * float(opt_particleNiceDist))
					ent->SetBlockHitAction(ParticleSpriteEntity::BounceWeak);
				else
					ent->SetBlockHitAction(ParticleSpriteEntity::Delete);
				localEntities.emplace_back(ent);
			}
			//stationary "smoke" after getting destroyed
			if (distance < distLimit * 0.75f * float(opt_particleNiceDist) && !cg_reduceSmoke)
			{
				ParticleSpriteEntity *ent =
					new ParticleSpriteEntity(this, img3, color);

				ent->SetTrajectory(origin, MakeVector3(0, 0, 0), 1.f, 0.f);
				ent->SetRotation(GetRandom() * 6.48f);
				ent->SetRadius(0.5f, 0.25f);
				ent->SetLifeTime(2.f, 0.f, 2.f);
				ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
				localEntities.emplace_back(ent);
			}
		}
//MUZZLEFLASH-----------------------------------------------------------------------------------------	
		void Client::MuzzleFire(spades::Vector3 origin,
								spades::Vector3 dir) 
		{
			int distance = (origin - lastSceneDef.viewOrigin).GetLength();

			if (r_dlights)
			{
				DynamicLightParam l;
				l.origin = origin;
				l.radius = 5.f;
				l.type = DynamicLightTypePoint;
				l.color = MakeVector3(3.f, 1.6f, 0.5f);
				flashDlights.push_back(l);
			}
		
			if (int(opt_muzzleFlash) == 2 && !bool(cg_reduceSmoke))
			{
				//16x16 round blob (5<x<96 blocks)
				Handle<IImage> img3 = renderer->RegisterImage("Gfx/WhiteSmoke.tga");

				Vector4 color;
				Vector3 velBias = { 0, 0, -0.5f };
				color = MakeVector4(.8f, .8f, .8f, .3f);
				// rapid smoke
				for (int i = 0; i < 1; i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, img3, color);
					ent->SetTrajectory(origin,
						(MakeVector3(GetRandom() - GetRandom(),
						GetRandom() - GetRandom(),
						GetRandom() - GetRandom()) + velBias*.5f) * 0.3f,
						1.f, 0.f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(0.4f,
						3.f, 0.01f);
					ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
					ent->SetLifeTime(0.2f + GetRandom()*0.1f, 0.f, .30f);
					localEntities.emplace_back(ent);
				}
			}
		}
//GRENADE EXPLOSION-----------------------------------------------------------------------------------
		//soundDistance of grenades is done elsewhere
		void Client::GrenadeExplosion(spades::Vector3 origin)
		{
			//Chameleon: distance cull
			int distance = (origin - lastSceneDef.viewOrigin).GetLength();

			//Chameleon: distance is configure-able.
			int distLimit = int(opt_particleMaxDist);

			// distance cull
			if (distance > distLimit)
				return;

			//Chameleon: the farther it is, the more it merges into fog
			float transparency = (distLimit - distance) / (distLimit*1.0f);

			if (transparency < 0.01f)
				transparency = 0.01;

			//basic 1x1 pixel (>64 blocks)
			Handle<IImage> img = renderer->RegisterImage("Gfx/WhitePixel.tga");
			//8x8 round blob (<64 blocks)
			Handle<IImage> img2 = renderer->RegisterImage("Gfx/WhiteDisk.tga");
			//16x16 round blob (5<x<96 blocks)
			Handle<IImage> img3 = renderer->RegisterImage("Gfx/WhiteSmoke.tga");
			
			//grenade vibration
			//old code
			//grenadeVibration += 2.f / (distance + 10.f);
			float temp = (30.f - distance) / 120.f;
			if (temp > 0)
			{
				grenadeVibration += temp;
			}
			if (grenadeVibration > 0.25f)
				grenadeVibration = 0.25f;			

			//dynamic lights (additional check for SW renderer)
			if (r_dlights)
			{
				DynamicLightParam l;
				l.origin = origin;
				l.radius = 16.f;
				l.type = DynamicLightTypePoint;
				l.color = MakeVector3(3.f, 1.6f, 0.5f);
				l.useLensFlare = false;
				flashDlights.push_back(l);
			}
			
			Vector3 colBlock = {0.2f, 0.2f, 0.2f};
			Vector3 velBias = {0,0,0};
			if (!map->ClipBox(origin.x, origin.y, origin.z))
			{
				if (map->ClipBox(origin.x + 1.f, origin.y, origin.z))
				{
					velBias.x -= 1.f;
				}
				if (map->ClipBox(origin.x - 1.f, origin.y, origin.z))
				{
					velBias.x += 1.f;
				}
				if (map->ClipBox(origin.x, origin.y + 1.f, origin.z))
				{
					velBias.y -= 1.f;
				}
				if (map->ClipBox(origin.x, origin.y - 1.f, origin.z))
				{
					velBias.y += 1.f;
				}
				if (map->ClipBox(origin.x, origin.y, origin.z + 1.f))
				{
					velBias.z -= 1.f;
					//the grenade has landed on a block. It should explode with particles ~ the colour of the block
					uint32_t col = map->GetColor(origin.x, origin.y, origin.z + 1);
					colBlock = MakeVector3((uint8_t)col / 510.f, (uint8_t)(col >> 8) / 510.f, (uint8_t)(col >> 16) / 510.f);
				}
				if (map->ClipBox(origin.x, origin.y, origin.z - 1.f))
				{
					velBias.z += 1.f;
				}
			}
			else
			{
				//display only fire particle
				Vector4 color = MakeVector4(1.f, 0.9f, .5f, transparency + 0.25f);
				ParticleSpriteEntity *ent =
					new ParticleSpriteEntity(this, img2, color);
				ent->SetTrajectory(origin,
					MakeVector3(0, 0, 0),
					1.f, 0.f);
				ent->SetRotation(GetRandom() * 6.48f);
				ent->SetRadius(1.0f, 4.f, 25.f);
				ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
				ent->SetLifeTime(0.2f, 0.f, 0.1f);
				localEntities.emplace_back(ent);

				return; //grenade is inside a block, which is performance loss when displaying other particles
			}

			Vector4 color;
			color = MakeVector4(.5f, .5f, .5f, (transparency*0.25f) + 0.25f);

			// slow smoke
			//color.w *= 0.5f;
			if (!cg_reduceSmoke && distance <= distLimit*0.5f) //if not reducing smoke - make 3 particles
			{
				color.w *= 0.5f;
				for (int i = 0; i < 2.f * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, img3, color);
					ent->SetTrajectory(origin,
										(MakeVector3(GetRandom() - GetRandom(),
													 GetRandom() - GetRandom(),
													(GetRandom() - GetRandom()) *0.5f)) * 3.f,
										0.5f, 0.f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(1.0f + GetRandom()*1.0f,	0.5f);
					ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
					ent->SetLifeTime(3.f + GetRandom()*1.f, 0.f, 2.f);
					localEntities.emplace_back(ent);
				}
			}
			else //if reducing smoke - make 1 particle
			{
				for (int i = 0; i < 1.f * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, img3, color);
					ent->SetTrajectory(origin,
										(MakeVector3(GetRandom()-GetRandom(),
													 GetRandom()-GetRandom(),
													(GetRandom()-GetRandom()) *0.5f)) * 2.f,
										0.5f, 0.f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(2.0f, 0.5f);
					ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
					ent->SetLifeTime(3.f + GetRandom()*1.f, 0.f, 2.f);
					localEntities.emplace_back(ent);
				}
			}
			
			// fragments
			color = MakeVector4(colBlock.x, colBlock.y, colBlock.z, transparency);
			//<64 blocks away
			if (distance <= distLimit*0.5f)
			{
				for (int i = 0; i < 32 * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, img2, color);

					Vector3 dir = MakeVector3(GetRandom() - GetRandom(),
											  GetRandom() - GetRandom(),
											  GetRandom() - GetRandom()) + velBias;
					ent->SetTrajectory(origin + dir * 0.5f,
										dir * 30.f,
										0.3f, 1.f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(0.2f + GetRandom()*0.1f);
					ent->SetLifeTime(3.f + GetRandom() * 2.f, 0.f, 1.f);
					//if they are close enough, they bounce, else they are deleted (ignore might look bad in buildings)
					if (distance < 32.f*float(opt_particleNiceDist))
						ent->SetBlockHitAction(ParticleSpriteEntity::BounceWeak);
					else
						ent->SetBlockHitAction(ParticleSpriteEntity::Delete);
					localEntities.emplace_back(ent);
				}
			}
			//>64 blocks away
			else //if (distance > distLimit*0.5f)
			{
				for (int i = 0; i < 16 * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, img, color);

					Vector3 dir = MakeVector3(GetRandom() - GetRandom(),
						GetRandom() - GetRandom(),
						GetRandom() - GetRandom()) + velBias*1.5f;
					ent->SetTrajectory(origin + dir * 0.5f,
						dir * 30.f,
						0.3f, 1.f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(0.2f + GetRandom()*0.1f);
					ent->SetLifeTime(3.f + GetRandom() * 2.f, 0.f, 1.f);
					ent->SetBlockHitAction(ParticleSpriteEntity::Delete);
					localEntities.emplace_back(ent);
				}
			}

			// fire smoke
			// one particle only
			color = MakeVector4(1.f, 0.9f, 0.5f, (transparency/2.f)+0.5f);
			ParticleSpriteEntity *ent =
				new ParticleSpriteEntity(this, img3, color);
			ent->SetTrajectory(origin,
								MakeVector3(0,0,0),
								1.f, 0.f);
			ent->SetRotation(GetRandom() * 6.48f);
			ent->SetRadius(1.0f, 4.f, 25.f);
			ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
			ent->SetLifeTime(0.2f, 0.f, 0.1f);
			localEntities.emplace_back(ent);
		}
		
//GRENADE EXPLOSION UNDERWATER------------------------------------------------------------------------
		void Client::GrenadeExplosionUnderwater(spades::Vector3 origin)
		{
			//Chameleon: distance cull
			int distance = (origin - lastSceneDef.viewOrigin).GetLength();

			//Chameleon: distance is configure-able.
			int distLimit = int(opt_particleMaxDist);

			// distance cull
			if (distance > distLimit)
				return;

			//Chameleon: the farther it is, the more it merges into fog
			float transparency = (distLimit - distance) / (distLimit*1.0f);

			if (transparency < 0.01f)
				transparency = 0.01;

			//grenade vibration
			float temp = (30.f - distance) / 120.f;
			if (temp > 0)
			{
				grenadeVibration += temp;
			}
			if (grenadeVibration > 0.25f)
				grenadeVibration = 0.25f;
			
			//colour of the block grenade has landed on(or in)
			Vector3 colBlock; //= { 0.2f, 0.2f, 0.2f };			
			uint32_t col = map->GetColor(origin.x, origin.y, origin.z);
			colBlock = MakeVector3((uint8_t)col / 510.f, (uint8_t)(col >> 8) / 510.f, (uint8_t)(col >> 16) / 510.f);

			Vector4 color;
			color = MakeVector4(colBlock.x + 0.6f, colBlock.y + 0.6f, colBlock.z + 0.6f, transparency);
			// water1
			//+decrease WaterExpl
			//+decrease Fluid
			//+decrease/erase slow smoke
			//-make WPixel depend on distance

			//Water Explosion
			Handle<IImage> imgWE = renderer->RegisterImage("Textures/WaterExpl.png");
			//basic 1x1 pixel (>64 blocks)
			Handle<IImage> img = renderer->RegisterImage("Gfx/WhitePixel.tga");
			//8x8 round blob (<64 blocks)
			Handle<IImage> img2 = renderer->RegisterImage("Gfx/WhiteDisk.tga");

			//vector3 velocity factor
			Vector3 velBias = { 0, 0, -1 };
			if (!map->ClipBox(origin.x, origin.y, origin.z))
			{
				if (map->ClipBox(origin.x + 1.f, origin.y, origin.z))
				{
					velBias.x -= 1.f;
				}
				if (map->ClipBox(origin.x - 1.f, origin.y, origin.z))
				{
					velBias.x += 1.f;
				}
				if (map->ClipBox(origin.x, origin.y + 1.f, origin.z))
				{
					velBias.y -= 1.f;
				}
				if (map->ClipBox(origin.x, origin.y - 1.f, origin.z))
				{
					velBias.y += 1.f;
				}
			}

			//Water Explosion
			if (!bool(cg_reduceSmoke) && distance <= distLimit*0.5f) //if not reducing smoke - make 3 particles
			{
				color.w *= 0.5f;
				for (int i = 0; i < 3.f * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, imgWE, color);
					ent->SetTrajectory(origin,
									(MakeVector3(GetRandom()-GetRandom(),
												GetRandom()-GetRandom(),
												-GetRandom()*4.f)) * 5.0f + velBias*5.f,
									0.3f, 0.4f);
					ent->SetRotation(0.f);
					ent->SetRadius(1.5f + GetRandom()*GetRandom()*0.5f, 1.5f);
					ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
					ent->SetLifeTime(3.f + GetRandom()*1.0f, 0.f, 1.0f);
					localEntities.emplace_back(ent);
				}
			}
			else //if reducing smoke - make 1 particle
			{
				for (int i = 0; i < 1.f * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, imgWE, color);
					ent->SetTrajectory(origin,
									(MakeVector3(GetRandom()-GetRandom(),
												GetRandom()-GetRandom(),
												-GetRandom()*3.f)) * 5.0f + velBias*4.f,
									0.3f, 0.4f);
					ent->SetRotation(0.f);
					ent->SetRadius(1.5f + GetRandom()*GetRandom()*0.5f, 1.5f);
					ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
					ent->SetLifeTime(3.f + GetRandom()*1.0f, 0.f, 1.0f);
					localEntities.emplace_back(ent);
				}
			}

			//OLD STUFF COMMENTED OUT
			/*
			//water expl
			for(int i = 0; i < 7; i++){
				ParticleSpriteEntity *ent =
				new ParticleSpriteEntity(this, img, color);
				ent->SetTrajectory(origin,
								   (MakeVector3(GetRandom()-GetRandom(),
												GetRandom()-GetRandom(),
												-GetRandom()*7.f)) * 2.5f,
								   .3f, .6f);
				ent->SetRotation(0.f);
				ent->SetRadius(1.5f + GetRandom()*GetRandom()*0.4f,
							   1.3f);
				ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
				ent->SetLifeTime(3.f + GetRandom()*0.3f, 0.f, .60f);
				localEntities.emplace_back(ent);
			}
			*/
			/*
			//small water droplets
			img = renderer->RegisterImage("Textures/Fluid.png");
			color.w = .9f;
			if(cg_reduceSmoke) color.w = .4f;
			for(int i = 0; i < 16; i++){
				ParticleSpriteEntity *ent =
				new ParticleSpriteEntity(this, img, color);
				ent->SetTrajectory(origin,
								   (MakeVector3(GetRandom()-GetRandom(),
												GetRandom()-GetRandom(),
												-GetRandom()*10.f)) * 3.5f,
								   1.f, 1.f);
				ent->SetRotation(GetRandom() * 6.48f);
				ent->SetRadius(0.9f + GetRandom()*GetRandom()*0.4f,
							   0.7f);
				ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
				ent->SetLifeTime(3.f + GetRandom()*0.3f, .7f, .60f);
				localEntities.emplace_back(ent);
			}
			*/			
			/*
			// slow smoke
			color.w = .4f;
			if(cg_reduceSmoke) color.w = .2f;
			for(int i = 0; i < 8; i++){
				ParticleSpriteEntity *ent =
				//new SmokeSpriteEntity(this, color, 20.f);
				ent->SetTrajectory(origin,
								   (MakeVector3(GetRandom()-GetRandom(),
												GetRandom()-GetRandom(),
												(GetRandom()-GetRandom()) * .2f)) * 2.f,
								   1.f, 0.f);
				ent->SetRotation(GetRandom() * 6.48f);
				ent->SetRadius(1.4f + GetRandom()*GetRandom()*0.8f,
							   0.2f);
				ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
				ent->SetLifeTime((cg_reduceSmoke ? 3.f : 6.f) + GetRandom() * 5.f, 0.1f, 8.f);
				localEntities.emplace_back(ent);
			}
			*/
			//END OF OLD STUFF

			// fragments
			color = MakeVector4(colBlock.x*2.f, colBlock.y*2.f, colBlock.z*2.f, transparency);
			
			//<64 blocks away
			if (distance <= distLimit*0.5f)
			{
				for (int i = 0; i < 32 * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, img2, color);

					Vector3 dir = MakeVector3(GetRandom()-GetRandom(),
											GetRandom()-GetRandom(),
											-GetRandom())*2.f + velBias;
					ent->SetTrajectory(origin + dir * 0.25f + MakeVector3(0, 0, -1.2f),
										dir * 15.f,
										0.3f, 1.f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(0.2f + GetRandom()*0.1f);
					ent->SetLifeTime(3.f + GetRandom() * 2.f, 0.f, 1.f);
					ent->SetBlockHitAction(ParticleSpriteEntity::Delete);
					localEntities.emplace_back(ent);
				}
			}
			//>64 blocks away
			else //if (distance > distLimit*0.5f)
			{
				for (int i = 0; i < 16 * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, img, color);

					Vector3 dir = MakeVector3(GetRandom()-GetRandom(),
											GetRandom()-GetRandom(),
											-GetRandom())*2.f + velBias;
					ent->SetTrajectory(origin + dir * 0.25f + MakeVector3(0, 0, -1.5f),
										dir * 20.f,
										0.3f, 1.f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(0.2f + GetRandom()*0.1f);
					ent->SetLifeTime(3.f + GetRandom() * 2.f, 0.f, 1.f);
					ent->SetBlockHitAction(ParticleSpriteEntity::Delete);
					localEntities.emplace_back(ent);
				}
			}

			//OLD fragments
			/*
			color = MakeVector4(1,1,1, 0.7f);
			for(int i = 0; i < 42; i++){
				ParticleSpriteEntity *ent =
				new ParticleSpriteEntity(this, img, color);
				Vector3 dir = MakeVector3(GetRandom()-GetRandom(),
										  GetRandom()-GetRandom(),
										  -GetRandom() * 3.f);
				dir += velBias * .5f;
				float radius = 0.1f + GetRandom()*GetRandom()*0.2f;
				ent->SetTrajectory(origin + dir * .2f + MakeVector3(0, 0, -1.2f),
								   dir * 13.f,
								   .1f + radius * 3.f, 1.f);
				ent->SetRotation(GetRandom() * 6.48f);
				ent->SetRadius(radius);
				ent->SetLifeTime(3.5f + GetRandom() * 2.f, 0.f, 1.f);
				ent->SetBlockHitAction(ParticleSpriteEntity::Delete);
				localEntities.emplace_back(ent);
			}
			*/		
			// TODO: wave?
			// Chameleon: pls no, unless it's cheap&simple (like an expanding texture on water surface)
		}
		
//BULLET HIT WATER------------------------------------------------------------------------------------
		void Client::BulletHitWaterSurface(spades::Vector3 origin)
		{
			// distance cull
			float distance = (origin - lastSceneDef.viewOrigin).GetLength();
			//Chameleon: I added this so distance is configure-able.
			int distLimit = int(opt_particleMaxDist);
			if(distance > distLimit)
				return;
			if (!cg_waterImpact)
				return;

			//the farther it is, the more it merges into fog
			float transparency = (distLimit - distance) / (distLimit*1.0f);

			if (transparency < 0.01f)
				transparency = 0.01;

			//basic 1x1 pixel (>64 blocks)
			Handle<IImage> img = renderer->RegisterImage("Gfx/WhitePixel.tga");
			//8x8 round blob (<64 blocks)
			Handle<IImage> img2 = renderer->RegisterImage("Gfx/WhiteDisk.tga");
			//16x16 round blob (5<x<96 blocks)
			Handle<IImage> img3 = renderer->RegisterImage("Gfx/WhiteSmoke.tga");

			//colour of water block bullet landed on(or in)
			Vector3 colBlock;
			uint32_t col = map->GetColor(origin.x, origin.y, origin.z+1);
			colBlock = MakeVector3((uint8_t)col / 255.f, (uint8_t)(col >> 8) / 255.f, (uint8_t)(col >> 16) / 255.f);
			Vector4 color;

			//fragment colour
			colBlock = colBlock - MakeVector3(0.1f, 0.1f, 0.2f);
			color = MakeVector4(colBlock.x + 0.1f, colBlock.y + 0.1f, colBlock.z + 0.2f, transparency + 0.25f);
			//fragments
			//if (cg_)distLimit is 128, then distance is 64
			//closer particles //get thrown everywhere
			if (distance <= distLimit*0.5f)
			{
				for (int i = 0; i < 6.f * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, img2, color);
					Vector3 dir = MakeVector3(GetRandom()-GetRandom(),
											GetRandom()-GetRandom(),
											-GetRandom()*2.f);
					ent->SetTrajectory(origin + dir * 0.2f + MakeVector3(0, 0, 0.f),
									dir * 7.5f,
									0.3f, 1.f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(0.1f+GetRandom()*0.1f);
					ent->SetLifeTime(3.f, 0.f, 1.f);
					ent->SetBlockHitAction(ParticleSpriteEntity::Delete);
					localEntities.emplace_back(ent);
				}
			}
			//farther particles //get thrown upwards, so they are more noticeable
			else //if (distance > distLimit*0.5f)
			{
				for (int i = 0; i < 3.f * float(opt_particleNumScale); i++)
				{
					ParticleSpriteEntity *ent =
						new ParticleSpriteEntity(this, img, color);
					Vector3 dir = MakeVector3(GetRandom()-GetRandom(),
											GetRandom()-GetRandom(),
											-GetRandom()*3.f);
					ent->SetTrajectory(origin + dir * 0.5f + MakeVector3(0, 0, -1.2f),
									dir * 7.5f,
									0.3f, 1.f);
					ent->SetRotation(GetRandom() * 6.48f);
					ent->SetRadius(0.2f);
					ent->SetLifeTime(3.f, 0.f, 1.f);
					ent->SetBlockHitAction(ParticleSpriteEntity::Delete);
					localEntities.emplace_back(ent);
				}
			}
			
			//smoke colour
			colBlock = colBlock / 2.f;
			color = MakeVector4(colBlock.x + 0.5f, colBlock.y + 0.5f, colBlock.z + 0.5f, transparency);
			//smoke particle, exists up to half maxDistance
			if (!bool(cg_reduceSmoke) && distance <= distLimit*0.5f && float(opt_particleNumScale) > 0.49f) //if not reducing smoke
			{
				color.w *= 0.25f;
				//for (int i = 0; i < 1.f * float(opt_particleNumScale); i++)
				//{
				ParticleSpriteEntity *ent =
					new ParticleSpriteEntity(this, img3, color);
				ent->SetTrajectory(origin,
								(MakeVector3(GetRandom() - GetRandom(),
											GetRandom() - GetRandom(),
											-GetRandom()*2.f)) * 2.f,
								0.01f, 0.1f);
				ent->SetRotation(GetRandom() * 6.48f);
				ent->SetRadius(0.1f, 1.0f);
				ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
				ent->SetLifeTime(2.f, 0.25f, 1.75f);
				localEntities.emplace_back(ent);
				//}
			}
			
			// water1
			/*
			Handle<IImage> img = renderer->RegisterImage("Textures/WaterExpl.png");
			if(cg_reduceSmoke) color.w = .2f;
			for(int i = 0; i < 2; i++){
				ParticleSpriteEntity *ent =
				new ParticleSpriteEntity(this, img, color);
				ent->SetTrajectory(origin,
								   (MakeVector3(GetRandom()-GetRandom(),
												GetRandom()-GetRandom(),
												-GetRandom()*7.f)) * 1.f,
								   .3f, .6f);
				ent->SetRotation(0.f);
				ent->SetRadius(0.6f + GetRandom()*GetRandom()*0.4f,
							   .7f);
				ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
				ent->SetLifeTime(3.f + GetRandom()*0.3f, 0.1f, .60f);
				localEntities.emplace_back(ent);
			}
			*/			
			// water2
			/*
			img = renderer->RegisterImage("Textures/Fluid.png");
			color.w = .9f;
			if(cg_reduceSmoke) color.w = .4f;
			for(int i = 0; i < 6; i++)
			{
				ParticleSpriteEntity *ent =
				new ParticleSpriteEntity(this, img3, color);
				ent->SetTrajectory(origin,
								   (MakeVector3(GetRandom()-GetRandom(),
												GetRandom()-GetRandom(),
												-GetRandom()*10.f)) * 2.f,
								   1.f, 1.f);
				ent->SetRotation(GetRandom() * 6.48f);
				ent->SetRadius(0.6f + GetRandom()*GetRandom()*0.6f,
							   0.6f);
				ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
				ent->SetLifeTime(3.f + GetRandom()*0.3f, GetRandom() * 0.3f, .60f);
				localEntities.emplace_back(ent);
			}
			*/			
			// fragments
			/*
			img = renderer->RegisterImage("Gfx/WhitePixel.tga");
			color = MakeVector4(1,1,1, 0.7f);
			for(int i = 0; i < 10; i++){
				ParticleSpriteEntity *ent =
				new ParticleSpriteEntity(this, img, color);
				Vector3 dir = MakeVector3(GetRandom()-GetRandom(),
										  GetRandom()-GetRandom(),
										  -GetRandom() * 3.f);
				float radius = 0.03f + GetRandom()*GetRandom()*0.05f;
				ent->SetTrajectory(origin + dir * .2f +
								   MakeVector3(0, 0, -1.2f),
								   dir * 5.f,
								   .1f + radius * 3.f, 1.f);
				ent->SetRotation(GetRandom() * 6.48f);
				ent->SetRadius(radius);
				ent->SetLifeTime(3.5f + GetRandom() * 2.f, 0.f, 1.f);
				ent->SetBlockHitAction(ParticleSpriteEntity::Delete);
				localEntities.emplace_back(ent);
			}
			*/
			// TODO: wave?
			//nah, unless it's just cheap expanding image
		}
		
#pragma mark - Camera Control
		//Chameleon: this should be disabled by now because i don't like candies
		enum { AutoFocusPoints = 4 };
		void Client::UpdateAutoFocus(float dt) 
		{
			if (autoFocusEnabled && world && (int)cg_manualFocus) {
				// Compute focal length
				float measureRange = tanf(lastSceneDef.fovY * .5f) * .2f;
				const Vector3 camOrigin = lastSceneDef.viewOrigin;
				const float lenScale = 1.f / lastSceneDef.viewAxis[2].GetLength();
				const Vector3 camDir = lastSceneDef.viewAxis[2].Normalize();
				const Vector3 camX = lastSceneDef.viewAxis[0].Normalize() * measureRange;
				const Vector3 camY = lastSceneDef.viewAxis[1].Normalize() * measureRange;
				
				float distances[AutoFocusPoints * AutoFocusPoints];
				std::size_t numValidDistances = 0;
				Vector3 camDir1 = camDir - camX - camY;
				const Vector3 camDX = camX * (2.f / (AutoFocusPoints - 1));
				const Vector3 camDY = camY * (2.f / (AutoFocusPoints - 1));
				for (int x = 0; x < AutoFocusPoints; ++x) {
					Vector3 camDir2 = camDir1;
					for (int y = 0; y < AutoFocusPoints; ++y) {
						float dist = RayCastForAutoFocus(camOrigin, camDir2);
						
						dist *= lenScale;
						
						if (std::isfinite(dist) && dist > 0.8f) {
							distances[numValidDistances++] = dist;
						}
						
						camDir2 += camDY;
					}
					camDir1 += camDX;
				}
				
				if (numValidDistances > 0) {
					// Take median
					std::sort(distances, distances + numValidDistances);
					
					float dist = (numValidDistances & 1) ?
						distances[numValidDistances >> 1] :
						(distances[numValidDistances >> 1] + distances[(numValidDistances >> 1) - 1]) * 0.5f;
					
					targetFocalLength = dist;
					
				}
			}
			
			// Change the actual focal length slowly
			{
				float dist = 1.f / targetFocalLength;
				float curDist = 1.f / focalLength;
				const float maxSpeed = cg_autoFocusSpeed;
				
				if (dist > curDist) {
					curDist = std::min(dist, curDist + maxSpeed * dt);
				} else {
					curDist = std::max(dist, curDist - maxSpeed * dt);
				}
				
				focalLength = 1.f / curDist;
			}
		}
		float Client::RayCastForAutoFocus(const Vector3 &origin,
								  const Vector3 &direction)
		{
			SPAssert(world);
			
			const auto &lastSceneDef = this->lastSceneDef;
			World::WeaponRayCastResult result =
				world->WeaponRayCast(origin,
									 direction,
									 nullptr);
			if (result.hit) {
				return Vector3::Dot(result.hitPos - origin, lastSceneDef.viewAxis[2]);
			}
			
			return std::nan(nullptr);
		}

	}
}
