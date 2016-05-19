/*
 Copyright (c) 2013 yvt
 
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


#include "ClientPlayer.h"
#include "Player.h"
#include <ScriptBindings/ScriptFunction.h>
#include "IImage.h"
#include "IModel.h"
#include "IRenderer.h"
#include "Client.h"
#include "World.h"
#include <Core/Settings.h>
#include "CTFGameMode.h"
#include "Weapon.h"
#include "GunCasing.h"
#include <stdlib.h>
#include <ScriptBindings/IToolSkin.h>
#include <ScriptBindings/IViewToolSkin.h>
#include <ScriptBindings/IThirdPersonToolSkin.h>
#include <ScriptBindings/ISpadeSkin.h>
#include <ScriptBindings/IBlockSkin.h>
#include <ScriptBindings/IGrenadeSkin.h>
#include <ScriptBindings/IWeaponSkin.h>
#include "IAudioDevice.h"
#include "GunCasing.h"
#include "IAudioChunk.h"

SPADES_SETTING(cg_ragdoll, "");
SPADES_SETTING(cg_ejectBrass, "");

SPADES_SETTING(r_dlights, "");

//Maximum distance of particles
SPADES_SETTING(opt_particleMaxDist, "");
//Maximum distance for detailed particles
SPADES_SETTING(opt_particleNiceDist, "");
//maximum distance of models, not really useful, just default values were shit
SPADES_SETTING(opt_modelMaxDist, "");

//draw 1st person torso
SPADES_SETTING(v_drawTorso, "0");
//draw 1st person arms (like in 3rd person)
SPADES_SETTING(v_drawArms, "0");
//draw 1st person legs
SPADES_SETTING(v_drawLegs, "0");

//grenade binocs zoom
SPADES_SETTING(v_binocsZoom, "2");

//SPADES_SETTING(d_a, "0");
//SPADES_SETTING(d_b, "0");
//SPADES_SETTING(d_c, "0");
//SPADES_SETTING(d_x, "0");
//SPADES_SETTING(d_y, "0");

namespace spades {
	namespace client {
		
		class SandboxedRenderer : public IRenderer {
			Handle<IRenderer> base;
			AABB3 clipBox;
			bool allowDepthHack;
			
			void OnProhibitedAction() {
			}
			
			bool CheckVisibility(const AABB3 &box) {
				if (!clipBox.Contains(box) ||
					!std::isfinite(box.min.x) || !std::isfinite(box.min.y) || !std::isfinite(box.min.z) ||
					!std::isfinite(box.max.x) || !std::isfinite(box.max.y) || !std::isfinite(box.max.z)) {
					OnProhibitedAction();
					return false;
				}
				return true;
			}
		protected:
			~SandboxedRenderer(){}
		public:
			
			SandboxedRenderer(IRenderer *base) :
			base(base) {}
			
			void SetClipBox(const AABB3 &b)
			{ clipBox = b; }
			void SetAllowDepthHack(bool h)
			{ allowDepthHack = h; }
			
			void Init() { OnProhibitedAction(); }
			void Shutdown() { OnProhibitedAction(); }
			
			IImage *RegisterImage(const char *filename)
			{ return base->RegisterImage(filename); }
			IModel *RegisterModel(const char *filename)
			{ return base->RegisterModel(filename); }
			
			IImage *CreateImage(Bitmap *bmp)
			{ return base->CreateImage(bmp); }
			IModel *CreateModel(VoxelModel *m)
			{ return base->CreateModel(m); }
			
			void SetGameMap(GameMap *)
			{ OnProhibitedAction(); }
			
			void SetFogDistance(float)
			{ OnProhibitedAction(); }
			void SetFogColor(Vector3)
			{ OnProhibitedAction(); }
			
			void StartScene(const SceneDefinition&)
			{ OnProhibitedAction(); }
			
			void AddLight(const client::DynamicLightParam& light) {
				Vector3 rad(light.radius, light.radius, light.radius);
				if (CheckVisibility(AABB3(light.origin - rad, light.origin + rad))) {
					base->AddLight(light);
				}
			}
			
			void RenderModel(IModel *model, const ModelRenderParam& p) {
				if (!model) {
					SPInvalidArgument("model");
					return;
				}
				if (p.depthHack && !allowDepthHack) 
				{
					OnProhibitedAction();
					return;
				}
				auto bounds = (p.matrix * OBB3(model->GetBoundingBox())).GetBoundingAABB();
				if (CheckVisibility(bounds)) 
				{
					base->RenderModel(model, p);
				}
			}
			void AddDebugLine(Vector3 a, Vector3 b, Vector4 color) {
				OnProhibitedAction();
			}
			
			void AddSprite(IImage *image, Vector3 center, float radius, float rotation) {
				Vector3 rad(radius * 1.5f, radius * 1.5f, radius * 1.5f);
				if (CheckVisibility(AABB3(center - rad, center + rad))) {
					base->AddSprite(image, center, radius, rotation);
				}
			}
			void AddLongSprite(IImage *image, Vector3 p1, Vector3 p2, float radius)
			{
				Vector3 rad(radius * 1.5f, radius * 1.5f, radius * 1.5f);
				AABB3 bounds1(p1 - rad, p1 + rad);
				AABB3 bounds2(p2 - rad, p2 + rad);
				bounds1 += bounds2;
				if (CheckVisibility(bounds1)) {
					base->AddLongSprite(image, p1, p2, radius);
				}
			}
			
			void EndScene() { OnProhibitedAction(); }
			
			void MultiplyScreenColor(Vector3) { OnProhibitedAction(); }
			
			/** Sets color for image drawing. Deprecated because
			 * some methods treats this as an alpha premultiplied, while
			 * others treats this as an alpha non-premultiplied.
			 * @deprecated */
			void SetColor(Vector4 col) {
				base->SetColor(col);
			}
			
			/** Sets color for image drawing. Always alpha premultiplied. */
			void SetColorAlphaPremultiplied(Vector4 col) {
				base->SetColorAlphaPremultiplied(col);
			}
			
			void DrawImage(IImage *img, const Vector2& outTopLeft)
			{
				if (allowDepthHack)
					base->DrawImage(img, outTopLeft);
				else
					OnProhibitedAction();
			}
			void DrawImage(IImage *img, const AABB2& outRect)
			{
				if (allowDepthHack)
					base->DrawImage(img, outRect);
				else
					OnProhibitedAction();
			}
			void DrawImage(IImage *img, const Vector2& outTopLeft, const AABB2& inRect)
			{
				if (allowDepthHack)
					base->DrawImage(img, outTopLeft, inRect);
				else
					OnProhibitedAction();
			}
			void DrawImage(IImage *img, const AABB2& outRect, const AABB2& inRect)
			{
				if (allowDepthHack)
					base->DrawImage(img, outRect, inRect);
				else
					OnProhibitedAction();
			}
			void DrawImage(IImage *img, const Vector2& outTopLeft, const Vector2& outTopRight, const Vector2& outBottomLeft, const AABB2& inRect)
			{
				if (allowDepthHack)
					base->DrawImage(img, outTopLeft, outTopRight, outBottomLeft, inRect);
				else
					OnProhibitedAction();
			}
			
			void DrawFlatGameMap(const AABB2& outRect, const AABB2& inRect)
			{ OnProhibitedAction(); }
			
			void FrameDone()
			{ OnProhibitedAction(); }
			
			void Flip()
			{ OnProhibitedAction(); }
			
			Bitmap *ReadBitmap()
			{ OnProhibitedAction(); return nullptr; }
			
			float ScreenWidth() { return base->ScreenWidth(); }
			float ScreenHeight() { return base->ScreenHeight(); }
		};
		
		ClientPlayer::ClientPlayer(Player *p,
								   Client *c):
		player(p), client(c){
			SPADES_MARK_FUNCTION();
			
			sprintState = 0.f;
			aimDownState = 0.f;
			toolRaiseState = 0.f;
			currentTool = p->GetTool();
			localFireVibrationTime = -100.f;
			time = 0.f;
			viewWeaponOffset = MakeVector3(0, 0, 0);
			
			ScriptContextHandle ctx;
			IRenderer *renderer = client->GetRenderer();
			IAudioDevice *audio = client->GetAudioDevice();
			
			sandboxedRenderer.Set(new SandboxedRenderer(renderer), false);
			renderer = sandboxedRenderer;

			
			if (p->GetTeamId() != 1)
			{
				static ScriptFunction spadeFactory("ISpadeSkin@ CreateThirdPersonSpadeSkinA(Renderer@, AudioDevice@)");
				spadeSkin = initScriptFactory(spadeFactory, renderer, audio);

				static ScriptFunction spadeViewFactory("ISpadeSkin@ CreateViewSpadeSkinA(Renderer@, AudioDevice@)");
				spadeViewSkin = initScriptFactory(spadeViewFactory, renderer, audio);

				static ScriptFunction blockFactory("IBlockSkin@ CreateThirdPersonBlockSkinA(Renderer@, AudioDevice@)");
				blockSkin = initScriptFactory(blockFactory, renderer, audio);

				static ScriptFunction blockViewFactory("IBlockSkin@ CreateViewBlockSkinA(Renderer@, AudioDevice@)");
				blockViewSkin = initScriptFactory(blockViewFactory, renderer, audio);

				static ScriptFunction grenadeFactory("IGrenadeSkin@ CreateThirdPersonGrenadeSkinA(Renderer@, AudioDevice@)");
				grenadeSkin = initScriptFactory(grenadeFactory, renderer, audio);

				static ScriptFunction grenadeViewFactory("IGrenadeSkin@ CreateViewGrenadeSkinA(Renderer@, AudioDevice@)");
				grenadeViewSkin = initScriptFactory(grenadeViewFactory, renderer, audio);

				static ScriptFunction rifleFactory("IWeaponSkin@ CreateThirdPersonRifleSkinA(Renderer@, AudioDevice@)");
				static ScriptFunction smgFactory("IWeaponSkin@ CreateThirdPersonSMGSkinA(Renderer@, AudioDevice@)");
				static ScriptFunction shotgunFactory("IWeaponSkin@ CreateThirdPersonShotgunSkinA(Renderer@, AudioDevice@)");
				static ScriptFunction rifleViewFactory("IWeaponSkin@ CreateViewRifleSkinA(Renderer@, AudioDevice@)");
				static ScriptFunction smgViewFactory("IWeaponSkin@ CreateViewSMGSkinA(Renderer@, AudioDevice@)");
				static ScriptFunction shotgunViewFactory("IWeaponSkin@ CreateViewShotgunSkinA(Renderer@, AudioDevice@)");
			
				switch (p->GetWeapon()->GetWeaponType())
				{
				case RIFLE_WEAPON:
					weaponSkin = initScriptFactory(rifleFactory, renderer, audio);
					weaponViewSkin = initScriptFactory(rifleViewFactory, renderer, audio);
					break;
				case SMG_WEAPON:
					weaponSkin = initScriptFactory(smgFactory, renderer, audio);
					weaponViewSkin = initScriptFactory(smgViewFactory, renderer, audio);
					break;
				case SHOTGUN_WEAPON:
					weaponSkin = initScriptFactory(shotgunFactory, renderer, audio);
					weaponViewSkin = initScriptFactory(shotgunViewFactory, renderer, audio);
					break;
				default:
					SPAssert(false);
				}
			}
			else //if (p->GetTeamId() == 1)
			{
				static ScriptFunction spadeFactory("ISpadeSkin@ CreateThirdPersonSpadeSkinB(Renderer@, AudioDevice@)");
				spadeSkin = initScriptFactory(spadeFactory, renderer, audio);

				static ScriptFunction spadeViewFactory("ISpadeSkin@ CreateViewSpadeSkinB(Renderer@, AudioDevice@)");
				spadeViewSkin = initScriptFactory(spadeViewFactory, renderer, audio);

				static ScriptFunction blockFactory("IBlockSkin@ CreateThirdPersonBlockSkinB(Renderer@, AudioDevice@)");
				blockSkin = initScriptFactory(blockFactory, renderer, audio);

				static ScriptFunction blockViewFactory("IBlockSkin@ CreateViewBlockSkinB(Renderer@, AudioDevice@)");
				blockViewSkin = initScriptFactory(blockViewFactory, renderer, audio);

				static ScriptFunction grenadeFactory("IGrenadeSkin@ CreateThirdPersonGrenadeSkinB(Renderer@, AudioDevice@)");
				grenadeSkin = initScriptFactory(grenadeFactory, renderer, audio);

				static ScriptFunction grenadeViewFactory("IGrenadeSkin@ CreateViewGrenadeSkinB(Renderer@, AudioDevice@)");
				grenadeViewSkin = initScriptFactory(grenadeViewFactory, renderer, audio);

				static ScriptFunction rifleFactory("IWeaponSkin@ CreateThirdPersonRifleSkinB(Renderer@, AudioDevice@)");
				static ScriptFunction smgFactory("IWeaponSkin@ CreateThirdPersonSMGSkinB(Renderer@, AudioDevice@)");
				static ScriptFunction shotgunFactory("IWeaponSkin@ CreateThirdPersonShotgunSkinB(Renderer@, AudioDevice@)");
				static ScriptFunction rifleViewFactory("IWeaponSkin@ CreateViewRifleSkinB(Renderer@, AudioDevice@)");
				static ScriptFunction smgViewFactory("IWeaponSkin@ CreateViewSMGSkinB(Renderer@, AudioDevice@)");
				static ScriptFunction shotgunViewFactory("IWeaponSkin@ CreateViewShotgunSkinB(Renderer@, AudioDevice@)");
				
				switch (p->GetWeapon()->GetWeaponType())
				{
				case RIFLE_WEAPON:
					weaponSkin = initScriptFactory(rifleFactory, renderer, audio);
					weaponViewSkin = initScriptFactory(rifleViewFactory, renderer, audio);
					break;
				case SMG_WEAPON:
					weaponSkin = initScriptFactory(smgFactory, renderer, audio);
					weaponViewSkin = initScriptFactory(smgViewFactory, renderer, audio);
					break;
				case SHOTGUN_WEAPON:
					weaponSkin = initScriptFactory(shotgunFactory, renderer, audio);
					weaponViewSkin = initScriptFactory(shotgunViewFactory, renderer, audio);
					break;
				default:
					SPAssert(false);
				}
			}			
		}
		ClientPlayer::~ClientPlayer() {
			spadeSkin->Release();
			blockSkin->Release();
			weaponSkin->Release();
			grenadeSkin->Release();
			
			spadeViewSkin->Release();
			blockViewSkin->Release();
			weaponViewSkin->Release();
			grenadeViewSkin->Release();
			
		}

		asIScriptObject* ClientPlayer::initScriptFactory( ScriptFunction& creator, IRenderer* renderer, IAudioDevice* audio )
		{
			ScriptContextHandle ctx = creator.Prepare();
			ctx->SetArgObject(0, reinterpret_cast<void*>(renderer));
			ctx->SetArgObject(1, reinterpret_cast<void*>(audio));
			ctx.ExecuteChecked();
			asIScriptObject* result = reinterpret_cast<asIScriptObject *>(ctx->GetReturnObject());
			result->AddRef();
			return result;
		}

		void ClientPlayer::Invalidate() {
			player = NULL;
		}
		
		bool ClientPlayer::IsChangingTool()
		{
			return currentTool != player->GetTool() ||
			toolRaiseState < .999f;
		}
		
		float ClientPlayer::GetLocalFireVibration() 
		{
			float localFireVibration = 0.f;
			localFireVibration = time - localFireVibrationTime;
			localFireVibration = 1.f - localFireVibration / 0.1f;
			if(localFireVibration < 0.f)
				localFireVibration = 0.f;
			return localFireVibration;
		}
		
		void ClientPlayer::Update(float dt)
		{
			time += dt;
			
			PlayerInput actualInput = player->GetInput();
			WeaponInput actualWeapInput = player->GetWeaponInput();
			Weapon *plrWeap = player->GetWeapon();
			Vector3 vel = player->GetVelocity();
			vel.z = 0.f;

			if(actualInput.sprint && player->IsAlive() &&
			   vel.GetLength() > .1f)
			{
				if (actualInput.moveBackward || actualInput.moveForward || actualInput.moveLeft || actualInput.moveRight)
					sprintState += dt * 4.f;
				if(sprintState > 1.f)
					sprintState = 1.f;
			}
			else
			{
				sprintState -= dt * 3.f;
				if(sprintState < 0.f)
					sprintState = 0.f;
			}

			//Unzoom when jumping //unzooms only when not touching the ground...
			/*if (actualInput.jump || player->GetVelocity().z < -0.25f || player->GetVelocity().z > 0.25f)
				actualWeapInput.secondary = false;*/

			if(actualWeapInput.secondary && player->IsToolWeapon() && player->IsAlive())
			{
				aimDownState += dt * 10.f;
				if(aimDownState > 1.f)
					aimDownState = 1.f;
			}
			else if (actualWeapInput.secondary && player->GetTool() == Player::ToolGrenade && (int)v_binocsZoom != -1 && player->IsAlive())
			{
				aimDownState += dt * 20.f;
				if (aimDownState > 1.f)
					aimDownState = 1.f;
			}
			else
			{
				aimDownState -= dt * 10.f;
				if(aimDownState < 0.f)
					aimDownState = 0.f;
			}

			//drunk mode
			if (actualWeapInput.secondary && player->GetTool() == Player::ToolGrenade && (int)v_binocsZoom == -1)
			{
				if (GetRandom() > 0.3f)
					client->Leak(player->GetPosition()+Vector3(0,0,1), player->GetVelocity()*0.2f+player->GetFront()+Vector3(0,0,-0.3f));

				if (client->mouseRoll < 10)
					client->mouseRoll += client->weapX*dt*10;
				else
					client->mouseRoll += client->weapX*dt*2;
			}
			
			if(currentTool == player->GetTool()) 
			{
				toolRaiseState += dt * 4.f;
				if(toolRaiseState > 1.f)
					toolRaiseState = 1.f;
				if(toolRaiseState < 0.f)
					toolRaiseState = 0.f;
				//Don't start digging/dragging if just zoomed weapon
				if (toolRaiseState < 0.9f)
					actualWeapInput.secondary = false;
			}
			else
			{
				actualWeapInput.secondary = false;
				toolRaiseState -= dt * 4.f;
				if(toolRaiseState < 0.f)
				{
					toolRaiseState = 0.f;
					currentTool = player->GetTool();
					
					// play tool change sound
					if(player->IsLocalPlayer())
					{
						auto *audioDevice = client->GetAudioDevice();
						Handle<IAudioChunk> c;
						switch(player->GetTool()) {
							case Player::ToolSpade:
								c = audioDevice->RegisterSound("Sounds/Weapons/Select/Spade.wav");
								break;
							case Player::ToolBlock:
								c = audioDevice->RegisterSound("Sounds/Weapons/Select/Block.wav");
								break;
							case Player::ToolWeapon:
								switch(player->GetWeapon()->GetWeaponType()){
									case RIFLE_WEAPON:
										c = audioDevice->RegisterSound("Sounds/Weapons/Select/Rifle.wav");
										break;
									case SMG_WEAPON:
										c = audioDevice->RegisterSound("Sounds/Weapons/Select/SMG.wav");
										break;
									case SHOTGUN_WEAPON:
										c = audioDevice->RegisterSound("Sounds/Weapons/Select/Shotgun.wav");
										break;
								}
								
								break;
							case Player::ToolGrenade:
								c = audioDevice->RegisterSound("Sounds/Weapons/Select/Grenade.wav");
								break;
						}
						audioDevice->PlayLocal(c, MakeVector3(.4f, -.3f, .5f),
											   AudioParam());
					}
				}
				else if(toolRaiseState > 1.f)
				{
					toolRaiseState = 1.f;
				}
			}
			
			//Set weapon input (spade/block fix)
			player->SetWeaponInput(actualWeapInput);

			if (player->IsLocalPlayer())
			{
				float scale = dt;
				Vector3 vel = player->GetVelocity();
				Vector3 front = player->GetFront();
				Vector3 right = player->GetRight();
				Vector3 up = player->GetUp();
				viewWeaponOffset.x += Vector3::Dot(vel, right) * scale*2;
				viewWeaponOffset.y -= Vector3::Dot(vel, front) * scale;
				viewWeaponOffset.z += Vector3::Dot(vel, up) * scale;

				//Chameleon: weapon visual lag
				{
					//reverse it when not aiming down
					viewWeaponOffset.x -= client->weapX*dt*5 * (2*GetAimDownState() - 1);
					viewWeaponOffset.z += client->weapY*dt*5 * (2*GetAimDownState() - 1);

					client->weapX -= client->weapX*dt*5;
					client->weapY -= client->weapY*dt*5;
				}

				if(dt > 0.f)
					viewWeaponOffset *= powf(.02f, dt);
 
				if(currentTool == Player::ToolWeapon &&
				   player->GetWeaponInput().secondary) 
				{
					
					if(dt > 0.f)
						viewWeaponOffset *= powf(.01f, dt);
					
					const float limitX = 0.0025f;
					const float limitY = 0.0025f;
					if(viewWeaponOffset.x < -limitX)
						viewWeaponOffset.x = Mix(viewWeaponOffset.x, -limitX, 0.5f);
					if(viewWeaponOffset.x > limitX)
						viewWeaponOffset.x = Mix(viewWeaponOffset.x, limitX, 0.5f);
					if(viewWeaponOffset.z < 0.f)
						viewWeaponOffset.z = Mix(viewWeaponOffset.z, 0.f, 0.5f);
					if(viewWeaponOffset.z > limitY)
						viewWeaponOffset.z = Mix(viewWeaponOffset.z, limitY, 0.5f);
				}

				//testing
				/*{
					client->weapX = (float)d_x;
					client->weapY = (float)d_y;
					viewWeaponOffset.x = client->weapX* (2 * GetAimDownState() - 1);
					viewWeaponOffset.z = client->weapY* (2 * GetAimDownState() - 1);
					viewWeaponOffset.y = 0;
				}*/
			}
			
			// FIXME: should do for non-active skins?
			asIScriptObject *skin;
			if(ShouldRenderInThirdPersonView() && !player->IsLocalPlayer())
			{
				if(currentTool == Player::ToolSpade) {
					skin = spadeSkin;
				}else if(currentTool == Player::ToolBlock) {
					skin = blockSkin;
				}else if(currentTool == Player::ToolGrenade) {
					skin = grenadeSkin;
				}else if(currentTool == Player::ToolWeapon) {
					skin = weaponSkin;
				}else{
					SPInvalidEnum("currentTool", currentTool);
				}
			}
			else
			{
				if(currentTool == Player::ToolSpade) {
					skin = spadeViewSkin;
				}else if(currentTool == Player::ToolBlock) {
					skin = blockViewSkin;
				}else if(currentTool == Player::ToolGrenade) {
					skin = grenadeViewSkin;
				}else if(currentTool == Player::ToolWeapon) {
					skin = weaponViewSkin;
				}else{
					SPInvalidEnum("currentTool", currentTool);
				}
			}

			{
				ScriptIToolSkin interface(skin);
				interface.Update(dt);
			}
		}
		
		Matrix4 ClientPlayer::GetEyeMatrix() {
			Player *p = player;
			return Matrix4::FromAxis(-p->GetRight(), p->GetFront(), -p->GetUp(), p->GetEye());
		}
		
		void ClientPlayer::SetSkinParameterForTool(Player::ToolType type,
												   asIScriptObject *skin)
		{
			Player *p = player;
			if(currentTool == Player::ToolSpade) 
			{				
				ScriptISpadeSkin interface(skin);
				WeaponInput inp = p->GetWeaponInput();
				//Don't start digging after switching from aimed weapon
				if (toolRaiseState < 0.9f)
				{
					inp.secondary = false;
					p->SetWeaponInput(inp);
				}

				if(p->GetTool() != Player::ToolSpade)
				{
					interface.SetActionType(SpadeActionTypeIdle);
					interface.SetActionProgress(0.f);
				}
				else if(inp.primary) 
				{
					interface.SetActionType(SpadeActionTypeBash);
					interface.SetActionProgress(p->GetSpadeAnimationProgress());
				}
				else if(inp.secondary) 
				{
					interface.SetActionType(p->IsFirstDig() ?
					SpadeActionTypeDigStart :
											SpadeActionTypeDig);
					interface.SetActionProgress(p->GetDigAnimationProgress());
				}
				else
				{
					interface.SetActionType(SpadeActionTypeIdle);
					interface.SetActionProgress(0.f);
				}
			}
			else if(currentTool == Player::ToolBlock) 
			{				
				// TODO: smooth ready state
				ScriptIBlockSkin interface(skin);
				
				//Don't start building after switching from aimed weapon
				if (toolRaiseState < 0.9f)
				{
					WeaponInput inp = p->GetWeaponInput();
					inp.secondary = false;
					p->SetWeaponInput(inp);
				}

				if(p->GetTool() != Player::ToolBlock)
				{
					// FIXME: use block's IsReadyToUseTool
					// for smoother transition
					interface.SetReadyState(0.f);
				}
				else if(p->IsReadyToUseTool()) 
				{
					interface.SetReadyState(1.f);
				}
				else
				{
					interface.SetReadyState(0.f);
				}
				
				interface.SetBlockColor(MakeVector3(p->GetBlockColor()) / 255.f);
				if (player->IsLocalPlayer())
					interface.SetIsDragging(player->IsBlockCursorDragging());
				else
					interface.SetIsDragging(false);
			}
			else if(currentTool == Player::ToolGrenade)
			{
				ScriptIGrenadeSkin interface(skin);
				if (p->GetNumGrenades() > 0)
					interface.SetReadyState(1.f - p->GetTimeToNextGrenade() / 0.5f);
				else
					interface.SetReadyState(0.f);

				WeaponInput inp = p->GetWeaponInput();
				if(inp.primary)
				{
					interface.SetCookTime(p->GetGrenadeCookTime());
				}
				else
				{
					interface.SetCookTime(0.f);
				}
			}
			else if(currentTool == Player::ToolWeapon) 
			{
				Weapon *w = p->GetWeapon();
				ScriptIWeaponSkin interface(skin);
				interface.SetReadyState(1.f - w->TimeToNextFire() / w->GetDelay());
				interface.SetAimDownSightState(aimDownState);
				interface.SetAmmo(w->GetAmmo());
				interface.SetClipSize(w->GetClipSize());
				interface.SetReloading(w->IsReloading());
				interface.SetReloadProgress(w->GetReloadProgress());
			}
			else
			{
				SPInvalidEnum("currentTool", currentTool);
			}
		}
		
		void ClientPlayer::SetCommonSkinParameter(asIScriptObject *skin)
		{
			asIScriptObject *curSkin;
			if(ShouldRenderInThirdPersonView())
			{
				if(currentTool == Player::ToolSpade) {
					curSkin = spadeSkin;
				}else if(currentTool == Player::ToolBlock) {
					curSkin = blockSkin;
				}else if(currentTool == Player::ToolGrenade) {
					curSkin = grenadeSkin;
				}else if(currentTool == Player::ToolWeapon) {
					curSkin = weaponSkin;
				}else{
					SPInvalidEnum("currentTool", currentTool);
				}
			}
			else
			{
				if(currentTool == Player::ToolSpade) {
					curSkin = spadeViewSkin;
				}else if(currentTool == Player::ToolBlock) {
					curSkin = blockViewSkin;
				}else if(currentTool == Player::ToolGrenade) {
					curSkin = grenadeViewSkin;
				}else if(currentTool == Player::ToolWeapon) {
					curSkin = weaponViewSkin;
				}else{
					SPInvalidEnum("currentTool", currentTool);
				}
			}
			
			float sprint = SmoothStep(sprintState);
			float putdown = 1.f - toolRaiseState;
			putdown *= putdown;
			putdown = std::min(1.f, putdown * 1.5f);
			{
				ScriptIToolSkin interface(skin);
				interface.SetRaiseState((skin == curSkin)?
										(1.f - putdown):
										0.f);
				interface.SetSprintState(sprint);
				interface.SetMuted(client->IsMuted());

				if (client->GetWorld()->GetLocalPlayer())
					interface.SetClientDistance((player->GetPosition() - client->GetWorld()->GetLocalPlayer()->GetPosition()).GetLength());
				else
					interface.SetClientDistance((player->GetPosition() - client->GetLastSceneDef().viewOrigin).GetLength());

				interface.SetSoundDistance(client->soundDistance);
				if (player->IsLocalPlayer())
					interface.SetTeamColor(player->GetColor());
			}
		}
		
		void ClientPlayer::AddToSceneFirstPersonView() 
		{
			Player *p = player;
			IRenderer *renderer = client->GetRenderer();
			World *world = client->GetWorld();
			Matrix4 eyeMatrix = GetEyeMatrix();
			
			sandboxedRenderer->SetClipBox(AABB3(eyeMatrix.GetOrigin() - Vector3(20.f, 20.f, 20.f),
												eyeMatrix.GetOrigin() + Vector3(20.f, 20.f, 20.f)));
			sandboxedRenderer->SetAllowDepthHack(true);
			
			if(client->flashlightOn && r_dlights)
			{
				float brightness;
				brightness = client->time - client->flashlightOnTime;
				brightness = 1.f - expf(-brightness * 3.f);
				
				// add flash light
				DynamicLightParam light;
				light.origin = (eyeMatrix * MakeVector3(0, -0.1f, -0.5f)).GetXYZ();
				light.color = MakeVector3(1, .8f, .5f) * 1.5f * brightness;
				light.radius = 50.f;
				light.type = DynamicLightTypeSpotlight;
				light.spotAngle = 30.f * M_PI / 180.f;
				light.spotAxis[0] = p->GetRight();
				light.spotAxis[1] = p->GetUp();
				light.spotAxis[2] = p->GetFront();
				light.image = renderer->RegisterImage("Gfx/Spotlight.tga");
				renderer->AddLight(light);
				
				light.color *= 0.2f;
				light.radius = 5.f;
				light.type = DynamicLightTypePoint;
				light.image = NULL;
				renderer->AddLight(light);
				
				// add glare
				//renderer->SetColorAlphaPremultiplied(MakeVector4(1, .7f, .5f, 0) * brightness * .3f);
				//renderer->AddSprite(renderer->RegisterImage("Gfx/Glare.tga"), (eyeMatrix * MakeVector3(0, 0.3f, -0.3f)).GetXYZ(), .8f, 0.f);
			}
			
			Vector3 leftHand, rightHand;
			leftHand = MakeVector3(0, 0, 0);
			rightHand = MakeVector3(0, 0, 0);
			
			// view weapon
			
			Vector3 viewWeaponOffset = this->viewWeaponOffset;
			
			// bobbing
			{
				float sp = 2.f - aimDownState;
				sp *= 0.015f;
				sp *= std::min(1.f, p->GetVelocity().GetLength() * 5.f);
				viewWeaponOffset.x += sinf(p->GetWalkAnimationProgress() * M_PI * 2.f) * 0.01f * sp;
				float vl = cosf(p->GetWalkAnimationProgress() * M_PI * 2.f);
				vl *= vl;
				viewWeaponOffset.z += vl * 0.012f * sp;
			}
			
			// slow pulse
			{
				float sp = 1.f - aimDownState;
				float vl = sinf(world->GetTime() * 1.f);
				
				viewWeaponOffset.x += vl * 0.001f * sp;
				viewWeaponOffset.y += vl * 0.0007f * sp;
				viewWeaponOffset.z += vl * 0.003f * sp;
			}
			
			asIScriptObject *skin;
			if (currentTool == Player::ToolSpade)
			{
				skin = spadeViewSkin;
			}
			else if (currentTool == Player::ToolBlock)
			{
				skin = blockViewSkin;
			}
			else if (currentTool == Player::ToolGrenade)
			{
				skin = grenadeViewSkin;
			}
			else if (currentTool == Player::ToolWeapon)
			{
				skin = weaponViewSkin;
			}
			else
			{
				SPInvalidEnum("currentTool", currentTool);
			}
			SetSkinParameterForTool(currentTool, skin);
			
			SetCommonSkinParameter(skin);
			
			// common process
			{
				ScriptIViewToolSkin interface(skin);
				interface.SetEyeMatrix(GetEyeMatrix());
				interface.SetSwing(viewWeaponOffset);

				if (currentTool == Player::ToolWeapon)
				{
					if (client->scopeOn && client->scopeView)
						interface.SetScopeZoom(client->scopeZoom);
					else if (client->scopeOn && !client->scopeView)
						interface.SetScopeZoom(-1);
					else
						interface.SetScopeZoom(0);
				}
			}
			{
				ScriptIToolSkin interface(skin);
				interface.AddToScene();	
			}
			{
				ScriptIViewToolSkin interface(skin);
				leftHand = interface.GetLeftHandPosition();
				rightHand = interface.GetRightHandPosition();
				//
			}
			
			// view hands
			if(leftHand.GetPoweredLength() > 0.001f &&
			   rightHand.GetPoweredLength() > 0.001f)
			{
				
				
				ModelRenderParam param;
				//param.depthHack = true;
				
				IModel *model = (p->GetTeamId() == 0) ? 
					renderer->RegisterModel("Models/PlayerA/Arm_Low.kv6"):
					renderer->RegisterModel("Models/PlayerB/Arm_Low.kv6");
				IModel *model2 = (p->GetTeamId() == 0) ? 
					renderer->RegisterModel("Models/PlayerA/Arm_Up.kv6"):
					renderer->RegisterModel("Models/PlayerB/Arm_Up.kv6");
				
				
				IntVector3 col = p->GetColor();
				param.customColor = MakeVector3(col.x/255.f, col.y/255.f, col.z/255.f);
				
				const float armlen = 0.5f;
				
				Vector3 shoulders[] = {{0.4f, 0.0f, 0.25f},
					{-0.4f, 0.0f, 0.25f}};
				Vector3 hands[] = {leftHand, rightHand};
				Vector3 benddirs[] = {{0.5f, 0.2f, 0.f},
					{-0.5f, 0.2f, 0.f}};
				for(int i = 0; i < 2; i++){
					Vector3 shoulder = shoulders[i];
					Vector3 hand = hands[i];
					Vector3 benddir = benddirs[i];
					
					float len2 = (hand - shoulder).GetPoweredLength();
					// len2/4 + x^2 = armlen^2
					float bendlen = sqrtf(std::max(armlen*armlen - len2*.25f, 0.f));
					
					Vector3 bend = Vector3::Cross(benddir, hand-shoulder);
					bend = bend.Normalize();
					
					if(bend.z < 0.f) bend.z = -bend.z;
					
					Vector3 elbow = (hand + shoulder) * .5f;
					elbow += bend * bendlen;
					
					{
						Vector3 axises[3];
						axises[2] = (hand - elbow).Normalize();
						axises[0] = MakeVector3(0, 0, 1);
						axises[1] = Vector3::Cross(axises[2], axises[0]).Normalize();
						axises[0] = Vector3::Cross(axises[1], axises[2]).Normalize();
						
						Matrix4 mat = Matrix4::Scale(0.025f, 0.025f, 0.025f);
						mat = Matrix4::FromAxis(axises[0], axises[1], axises[2], elbow) * mat;
						mat = eyeMatrix * mat;
						
						param.matrix = mat;
						renderer->RenderModel(model, param);
					}
					
					{
						Vector3 axises[3];
						axises[2] = (elbow - shoulder).Normalize();
						axises[0] = MakeVector3(0, 0, 1);
						axises[1] = Vector3::Cross(axises[2], axises[0]).Normalize();
						axises[0] = Vector3::Cross(axises[1], axises[2]).Normalize();
						
						Matrix4 mat = Matrix4::Scale(0.025f, 0.025f, 0.025f);
						mat = Matrix4::FromAxis(axises[0], axises[1], axises[2], shoulder) * mat;
						mat = eyeMatrix * mat;
						
						param.matrix = mat;
						renderer->RenderModel(model2, param);
					}
				}
			}
			// --- local view ends
		}
		
		
		void ClientPlayer::AddToSceneThirdPersonView()
		{

			Player *p = player;
			IRenderer *renderer = client->GetRenderer();
			World *world = client->GetWorld();
			//Chameleon - for distance to models, LOD
			
			
			if(!p->IsAlive())
			{
				if(!cg_ragdoll)
				{
					ModelRenderParam param;
					//param.matrix = Matrix4::Translate(p->GetOrigin() + Vector3(0,0,1)); //old, all face north
					Vector3 up = MakeVector3(0, 0, 1);
					Vector3 dir = Vector3::Cross(p->GetRight(), up).Normalize();

					param.matrix = Matrix4::FromAxis(-p->GetRight(), dir, up, p->GetOrigin() + MakeVector3(0, 0, 1));
					param.matrix = param.matrix * Matrix4::Scale(.1f);
					IntVector3 col = p->GetColor();
					param.customColor = MakeVector3(col.x/255.f, col.y/255.f, col.z/255.f);
					
					IModel *model = (p->GetTeamId() == 0) ?
						renderer->RegisterModel("Models/PlayerA/Dead.kv6") :
						renderer->RegisterModel("Models/PlayerB/Dead.kv6");

					//Chameleon
					Vector3 fixDim = MakeVector3(23, 25, 6);
					Vector3 ModelDim = MakeVector3(model->GetDimensions());
					Vector3 scale = ModelDim/fixDim;
					ModelRenderParam paramTMP = param;
					paramTMP.matrix *= Matrix4::Scale(scale);
					//Chameleon
					renderer->RenderModel(model, paramTMP);
				}
				return;
			}
			
			auto origin = p->GetOrigin();
			sandboxedRenderer->SetClipBox(AABB3(origin - Vector3(4.f, 4.f, 5.f),
												origin + Vector3(4.f, 4.f, 3.f)));
			sandboxedRenderer->SetAllowDepthHack(false);
			
			// ready for tool rendering
			asIScriptObject *skin;

			if (currentTool == Player::ToolSpade)
			{
				skin = spadeSkin;
			}
			else if (currentTool == Player::ToolBlock)
			{
				skin = blockSkin;
			}
			else if (currentTool == Player::ToolGrenade)
			{
				skin = grenadeSkin;
			}
			else if (currentTool == Player::ToolWeapon)
			{
				skin = weaponSkin;
			}
			else
			{
				SPInvalidEnum("currentTool", currentTool);
			}

			SetSkinParameterForTool(currentTool, skin);

			SetCommonSkinParameter(skin);

			float pitchBias;
			{
				ScriptIThirdPersonToolSkin interface(skin);
				pitchBias = interface.GetPitchBias();
			}
			ModelRenderParam param;
			//Chameleon
			ModelRenderParam paramTMP; //temporary, used for scaling LOD models
			IModel *model;
			Vector3 front = p->GetFront();
			IntVector3 col = p->GetColor();
			param.customColor = MakeVector3(col.x/255.f, col.y/255.f, col.z/255.f);
			
			float yaw = atan2(front.y, front.x) + M_PI * .5f;
			float pitch = -atan2(front.z, sqrt(front.x * front.x + front.y * front.y));
			
			// lower axis
			Matrix4 lower = Matrix4::Translate(p->GetOrigin());
			lower = lower * Matrix4::Rotate(MakeVector3(0,0,1), yaw);
			
			Matrix4 scaler = Matrix4::Scale(0.1f);
			scaler  = scaler * Matrix4::Scale(-1,-1,1);
			
			PlayerInput inp = p->GetInput();
			
			// lower
			Matrix4 torso, head, arms;
			if(inp.crouch)
			{
				Matrix4 leg1 = Matrix4::Translate(-0.25f, 0.2f, -0.1f);
				Matrix4 leg2 = Matrix4::Translate( 0.25f, 0.2f, -0.1f);
				
				float ang = sinf(p->GetWalkAnimationProgress() * M_PI * 2.f) * 0.6f;
				float walkVel = Vector3::Dot(p->GetVelocity(), p->GetFront2D()) * 4.f;
				leg1 = leg1 * Matrix4::Rotate(MakeVector3(1,0,0), ang * walkVel);
				leg2 = leg2 * Matrix4::Rotate(MakeVector3(1,0,0), -ang * walkVel);
				
				walkVel = Vector3::Dot(p->GetVelocity(), p->GetRight()) * 3.f;
				leg1 = leg1 * Matrix4::Rotate(MakeVector3(0,1,0), ang * walkVel);
				leg2 = leg2 * Matrix4::Rotate(MakeVector3(0,1,0), -ang * walkVel);
				
				leg1 = lower * leg1;
				leg2 = lower * leg2;
				
				model = (p->GetTeamId() == 0) ?
					renderer->RegisterModel("Models/PlayerA/LegCrouch.kv6") :
					renderer->RegisterModel("Models/PlayerB/LegCrouch.kv6");

				//Chameleon
				Vector3 fixDim = MakeVector3(3, 7, 8);
				Vector3 ModelDim = MakeVector3(model->GetDimensions());
				Vector3 scale = fixDim / ModelDim;
				ModelRenderParam paramTMP = param;
				//Chameleon
				
				paramTMP.matrix = leg1 * scaler;
				paramTMP.matrix *= Matrix4::Scale(scale);//Chameleon
				renderer->RenderModel(model, paramTMP);
				paramTMP.matrix = leg2 * scaler;
				paramTMP.matrix *= Matrix4::Scale(scale);//Chameleon
				renderer->RenderModel(model, paramTMP);
				
				torso = Matrix4::Translate(0.f,0.f,-0.55f);
				torso = lower * torso;
				
				model = (p->GetTeamId() == 0) ?
					renderer->RegisterModel("Models/PlayerA/TorsoCrouch.kv6") :
					renderer->RegisterModel("Models/PlayerB/TorsoCrouch.kv6");

				//Chameleon
				fixDim = MakeVector3(8, 8, 7);
				ModelDim = MakeVector3(model->GetDimensions());
				scale = fixDim / ModelDim;
				paramTMP = param;
				//Chameleon

				paramTMP.matrix = torso * scaler;
				paramTMP.matrix *= Matrix4::Scale(scale);//Chameleon
				renderer->RenderModel(model, paramTMP);
				
				head = Matrix4::Translate(0.f,0.f,-0.0f);
				head = torso * head;
				
				arms = Matrix4::Translate(0.f,0.f,-0.0f);
				arms = torso * arms;
			}
			else
			{
				Matrix4 leg1 = Matrix4::Translate(-0.25f, 0.f, -0.1f);
				Matrix4 leg2 = Matrix4::Translate( 0.25f, 0.f, -0.1f);
				
				float ang = sinf(p->GetWalkAnimationProgress() * M_PI * 2.f) * 0.6f;
				float walkVel = Vector3::Dot(p->GetVelocity(), p->GetFront2D()) * 4.f;
				leg1 = leg1 * Matrix4::Rotate(MakeVector3(1,0,0), ang * walkVel);
				leg2 = leg2 * Matrix4::Rotate(MakeVector3(1,0,0), -ang * walkVel);
				
				walkVel = Vector3::Dot(p->GetVelocity(), p->GetRight()) * 3.f;
				leg1 = leg1 * Matrix4::Rotate(MakeVector3(0,1,0), ang * walkVel);
				leg2 = leg2 * Matrix4::Rotate(MakeVector3(0,1,0), -ang * walkVel);
				
				leg1 = lower * leg1;
				leg2 = lower * leg2;
				
				model = (p->GetTeamId() == 0) ?
					renderer->RegisterModel("Models/PlayerA/Leg.kv6") :
					renderer->RegisterModel("Models/PlayerB/Leg.kv6");

				//Chameleon
				Vector3 fixDim = MakeVector3(3, 5, 12);
				Vector3 ModelDim = MakeVector3(model->GetDimensions());
				Vector3 scale = fixDim / ModelDim;
				ModelRenderParam paramTMP = param;
				//Chameleon

				paramTMP.matrix = leg1 * scaler;
				paramTMP.matrix *= Matrix4::Scale(scale);//Chameleon
				renderer->RenderModel(model, paramTMP);
				paramTMP.matrix = leg2 * scaler;
				paramTMP.matrix *= Matrix4::Scale(scale);//Chameleon
				renderer->RenderModel(model, paramTMP);
				
				torso = Matrix4::Translate(0.f,0.f,-1.0f);
				torso = lower * torso;
				
				model = (p->GetTeamId() == 0) ?
					renderer->RegisterModel("Models/PlayerA/Torso.kv6") :
					renderer->RegisterModel("Models/PlayerB/Torso.kv6");

				//Chameleon
				fixDim = MakeVector3(8, 4, 9);
				ModelDim = MakeVector3(model->GetDimensions());
				scale = fixDim / ModelDim;
				paramTMP = param;
				//Chameleon

				paramTMP.matrix = torso * scaler;
				paramTMP.matrix *= Matrix4::Scale(scale);//Chameleon
				renderer->RenderModel(model, paramTMP);
				
				head = Matrix4::Translate(0.f,0.f,-0.0f);
				head = torso * head;
				
				arms = Matrix4::Translate(0.f,0.f,0.1f);
				arms = torso * arms;
			}
			
			float armPitch = pitch;
			if(inp.sprint) {
				armPitch -= .5f;
			}
			armPitch += pitchBias;

			if (p->GetTool() == Player::ToolWeapon && !p->GetWeaponInput().secondary)
				armPitch -= 0.2f;

			if(armPitch < 0.f) {
				armPitch = std::max(armPitch, -(float)M_PI * 0.5f);
				armPitch *= 0.9f;
			}

			arms = arms * Matrix4::Rotate(MakeVector3(1,0,0), armPitch);
			
			if (p->GetTool() != Player::ToolWeapon)
			{
				model = (p->GetTeamId() == 0) ?
					renderer->RegisterModel("Models/PlayerA/ArmsTool.kv6") :
					renderer->RegisterModel("Models/PlayerB/ArmsTool.kv6");
			}
			else
			{
				model = (p->GetTeamId() == 0) ?
					renderer->RegisterModel("Models/PlayerA/ArmsWeap.kv6") :
					renderer->RegisterModel("Models/PlayerB/ArmsWeap.kv6");
			}

			//Chameleon
			Vector3 fixDim = MakeVector3(12, 14, 7);
			if (p->GetTool() != Player::ToolWeapon)
				fixDim = MakeVector3(12, 10, 7);

			Vector3 ModelDim = MakeVector3(model->GetDimensions());
			Vector3 scale = fixDim / ModelDim;
			paramTMP = param;
			//Chameleon

			paramTMP.matrix = arms * scaler;
			paramTMP.matrix *= Matrix4::Scale(scale);//Chameleon
			renderer->RenderModel(model, paramTMP);
			
			
			head = head * Matrix4::Rotate(MakeVector3(1,0,0), pitch);
			
			model = (p->GetTeamId() == 0) ?
				renderer->RegisterModel("Models/PlayerA/Head.kv6") :
				renderer->RegisterModel("Models/PlayerB/Head.kv6");

			//Chameleon
			fixDim = MakeVector3(5, 5, 5);
			ModelDim = MakeVector3(model->GetDimensions());
			scale = fixDim / ModelDim;
			paramTMP = param;
			//Chameleon

			paramTMP.matrix = head * scaler;
			paramTMP.matrix *= Matrix4::Scale(scale);//Chameleon
			if (p->GetWeaponInput().secondary) //when aiming, player tilts head to right
			{
				paramTMP.matrix *= Matrix4::Rotate(Vector3(0, 1, 0), 0.15f);//Chameleon
			}
			if (world->GetLocalPlayer())
			{
				if (!client->IsFollowing() && (world->GetLocalPlayer()->GetPosition() - p->GetPosition()).GetLength() > 0.5f)
					renderer->RenderModel(model, paramTMP);
				else if (client->IsFollowing() && player->GetId() != client->followingPlayerId)
					renderer->RenderModel(model, paramTMP);
			}
			else
			{
				renderer->RenderModel(model, paramTMP);
			}

			// draw tool
			{
				//hipped weapons are drawn lower
				if (p->GetTool() == Player::ToolWeapon && !p->GetWeaponInput().secondary)
				{
					arms = arms * Matrix4::Rotate(MakeVector3(1, 0, 0), 0.2f);
					arms = arms * Matrix4::Translate(0, 0, 0.2f);
				}
				ScriptIThirdPersonToolSkin interface(skin);
				interface.SetOriginMatrix(arms);
			}
			{
				//Chameleon: to do - when not aiming, move weapon to hip
				ScriptIToolSkin interface(skin);
				interface.AddToScene();
			}
			
			
			// draw intel in ctf
			IGameMode* mode = world->GetMode();
			if( mode && IGameMode::m_CTF == mode->ModeType() ){
				CTFGameMode *ctfMode = static_cast<CTFGameMode *>(world->GetMode());
				int tId = p->GetTeamId();
				if(tId < 3){
					CTFGameMode::Team& team = ctfMode->GetTeam(p->GetTeamId());
					if(team.hasIntel && team.carrier == p->GetId())
					{						
						IntVector3 col2 = world->GetTeam(1-p->GetTeamId()).color;
						param.customColor = MakeVector3(col2.x/255.f, col2.y/255.f, col2.z/255.f);
						Matrix4 mIntel = torso * Matrix4::Translate(0,0.5f,0.5f);
						
						//you carry other team's intel, not yours
						model = (p->GetTeamId() == 0) ?
							renderer->RegisterModel("Models/MapObjects/IntelB.kv6") :
							renderer->RegisterModel("Models/MapObjects/IntelA.kv6");

						//Chameleon
						Vector3 fixDim = MakeVector3(10, 3, 8);
						Vector3 ModelDim = MakeVector3(model->GetDimensions());
						Vector3 scale = fixDim / ModelDim;
						ModelRenderParam paramTMP = param;
						//Chameleon

						paramTMP.matrix = mIntel * scaler;
						paramTMP.matrix *= Matrix4::Scale(scale);//Chameleon
						renderer->RenderModel(model, paramTMP);
						//param.customColor = MakeVector3(col.x/255.f, col.y/255.f, col.z/255.f);
					}
				}
			}
			// third person player rendering, done
		}
		
		//Chameleon: don't draw head
		void ClientPlayer::AddToSceneMixedPersonView()
		{
			Player *p = player;
			IRenderer *renderer = client->GetRenderer();
			World *world = client->GetWorld();

			if (!p->IsAlive())
			{
				return;
			}

			auto origin = p->GetOrigin();
			sandboxedRenderer->SetClipBox(AABB3(origin - Vector3(2.f, 2.f, 4.f),
				origin + Vector3(2.f, 2.f, 2.f)));
			sandboxedRenderer->SetAllowDepthHack(true);

			ModelRenderParam param;
			//Chameleon
			ModelRenderParam paramTMP; //temporary, used for scaling LOD models
			IModel *model;
			Vector3 front = p->GetFront();
			IntVector3 col = p->GetColor();
			param.customColor = MakeVector3(col.x / 255.f, col.y / 255.f, col.z / 255.f);

			float yaw = atan2(front.y, front.x) + M_PI * .5f;
			float pitch = -atan2(front.z, sqrt(front.x * front.x + front.y * front.y));

			// lower axis
			Matrix4 lower = Matrix4::Translate(p->GetOrigin());
			lower = lower * Matrix4::Rotate(MakeVector3(0, 0, 1), yaw);

			Matrix4 scaler = Matrix4::Scale(0.1f);
			scaler = scaler * Matrix4::Scale(-1, -1, 1);

			PlayerInput inp = p->GetInput();

			// lower
			Matrix4 torso, arms;
			if (inp.crouch)
			{
				//Legs
				if (v_drawLegs) // || ((int)v_drawLegs == 2 && p->GetFront().z < 0.9)
				{
					Matrix4 leg1 = Matrix4::Translate(-0.25f, 0.2f, -0.1f);
					Matrix4 leg2 = Matrix4::Translate(0.25f, 0.2f, -0.1f);

					if ((int)v_drawLegs == 2 && p->GetFront().z > -0.2f)
					{
						float move = (p->GetFront().z+0.2f)/1.2f;
						leg1 *= Matrix4::Translate(0.f, move, 0.f);
						leg2 *= Matrix4::Translate(0.f, move, 0.f);
					}

					float ang = sinf(p->GetWalkAnimationProgress() * M_PI * 2.f) * 0.6f;
					float walkVel = Vector3::Dot(p->GetVelocity(), p->GetFront2D()) * 4.f;
					leg1 = leg1 * Matrix4::Rotate(MakeVector3(1, 0, 0), ang * walkVel);
					leg2 = leg2 * Matrix4::Rotate(MakeVector3(1, 0, 0), -ang * walkVel);

					walkVel = Vector3::Dot(p->GetVelocity(), p->GetRight()) * 3.f;
					leg1 = leg1 * Matrix4::Rotate(MakeVector3(0, 1, 0), ang * walkVel);
					leg2 = leg2 * Matrix4::Rotate(MakeVector3(0, 1, 0), -ang * walkVel);

					leg1 = lower * leg1;
					leg2 = lower * leg2;

					model = (p->GetTeamId() == 0) ?
						renderer->RegisterModel("Models/PlayerA/LegCrouch.kv6") :
						renderer->RegisterModel("Models/PlayerB/LegCrouch.kv6");

					//Chameleon
					Vector3 fixDim = MakeVector3(3, 7, 8);
					Vector3 ModelDim = MakeVector3(model->GetDimensions());
					Vector3 scale = fixDim / ModelDim;
					ModelRenderParam paramTMP = param;
					//Chameleon

					paramTMP.matrix = leg1 * scaler;
					paramTMP.matrix *= Matrix4::Scale(scale);
					renderer->RenderModel(model, paramTMP);
					paramTMP.matrix = leg2 * scaler;
					paramTMP.matrix *= Matrix4::Scale(scale);
					renderer->RenderModel(model, paramTMP);
				}
				//Legs done

				//Torso
				torso = Matrix4::Translate(0.f, 0.f, -0.55f);
				torso = lower * torso;

				if (v_drawTorso) // || ((int)v_drawTorso == 2 && p->GetFront().z < 0.7)
				{
					model = (p->GetTeamId() == 0) ?
						renderer->RegisterModel("Models/PlayerA/TorsoCrouch.kv6") :
						renderer->RegisterModel("Models/PlayerB/TorsoCrouch.kv6");

					//Chameleon
					Vector3 fixDim = MakeVector3(8, 8, 5);
					Vector3 ModelDim = MakeVector3(model->GetDimensions());
					Vector3 scale = fixDim / ModelDim;
					ModelRenderParam paramTMP = param;
					//Chameleon

					paramTMP.matrix = torso;
					if ((int)v_drawTorso == 2 && p->GetFront().z > -0.2f)
					{
						float move = (p->GetFront().z+0.2f)/1.2f;
						paramTMP.matrix *= Matrix4::Translate(0.f, move, 0.f);
					}					
					paramTMP.matrix *= Matrix4::Scale(scale) * scaler;
					renderer->RenderModel(model, paramTMP);
				}

				arms = Matrix4::Translate(-viewWeaponOffset.x-0.25f, -viewWeaponOffset.y-p->GetFront().z*0.25f, viewWeaponOffset.z+0.2f);
				arms = torso * arms;
				//Torso done
			}
			else
			{
				//Legs
				if (v_drawLegs) // || ((int)v_drawLegs == 2 && p->GetFront().z < 0.95)
				{
					Matrix4 leg1 = Matrix4::Translate(-0.25f, 0.f, -0.1f);
					Matrix4 leg2 = Matrix4::Translate(0.25f, 0.f, -0.1f);

					if ((int)v_drawLegs == 2 && p->GetFront().z > -0.2f)
					{
						float move = (p->GetFront().z+0.2f)/1.2f;
						leg1 *= Matrix4::Translate(0.f, move, 0.f);
						leg2 *= Matrix4::Translate(0.f, move, 0.f);
					}

					float ang = sinf(p->GetWalkAnimationProgress() * M_PI * 2.f) * 0.6f;
					float walkVel = Vector3::Dot(p->GetVelocity(), p->GetFront2D()) * 4.f;
					leg1 = leg1 * Matrix4::Rotate(MakeVector3(1, 0, 0), ang * walkVel);
					leg2 = leg2 * Matrix4::Rotate(MakeVector3(1, 0, 0), -ang * walkVel);

					walkVel = Vector3::Dot(p->GetVelocity(), p->GetRight()) * 3.f;
					leg1 = leg1 * Matrix4::Rotate(MakeVector3(0, 1, 0), ang * walkVel);
					leg2 = leg2 * Matrix4::Rotate(MakeVector3(0, 1, 0), -ang * walkVel);

					leg1 = lower * leg1;
					leg2 = lower * leg2;

					model = (p->GetTeamId() == 0) ?
						renderer->RegisterModel("Models/PlayerA/Leg.kv6") :
						renderer->RegisterModel("Models/PlayerB/Leg.kv6");

					//Chameleon
					Vector3 fixDim = MakeVector3(3, 5, 12);
					Vector3 ModelDim = MakeVector3(model->GetDimensions());
					Vector3 scale = fixDim / ModelDim;
					ModelRenderParam paramTMP = param;
					//Chameleon

					paramTMP.matrix = leg1 * scaler;
					paramTMP.matrix *= Matrix4::Scale(scale);
					renderer->RenderModel(model, paramTMP);
					paramTMP.matrix = leg2 * scaler;
					paramTMP.matrix *= Matrix4::Scale(scale);
					renderer->RenderModel(model, paramTMP);
				}
				//Legs done

				//Torso
				torso = Matrix4::Translate(0.f, 0.f, -1.0f);
				torso = lower * torso;

				if (v_drawTorso) // || ((int)v_drawTorso == 2 && p->GetFront().z < 0.7)
				{
					model = (p->GetTeamId() == 0) ?
						renderer->RegisterModel("Models/PlayerA/Torso.kv6") :
						renderer->RegisterModel("Models/PlayerB/Torso.kv6");

					//Chameleon
					Vector3 fixDim = MakeVector3(8, 4, 6);
					Vector3 ModelDim = MakeVector3(model->GetDimensions());
					Vector3 scale = fixDim / ModelDim;
					ModelRenderParam paramTMP = param;
					//Chameleon

					paramTMP.matrix = torso;
					if ((int)v_drawTorso == 2 && p->GetFront().z > -0.2f)
					{
						float move = (p->GetFront().z+0.2f)/1.2f;
						paramTMP.matrix *= Matrix4::Translate(0.f, move, 0.f);
					}
					paramTMP.matrix *= Matrix4::Scale(scale) * scaler;
					renderer->RenderModel(model, paramTMP);
				}
				//Torso done

				arms = Matrix4::Translate(-viewWeaponOffset.x-0.225f, -viewWeaponOffset.y-p->GetFront().z*0.25f, viewWeaponOffset.z+0.2f);
				arms = torso * arms;
			}

			//Arms
			if ((int)v_drawArms == 1 || ((int)v_drawArms == 2 && p->GetFront().z < 0.9 && p->GetFront().z > -0.9))
			{
				float armPitch = pitch;
				if (inp.sprint) {
					armPitch -= 1.0f;
				}
				if (armPitch < 0.f) {
					armPitch = std::max(armPitch, -(float)M_PI * .5f);
					armPitch *= 0.9f;
				}

				arms = arms * Matrix4::Rotate(MakeVector3(1, 0, 0), armPitch);

				if (p->GetTool() != Player::ToolWeapon && (int)v_drawArms != 1)
				{
					model = (p->GetTeamId() == 0) ?
						renderer->RegisterModel("Models/PlayerA/ArmsTool.kv6") :
						renderer->RegisterModel("Models/PlayerB/ArmsTool.kv6");
				}
				else
				{
					model = (p->GetTeamId() == 0) ?
						renderer->RegisterModel("Models/PlayerA/ArmsWeap.kv6") :
						renderer->RegisterModel("Models/PlayerB/ArmsWeap.kv6");
				}

				//Chameleon
				Vector3 fixDim = MakeVector3(12, 14, 7);
				if (p->GetTool() != Player::ToolWeapon)
					fixDim = MakeVector3(12, 10, 7);

				Vector3 ModelDim = MakeVector3(model->GetDimensions());
				Vector3 scale = fixDim / ModelDim;
				paramTMP = param;
				//Chameleon

				paramTMP.matrix = arms * scaler;
				paramTMP.matrix *= Matrix4::Scale(scale);//Chameleon
				renderer->RenderModel(model, paramTMP);
			}
			//Arms done

			//Intel
			IGameMode* mode = world->GetMode();
			if (mode && IGameMode::m_CTF == mode->ModeType()){
				CTFGameMode *ctfMode = static_cast<CTFGameMode *>(world->GetMode());
				int tId = p->GetTeamId();
				if (tId < 3){
					CTFGameMode::Team& team = ctfMode->GetTeam(p->GetTeamId());
					if (team.hasIntel && team.carrier == p->GetId())
					{
						IntVector3 col2 = world->GetTeam(1 - p->GetTeamId()).color;
						param.customColor = MakeVector3(col2.x / 255.f, col2.y / 255.f, col2.z / 255.f);
						Matrix4 mIntel = torso * Matrix4::Translate(0, 0.5f, 0.5f);

						//you carry other team's intel, not yours
						model = (p->GetTeamId() == 0) ?
							renderer->RegisterModel("Models/MapObjects/IntelB.kv6") :
							renderer->RegisterModel("Models/MapObjects/IntelA.kv6");

						//Chameleon
						Vector3 fixDim = MakeVector3(10, 3, 8);
						Vector3 ModelDim = MakeVector3(model->GetDimensions());
						Vector3 scale = fixDim / ModelDim;
						ModelRenderParam paramTMP = param;
						//Chameleon

						paramTMP.matrix = mIntel * scaler;
						paramTMP.matrix *= Matrix4::Scale(scale);//Chameleon
						renderer->RenderModel(model, paramTMP);
						//param.customColor = MakeVector3(col.x/255.f, col.y/255.f, col.z/255.f);
					}
				}
			}
			//Intel done

			// mixed person player rendering done
		}

		void ClientPlayer::AddToScene() 
		{
			SPADES_MARK_FUNCTION();
			
			Player *p = player;
			IRenderer *renderer = client->GetRenderer();
			const SceneDefinition& lastSceneDef = client->GetLastSceneDef();

			if(p->GetTeamId() >= 2)
			{
				// spectator, or dummy player
				return;				
			}
			// debug
			/*if(false)
			{
				Handle<IImage> img = renderer->RegisterImage("Gfx/Ball.png");
				renderer->SetColorAlphaPremultiplied(MakeVector4(1, 0, 0, 0));
				renderer->AddLongSprite(img, lastSceneDef.viewOrigin + MakeVector3(0, 0, 1), p->GetOrigin(), 0.5f);
			}*/
			float distance = (p->GetOrigin() - lastSceneDef.viewOrigin).GetLength();
			if (distance > int(opt_modelMaxDist))
			{
				return;
			}
			
			/*if(!ShouldRenderInThirdPersonView())
			{
				AddToSceneFirstPersonView();
			} 
			else
			{*/
			if (!ShouldRenderInThirdPersonView())
			{
				AddToSceneFirstPersonView();
				AddToSceneMixedPersonView();				
			}
			else
			{
				AddToSceneThirdPersonView();
			}
		}
		
		void ClientPlayer::Draw2D() 
		{
			//if(!ShouldRenderInThirdPersonView()) 
			//if (player == player->GetWorld()->GetLocalPlayer())
			//{
				asIScriptObject *skin;
				
				if(currentTool == Player::ToolSpade) 
				{
					skin = spadeViewSkin;
				}
				else if(currentTool == Player::ToolBlock) 
				{
					skin = blockViewSkin;
				}
				else if(currentTool == Player::ToolGrenade) 
				{
					skin = grenadeViewSkin;
				}
				else if(currentTool == Player::ToolWeapon) 
				{
					skin = weaponViewSkin;
				}
				else
				{
					SPInvalidEnum("currentTool", currentTool);
				}
				
				SetSkinParameterForTool(currentTool, skin);
				
				SetCommonSkinParameter(skin);
				
				// common process
				{
					sandboxedRenderer->SetAllowDepthHack(true);

					ScriptIViewToolSkin interface(skin);
					interface.SetEyeMatrix(GetEyeMatrix());
					interface.SetSwing(viewWeaponOffset);
					interface.Draw2D();

					sandboxedRenderer->SetAllowDepthHack(false);
				}
			//}
		}
		
		bool ClientPlayer::ShouldRenderInThirdPersonView() 
		{
			if(player != player->GetWorld()->GetLocalPlayer())
				return true;
			return client->ShouldRenderInThirdPersonView();
		}
		
		void ClientPlayer::FiredWeapon() 
		{
			World *world = player->GetWorld();
			Vector3 muzzle;
			const SceneDefinition& lastSceneDef = client->GetLastSceneDef();
			IRenderer *renderer = client->GetRenderer();
			IAudioDevice *audioDevice = client->GetAudioDevice();
			Player *p = player;
			
			// make dlight
			{
				Vector3 vec;
				Matrix4 eyeMatrix = GetEyeMatrix();
				Matrix4 mat;
				mat = Matrix4::Translate(-0.13f,
										 .5f,
										 0.2f);
				mat = eyeMatrix * mat;
				
				vec = (mat * MakeVector3(0, 1, 0)).GetXYZ();
				muzzle = vec;
				client->MuzzleFire(vec, player->GetFront());
			}
			
			if(cg_ejectBrass)
			{
				int dist = (player->GetOrigin() - lastSceneDef.viewOrigin).GetLength();

				if (dist < int(opt_particleMaxDist)*((float(opt_particleNiceDist)+1.f)/2.f)*0.75f)
				{
					IModel *model = NULL;
					Handle<IAudioChunk> snd = NULL;
					Handle<IAudioChunk> snd2 = NULL;

					switch(player->GetWeapon()->GetWeaponType())
					{
						case RIFLE_WEAPON:
							model = renderer->RegisterModel("Models/Weapons/Objects/RifleCasing.kv6");
							snd = (rand()&0x1000)?
							audioDevice->RegisterSound("Sounds/Weapons/Objects/RifleCasing1.wav"):
							audioDevice->RegisterSound("Sounds/Weapons/Objects/RifleCasing2.wav");
							snd2 =
							audioDevice->RegisterSound("Sounds/Weapons/Objects/RifleCasingWater.wav");
							break;
						case SHOTGUN_WEAPON:
							// FIXME: don't want to show shotgun't casing
							// because it isn't ejected when firing
							//model = renderer->RegisterModel("Models/Weapons/Shotgun/Casing.kv6");
							break;
						case SMG_WEAPON:
							model = renderer->RegisterModel("Models/Weapons/Objects/SMGCasing.kv6");
							snd = (rand()&0x1000)?
							audioDevice->RegisterSound("Sounds/Weapons/Objects/SMGCasing1.wav"):
							audioDevice->RegisterSound("Sounds/Weapons/Objects/SMGCasing2.wav");
							snd2 =
							audioDevice->RegisterSound("Sounds/Weapons/Objects/SMGCasingWater.wav");
							break;
					}
					if(model)
					{
						Vector3 origin;
						origin = muzzle - p->GetFront() * 0.5f;
						
						float velRand1 = GetRandom() + 0.5f; //from 0.5 to 1.5
						float velRand2 = GetRandom() + 0.5f; //from 0.5 to 1.5
						float velRand3 = GetRandom() + 1.f; //from 0.5 to 1.5

						Vector3 vel;						
						vel = p->GetFront()*velRand1 + p->GetRight()*velRand2 + p->GetUp()*velRand3*0.5f;

						switch(p->GetWeapon()->GetWeaponType())
						{
							case SMG_WEAPON:
								vel -= p->GetFront() * 1.f;
								vel -= p->GetUp() * 0.5f;
								break;
							//case SHOTGUN_WEAPON:
							//	vel *= .5f;
								break;
							default:
								break;
						}

						ILocalEntity *ent;
						ent = new GunCasing(client, model, snd, snd2,
											origin, p->GetFront(),
											vel);
						client->AddLocalEntity(ent);
						
					}
				}
			}
			
			asIScriptObject *skin;
			// FIXME: what if current tool isn't weapon?
			if(ShouldRenderInThirdPersonView())
			{
				skin = weaponSkin;
			}
			else
			{
				skin = weaponViewSkin;
			}
			
			{
				ScriptIToolSkin interface(skin);
				if (client->GetWorld()->GetLocalPlayer())
					interface.SetClientDistance((player->GetPosition() - client->GetWorld()->GetLocalPlayer()->GetPosition()).GetLength());
				else
					interface.SetClientDistance((player->GetPosition() - client->GetLastSceneDef().viewOrigin).GetLength());
				interface.SetSoundDistance(client->soundDistance);
			}

			{
				ScriptIWeaponSkin interface(skin);
				interface.WeaponFired();
			}
	
		}
		
		void ClientPlayer::ReloadingWeapon() 
		{
			asIScriptObject *skin;
			// FIXME: what if current tool isn't weapon?
			if(ShouldRenderInThirdPersonView())
			{
				skin = weaponSkin;
			}
			else
			{
				skin = weaponViewSkin;
			}
			
			{
				ScriptIToolSkin interface(skin);
				if (client->GetWorld()->GetLocalPlayer())
					interface.SetClientDistance((player->GetPosition() - client->GetWorld()->GetLocalPlayer()->GetPosition()).GetLength());
				else
					interface.SetClientDistance((player->GetPosition() - client->GetLastSceneDef().viewOrigin).GetLength());
				interface.SetSoundDistance(client->soundDistance);
			}

			{
				
				ScriptIWeaponSkin interface(skin);
				interface.ReloadingWeapon();
			}
		}
		
		void ClientPlayer::ReloadedWeapon()
		{
			asIScriptObject *skin;
			// FIXME: what if current tool isn't weapon?
			if(ShouldRenderInThirdPersonView())
			{
				skin = weaponSkin;
			}
			else
			{
				skin = weaponViewSkin;
			}

			{
				ScriptIToolSkin interface(skin);
				if (client->GetWorld()->GetLocalPlayer())
					interface.SetClientDistance((player->GetPosition() - client->GetWorld()->GetLocalPlayer()->GetPosition()).GetLength());
				else
					interface.SetClientDistance((player->GetPosition() - client->GetLastSceneDef().viewOrigin).GetLength());
				interface.SetSoundDistance(client->soundDistance);
			}

			{
				ScriptIWeaponSkin interface(skin);
				interface.ReloadedWeapon();
			}
		}
	}
}
