//
//  Tracer.h
//  OpenSpades
//
//  Created by Tomoaki Kawada on 8/30/13.
//  Copyright (c) 2013 yvt.jp. All rights reserved.
//

#pragma once

#include "ILocalEntity.h"
#include "../Core/Math.h"

namespace spades {
	namespace client {
		class Client;
		class Player;
		class IImage;
		class IAudioChunk;
		class IModel;
		class Tracer: public ILocalEntity {
			Client *client;
			Player *player;
			IImage *image;
			//IImage *image2;
			IAudioChunk *snd;
			IModel *model;
			Vector3 startPos, dir;
			Matrix4 matrix;
			float length;
			float curDistance;
			float visibleLength;
			float velocity;
			bool firstUpdate;
			//bool soundPlayed;
			float flyByDist;
			Vector3 colour;
			IntVector3 teamCol;
			bool dlight;
		public:
			Tracer(Client *, Player *, IModel *, Vector3 p1, Vector3 p2,
				   float bulletVel, IntVector3 team);
			virtual ~Tracer();
			
			virtual bool Update(float dt);
			virtual void Render3D();
		};
	}
}