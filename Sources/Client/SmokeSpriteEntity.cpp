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

#include "SmokeSpriteEntity.h"
#include "IRenderer.h"
#include "Client.h"
#include "IImage.h"
#include <stdio.h>

namespace spades{
	namespace client{
		static IRenderer *lastRenderer = NULL;
		static IImage *lastSeq;
		
        // FIXME: add "image manager"?
		static void Load(IRenderer *r) 
		{
			if(r == lastRenderer)
				return;
            
				r->RegisterImage("Gfx/WhitePixel.tga");			
			lastRenderer = r;
		}
		
		IImage *SmokeSpriteEntity::GetSequence(int i, IRenderer *r,
											   Type type){
			Load(r);
			return lastSeq;
		}
		
		SmokeSpriteEntity::SmokeSpriteEntity(Client *c,
											 Vector4 color,
											 float fps,
											 Type type):
		ParticleSpriteEntity(c, GetSequence(0, c->GetRenderer(), type), color), fps(fps),
		type(type){
			frame = 0.f;
		}
		
		void SmokeSpriteEntity::Preload(IRenderer *r) {
			Load(r);
		}
		
		bool SmokeSpriteEntity::Update(float dt)
		{
			SetImage(GetSequence(0, GetRenderer(), type));
			
			return ParticleSpriteEntity::Update(dt);
		}
	}
}
