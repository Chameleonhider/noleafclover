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
	class ViewBlockSkinA: 
	IToolSkin, IViewToolSkin, IBlockSkin {
		private float sprintState;
		private float raiseState;
		private IntVector3 teamColor;
		private Matrix4 eyeMatrix;
		private Vector3 swing;
		private Vector3 leftHand;
		private Vector3 rightHand;
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
		
		IntVector3 TeamColor { 
			set { teamColor = value; } 
		}
		
		bool IsMuted {
			set {
				// nothing to do
			}
		}
		
		Matrix4 EyeMatrix {
			set { eyeMatrix = value; }
		}
		
		Vector3 Swing {
			set { swing = value; }
		}	
		
		Vector3 LeftHandPosition {
			get {
				return leftHand;
			}
		}
		Vector3 RightHandPosition { 
			get  {
				return rightHand;
			}
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
		private Image@ sightImage1;
		private Image@ sightImage2;
		
		ViewBlockSkinA(Renderer@ r, AudioDevice@ dev) {
			@renderer = r;
			@audioDevice = dev;
			@model = renderer.RegisterModel
				("Models/Weapons/BlockA/Block1st.kv6");
			@sightImage1 = renderer.RegisterImage
				("Gfx/SightHorizontal.png");
			@sightImage2 = renderer.RegisterImage
				("Gfx/SightVertical.png");
			
		}
		
		void Update(float dt) {
		}
		
		void AddToScene() 
		{
			// draw xhair
			float reflexOpacity = 1.f;
			Vector3 reflexPos = eyeMatrix * Vector3(0.f, 0.2f, 0.f);
			renderer.Color = Vector4(1.f, 1.f, 1.f, readyState); // premultiplied alpha
			renderer.AddSprite(sightImage1, reflexPos, 0.05f, 0.f);
			if (isDragging)
				renderer.AddSprite(sightImage2, reflexPos, 0.05f, 0.f);
				
			if(readyState < .99f)
			{
				// not ready
				// leftHand = Vector3(0.5f, 1.f, 0.6f);
				// rightHand = Vector3(-0.5f, 1.f, 0.6f);
				
				leftHand = Vector3(0.f, 0.f, 0.f);
				rightHand = Vector3(0.f, 0.f, 0.f);
				return;
			}
			
			Matrix4 mat = CreateScaleMatrix(0.033f);
			
			if(sprintState > 0.f) {
				mat = CreateRotateMatrix(Vector3(0.f, 0.f, 1.f),
					sprintState * -0.3f) * mat;
				mat = CreateTranslateMatrix(Vector3(0.1f, -0.4f, -0.05f) * sprintState)
					* mat;
			}
			
			mat = CreateTranslateMatrix(-0.3f, 0.7f, 0.3f) * mat;
			mat = CreateTranslateMatrix(swing) * mat;
			
			mat = CreateTranslateMatrix(Vector3(-0.1f, -0.3f, 0.2f) * (1.f - raiseState))
				* mat;
			
			leftHand = mat * Vector3(5.f, -1.f, 4.f);
			rightHand = mat * Vector3(-5.5f, 3.f, -5.f);
			
			ModelRenderParam param;
			param.matrix = eyeMatrix * mat;
			param.customColor = blockColor;
			renderer.AddModel(model, param);
			
			leftHand = Vector3(0.f, 0.f, 0.f);
			rightHand = Vector3(0.f, 0.f, 0.f);
		}
		
		void Draw2D() 
		{
			// renderer.ColorNP = (Vector4(1.f, 1.f, 1.f, readyState));
			// renderer.DrawImage(sightImage1,
				// Vector2((renderer.ScreenWidth - sightImage1.Width) * 0.5f,
						// (renderer.ScreenHeight - sightImage1.Height) * 0.5f));
			// if (isDragging)
				// renderer.DrawImage(sightImage2,
					// Vector2((renderer.ScreenWidth - sightImage2.Width) * 0.5f,
							// (renderer.ScreenHeight - sightImage2.Height) * 0.5f));
		}
	}
	
	IBlockSkin@ CreateViewBlockSkinA(Renderer@ r, AudioDevice@ dev) {
		return ViewBlockSkinA(r, dev);
	}
}
