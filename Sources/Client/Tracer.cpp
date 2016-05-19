//
//  Tracer.cpp
//  OpenSpades
//
//  Created by Tomoaki Kawada on 8/30/13.
//  Copyright (c) 2013 yvt.jp. All rights reserved.
//

#include "Tracer.h"
#include "Client.h"
#include "IRenderer.h"
#include "IAudioDevice.h"
#include "IAudioChunk.h"

#include <Core/Settings.h>

SPADES_SETTING(r_renderer, "");
SPADES_SETTING(opt_tracers, "");

namespace spades {
	namespace client {
		Tracer::Tracer(Client *client,
					   Player *player,
					   IModel *model,
					   Vector3 p1,
					   Vector3 p2,
					   float bulletVel,
					   IntVector3 team) :
		client(client), player(player), model(model), startPos(p1), velocity(bulletVel), teamCol(team)
		{
			dir = (p2 - p1).Normalize();
			length = (p2 - p1).GetLength()-2;

			startPos += dir*8;

			const float maxTimeSpread = 1.f / 60.f;
			const float shutterTime = 0.3f / 60.f;
			
			visibleLength = shutterTime * bulletVel;
			curDistance = -visibleLength + 8;
			curDistance += maxTimeSpread; //* GetRandom();
			
			firstUpdate = true;
			
			image = client->GetRenderer()->RegisterImage("Gfx/WhitePixel.tga");

			//Chameleon: .wav sounds for tracers
			int x = GetRandom()*4;
			switch (x % 4)
			{
				case 0:
					snd = bulletVel > 400 ?
						client->GetAudioDevice()->RegisterSound("Sounds/Weapons/Objects/TracerSUP1.wav") :
						client->GetAudioDevice()->RegisterSound("Sounds/Weapons/Objects/TracerSUB1.wav");
					break;
				case 1:
					snd = bulletVel > 400 ?
						client->GetAudioDevice()->RegisterSound("Sounds/Weapons/Objects/TracerSUP2.wav") :
						client->GetAudioDevice()->RegisterSound("Sounds/Weapons/Objects/TracerSUB2.wav");
					break;
				case 2:
					snd = bulletVel > 400 ?
						client->GetAudioDevice()->RegisterSound("Sounds/Weapons/Objects/TracerSUP3.wav") :
						client->GetAudioDevice()->RegisterSound("Sounds/Weapons/Objects/TracerSUB3.wav");
					break;
				case 3:
					snd = bulletVel > 400 ?
						client->GetAudioDevice()->RegisterSound("Sounds/Weapons/Objects/TracerSUP4.wav") :
						client->GetAudioDevice()->RegisterSound("Sounds/Weapons/Objects/TracerSUB4.wav");
					break;
				default:
					break;
			}

			if (player && (p1 - player->GetPosition()).GetLength() < 8)
				flyByDist = -1;
			else
				flyByDist = 0;
			//soundPlayed = false;

			//Chameleon: .kv6 tracers
			Vector3 up = MakeVector3(0, 0, 1);
			Vector3 right = Vector3::Cross(dir, up).Normalize();
			up = Vector3::Cross(right, dir);

			matrix = Matrix4::FromAxis(right, dir, up, startPos);

			//Chameleon: teamCol tracers
			//int colFac = (int)opt_tracers;
			colour = Vector3(teamCol.x / 255.f, teamCol.y / 255.f, teamCol.z / 255.f);
			colour = Vector3(colour.x / colour.GetLength(), colour.y / colour.GetLength(), colour.z / colour.GetLength());
			colour = (colour + Vector3(1, 1, 1)) / 1.9f;
		}
		
		bool Tracer::Update(float dt) 
		{
			

			if (length < 16)
				return false;
			if(!firstUpdate)
			{
				curDistance += dt * velocity;
				if(curDistance > length) 
				{
					return false;
				}
			}
			firstUpdate = false;

			//Chameleon: .kv6 tracers
			if ((int)opt_tracers == 2)
			{
				matrix = Matrix4::Translate(dir * velocity * dt) * matrix;
			}

			//Chameleon: play sound if tracer after coming inbound starts going outbound.
			//or: when distance to tracer starts increasing after having decreased.
			if (player)
			{
				Vector3 pos1 = startPos + dir * curDistance;

				//if tracer is heading outbound
				if ((pos1 - player->GetPosition()).GetLength() > flyByDist && flyByDist > 0 && flyByDist * 4 < client->soundDistance)
				{
					IAudioDevice *audio = client->GetAudioDevice();
					AudioParam param = AudioParam();
					param.volume = 100.f / flyByDist;
					param.referenceDistance = flyByDist/2;
					audio->Play(snd, pos1, param);
					flyByDist = -1;
				}
				else if (flyByDist != -1)
				{
					flyByDist = (pos1 - player->GetEye()).GetLength();
				}
			}

			return true;
		}
		
		void Tracer::Render3D() 
		{
			float startDist = curDistance;
			float endDist = curDistance + visibleLength;
			startDist = std::max(startDist, 8.f);
			endDist = std::min(endDist, length);

			if (startDist >= endDist || length < 8)
			{
				return;
			}
			
			Vector3 pos1 = startPos + dir * startDist;
			Vector3 pos2 = startPos + dir * endDist;
			IRenderer *r = client->GetRenderer();
			
			Vector4 col = { colour.x, colour.y, colour.z, 0.f };
			/*
			if ((int)opt_tracers == 2)
			{
			if (teamId == 0)
			col = { 0.2f, 1.f, 0.2f, 0.f };
			else
			col = { 1.f, 0.2f, 0.2f, 0.f };
			}
			*/

			r->SetColorAlphaPremultiplied(col*0.25f);
			
			if (EqualsIgnoringCase(r_renderer, "sw") && (int)opt_tracers == 1)
			{
				r->AddSprite(image, pos1, .025f, 0);
				r->AddSprite(image, (pos1+pos1+pos2)/3.f, .05f, 0);
				r->AddSprite(image, (pos1+pos2)/2.f, .05f, 0);
				r->AddSprite(image, (pos1+pos2+pos2)/3.f, .075f, 0);
				r->AddSprite(image, pos2, .1f, 0);
			}
			else if ((int)opt_tracers == 1)
			{
				r->AddLongSprite(image, pos1, pos2, .05f);
			}
			else if ((int)opt_tracers == 2)
			{
				ModelRenderParam param;
				param.matrix = matrix * Matrix4::Scale(0.1f);
				param.customColor = colour;
				r->RenderModel(model, param);
			}
		}
		
		Tracer::~Tracer() 
		{
			
		}
	}
}
