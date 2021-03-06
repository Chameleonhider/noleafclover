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
	class ThirdPersonBlockSkinB: 
	IToolSkin, IThirdPersonToolSkin, IBlockSkin {
		private float sprintState;
		private float raiseState;
		private IntVector3 teamColor;
		private Matrix4 originMatrix;
		private Vector3 blockColor;
		private float readyState;
		//Chameleon
		private bool isDragging;
		private float clientDistance;
		private float soundDistance;
		
		float SprintState { 
			set { sprintState = value; }
		}
		
		float RaiseState { 
			set { raiseState = value; }
		}
		
		bool IsMuted {
			set {
				// nothing to do; building sound is not related to skin.
			}
		}
		
		IntVector3 TeamColor { 
			set { teamColor = value; } 
		}
		
		Matrix4 OriginMatrix {
			set { originMatrix = value; }
		}
		
		float PitchBias {
			get { return 0.f; }
		}
		
		Vector3 BlockColor {
			set { blockColor = value; }
		}
		
		float ReadyState {
			set { readyState = value; }
		}
		//Chameleon
		bool IsDragging
		{
			set { isDragging = value; }
		}
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
		
		ThirdPersonBlockSkinB(Renderer@ r, AudioDevice@ dev) {
			@renderer = r;
			@audioDevice = dev;
			@model = renderer.RegisterModel
				("Models/Weapons/BlockB/Block3rd.kv6");
		}
		
		void Update(float dt) {
		}
		
		void AddToScene() {
			Matrix4 mat = CreateScaleMatrix(0.05f);
			
			mat = CreateTranslateMatrix(0.35f, -1.f, 0.0f) * mat;
			
			ModelRenderParam param;
			param.matrix = originMatrix * mat;
			param.customColor = blockColor;
			renderer.AddModel(model, param);
		}
	}
	
	IBlockSkin@ CreateThirdPersonBlockSkinB(Renderer@ r, AudioDevice@ dev) {
		return ThirdPersonBlockSkinB(r, dev);
	}
}
