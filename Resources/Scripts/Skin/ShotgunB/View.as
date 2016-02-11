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
 
 namespace spades {
	class ViewShotgunSkinB: 
	IToolSkin, IViewToolSkin, IWeaponSkin,
	BasicViewWeapon
	{
		private int snd_maxDistance = ConfigItem("snd_maxDistance", "150").IntValue;
		
		private AudioDevice@ audioDevice;
		private Model@ gunModel;
		private Model@ gunModel2;
		private Model@ pumpModel;
		private Model@ sightModelF;
		private AudioChunk@ fireSound;
		private AudioChunk@ fireFarSound;
		private AudioChunk@ fireStereoSound;
		private AudioChunk@ reloadSound;
		private AudioChunk@ pumpSound;
		
		ViewShotgunSkinB(Renderer@ r, AudioDevice@ dev){
			super(r);
			@audioDevice = dev;
			@gunModel = renderer.RegisterModel
				("Models/Weapons/ShotgunA/Weapon1stMain.kv6");	
			@gunModel2 = renderer.RegisterModel
				("Models/Weapons/ShotgunA/Weapon1stSecond.kv6");
			@pumpModel = renderer.RegisterModel
				("Models/Weapons/ShotgunA/Weapon1stPump.kv6");
			@sightModelF = renderer.RegisterModel
				("Models/Weapons/ShotgunA/SightFront.kv6");
			@fireSound = dev.RegisterSound
				("Sounds/Weapons/ShotgunA/Fire0.wav");
			@reloadSound = dev.RegisterSound
				("Sounds/Weapons/ShotgunA/Reload.wav");
			@pumpSound = dev.RegisterSound
				("Sounds/Weapons/ShotgunA/Pump.wav");
		}
		
		void Update(float dt)
		{
			BasicViewWeapon::Update(dt);
		}
		
		void WeaponFired()
		{
			BasicViewWeapon::WeaponFired();
			
			if(!IsMuted)
			{
				Vector3 origin = Vector3(0.4f, -0.3f, 0.5f);
				AudioParam param;
				param.volume = 5.f;
				audioDevice.PlayLocal(fireSound, origin, param);
			}
		}
		
		void ReloadingWeapon()
		{
			if(!IsMuted)
			{
				Vector3 origin = Vector3(0.4f, -0.3f, 0.5f);
				AudioParam param;
				param.volume = 1.f;
				audioDevice.PlayLocal(reloadSound, origin, param);
			}
		}
		
		void ReloadedWeapon()
		{
			if(!IsMuted)
			{
				Vector3 origin = Vector3(0.4f, -0.3f, 0.5f);
				AudioParam param;
				param.volume = 1.f;
				audioDevice.PlayLocal(pumpSound, origin, param);
			}
		}
		float GetZPos() 
		{
			float debug_d = ConfigItem("debug_d", "0").FloatValue;
			return 0.2f - AimDownSightStateSmooth * 0.175f; // * debug_d; //0.05*3.5=0.175
			//return 0.2f - AimDownSightStateSmooth * 0.0535f;
		}
		
		// rotates gun matrix to ensure the sight is in
		// the center of screen (0, ?, 0).
		Matrix4 AdjustToAlignSight(Matrix4 mat, Vector3 sightPos, float fade) 
		{
			Vector3 p = mat * sightPos;
			mat = CreateRotateMatrix(Vector3(0.f, 0.f, 1.f), atan(p.x / p.y) * fade) * mat;
			mat = CreateRotateMatrix(Vector3(-1.f, 0.f, 0.f), atan(p.z / p.y) * fade) * mat;
			return mat;
		}
		
		void Draw2D()
		{
			// if(AimDownSightState > 0.6)
				// return;
			//BasicViewWeapon::Draw2D();
		}
		
		void AddToScene()
		{
			BasicViewWeapon::DrawXH();
			float move = 0;
			Matrix4 mat = CreateScaleMatrix(0.015625f);
			mat = GetViewWeaponMatrix() * mat;
			
			bool reloading = IsReloading;
			float reload = ReloadProgress;
			Vector3 leftHand, rightHand;
			
			//leftHand = mat * Vector3(0.f, 4.f, 2.f);
			//rightHand = mat * Vector3(0.f, -8.f, 2.f);
			leftHand = Vector3(0.f, 0.f, 0.f);
			rightHand = Vector3(0.f, 0.f, 0.f);
			
			Vector3 leftHand2 = mat * Vector3(5.f, -10.f, 4.f);
			Vector3 leftHand3 = mat * Vector3(1.f, 1.f, 2.f);
			
			if(AimDownSightStateSmooth > 0.f)
			{
				//mat = AdjustToAlignSight(mat, Vector3(0.f, 8.5f, -4.4f), AimDownSightStateSmooth);
				mat = AdjustToAlignSight(mat, Vector3(0.f, 16.f, -1.472f), (AimDownSightStateSmooth));
			}
			
			ModelRenderParam param;
			Matrix4 weapMatrix = eyeMatrix * mat;
			param.matrix = weapMatrix;
			renderer.AddModel(gunModel, param);
			renderer.AddModel(gunModel2, param);
			param.matrix *=	CreateTranslateMatrix(0.f, move, 0.f);
			renderer.AddModel(pumpModel, param);
			
			param.matrix = weapMatrix;
			param.matrix *= CreateTranslateMatrix(0.f, 95.75f, 0.f);
			param.matrix *= CreateScaleMatrix(0.5f);
			renderer.AddModel(sightModelF, param); // front
			// reload action
			reload *= 0.5f;
			
			
			if(reloading) {
				if(reload < 0.2f) {
					float per = reload / 0.2f;
					leftHand = Mix(leftHand, leftHand2,
						SmoothStep(per));
				}else if(reload < 0.35f){
					float per = (reload - 0.2f) / 0.15f;
					leftHand = Mix(leftHand2, leftHand3,
						SmoothStep(per));
				}else if(reload < 0.5f){
					float per = (reload - 0.35f) / 0.15f;
					leftHand = Mix(leftHand3, leftHand,
						SmoothStep(per));
				}
			}
			
			// motion blending parameter
			float cockFade = 1.f;
			if(reloading){
				if(reload < 0.25f ||
					ammo < (clipSize - 1)) {
					cockFade = 0.f;	
				}else{
					cockFade = (reload - 0.25f) * 10.f;
					cockFade = Min(cockFade, 1.f);
				}
			}
			
			if(cockFade > 0.f){
				float cock = 0.f;
				float tim = 1.f - readyState;
				if(tim < 0.f){
					// might be right after reloading
					if(ammo >= clipSize && reload > 0.5f && reload < 1.f){
						tim = reload - 0.5f;
						if(tim < 0.05f){
							cock = 0.f;
						}else if(tim < 0.12f){
							cock = (tim - 0.05f) / 0.07f;
						}else if(tim < 0.26f){
							cock = 1.f;
						}else if(tim < 0.36f){
							cock = 1.f - (tim - 0.26f) / 0.1f;
						}
					}
				}else if(tim < 0.2f){
					cock = 0.f;
				}else if(tim < 0.3f){
					cock = (tim - 0.2f) / 0.1f;
				}else if(tim < 0.42f){
					cock = 1.f;
				}else if(tim < 0.52f){
					cock = 1.f - (tim - 0.42f) / 0.1f;
				}else{
					cock = 0.f;
				}
				
				cock *= cockFade;
				mat = mat * CreateTranslateMatrix(0.f, cock * -1.5f, 0.f);
				
				leftHand = Mix(leftHand,
					mat * Vector3(0.f, 4.f, 2.f), cockFade);
			}
			
			param.matrix = eyeMatrix * mat;
			//renderer.AddModel(pumpModel, param);
			
			leftHand = Vector3(0.f, 0.f, 0.f);
			rightHand = Vector3(0.f, 0.f, 0.f);
			
			LeftHandPosition = leftHand;
			RightHandPosition = rightHand;
		}
	}
	
	IWeaponSkin@ CreateViewShotgunSkinB(Renderer@ r, AudioDevice@ dev) {
		return ViewShotgunSkinB(r, dev);
	}
}
