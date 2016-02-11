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

namespace spades
{
	class ViewRifleSkinA:
	IToolSkin, IViewToolSkin, IWeaponSkin,
	BasicViewWeapon
	{
		private int snd_maxDistance = ConfigItem("snd_maxDistance", "150").IntValue;
		
		private AudioDevice@ audioDevice;
		private Model@ gunModel;
		private Model@ moveModel;
		private Model@ magModel;
		private Model@ sightModelR;
		private Model@ sightModelF;
		
		private AudioChunk@ fireSound;
		private AudioChunk@ reloadSound;
		
		ViewRifleSkinA(Renderer@ r, AudioDevice@ dev)
		{
			super(r);
			@audioDevice = dev;
			@gunModel = renderer.RegisterModel
				("Models/Weapons/RifleA/Weapon1st.kv6");
			@moveModel = renderer.RegisterModel
				("Models/Weapons/RifleA/Weapon1stMove.kv6");
			@magModel = renderer.RegisterModel
				("Models/Weapons/RifleA/Magazine.kv6");
			@sightModelR = renderer.RegisterModel
				("Models/Weapons/RifleA/SightRear.kv6");
			@sightModelF = renderer.RegisterModel
				("Models/Weapons/RifleA/SightFront.kv6");
				
			@fireSound = dev.RegisterSound
				("Sounds/Weapons/RifleA/Fire0.wav");
			@reloadSound = dev.RegisterSound
				("Sounds/Weapons/RifleA/Reload.wav");
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
		
		float GetZPos()
		{
			return 0.2f - AimDownSightStateSmooth * 0.175f; // * debug_d; //0.05*3.5=0.175
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
			
			// leftHand = mat * Vector3(1.f, 6.f, 1.f);
			// rightHand = mat * Vector3(0.f, -8.f, 2.f);
			leftHand = Vector3(0.f, 0.f, 0.f);
			rightHand = Vector3(0.f, 0.f, 0.f);
			
			Vector3 leftHand2 = mat * Vector3(5.f, -10.f, 4.f);
			Vector3 rightHand3 = mat * Vector3(-2.f, -7.f, -4.f);
			Vector3 rightHand4 = mat * Vector3(-3.f, -4.f, -6.f);
			
			if(AimDownSightStateSmooth > 0.f)
			{
				//mat = AdjustToAlignSight(mat, Vector3(0.f, 16.f*debug_e, -4.6f*debug_f), (AimDownSightStateSmooth - 0.8f) / 0.2f);
				mat = AdjustToAlignSight(mat, Vector3(0.f, 16.f, -1.472f), (AimDownSightStateSmooth));
				//-4.6*0.25=-1.15
			}
			
			ModelRenderParam param;
			Matrix4 weapMatrix = eyeMatrix * mat;
			//weapMatrix *= CreateTranslateMatrix(debug_a, debug_b, debug_c);
			// draw weapon
			param.matrix = weapMatrix;
			renderer.AddModel(gunModel, param);
			param.matrix *=	CreateTranslateMatrix(0.f, move, 0.f);
			renderer.AddModel(moveModel, param);
			
			// draw sights
			param.matrix = weapMatrix;
			param.matrix *= CreateTranslateMatrix(0.f, 55.75f, 0.f);
			param.matrix *= CreateScaleMatrix(0.5f);
			renderer.AddModel(sightModelR, param); // rear
			
			param.matrix = weapMatrix;
			param.matrix *= CreateTranslateMatrix(0.f, 106.75f, 0.f);
			param.matrix *= CreateScaleMatrix(0.5f);
			renderer.AddModel(sightModelF, param); // front
			
			// magazine/reload action
			mat *= CreateTranslateMatrix(0.f, 1.f, 1.f);
			reload *= 2.5f;
			if(reloading) {
				if(reload < 0.1f){
					// move hand to magazine
					float per = reload / 0.1f;
					leftHand = Mix(leftHand,
						mat * Vector3(0.f, 0.f, 4.f),
						SmoothStep(per));
				}else if(reload < 0.7f){
					// magazine release
					float per = (reload - 0.1f) / 0.6f;
					if(per < 0.2f){
						// non-smooth pull out
						per *= 4.0f; per -= 0.4f;
						per = Clamp(per, 0.0f, 0.2f);
					}
					if(per > 0.5f) {
						// when per = 0.5f, the hand no longer holds the magazine,
						// so the free fall starts
						per += per - 0.5f;
					}
					mat *= CreateTranslateMatrix(0.f, 0.f, per*per*10.f);
					
					leftHand = mat * Vector3(0.f, 0.f, 4.f);
					if(per > 0.5f){
						per = (per - 0.5f);
						leftHand = Mix(leftHand, leftHand2, SmoothStep(per));
					}
				}else if(reload < 1.4f) {
					// insert magazine
					float per = (1.4f - reload) / 0.7f;
					if(per < 0.3f) {
						// non-smooth insertion
						per *= 4.f; per -= 0.4f;
						per = Clamp(per, 0.0f, 0.3f);
					}
					
					mat *= CreateTranslateMatrix(0.f, 0.f, per*per*10.f);
					leftHand = mat * Vector3(0.f, 0.f, 4.f);
				}else if(reload < 1.9f){
					// move the left hand to the original position
					// and start doing something with the right hand
					float per = (reload - 1.4f) / 0.5f;
					Vector3 orig = leftHand;
					leftHand = mat * Vector3(0.f, 0.f, 4.f);
					leftHand = Mix(leftHand, orig, SmoothStep(per));
					rightHand = Mix(rightHand, rightHand3, SmoothStep(per));
				}else if(reload < 2.2f){
					float per = (reload - 1.9f) / 0.3f;
					rightHand = Mix(rightHand3, rightHand4, SmoothStep(per));
				}else{
					float per = (reload - 2.2f) / 0.3f;
					rightHand = Mix(rightHand4, rightHand, SmoothStep(per));
				}
			}
			
			param.matrix = eyeMatrix * mat;
			//renderer.AddModel(magModel, param);
			
			leftHand = Vector3(0.f, 0.f, 0.f);
			rightHand = Vector3(0.f, 0.f, 0.f);
			
			LeftHandPosition = leftHand;
			RightHandPosition = rightHand;
		}
	}
	
	IWeaponSkin@ CreateViewRifleSkinA(Renderer@ r, AudioDevice@ dev) {
		return ViewRifleSkinA(r, dev);
	}
}
