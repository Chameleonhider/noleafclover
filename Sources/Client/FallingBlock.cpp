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
//Chameleon: I added this for configuration
#include <Core/Settings.h>

#include "FallingBlock.h"
#include "IRenderer.h"
#include "Client.h"
#include "IModel.h"
#include <limits.h>
#include "../Core/Debug.h"
#include "../Core/Exception.h"
#include "World.h"
#include "GameMap.h"
//#include "SmokeSpriteEntity.h"
#include "ParticleSpriteEntity.h"

//Chameleon: I added these for configuration
SPADES_SETTING(cg_reduceSmoke, "0");
//Maximum distance of particles
SPADES_SETTING(opt_particleMaxDist, "");
//Maximum distance for detailed particles
SPADES_SETTING(opt_particleNiceDist, "");
//Scales the amount of particles produced
SPADES_SETTING(opt_particleNumScale, "");
//Scales the amount of FallingBlock particles
SPADES_SETTING(opt_particleFallBlockReduce, "2");

namespace spades {
	namespace client {
		FallingBlock::FallingBlock(Client *client,
								   std::vector<IntVector3> blocks):
		client(client){
			if(blocks.empty())
				SPRaise("No block given");
			
			// find min/max
			int maxX = -1, maxY = -1, maxZ = -1;
			int minX = INT_MAX, minY=INT_MAX, minZ = INT_MAX;
			uint64_t xSum = 0, ySum = 0, zSum = 0;
			numBlocks = (int)blocks.size();
			for(size_t i = 0; i < blocks.size(); i++){
				IntVector3 v = blocks[i];
				if(v.x < minX) minX = v.x;
				if(v.y < minY) minY = v.y;
				if(v.z < minZ) minZ = v.z;
				if(v.x > maxX) maxX = v.x;
				if(v.y > maxY) maxY = v.y;
				if(v.z > maxZ) maxZ = v.z;
				xSum += v.x;
				ySum += v.y;
				zSum += v.z;
			}
			
			GameMap *map = client->GetWorld()->GetMap();
			
			// build voxel model
			vmodel = new VoxelModel(maxX - minX + 1, maxY - minY + 1,
									maxZ - minZ + 1);
			for(size_t i = 0; i < blocks.size(); i++){
				IntVector3 v = blocks[i];
				uint32_t col = map->GetColor(v.x, v.y, v.z);
				vmodel->SetSolid(v.x - minX, v.y - minY, v.z - minZ,
								 col);
			}
			
			// center of gravity
			Vector3 origin;
			origin.x = (float)minX - (float)xSum / (float)blocks.size();
			origin.y = (float)minY - (float)ySum / (float)blocks.size();
			origin.z = (float)minZ - (float)zSum / (float)blocks.size();
			vmodel->SetOrigin(origin);
			
			Vector3 matTrans = MakeVector3((float)minX, (float)minY,
										   (float)minZ);
			matTrans += .5f; // voxelmodel's (0,0,0) origins on block center
			matTrans -= origin; // cancel origin
			matrix = Matrix4::Translate(matTrans);
			
			// build renderer model
			model = client->GetRenderer()->CreateModel(vmodel);
			
			time = 0.f;
		}
		
		FallingBlock::~FallingBlock(){
			model->Release();
			vmodel->Release();
		}
		
		bool FallingBlock::Update(float dt) {
			time += dt;
			
			GameMap *map = client->GetWorld()->GetMap();
			Vector3 orig = matrix.GetOrigin();
			
			if(time > 2.f ||  map->ClipBox(orig.x, orig.y, orig.z))
			{
				

				// destroy
				int w = vmodel->GetWidth();
				int h = vmodel->GetHeight();
				int d = vmodel->GetDepth();
				
				Matrix4 vmat = lastMatrix;
				vmat = vmat * Matrix4::Translate(vmodel->GetOrigin());
				
				// block center
				
				Vector3 vmOrigin = vmat.GetOrigin();
				Vector3 vmAxis1 = vmat.GetAxis(0);
				Vector3 vmAxis2 = vmat.GetAxis(1);
				Vector3 vmAxis3 = vmat.GetAxis(2);
						
				//basic 1x1 pixel (>64 blocks)
				Handle<IImage> img = client->GetRenderer()->RegisterImage("Gfx/WhitePixel.tga");
				//8x8 round blob (<64 blocks)
				Handle<IImage> img2 = client->GetRenderer()->RegisterImage("Gfx/WhiteDisk.tga");
				//16x16 round blob (5<x<96 blocks) NOT USING IT
				Handle<IImage> img3 = client->GetRenderer()->RegisterImage("Gfx/WhiteSmoke.tga");

				bool usePrecisePhysics = false;

				float distance =(client->GetLastSceneDef().viewOrigin - matrix.GetOrigin()).GetLength();
				int distLimit = int(opt_particleMaxDist);
				if (distance < 16.f*float(opt_particleNiceDist) && numBlocks < 250 * float(opt_particleNumScale))
				{
					usePrecisePhysics = true;
				}
				
				float impact;
				if (distance > distLimit)
					return false;
				
				impact = (float)numBlocks / 100.f;
				
				client->grenadeVibration += impact / (distance + 5.f);
				if(client->grenadeVibration > 0.25f)
					client->grenadeVibration = 0.25f;

				bool bSkip = false;

				for (float x = 0; x < w; x++, bSkip = !bSkip)
				{
					Vector3 p1 = vmOrigin + vmAxis1 * (float)x;
					for (float y = 0; y < h; y++, bSkip = !bSkip)
					{
						Vector3 p2 = p1 + vmAxis2 * (float)y;
						for (float z = 0; z < d; z++, bSkip = !bSkip)
						{
							if (int(opt_particleFallBlockReduce) == 1 && bSkip) //allways
								continue;
							else if (int(opt_particleFallBlockReduce) == 2 && bSkip && numBlocks > 250) //above 250 blocks
								continue;

							if (!vmodel->IsSolid((int)x, (int)y, (int)z))
								continue;
							// inner voxel?
							if(x > 0 && y > 0 && z > 0 &&
							   x < w-1 && y < h-1 && z < d-1 &&
							   vmodel->IsSolid(x-1, y, z) &&
							   vmodel->IsSolid(x+1, y, z) &&
							   vmodel->IsSolid(x, y-1, z) &&
							   vmodel->IsSolid(x, y+1, z) &&
							   vmodel->IsSolid(x, y, z-1) &&
							   vmodel->IsSolid(x, y, z+1))
								continue;
							uint32_t c = vmodel->GetColor(x, y, z);
							//transparency for colour
							float transparency = (distLimit - distance) / (distLimit*1.0f);
							Vector4 col;
							col.x = (float)((uint8_t)(c)) / 255.f;
							col.y = (float)((uint8_t)(c>>8)) / 255.f;
							col.z = (float)((uint8_t)(c>>16)) / 255.f;
							col.w = transparency+0.5f;
							
							Vector3 p3 = p2 + vmAxis3 * (float)z;
							
							//particles copied from Client_LocalEnts.cpp - EmitBlockDestroyFragments()
							//particle (gravity)
							if (float(opt_particleNumScale) > 0.24f)
							{
								ParticleSpriteEntity *ent =
									new ParticleSpriteEntity(client, img2, col);

								ent->SetTrajectory(p3,
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
								client->AddLocalEntity(ent);
							}
							//stationary "smoke" after getting destroyed
							if (float(opt_particleNumScale) > 0.49f && distance < distLimit * 0.75f * float(opt_particleNiceDist) && !cg_reduceSmoke)
							{
								ParticleSpriteEntity *ent =
									new ParticleSpriteEntity(client, img3, col);

								ent->SetTrajectory(p3, MakeVector3(0, 0, 0), 1.f, 0.f);
								ent->SetRotation(GetRandom() * 6.48f);
								ent->SetRadius(0.5f, 0.25f);
								ent->SetLifeTime(2.f, 0.f, 2.f);
								ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
								client->AddLocalEntity(ent);
							}
							//OLD smoke+particle
							/*if (!cg_reduceSmoke)
							{
								ParticleSpriteEntity *ent =
									new ParticleSpriteEntity(client, img3, col);
								ent->SetTrajectory(p3,
												   (MakeVector3(GetRandom()-GetRandom(),
																GetRandom()-GetRandom(),
																GetRandom()-GetRandom())) * 0.2f,
												   1.f, 0.f);
								ent->SetRotation(GetRandom() * (float)M_PI * 2.f);
								ent->SetRadius(1.0f,
											   0.5f);
								ent->SetBlockHitAction(ParticleSpriteEntity::Ignore);
								ent->SetLifeTime(1.0f + GetRandom()*0.5f, 0.f, 1.0f);
								client->AddLocalEntity(ent);
							}
							
							col.w = 1.f;
							for(int i = 0; i < 6; i++){
								ParticleSpriteEntity *ent =
								new ParticleSpriteEntity(client, img, col);
								ent->SetTrajectory(p3,
												   MakeVector3(GetRandom()-GetRandom(),
															   GetRandom()-GetRandom(),
															   GetRandom()-GetRandom()) * 13.f,
												   1.f, .6f);
								ent->SetRotation(GetRandom() * (float)M_PI * 2.f);
								ent->SetRadius(0.35f + GetRandom()*GetRandom()*0.1f);
								ent->SetLifeTime(2.f, 0.f, 1.f);
								if(usePrecisePhysics)
									ent->SetBlockHitAction(ParticleSpriteEntity::BounceWeak);
								client->AddLocalEntity(ent);
							}*/
						}
					}
				}
				return false;
			}
			
			lastMatrix = matrix;
			
			Matrix4 rot;
			rot = Matrix4::Rotate(MakeVector3(1.f, 1.f, 0.f),
								  time * 1.4f * dt);
			matrix = matrix * rot;
			
			Matrix4 trans;
			trans = Matrix4::Translate(0, 0, time * dt * 4.f);
			matrix = trans * matrix;
			
			return true;
		}
		
		void FallingBlock::Render3D() {
			ModelRenderParam param;
			param.matrix = matrix;
			client->GetRenderer()->RenderModel(model, param);
			
		}
	}
}