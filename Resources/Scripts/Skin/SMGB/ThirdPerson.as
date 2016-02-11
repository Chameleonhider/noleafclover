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
	class ThirdPersonSMGSkinB: 
	IToolSkin, IThirdPersonToolSkin, IWeaponSkin 
	{
		private int snd_maxDistance = ConfigItem("snd_maxDistance", "150").IntValue;
	
		private float sprintState;
		private float raiseState;
		private IntVector3 teamColor;
		private bool muted;
		private Matrix4 originMatrix;
		private float aimDownSightState;
		private float readyState;
		private bool reloading;
		private float reloadProgress;
		private int ammo, clipSize;
		//Chameleon
		private float clientDistance;
		private float soundDistance;
		
		float SprintState 
		{ 
			set { sprintState = value; }
		}
		
		float RaiseState 
		{ 
			set { raiseState = value; }
		}
		
		IntVector3 TeamColor 
		{ 
			set { teamColor = value; } 
		}
		
		bool IsMuted 
		{
			set { muted = value; }
		}
		
		Matrix4 OriginMatrix 
		{
			set { originMatrix = value; }
		}
		
		float PitchBias 
		{
			get { return 0.f; }
		}
		
		float AimDownSightState 
		{
			set { aimDownSightState = value; }
		}
		
		bool IsReloading
		{
			set { reloading = value; }
		}
		float ReloadProgress
		{
			set { reloadProgress = value; }
		}
		int Ammo 
		{
			set { ammo = value; }
		}
		int ClipSize 
		{
			set { clipSize = value; }
		}
		
		float ReadyState
		{
			set { readyState = value; }
		}
		//Chameleon
		float ClientDistance
		{
			set { clientDistance = value; }
		}
		float SoundDistance
		{
			set { soundDistance = value; }
		}
		
		private Renderer@ renderer;
		private AudioDevice@ audioDevice;
		private Model@ model;
		
		private AudioChunk@ fire0;
		private AudioChunk@ fire1;
		private AudioChunk@ fire2;
		private AudioChunk@ reloadSound;
		
		ThirdPersonSMGSkinB(Renderer@ r, AudioDevice@ dev) {
			@renderer = r;
			@audioDevice = dev;
			@model = renderer.RegisterModel
				("Models/Weapons/SMGB/Weapon3rd.kv6");
				
			@fire0 = dev.RegisterSound
				("Sounds/Weapons/SMGB/Fire0.wav");
			@fire1 = dev.RegisterSound
				("Sounds/Weapons/SMGB/Fire1.wav");
			@fire2 = dev.RegisterSound
				("Sounds/Weapons/SMGB/Fire2.wav");
			@reloadSound = dev.RegisterSound
				("Sounds/Weapons/SMGB/Reload.wav");
				
		}
		
		void Update(float dt) 
		{
		
		}
		
		void WeaponFired()
		{
			if(!muted)
			{
				Vector3 origin = originMatrix.GetOrigin();
				AudioParam param;
				param.referenceDistance = clientDistance;
				if (clientDistance < snd_maxDistance/2.f)
				{
					param.volume = 5.f;
					audioDevice.Play(fire0, origin, param);
				}
				else if (clientDistance < snd_maxDistance)
				{
					param.volume = 1.f;
					audioDevice.Play(fire1, origin, param);
				}
				else if (clientDistance < snd_maxDistance*2.f)
				{
					param.volume = 0.5f;
					audioDevice.Play(fire2, origin, param);
				}
			}
		}
		void ReloadingWeapon() 
		{
			if(!muted)
			{
				Vector3 origin = originMatrix.GetOrigin();
				AudioParam param;
				param.volume = 1.f;
				param.referenceDistance = clientDistance;
				if (clientDistance*2 < soundDistance)
				{
					audioDevice.Play(reloadSound, origin, param);
				}
			}
		}
		
		void ReloadedWeapon() 
		{
		
		}
		
		void AddToScene() 
		{
			Matrix4 mat = CreateScaleMatrix(0.03125f);
			mat = mat * CreateScaleMatrix(-1.f, -1.f, 1.f);
			//0 21 4
			mat = CreateTranslateMatrix(0.35f, -0.4f, -0.125f) * mat;
			
			ModelRenderParam param;
			param.matrix = originMatrix * mat;
			renderer.AddModel(model, param);
		}
	}
	
	IWeaponSkin@ CreateThirdPersonSMGSkinB(Renderer@ r, AudioDevice@ dev) {
		return ThirdPersonSMGSkinB(r, dev);
	}
}
