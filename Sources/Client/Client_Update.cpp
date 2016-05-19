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

#include "World.h"
#include "Weapon.h"
#include "GameMap.h"
#include "Grenade.h"

#include "NetClient.h"

SPADES_SETTING(cg_ragdoll, "0");
SPADES_SETTING(cg_blood, "1");
SPADES_SETTING(cg_ejectBrass, "1");

SPADES_SETTING(cg_alerts, "");
//Chameleon: absolute maximum sound distance. footstep&other sound limit is 0.75 times this
SPADES_SETTING(snd_maxDistance, "");
//switches between default sprint bob and Chameleon's
SPADES_SETTING(v_defaultSprintBob, "");
//Turns off/default/turns some off center messages
SPADES_SETTING(hud_centerMessage, "");
//switches between default fire vibration and Chameleon's
//SPADES_SETTING(v_defaultFireVibration, ""); //from Client_LocalEnts
//made cg_fov refresh only when dead
SPADES_SETTING(cg_fov, "90");
//weapon scope (refresh only when dead/refill)
SPADES_SETTING(weap_scope, "1");
//weapon scope magnification (refresh only when dead/refill)
SPADES_SETTING(weap_scopeZoom, "2");

namespace spades {
	namespace client {
		
#pragma mark - World States
		
		float Client::GetSprintState() 
		{
			if(!world) return 0.f;
			if(!world->GetLocalPlayer())
				return 0.f;
			
			ClientPlayer *p = clientPlayers[(int)world->GetLocalPlayerIndex()];
			if(!p)
				return 0.f;
			return p->GetSprintState();
		}
		
		float Client::GetAimDownState() {
			if(!world) return 0.f;
			if(!world->GetLocalPlayer())
				return 0.f;
			
			ClientPlayer *p = clientPlayers[(int)world->GetLocalPlayerIndex()];
			if(!p)
				return 0.f;
			return p->GetAimDownState();
		}
		
#pragma mark - World Actions
		/** Captures the color of the block player is looking at. */
		void Client::CaptureColor() {
			if(!world) return;
			Player *p = world->GetLocalPlayer();
			if(!p) return;
			if(!p->IsAlive()) return;
			
			IntVector3 outBlockCoord;
			uint32_t col;
			if(!world->GetMap()->CastRay(p->GetEye(),
										 p->GetFront(),
										 256.f, outBlockCoord)){
				auto c = world->GetFogColor();
				col = c.x | c.y<<8 | c.z<<16;
			}else{
				col = world->GetMap()->GetColorWrapped(outBlockCoord.x,
																outBlockCoord.y,
																outBlockCoord.z);
			}
			
			IntVector3 colV;
			colV.x = (uint8_t)(col);
			colV.y = (uint8_t)(col >> 8);
			colV.z = (uint8_t)(col >> 16);
			
			p->SetHeldBlockColor(colV);
			net->SendHeldBlockColor();
		}
		
		void Client::SetSelectedTool(Player::ToolType type, bool quiet) {
			if(type == world->GetLocalPlayer()->GetTool())
				return;
			lastTool = world->GetLocalPlayer()->GetTool();
			hasLastTool = true;
			
			world->GetLocalPlayer()->SetTool(type);
			net->SendTool();

			//Chameleon: limits the distance of this sound MEDIUM
			if (!quiet && soundDistance > int(snd_maxDistance) / 2.f)
			{
				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Switch.wav");
				audioDevice->PlayLocal(c, MakeVector3(.4f, -.3f, .5f), AudioParam());
			}
		}
		
#pragma mark - World Update
		
		void Client::UpdateWorld(float dt) {
			SPADES_MARK_FUNCTION();
			
			Player* player = world->GetLocalPlayer();
			
			if(player)
			{
				
				// disable input when UI is open
				if(scriptedUI->NeedsInput())
				{
					weapInput.primary = false;
					if(player->GetTeamId() >= 2 || player->GetTool() != Player::ToolWeapon)
					{
						weapInput.secondary = false;
					}
					//Useful for later!
					playerInput = PlayerInput();
				}
				
				if(player->GetTeamId() >= 2) 
				{
					UpdateLocalSpectator(dt);
				}
				else
				{
					UpdateLocalPlayer(dt);
				}
			}
			
#if 0
			// dynamic time step
			// physics diverges from server
			world->Advance(dt);
#else
			// accurately resembles server's physics
			// but not smooth
			if(dt > 0.f)
				worldSubFrame += dt;
			
			float frameStep = 1.f / 60.f;
			while(worldSubFrame >= frameStep)
			{
				world->Advance(frameStep);
				worldSubFrame -= frameStep;
			}
#endif
			
			// update player view (doesn't affect physics/game logics)
			for(size_t i = 0; i < clientPlayers.size(); i++)
			{
				if(clientPlayers[i])
				{
					clientPlayers[i]->Update(dt);
				}
			}
			
			// corpse never accesses audio nor renderer, so
			// we can do it in the separate thread
			class CorpseUpdateDispatch: public ConcurrentDispatch{
				Client *client;
				float dt;
			public:
				CorpseUpdateDispatch(Client *c, float dt):
				client(c), dt(dt){}
				virtual void Run(){
					for(auto& c: client->corpses)
					{
						for(int i = 0; i < 4; i++)
							c->Update(dt / 4.f);
					}
				}
			};
			CorpseUpdateDispatch corpseDispatch(this, dt);
			corpseDispatch.Start();
			
			// local entities should be done in the client thread
			{
				decltype(localEntities)::iterator it;
				std::vector<decltype(it)> its;
				for(it = localEntities.begin(); it != localEntities.end(); it++){
					if(!(*it)->Update(dt))
						its.push_back(it);
				}
				for(size_t i = 0; i < its.size(); i++){
					localEntities.erase(its[i]);
				}
			}
			
			corpseDispatch.Join();
			
			if(grenadeVibration > 0.f)
			{
				grenadeVibration -= dt/2;
				if(grenadeVibration < 0.f)
					grenadeVibration = 0.f;
			}

			
			//Chameleon FOV changes
			if (player && (player->GetHealth() == 0 || player->GetTeamId() > 2))
			{
				//Checks for maximum/minimum FOV values
				if ((int)cg_fov < 10)
					cg_fov = "10";
				if ((int)cg_fov > 175)
					cg_fov = "175";

				scopeOn = (bool)weap_scope;
				scopeZoom = abs((int)weap_scopeZoom);
				FOV = (int)cg_fov;
			}
			else
			{
				//Checks for maximum/minimum FOV values
				if ((int)cg_fov < 10)
					cg_fov = "10";
				if ((int)cg_fov > 175)
					cg_fov = "175";

				scopeOn = (bool)weap_scope;
				scopeZoom = abs((int)weap_scopeZoom);
				FOV = (int)cg_fov;
			}

			//Chameleon: walk shake
			//needs changing
			if (walkProgress > 0.7f)
			{
				stepVibration += dt*0.125f;
			}
			else if (stepVibration > 0.f)
			{
				stepVibration -= dt*0.25f;
				if (stepVibration < 0.f)
				{
					stepVibration = 0;
					SwitchBFootSide();
				}
			}

			//Chameleon: drunk cam
			//(1.f - world->GetLocalPlayer()->GetHealth() / 200.f);
			if (!player)
			{
				mouseInertia = 0;
				mouseX = 0;
				mouseY = 0;

				if (mouseRoll < 0)
				{
					mouseRoll -= mouseRoll*dt*4;

					if (mouseRoll > -0.001f)
						mouseRoll = 0;
				}
				if (mouseRoll > 0)
				{
					mouseRoll -= mouseRoll*dt*4;

					if (mouseRoll < 0.001f)
						mouseRoll = 0;
				}
			}

			//Chameleon: shellshocktime decreases volume of all sounds //doesn't work, removed
			/*if (lastShellShockTime < 1)
			{
				lastShellShockTime += dt/2;
				if (lastShellShockTime > 1)
					lastShellShockTime = 1;				
			}*/

			//the less hearing you lost, the faster recovery
			if (soundDistance < int(snd_maxDistance))
			{
				soundDistance += (1 + soundDistance*15.f/int(snd_maxDistance))*dt;

				if (soundDistance > int(snd_maxDistance))
					soundDistance = snd_maxDistance;
			}

			if(hitFeedbackIconState > 0.f) 
			{
				hitFeedbackIconState -= dt * 5.f;
				if(hitFeedbackIconState < 0.f)
					hitFeedbackIconState = 0.f;
			}
			
			if(time > lastPosSentTime + 1.f &&
			   world->GetLocalPlayer()){
				Player *p = world->GetLocalPlayer();
				if(p->IsAlive() && p->GetTeamId() < 2){
					net->SendPosition();
					lastPosSentTime = time;
				}
			}
			
			
		}
		
		/** Handles movement of spectating local player. */
		void Client::UpdateLocalSpectator(float dt) {
			SPADES_MARK_FUNCTION();
			
			Vector3 lastPos = followPos;
			followVel *= powf(.3f, dt);
			followPos += followVel * dt;
			
			if(followPos.x < 0.f) {
				followVel.x = fabsf(followVel.x) * 0.2f;
				followPos = lastPos + followVel * dt;
			}
			if(followPos.y < 0.f) {
				followVel.y = fabsf(followVel.y) * 0.2f;
				followPos = lastPos + followVel * dt;
			}
			if(followPos.x > (float)GetWorld()->GetMap()->Width()) {
				followVel.x = fabsf(followVel.x) * -0.2f;
				followPos = lastPos + followVel * dt;
			}
			if(followPos.y > (float)GetWorld()->GetMap()->Height()) {
				followVel.y = fabsf(followVel.y) * -0.2f;
				followPos = lastPos + followVel * dt;
			}
			
			GameMap::RayCastResult minResult;
			float minDist = 1.e+10f;
			Vector3 minShift;
			
			// check collision
			if(followVel.GetLength() < .01){
				followPos = lastPos;
				followVel *= 0.f;
			}else{
				for(int sx = -1; sx <= 1; sx ++)
					for(int sy = -1; sy <= 1; sy++)
						for(int sz = -1; sz <= 1; sz++){
							GameMap::RayCastResult result;
							Vector3 shift = {sx*.1f, sy*.1f,sz*.1f};
							result = map->CastRay2(lastPos+shift, followPos - lastPos,
												   256);
							if(result.hit && !result.startSolid &&
							   Vector3::Dot(result.hitPos - followPos - shift,
											followPos - lastPos) < 0.f){
								   
								   float dist =  Vector3::Dot(result.hitPos - followPos - shift,
															  (followPos - lastPos).Normalize());
								   if(dist < minDist){
									   minResult = result;
									   minDist = dist;
									   minShift = shift;
								   }
							   }
						}
				
			}
			if(minDist < 1.e+9f){
				GameMap::RayCastResult result = minResult;
				Vector3 shift = minShift;
				followPos = result.hitPos - shift;
				followPos.x += result.normal.x * .02f;
				followPos.y += result.normal.y * .02f;
				followPos.z += result.normal.z * .02f;
				
				// bounce
				Vector3 norm = {(float)result.normal.x,
					(float)result.normal.y,
					(float)result.normal.z};
				float dot = Vector3::Dot(followVel, norm);
				followVel -= norm * (dot * 1.2f);
			}
			
			// acceleration
			Vector3 front;
			Vector3 up = {0, 0, -1};
			
			front.x = -cosf(followYaw) * cosf(followPitch);
			front.y = -sinf(followYaw) * cosf(followPitch);
			front.z = sinf(followPitch);
			
			Vector3 right = -Vector3::Cross(up, front).Normalize();
			Vector3 up2 = Vector3::Cross(right, front).Normalize();
			
			float scale = 10.f * dt;
			if(playerInput.sprint){
				scale *= 3.f;
			}
			front *= scale;
			right *= scale;
			up2 *= scale;
			
			if(playerInput.moveForward){
				followVel += front;
			}else if(playerInput.moveBackward){
				followVel -= front;
			}
			if(playerInput.moveLeft){
				followVel -= right;
			}else if(playerInput.moveRight){
				followVel += right;
			}
			if(playerInput.jump){
				followVel += up2;
			}else if(playerInput.crouch){
				followVel -= up2;
			}
			
			SPAssert(followVel.GetLength() < 100.f);
		}
		
		/** Handles movement of joined local player. */
		void Client::UpdateLocalPlayer(float dt) {
			SPADES_MARK_FUNCTION();
			
			auto *player = world->GetLocalPlayer();
			
			PlayerInput inp = playerInput;
			WeaponInput winp = weapInput;
			
			// weapon/tools are disabled while/soon after sprinting
			if(GetSprintState() > 0.001f) 
			{
				winp.primary = false;
				winp.secondary = false;
			}

			//Chameleon: spreadAdd. if more than 0, make weapon lag behind some more. if less than 0, reduce spread but increase recoil?
			//falling spread
			if (!player->IsOnGroundOrWade())
			{
				player->spreadAdd += dt;
			}
			//moving spread
			else if (inp.moveBackward || inp.moveForward || inp.moveLeft || inp.moveRight)
			{
				//sneaking
				if (player->crouching || inp.sneak || weapInput.secondary) 
				{
					if (player->spreadAdd < player->GetVelocity().GetLength() * 8)
						player->spreadAdd = player->GetVelocity().GetLength() * 8;
					if (player->spreadAdd > player->GetVelocity().GetLength() * 8)
						player->spreadAdd -= dt/2.f;
				}
				//running
				else if (!inp.sprint)
				{
					if (player->spreadAdd < player->GetVelocity().GetLength() * 4)
						player->spreadAdd = player->GetVelocity().GetLength() * 4;
					if (player->spreadAdd > player->GetVelocity().GetLength() * 4)
						player->spreadAdd -= dt/4.f;
				}
				//sprinting
				else
				{
					if (player->spreadAdd < player->GetVelocity().GetLength() * 4)
						player->spreadAdd = player->GetVelocity().GetLength() * 4;
					if (player->spreadAdd < player->GetVelocity().GetLength() * 6)
						player->spreadAdd += dt/10.f;
				}
				
			}
			//spread decrease when stationary
			else
			{
				if (player->spreadAdd > 1)
					player->spreadAdd -= dt;
				else if (player->spreadAdd > 0.5f)
					player->spreadAdd -= dt/2.f;
				else if (player->spreadAdd > 0.0f)
					player->spreadAdd -= dt/4.f;
			}

			if (player->spreadAdd < 0)
				player->spreadAdd = 0;
			else if (player->spreadAdd > 4)
				player->spreadAdd = 4;

			//Chameleon: mouse inertia
			{
				//if player is no longer alive, kill inertia
				if (!world->GetLocalPlayer()->IsAlive())
				{
					MouseEventInertia(mouseX, mouseY);
					mouseX = 0.f;
					mouseY = 0.f;
				}
				else
				{
					float health = world->GetLocalPlayer()->GetHealth()*0.01f;
					//mouseInertia is used in Client_Input(), but not here
					mouseInertia = 0.3f - health*0.2f;
					if (mouseInertia > 1.f)
						mouseInertia = 1.f;
					if (mouseInertia < 0.f)
						mouseInertia = 0.f;

					//smoothly reduces inertia
					//if (mouseX > 500-2.5f*world->GetLocalPlayer()->GetHealth() || mouseX*(-1) > 500-2.5f*world->GetLocalPlayer()->GetHealth())
					//float inertiaX = mouseX / (250 * (2 - 0.01f*health));
					//float inertiaY = mouseY / (250 * (2 - 0.01f*health));
					float inertiaX = mouseX / (20 * (2 - health));
					float inertiaY = mouseY / (20 * (2 - health));
					{
						if (inertiaX < 0)
							inertiaX *= (-1);
						if (inertiaY < 0)
							inertiaY *= (-1);
						MouseEventInertia(mouseX*dt*inertiaX, mouseY*dt*inertiaY);
						mouseX -= mouseX*dt*inertiaX;
						mouseY -= mouseY*dt*inertiaY;
					}

					//linear mouse velocity reduction, from 50 to 25 per second
					inertiaX = 0;
					inertiaY = 0;
					//inertia mouseX
					if (mouseX != 0)
					{
						inertiaX = 20 * dt / (2 - health);
						if (abs(mouseX) < inertiaX)
							inertiaX = abs(mouseX);

						inertiaX *= (mouseX / abs(mouseX));
					}
					//inertia mouseY
					if (mouseY != 0)
					{
						inertiaY = 20 * dt / (2 - health);
						if (abs(mouseY) < inertiaY)
							inertiaY = abs(mouseY);

						inertiaY *= (mouseY / abs(mouseY));
					}
					MouseEventInertia(inertiaX, inertiaY);
					mouseX -= inertiaX;
					mouseY -= inertiaY;
				}

			//Chameleon: drunk cam
				if (mouseRoll < 0)
				{
					mouseRoll -= mouseRoll*dt * 2 / (1.f - world->GetLocalPlayer()->GetHealth()*0.0075f);

					if (mouseRoll > -0.001f)
						mouseRoll = 0;
				}
				if (mouseRoll > 0)
				{
					mouseRoll -= mouseRoll*dt * 2 / (1.f - world->GetLocalPlayer()->GetHealth()*0.0075f);

					if (mouseRoll < 0.001f)
						mouseRoll = 0;
				}
			}
			//End of mouse inertia/drunk cam

			// don't allow to stand up when ceilings are too low
			if(inp.crouch == false)
			{
				if(player->GetInput().crouch)
				{
					if(!player->TryUncrouch(false))
					{
						inp.crouch = true;
					}
				}
			}
			
			// don't allow jumping in the air
			if(inp.jump)
			{
				if(!player->IsOnGroundOrWade())
					inp.jump = false;
			}
			
			// weapon/tools are disabled while changing tools
			if(clientPlayers[world->GetLocalPlayerIndex()]->IsChangingTool()) 
			{
				winp.primary = false;
				winp.secondary = false;
			}
			
			// disable weapon while reloading (except shotgun)
			if(player->GetTool() == Player::ToolWeapon &&
			   player->IsAwaitingReloadCompletion() &&
			   !player->GetWeapon()->IsReloadSlow()) 
			{
				winp.primary = false;
			}
			
			player->SetInput(inp);
			player->SetWeaponInput(winp);
			
			//send player input
			// FIXME: send only there are any changed?
			net->SendPlayerInput(inp);
			WeaponInput winpTMP = winp;

			//if (world->GetListener()->GetMaxShots() > 0 && world->GetListener()->GetShotsFired() >= world->GetListener()->GetMaxShots())
			winpTMP.primary = player->GetWeapon()->IsShooting();

			net->SendWeaponInput(winpTMP);
			
			if(hasDelayedReload) 
			{
				world->GetLocalPlayer()->Reload();
				net->SendReload();
				hasDelayedReload = false;
			}
			
			//PlayerInput actualInput = player->GetInput();
			WeaponInput actualWeapInput = player->GetWeaponInput();
			
			//Chameleon
			if (!player->IsAlive())
			{
				player->spreadAdd = 0;
				ShotsFired = 0;
				weapInput.secondary = false;

				if (player->GetWeaponType() == SMG_WEAPON)
					MaxShots = -1;
				else
					MaxShots = 1;
			}
			
			// is the selected tool no longer usable (ex. out of ammo)?
			if(!player->IsToolSelectable(player->GetTool())) 
			{
				// release mouse button before auto-switching tools
				winp.primary = false;
				winp.secondary = false;
				weapInput = winp;
				net->SendWeaponInput(weapInput);
				actualWeapInput = winp = player->GetWeaponInput();
				
				// select another tool
				Player::ToolType t = player->GetTool();
				do
				{
					switch(t)
					{
						case Player::ToolSpade:
							t = Player::ToolGrenade;
							break;
						case Player::ToolBlock:
							t = Player::ToolSpade;
							break;
						case Player::ToolWeapon:
							t = Player::ToolBlock;
							break;
						case Player::ToolGrenade:
							t = Player::ToolWeapon;
							break;
					}
				}
				while(!world->GetLocalPlayer()->IsToolSelectable(t));
				SetSelectedTool(t);
			}
			
			// send orientation
			Vector3 curFront = player->GetFront();
			if(curFront.x != lastFront.x ||
			   curFront.y != lastFront.y ||
			   curFront.z != lastFront.z) {
				lastFront = curFront;
				net->SendOrientation(curFront);
			}
			
			lastKills = world->GetPlayerPersistent(player->GetId()).kills;
			
			// show block count when building block lines.
			if(player->IsAlive() &&
			   player->GetTool() == Player::ToolBlock &&
			   player->GetWeaponInput().secondary &&
			   player->IsBlockCursorDragging()) 
			{
				if(player->IsBlockCursorActive()) 
				{
					auto blocks = std::move
					(world->CubeLine(player->GetBlockCursorDragPos(),
									 player->GetBlockCursorPos(),
									 256));
					auto msg = _TrN("Client",
									"{0} block", "{0} blocks",
									blocks.size());
					AlertType type =
					static_cast<int>(blocks.size()) > player->GetNumBlocks() ?
					AlertType::Warning : AlertType::Notice;
					ShowAlert(msg, type, 0.f, true);
				}
				else
				{
					// invalid
					auto msg = _Tr("Client", "-- blocks");
					AlertType type = AlertType::Warning;
					ShowAlert(msg, type, 0.f, true);
				}
			}
			
			if(player->IsAlive())
				lastAliveTime = time;
			
			if(player->GetHealth() < lastHealth)
			{
				//Chameleon: local blood
				Bleed(player->GetEye());
				Bleed(player->GetPosition());
				// ouch!
				lastHealth = player->GetHealth();
				lastHurtTime = world->GetTime();
				
				Handle<IAudioChunk> c;
				switch((rand() >> 3) & 3){
					case 0:
						c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/FleshLocal1.wav");
						break;
					case 1:
						c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/FleshLocal2.wav");
						break;
					case 2:
						c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/FleshLocal3.wav");
						break;
					case 3:
						c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/FleshLocal4.wav");
						break;
				}
				audioDevice->PlayLocal(c, AudioParam());
				
				float hpper = player->GetHealth() / 100.f;
				int cnt = 18 - (int)(player->GetHealth() / 100.f * 8.f);
				hurtSprites.resize(std::max(cnt, 6));
				for(size_t i = 0; i < hurtSprites.size(); i++) {
					HurtSprite& spr = hurtSprites[i];
					spr.angle = GetRandom() * (2.f * static_cast<float>(M_PI));
					spr.scale = .2f + GetRandom() * GetRandom() * .7f;
					spr.horzShift = GetRandom();
					spr.strength = .3f + GetRandom() * .7f;
					if(hpper > .5f) {
						spr.strength *= 1.5f - hpper;
					}
				}
				
			}
			else
			{
				lastHealth = player->GetHealth();
			}
			//inp.jump = false;
		}
		
		
#pragma mark - IWorldListener Handlers
		
		void Client::PlayerObjectSet(int id) {
			if(clientPlayers[id]){
				clientPlayers[id]->Invalidate();
				clientPlayers[id] = nullptr;
			}
			
			Player *p = world->GetPlayer(id);
			if(p)
				clientPlayers[id].Set(new ClientPlayer(p, this), false);
		}
		
		void Client::PlayerJumped(spades::client::Player *p){
			SPADES_MARK_FUNCTION();
			
			int distance = (p->GetOrigin() - lastSceneDef.viewOrigin).GetLength();
			if (!IsMuted() && distance*2 < soundDistance)
			{				
				Handle<IAudioChunk> c = p->GetWade() ?
				audioDevice->RegisterSound("Sounds/Player/WaterJump.wav"):
				audioDevice->RegisterSound("Sounds/Player/Jump.wav");
				audioDevice->Play(c, p->GetOrigin(), AudioParam());
			}
		}
		
		void Client::PlayerLanded(spades::client::Player *p,
								  bool hurt) {
			SPADES_MARK_FUNCTION();
			
			int distance = (p->GetOrigin() - lastSceneDef.viewOrigin).GetLength();
			if (!IsMuted() && distance*2 < soundDistance)
			{
				Handle<IAudioChunk> c;
				if(hurt)
					c = audioDevice->RegisterSound("Sounds/Player/FallHurt.wav");
				else if(p->GetWade())
					c = audioDevice->RegisterSound("Sounds/Player/WaterLand.wav");
				else
					c = audioDevice->RegisterSound("Sounds/Player/Land.wav");
				audioDevice->Play(c, p->GetOrigin(), AudioParam());
			}
		}
		
		void Client::PlayerMadeFootstep(spades::client::Player *p){
			SPADES_MARK_FUNCTION();
			
			/*if (p == world->GetLocalPlayer() && !v_defaultSprintBob)
			{
				if (grenadeVibration < 0.05f && clientPlayers[p->GetId()]->GetSprintState() > 0.5f)
					grenadeVibration += 0.025;
				else if (grenadeVibration < 0.05f && clientPlayers[p->GetId()]->GetAimDownState() > 0.5f)
					grenadeVibration += 0.025;
			}*/
			//disabled, to be replaced with newer methods
			/*if (p == world->GetLocalPlayer() && !v_defaultSprintBob)
			{
				if (stepVibration < 0.05f && clientPlayers[p->GetId()]->GetSprintState() > 0.5f)
					stepVibration += 0.03;
				else if (stepVibration < 0.05f)
					stepVibration += 0.02;
			}*/

			int distance = (p->GetOrigin() - lastSceneDef.viewOrigin).GetLength();
			if (!IsMuted() && distance*2 < soundDistance)
			{
				const char *snds[] = {
					"Sounds/Player/Footstep1.wav",
					"Sounds/Player/Footstep2.wav",
					"Sounds/Player/Footstep3.wav",
					"Sounds/Player/Footstep4.wav",
					/*"Sounds/Player/Footstep5.wav",
					"Sounds/Player/Footstep6.wav",
					"Sounds/Player/Footstep7.wav",
					"Sounds/Player/Footstep8.wav"*/
				};
				const char *rsnds[] = {
					"Sounds/Player/Run1.wav",
					"Sounds/Player/Run2.wav",
					"Sounds/Player/Run3.wav",
					"Sounds/Player/Run4.wav",
					/*"Sounds/Player/Run5.wav",
					"Sounds/Player/Run6.wav",
					"Sounds/Player/Run7.wav",
					"Sounds/Player/Run8.wav",*/
				};
				const char *wsnds[] = {
					"Sounds/Player/Wade1.wav",
					"Sounds/Player/Wade2.wav",
					"Sounds/Player/Wade3.wav",
					"Sounds/Player/Wade4.wav",
					/*"Sounds/Player/Wade5.wav",
					"Sounds/Player/Wade6.wav",
					"Sounds/Player/Wade7.wav",
					"Sounds/Player/Wade8.wav"*/
				};
				bool sprinting = clientPlayers[p->GetId()] ? clientPlayers[p->GetId()]->GetSprintState() > 0.5f : false;
				Handle<IAudioChunk> c = p->GetWade() ?
				audioDevice->RegisterSound(wsnds[(rand() >> 4) % 4]):
				audioDevice->RegisterSound(snds[(rand() >> 4) % 4]);
				if (!sprinting || p->GetWade())
				{
					audioDevice->Play(c, p->GetOrigin(), AudioParam());
				}
				else				
				{
					AudioParam param;
					param.volume *= clientPlayers[p->GetId()]->GetSprintState()+0.1f;
					c = audioDevice->RegisterSound(rsnds[(rand() >> 4) % 4]);
					audioDevice->Play(c, p->GetOrigin(), param);
				}
			}
		}

		void Client::PlayerFiredWeapon(spades::client::Player *p)
		{
			SPADES_MARK_FUNCTION();
			
			if(p == world->GetLocalPlayer())
			{
				localFireVibrationTime = time;
				//alt fire vibration
				float addGV;

				switch (p->GetWeapon()->GetWeaponType())
				{
				case (RIFLE_WEAPON) :
					addGV = 0.08f;
					break;
				case (SMG_WEAPON) :
					addGV = 0.03f;
					break;
				case (SHOTGUN_WEAPON) :
					addGV = 0.10f;
					break;
				default:
					break;
				}	
				//not enough hearing loss...
				soundDistance -= (2 + addGV*10) * soundDistance / 5.f;

				addGV *= (4.f-GetAimDownState())/4.f; //from 1 to 0.75

				if (grenadeVibration < addGV*2)
					grenadeVibration += addGV;		

				//add single and auto modes for SMG //added
			}			
			clientPlayers[p->GetId()]->FiredWeapon();
		}
		void Client::PlayerDryFiredWeapon(spades::client::Player *p) {
			SPADES_MARK_FUNCTION();

			Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/DryFire.wav");
			audioDevice->Play(c, p->GetEye(), AudioParam());
		}
		
		void Client::PlayerReloadingWeapon(spades::client::Player *p) {
			SPADES_MARK_FUNCTION();
			
			clientPlayers[p->GetId()]->ReloadingWeapon();
		}
		
		void Client::PlayerReloadedWeapon(spades::client::Player *p){
			SPADES_MARK_FUNCTION();
			
			
			clientPlayers[p->GetId()]->ReloadedWeapon();
		}
		
		void Client::PlayerChangedTool(spades::client::Player *p){
			SPADES_MARK_FUNCTION();
			
			int distance = (p->GetOrigin() - lastSceneDef.viewOrigin).GetLength();
			if (!IsMuted() && distance*2 < soundDistance)
			{
				bool isLocal = p == world->GetLocalPlayer();
				Handle<IAudioChunk> c;
				if(isLocal)
				{
					// played by ClientPlayer::Update
					return;
				}
				else
				{
					c = audioDevice->RegisterSound("Sounds/Weapons/Switch.wav");
					audioDevice->Play(c, p->GetEye() + p->GetFront() * 0.5f
									  - p->GetUp() * .3f
									  + p->GetRight() * .4f,
									  AudioParam());
				}
			}
		}
		
		void Client::PlayerRestocked(spades::client::Player *p)
		{
			if(!IsMuted())
			{
				bool isLocal = p == world->GetLocalPlayer();
				int distance = (p->GetOrigin() - lastSceneDef.viewOrigin).GetLength();
				if (isLocal)
				{
					//update weapon scope and scope magnification
					scopeOn = (bool)weap_scope;
					scopeZoom = abs((int)weap_scopeZoom);

					Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Restock.wav");
					audioDevice->PlayLocal(c, MakeVector3(.4f, -.3f, .5f),
						AudioParam());
				}
				else if (distance*2 < soundDistance)
				{
					Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Restock.wav");
					audioDevice->Play(c, p->GetEye() + p->GetFront() * 0.5f
						- p->GetUp() * .3f
						+ p->GetRight() * .4f,
						AudioParam());
				}
			}
		}
		
		void Client::PlayerThrownGrenade(spades::client::Player *p, Grenade *g){
			SPADES_MARK_FUNCTION();
			

			if(!IsMuted())
			{
				int distance = (p->GetOrigin() - lastSceneDef.viewOrigin).GetLength();
				bool isLocal = p == world->GetLocalPlayer();

				if (g && isLocal)
				{
					net->SendGrenade(g);
				}

				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/Throw.wav");

				if(isLocal)
					audioDevice->PlayLocal(c, MakeVector3(.4f, 0.1f, .3f),
										   AudioParam());
				else if (distance*2 < soundDistance)
					audioDevice->Play(c, p->GetEye() + p->GetFront() * 0.5f
									  - p->GetUp() * .2f
									  + p->GetRight() * .3f,
									  AudioParam());
			}
		}
		
		void Client::PlayerMissedSpade(spades::client::Player *p){
			SPADES_MARK_FUNCTION();
			
			if(!IsMuted())
			{
				int distance = (p->GetOrigin() - lastSceneDef.viewOrigin).GetLength();
				
				bool isLocal = p == world->GetLocalPlayer();
				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Spade/Woosh.wav");
				if(isLocal)
					audioDevice->PlayLocal(c, MakeVector3(.2f, -.1f, 0.7f),
										   AudioParam());
				else if(distance*4 < soundDistance)
					audioDevice->Play(c, p->GetOrigin() + p->GetFront() * 0.8f
									  - p->GetUp() * .2f,
									  AudioParam());
			}
		}
		
		void Client::PlayerHitBlockWithSpade(spades::client::Player *p,
											 Vector3 hitPos,
											 IntVector3 blockPos,
											 IntVector3 normal){
			SPADES_MARK_FUNCTION();
			
			bool isLocal = p == world->GetLocalPlayer();

			uint32_t col = map->GetColor(blockPos.x, blockPos.y, blockPos.z);
			IntVector3 colV = {(uint8_t)col,
				(uint8_t)(col >> 8), (uint8_t)(col >> 16)};
			Vector3 shiftedHitPos = hitPos;
			shiftedHitPos.x += normal.x * .05f;
			shiftedHitPos.y += normal.y * .05f;
			shiftedHitPos.z += normal.z * .05f;
			
			EmitBlockFragments(shiftedHitPos, colV, isLocal);
			
			if(p == world->GetLocalPlayer())
			{
				localFireVibrationTime = time;
			}
			
			if(!IsMuted())
			{
				int distance = (p->GetOrigin() - lastSceneDef.viewOrigin).GetLength();
				
				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Spade/HitBlock.wav");
				if(isLocal)
					audioDevice->PlayLocal(c, MakeVector3(.1f, -.1f, 1.2f), AudioParam());
				else if (distance*2 < soundDistance)
					audioDevice->Play(c, p->GetOrigin() + p->GetFront() * 0.5f
									  - p->GetUp() * .2f,
									  AudioParam());
			}
		}
		
		void Client::PlayerKilledPlayer(spades::client::Player *killer,
										spades::client::Player *victim,
										KillType kt)
		{
			Player* local = world->GetLocalPlayer();
			// play hit sound
			if(kt == KillTypeWeapon ||
			   kt == KillTypeHeadshot) {
				// don't play on local: see BullethitPlayer
				if(victim != world->GetLocalPlayer()) 
				{
					int distance = (victim->GetOrigin() - lastSceneDef.viewOrigin).GetLength();
					if (!IsMuted() && distance*2 < soundDistance)
					{
						Handle<IAudioChunk> c;
						switch(rand()%4)
						{
							case 0:
								c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Flesh1.wav");
								break;
							case 1:
								c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Flesh2.wav");
								break;
							case 2:
								c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Flesh3.wav");
								break;
							case 3:
								c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Flesh4.wav");
								break;
						}
						AudioParam param;
						param.volume = 4.f;
						audioDevice->Play(c, victim->GetEye(), param);
					}
				}
			}
			
			// begin following
			if(victim == world->GetLocalPlayer()){
				followingPlayerId = victim->GetId();
				
				Vector3 v = -victim->GetFront();
				followYaw = atan2(v.y, v.x);
				followPitch = 30.f * M_PI /180.f;
			}
			
			// emit blood (also for local player)
			// FIXME: emiting blood for either
			// client-side or server-side hit?
			switch(kt){
				case KillTypeGrenade:
				case KillTypeHeadshot:
				case KillTypeMelee:
				case KillTypeWeapon:
					Bleed(victim->GetEye());
					break;
				default:
					break;
			}
			
			// create ragdoll corpse
			if(cg_ragdoll && victim->GetTeamId() < 2){
				Corpse *corp;
				corp = new Corpse(renderer, map, victim);
				if(victim == world->GetLocalPlayer())
					lastMyCorpse = corp;
				if(killer != victim && kt != KillTypeGrenade){
					Vector3 dir = victim->GetPosition() - killer->GetPosition();
					dir = dir.Normalize();
					if(kt == KillTypeMelee){
						dir *= 6.f;
					}else{
						if(killer->GetWeapon()->GetWeaponType() == SMG_WEAPON){
							dir *= 8.f;
						}else if(killer->GetWeapon()->GetWeaponType() == SHOTGUN_WEAPON){
							dir *= 32.f;
						}else{
							dir *= 16.f;
						}
					}
					dir.z -= 0.1f;
					corp->AddBodyImpulse(dir);
				}else if(kt == KillTypeGrenade){
					corp->AddImpulse(MakeVector3(0, 0, -8.f-GetRandom()*8.f));
				}
				corp->AddImpulse(victim->GetVelocity() * 32.f);
				corpses.emplace_back(corp);
				
				if(corpses.size() > corpseHardLimit)
				{
					corpses.pop_front();
				}
				else if(corpses.size() > corpseSoftLimit)
				{
					RemoveInvisibleCorpses();
				}
			}
			
			// add chat message
			std::string s;
			s = ChatWindow::TeamColorMessage(killer->GetName(), killer->GetTeamId());
			
			std::string cause;
			bool ff = killer->GetTeamId() == victim->GetTeamId();
			if(killer == victim)
				ff = false;
			
			cause = " [";
			Weapon* w = killer ? killer->GetWeapon() : nullptr;	//only used in case of KillTypeWeapon
			cause += ChatWindow::killImage( kt, w ? w->GetWeaponType() : RIFLE_WEAPON );
			cause += "] ";
			
			if(ff)
				s += ChatWindow::ColoredMessage(cause, MsgColorRed);
			else if(killer == local || victim == local)
				s += ChatWindow::ColoredMessage(cause, MsgColorBlack); //chameleon - for local kills
			else
				s += cause;
			
			if(killer != victim){
				s += ChatWindow::TeamColorMessage(victim->GetName(), victim->GetTeamId());
			}
			
			killfeedWindow->AddMessage(s);
			
			// log to netlog
			if(killer != victim) {
				NetLog("%s (%s)%s%s (%s)",
					   killer->GetName().c_str(),
					   world->GetTeam(killer->GetTeamId()).name.c_str(),
					   cause.c_str(),
					   victim->GetName().c_str(),
					   world->GetTeam(victim->GetTeamId()).name.c_str());
			}else{
				NetLog("%s (%s)%s",
					   killer->GetName().c_str(),
					   world->GetTeam(killer->GetTeamId()).name.c_str(),
					   cause.c_str());
			}
			
			// show big message if player is involved
			if(victim != killer)
			{
				if(killer == local || victim == local)
				{
					std::string msg;
					if (killer == local)
					{
						if ((int)hud_centerMessage == 2) msg = _Tr("Client", "You have killed {0}", victim->GetName());
					} 
					else
					{
						msg = _Tr("Client", "You were killed by {0}", killer->GetName());
					}
					centerMessageView->AddMessage(msg);
				}
			}
		}
		
		void Client::BulletHitPlayer(spades::client::Player *hurtPlayer,
									 HitType type,
									 spades::Vector3 hitPos,
									 spades::client::Player *by) {
			SPADES_MARK_FUNCTION();
			
			SPAssert(type != HitTypeBlock);

			// don't bleed local player
			//if(hurtPlayer != world->GetLocalPlayer() ||
			//   ShouldRenderInThirdPersonView()){
			//	Bleed(hitPos);
			//}
			if (hurtPlayer != world->GetLocalPlayer())
			{
				Bleed(hitPos);
			}
			
			if(hurtPlayer == world->GetLocalPlayer())
			{
				// don't player hit sound now;
				// local bullet impact sound is
				// played by checking the decrease of HP
				return;
			}

			int distance = (hitPos - lastSceneDef.viewOrigin).GetLength();
			if (!IsMuted() && distance*2 < soundDistance)
			{
				if(type == HitTypeMelee)
				{
					Handle<IAudioChunk> c =
					audioDevice->RegisterSound("Sounds/Weapons/Spade/HitPlayer.wav");
					audioDevice->Play(c, hitPos,
									  AudioParam());
				}
				else
				{
					Handle<IAudioChunk> c;
					switch((rand()>>6)%3){
						case 0:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Flesh1.wav");
							break;
						case 1:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Flesh2.wav");
							break;
						case 2:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Flesh3.wav");
							break;
					}
					AudioParam param;
					param.volume = 4.f;
					audioDevice->Play(c, hitPos,
									  param);
				}
			}
			
			if(by == world->GetLocalPlayer() && hurtPlayer)
			{
				net->SendHit(hurtPlayer->GetId(), type);
				
				hitFeedbackIconState = 1.f;
				if(hurtPlayer->GetTeamId() == world->GetLocalPlayer()->GetTeamId())
				{
					hitFeedbackFriendly = true;
				}
				else
				{
					hitFeedbackFriendly = false;
				}
			}
			
			
			
		}
		
		void Client::BulletHitBlock(Vector3 hitPos,
									IntVector3 blockPos,
									IntVector3 normal){
			SPADES_MARK_FUNCTION();
			
			uint32_t col = map->GetColor(blockPos.x, blockPos.y, blockPos.z);
			IntVector3 colV = {(uint8_t)col,
				(uint8_t)(col >> 8), (uint8_t)(col >> 16)};
			Vector3 shiftedHitPos = hitPos;
			shiftedHitPos.x += normal.x * .05f;
			shiftedHitPos.y += normal.y * .05f;
			shiftedHitPos.z += normal.z * .05f;
			
			if(blockPos.z == 63)
			{
				BulletHitWaterSurface(shiftedHitPos);
				int distance = (hitPos - lastSceneDef.viewOrigin).GetLength();
				if (!IsMuted() && distance*4 < soundDistance)
				{
					AudioParam param;
					param.volume = 2.f;
					
					Handle<IAudioChunk> c;
					
					param.pitch = .9f + GetRandom() * 0.2f;
					switch((rand() >> 6) & 3){
						case 0:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Water1.wav");
							break;
						case 1:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Water2.wav");
							break;
						case 2:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Water3.wav");
							break;
						case 3:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Water4.wav");
							break;
					}
					audioDevice->Play(c, shiftedHitPos,
									  param);
				}
			}
			else
			{
				EmitBlockFragments(shiftedHitPos, colV, false);
				
				int distance = (hitPos - lastSceneDef.viewOrigin).GetLength();
				if (!IsMuted() && distance*2 < soundDistance)
				{
					AudioParam param;
					param.volume = 2.f;
					
					Handle<IAudioChunk> c;
					
					/*switch((rand() >> 6) & 3)
					{
						case 0:
						case 1:
						case 2:
						case 3:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Block.wav");
							break;
					}
					audioDevice->Play(c, shiftedHitPos, param);*/
					
					param.pitch = .9f + GetRandom() * 0.2f;
					param.volume = 2.f;
					switch((rand() >> 6) & 3)
					{
						case 0:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Ricochet1.wav");
							break;
						case 1:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Ricochet2.wav");
							break;
						case 2:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Ricochet3.wav");
							break;
						case 3:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Ricochet4.wav");
							break;
					}
					audioDevice->Play(c, shiftedHitPos,
									  param);
				}
			}		
		}
		
		void Client::AddBulletTracer(spades::client::Player *player,
									 spades::Vector3 muzzlePos,
									 spades::Vector3 hitPos) {
			SPADES_MARK_FUNCTION();
			
			Tracer *t;
			float vel;
			IModel *model;
			switch(player->GetWeapon()->GetWeaponType()) 
			{
				case RIFLE_WEAPON:
					vel = 600.f;
					model = renderer->RegisterModel("Models/Weapons/Objects/Tracer.kv6");
					break;
				case SMG_WEAPON:
					vel = 300.f;
					model = renderer->RegisterModel("Models/Weapons/Objects/Tracer.kv6");
					if (player->GetWeapon()->GetAmmo() % 2 == 0)
						return;
					break;
				case SHOTGUN_WEAPON:
					return;
			}
			if (!world->GetLocalPlayer())
				t = new Tracer(this, NULL, model, muzzlePos, hitPos, vel, player->GetColor());
			else if (player == world->GetLocalPlayer())
				t = new Tracer(this, NULL, model, muzzlePos, hitPos, vel, player->GetColor());
			else
				t = new Tracer(this, world->GetLocalPlayer(), model, muzzlePos, hitPos, vel, player->GetColor());
			AddLocalEntity(t);
		}
		
		void Client::BlocksFell(std::vector<IntVector3> blocks)
		{
			SPADES_MARK_FUNCTION();
			
			if(blocks.empty())
				return;
			FallingBlock *b = new FallingBlock(this, blocks);
			AddLocalEntity(b);
			
			if(!IsMuted()){
				
				IntVector3 v = blocks[0];
				Vector3 o;
				o.x = v.x; o.y = v.y; o.z = v.z;
				o += .5f;
				
				Handle<IAudioChunk> c =
				audioDevice->RegisterSound("Sounds/Misc/BlockFall.wav");
				audioDevice->Play(c, o, AudioParam());
			}
		}
		
		void Client::GrenadeBounced(spades::client::Grenade *g){
			SPADES_MARK_FUNCTION();
			
			if(g->GetPosition().z < 63.f)
			{
				int distance = (g->GetPosition() - lastSceneDef.viewOrigin).GetLength();
				if (!IsMuted() && distance*4 < soundDistance)
				{
					Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Objects/GrenadeBounce.wav");
					audioDevice->Play(c, g->GetPosition(), AudioParam());
				}
			}
		}
		
		void Client::GrenadeDroppedIntoWater(spades::client::Grenade *g){
			SPADES_MARK_FUNCTION();
			
			int distance = (g->GetPosition() - lastSceneDef.viewOrigin).GetLength();
			if (!IsMuted() && distance*4 < soundDistance)
			{
				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Objects/GrenadeWater.wav");
				audioDevice->Play(c, g->GetPosition(), AudioParam());
			}
		}
		
		void Client::GrenadeExploded(spades::client::Grenade *g) {
			SPADES_MARK_FUNCTION();
			
			bool inWater = g->GetPosition().z > 63.f;
			
			if(inWater)
			{
				GrenadeExplosionUnderwater(g->GetPosition());

				if(!IsMuted())
				{
					int distance = (g->GetPosition() - lastSceneDef.viewOrigin).GetLength();
					if (distance < (int)snd_maxDistance/2)
					{
						Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/ExplodeWater0.wav");
						AudioParam param;
						param.volume = 5.f;
						audioDevice->Play(c, g->GetPosition(), param);
					}
					else if (distance < (int)snd_maxDistance)
					{
						Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/ExplodeWater1.wav");
						AudioParam param;
						param.volume = 1.f;
						//param.referenceDistance = distance;
						audioDevice->Play(c, g->GetPosition(), param);
					}

					if (distance < soundDistance)
						soundDistance = distance;

					/*if (distance < 16.f)
					{
						lastShellShockTime = distance / 16.f;
						audioDevice->lastShellShockTime = lastShellShockTime;
					}*/						

					//Chameleon: NO STEREO SOUNDS - performance loss
					//c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/WaterExplodeStereo.wav");
					//param.volume = 2.f;
					//audioDevice->Play(c, g->GetPosition(),
					//				  param);
				}
			}
			else
			{
				GrenadeExplosion(g->GetPosition());
				
				if(!IsMuted())
				{					
					int distance = (g->GetPosition() - lastSceneDef.viewOrigin).GetLength();

					if (distance*4 < soundDistance || distance < 8)
					{
						// debri sound
						AudioParam param;
						Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Objects/GrenadeDebris.wav");
						param.volume = 5.f;
						param.referenceDistance = 3.f;
						IntVector3 outPos;
						Vector3 soundPos = g->GetPosition();
						if (world->GetMap()->CastRay(soundPos, MakeVector3(0, 0, 1), 8.f, outPos))
						{
							soundPos.z = (float)outPos.z - 0.2f;
						}
						audioDevice->Play(c, soundPos, param);
					}

					if (distance < (int)snd_maxDistance)
					{
						Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/Explode0.wav");
						AudioParam param;
						param.volume = 5.f;
						//param.referenceDistance = distance;
						audioDevice->Play(c, g->GetPosition(), param);						
					}
					else if (distance < (int)snd_maxDistance+soundDistance)
					{
						AudioParam param;
						Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/Explode1.wav");
						param.volume = 1.f;
						//param.referenceDistance = distance;
						audioDevice->Play(c, g->GetPosition(), param);
					}
					else if (distance < soundDistance*4)
					{
						AudioParam param;
						Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/Explode2.wav");
						param.volume = 0.2f;
						//param.referenceDistance = distance;
						audioDevice->Play(c, g->GetPosition(), param);
					}

					if (distance < soundDistance)
						soundDistance = distance;
					/*if (distance < 16.f)
						lastShellShockTime = distance / 16.f;*/

					//param.referenceDistance = 1.f;
					//audioDevice->Play(cs, g->GetPosition(),
					//				  param);
					//c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/ExplodeFarStereo.wav");
					//param.referenceDistance = 10.f;
					//audioDevice->Play(c, g->GetPosition(),
					//				  param);
				}
			}
		}
		
		void Client::LocalPlayerPulledGrenadePin(int teamId)
		{
			SPADES_MARK_FUNCTION();
			
			if(!IsMuted())
			{
				/*Handle<IAudioChunk> c = (teamId == 0 ?
					audioDevice->RegisterSound("Sounds/Weapons/GrenadeA/Fire.wav") :
					audioDevice->RegisterSound("Sounds/Weapons/GrenadeB/Fire.wav")); */
				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/Fire.wav");
				audioDevice->PlayLocal(c, MakeVector3(.4f, -.3f, .5f),
									   AudioParam());
			}
		}
		
		void Client::LocalPlayerBlockAction(spades::IntVector3 v, BlockActionType type){
			SPADES_MARK_FUNCTION();
			net->SendBlockAction(v, type);
		}
		void Client::LocalPlayerCreatedLineBlock(spades::IntVector3 v1, spades::IntVector3 v2) {
			SPADES_MARK_FUNCTION();
			net->SendBlockLine(v1, v2);
		}
		
		void Client::LocalPlayerHurt(HurtType type,
									 bool sourceGiven,
									 spades::Vector3 source) {
			SPADES_MARK_FUNCTION();
			
			if(sourceGiven){
				Player * p = world->GetLocalPlayer();
				if(!p)
					return;
				Vector3 rel = source - p->GetEye();
				rel.z = 0.f; rel = rel.Normalize();
				hurtRingView->Add(rel);
			}
		}
		
		void Client::LocalPlayerBuildError(BuildFailureReason reason) {
			SPADES_MARK_FUNCTION();
			
			if(!cg_alerts) {
				PlayAlertSound();
				return;
			}
			
			switch(reason) {
				case BuildFailureReason::InsufficientBlocks:
					ShowAlert(_Tr("Client", "Insufficient blocks."),
							  AlertType::Error);
					break;
				case BuildFailureReason::InvalidPosition:
					ShowAlert(_Tr("Client", "You cannot place a block there."),
							  AlertType::Error);
					break;
			}
		}

		//Chameleon: recoil for freeaim
		void Client::LocalPlayerRecoil(Vector2 rec)
		{
			//freaim
			mousePitch -= rec.x;

			if (world->GetLocalPlayer()->crouching)
				rec *= 0.5f;

			mouseY -= rec.y * 100000;
			//mouse inertia
			if (mouseY > 0)
				mouseY = 0;
			if (weapX != 0)
				mouseX -= rec.x * 1000 * weapX / abs(weapX);
			else
				mouseX += rec.x * sinf(world->GetTime() * 2.f);
		}

		//Chameleon: set weapon muzzle direction
		void Client::SetWeaponXY(Vector2 vec)
		{
			weapX += vec.x;
			weapY += vec.y;
		}
		//Chameleon: return weapon muzzle direction
		float Client::GetWeaponX()
		{
			return weapX;
		}
		//Chameleon: return weapon muzzle direction
		float Client::GetWeaponY()
		{
			return weapY;
		}
		//Chameleon: return visual weapon muzzle direction
		float Client::GetWeaponViewX()
		{
			return clientPlayers[world->GetLocalPlayerIndex()]->viewWeaponOffset.x;
		}
		//Chameleon: return visual weapon muzzle direction
		float Client::GetWeaponViewY()
		{
			return clientPlayers[world->GetLocalPlayerIndex()]->viewWeaponOffset.z;
		}
		
		//Chameleon: walking stuff
		void Client::SetWalkProgress(float fWalkProgress)
		{
			walkProgress = fWalkProgress;
		}
		//Chameleon: walking stuff
		void Client::SwitchBFootSide()
		{
			bFootSide = !bFootSide;
		}

		//Chameleon: firemodes
		int Client::GetShotsFired()
		{
			return ShotsFired;
		}
		//Chameleon: get firemode
		int Client::GetMaxShots()
		{
			if (world->GetLocalPlayer())
			{
				if (world->GetLocalPlayer()->GetWeaponType() == SMG_WEAPON)
					return MaxShots;
				else
					return (MaxShots = 1);
			}
		}
		//Chameleon: firemodes
		void Client::SetShotsFired(int value)
		{
			ShotsFired = value;
		}
		//Chameleon: switch firemode
		void Client::SetMaxShots(int value)
		{
			MaxShots = value;
		}
	}
}
