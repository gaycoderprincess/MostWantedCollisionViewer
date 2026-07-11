namespace CollView {
	const int COLLVIEW_MAX_TRIANGLES = 16383;

	bool bWireframeGround = false;
	bool bWireframeBarriers = false;

	int colR = 0;
	int colG = 0;
	int colB = 255;

	struct CollisionGeometryBuffer {
		float *position;
		float *normal;
		float *color;
		float *uv;
		uint16_t numTrianglesUsed;
	};

	NyaVec3 WorldToRender(NyaVec3 in) {
		auto out = in;
		out.y *= -1;
		return out;
	}

	template<int bufId>
	void RenderMario(CollisionGeometryBuffer marioBuffers) {
		int numFaces = COLLVIEW_MAX_TRIANGLES;
		int numVertices = 3 * numFaces;
		
		size_t vertexTotalSize = numVertices * sizeof(Render3D::CwoeeVertexData);
		size_t indexTotalSize = numFaces * 3 * 4;

		static IDirect3DVertexBuffer9* vertexBuffer = nullptr;
		static IDirect3DIndexBuffer9* indexBuffer = nullptr;

		static auto hr1 = g_pd3dDevice->CreateVertexBuffer(vertexTotalSize, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1, D3DPOOL_DEFAULT, &vertexBuffer, nullptr);
		if (hr1 != D3D_OK) {
			return;
		}
		static auto hr2 = g_pd3dDevice->CreateIndexBuffer(indexTotalSize, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &indexBuffer, nullptr);
		if (hr2 != D3D_OK) {
			return;
		}

		Render3D::CwoeeVertexData* verticesOut = nullptr;
		int* indicesOut = nullptr;
		auto hr = vertexBuffer->Lock(0, vertexTotalSize, (void**)&verticesOut, D3DLOCK_DISCARD);
		if (hr != D3D_OK) {
			return;
		}
		hr = indexBuffer->Lock(0, indexTotalSize, (void**)&indicesOut, D3DLOCK_DISCARD);
		if (hr != D3D_OK) {
			return;
		}

		DWORD fillMode;
		g_pd3dDevice->GetRenderState(D3DRS_FILLMODE, &fillMode);

		auto sunDir = NyaVec3(0.75, 0.0, 0.75);
		sunDir.Normalize();

		int numFacesUsed = marioBuffers.numTrianglesUsed;
		int numVerticesUsed = marioBuffers.numTrianglesUsed*3;
		for (int i = 0; i < numVerticesUsed; i++) {
			auto src = &marioBuffers.position[i*3];
			auto srcNormal = &marioBuffers.normal[i*3];
			auto srcColor = &marioBuffers.color[i*3];
			auto srcUV = &marioBuffers.uv[i*2];
			auto dest = &verticesOut[i];

			auto tmpPos = WorldToRender({src[0], src[1], src[2]});
			dest->vPos[0] = tmpPos[0];
			dest->vPos[1] = tmpPos[1];
			dest->vPos[2] = tmpPos[2];

			auto tmpNormal = WorldToRender({srcNormal[0], srcNormal[1], srcNormal[2]});

			dest->vNormals[0] = tmpNormal[0];
			dest->vNormals[1] = tmpNormal[1];
			dest->vNormals[2] = tmpNormal[2];

			dest->vTangents[0] = tmpNormal[0];
			dest->vTangents[1] = tmpNormal[1];
			dest->vTangents[2] = tmpNormal[2];

			if (fillMode == D3DFILL_SOLID && std::abs(tmpNormal.y) < 0.001) {
				float shading = std::clamp(tmpNormal.Dot(sunDir), 0.66, 1.0);

				auto tmp = NyaDrawing::CNyaRGBA32();
				tmp.b = srcColor[0] * 255 * shading;
				tmp.g = srcColor[1] * 255 * shading;
				tmp.r = srcColor[2] * 255 * shading;
				tmp.a = 255;
				dest->Color = *(uint32_t*)&tmp;
			}
			else {
				auto tmp = NyaDrawing::CNyaRGBA32();
				tmp.b = srcColor[0] * 255;
				tmp.g = srcColor[1] * 255;
				tmp.r = srcColor[2] * 255;
				tmp.a = 255;
				dest->Color = *(uint32_t*)&tmp;
			}

			dest->vUV[0] = srcUV[0];
			dest->vUV[1] = srcUV[1];
		}
		for (int i = 0; i < numFacesUsed*3; i++) {
			indicesOut[i] = i;
		}

		vertexBuffer->Unlock();
		indexBuffer->Unlock();

		Render3D::tModel tmpModel = {};
		tmpModel.pVertexBuffer = vertexBuffer;
		tmpModel.pIndexBuffer = indexBuffer;
		tmpModel.nVertexCount = numVerticesUsed;
		tmpModel.nFaceCount = numFacesUsed;

		auto mat = NyaMat4x4();
		mat.SetIdentity();

		static auto tex = LoadTexture("collView.png");
		tmpModel.pTexture = tex;
		tmpModel.RenderAt(WorldToRenderMatrix(mat));
	}

	std::vector<WCollisionTri> aCollisionTris;
	std::vector<WCollisionTri> aCollisionBarriers;

	struct CapturedArticle {
		struct CapturedInstance {
			int groupId;
			std::vector<WCollisionTri> aTris;
			std::vector<WCollisionTri> aBarriers;
		};
		std::vector<CapturedInstance> aInstances;
	};
	CapturedArticle aCapturedArticles[2700];

	void CaptureCollisionArticle(WCollisionInstance* inst, int articleId) {
		if (!inst) return;

		auto article = inst->fCollisionArticle;
		if (!article) return;

		UMath::Matrix4 instMat;
		inst->MakeMatrix(&instMat, true);

		auto& capture = aCapturedArticles[articleId];
		capture.aInstances.push_back({});

		auto& out = capture.aInstances[capture.aInstances.size()-1];
		out.groupId = inst->fGroupNumber;
		if (!out.aTris.empty() || !out.aBarriers.empty()) return;

		auto articles_end_ptr = (uintptr_t)(&article[1]);

		auto stripSphere = (WCollisionStripSphere*)articles_end_ptr;
		auto strip = (WCollisionStrip*)(&stripSphere[article->fNumStrips]);
		for (int i = 0; i < article->fNumStrips; i++) {
			int numToIterate = strip->numTrisOrSurfaceId - 2;
			for (int j = 0; j < numToIterate; j++) {
				WCollisionTri tri;
				tri.fSurface.fSurface = 0;
				tri.fSurface.fFlags = 0;
				WCollisionStrip::MakeFace(strip, j, &stripSphere->fPos, &tri);
				tri.fSurfaceRef = *(Attrib::Collection**)(articles_end_ptr + (4 * tri.fSurface.fSurface) + article->fStripsSize + article->fEdgesSize);

				tri.fPt0 -= instMat.p;
				tri.fPt1 -= instMat.p;
				tri.fPt2 -= instMat.p;

				out.aTris.push_back(tri);
			}
			strip += strip->numTrisOrSurfaceId;
			stripSphere++;
		}

		auto list = (WCollisionBarrier*)(articles_end_ptr + article->fStripsSize);
		for (int i = 0; i < article->fNumEdges; i++) {
			auto ptMin = list[i].fPts[0];
			auto ptMax = list[i].fPts[1];
			ptMin -= instMat.p;
			ptMax -= instMat.p;

			// first tri
			WCollisionTri tri;
			tri.fPt2.x = ptMin.x;
			tri.fPt2.y = ptMin.y;
			tri.fPt2.z = ptMin.z;
			tri.fPt1.x = ptMin.x;
			tri.fPt1.y = ptMax.y;
			tri.fPt1.z = ptMin.z;
			tri.fPt0.x = ptMax.x;
			tri.fPt0.y = ptMax.y;
			tri.fPt0.z = ptMax.z;
			out.aBarriers.push_back(tri);

			// second tri
			tri.fPt0.x = ptMin.x;
			tri.fPt0.y = ptMin.y;
			tri.fPt0.z = ptMin.z;
			tri.fPt1.x = ptMax.x;
			tri.fPt1.y = ptMin.y;
			tri.fPt1.z = ptMax.z;
			tri.fPt2.x = ptMax.x;
			tri.fPt2.y = ptMax.y;
			tri.fPt2.z = ptMax.z;
			out.aBarriers.push_back(tri);
		}
	}

	void ProcessCollisionBarriers(WCollisionBarrier* list, int count, NyaVec3 offset) {
		for (int i = 0; i < count; i++) {
			auto ptMin = list[i].fPts[0];
			auto ptMax = list[i].fPts[1];
			ptMin -= offset;
			ptMax -= offset;

			// first tri
			WCollisionTri tri;
			tri.fPt2.x = ptMin.x;
			tri.fPt2.y = ptMin.y;
			tri.fPt2.z = ptMin.z;
			tri.fPt1.x = ptMin.x;
			tri.fPt1.y = ptMax.y;
			tri.fPt1.z = ptMin.z;
			tri.fPt0.x = ptMax.x;
			tri.fPt0.y = ptMax.y;
			tri.fPt0.z = ptMax.z;
			aCollisionBarriers.push_back(tri);

			// second tri
			tri.fPt0.x = ptMin.x;
			tri.fPt0.y = ptMin.y;
			tri.fPt0.z = ptMin.z;
			tri.fPt1.x = ptMax.x;
			tri.fPt1.y = ptMin.y;
			tri.fPt1.z = ptMax.z;
			tri.fPt2.x = ptMax.x;
			tri.fPt2.y = ptMax.y;
			tri.fPt2.z = ptMax.z;
			aCollisionBarriers.push_back(tri);
		}
	}

	void ProcessCollisionArticle(WCollisionInstance* inst) {
		if (!inst) return;

		auto article = inst->fCollisionArticle;
		if (!article) return;

		// filter out unused stuff
		if (inst->fGroupNumber && !SceneryGroupEnabledTable[inst->fGroupNumber]) return;

		UMath::Matrix4 instMat;
		inst->MakeMatrix(&instMat, true);

		auto articles_end_ptr = (uintptr_t)(&article[1]);

		auto stripSphere = (WCollisionStripSphere*)articles_end_ptr;
		auto strip = (WCollisionStrip*)(&stripSphere[article->fNumStrips]);
		for (int i = 0; i < article->fNumStrips; i++) {
			int numToIterate = strip->numTrisOrSurfaceId - 2;
			for (int j = 0; j < numToIterate; j++) {
				WCollisionTri tri;
				WCollisionStrip::MakeFace(strip, j, &stripSphere->fPos, &tri);
				tri.fSurfaceRef = *(Attrib::Collection**)(articles_end_ptr + (4 * tri.fSurface.fSurface) + article->fStripsSize + article->fEdgesSize);

				tri.fPt0 -= instMat.p;
				tri.fPt1 -= instMat.p;
				tri.fPt2 -= instMat.p;

				aCollisionTris.push_back(tri);
			}
			strip += strip->numTrisOrSurfaceId;
			stripSphere++;
		}

		ProcessCollisionBarriers((WCollisionBarrier*)(articles_end_ptr + article->fStripsSize), article->fNumEdges, instMat.p);
	}

	template<int debugRenderId>
	void AddMarioStaticObject(std::vector<WCollisionTri>* collisions, bool doubleSided) {
		static CollisionGeometryBuffer colBuffers = {};
		static CollisionGeometryBuffer colBuffersFlip = {};
		if (!colBuffers.position) {
			colBuffers.position = new float[9 * COLLVIEW_MAX_TRIANGLES];
			colBuffers.normal = colBuffersFlip.normal = new float[9 * COLLVIEW_MAX_TRIANGLES];
			colBuffers.color = colBuffersFlip.color = new float[9 * COLLVIEW_MAX_TRIANGLES];
			colBuffers.uv = colBuffersFlip.uv = new float[6 * COLLVIEW_MAX_TRIANGLES];
			colBuffersFlip.position = new float[9 * COLLVIEW_MAX_TRIANGLES];
		}
		colBuffers.numTrianglesUsed = colBuffersFlip.numTrianglesUsed = std::min((int)collisions->size(), (int)COLLVIEW_MAX_TRIANGLES);

		auto colDrawPosition = &colBuffers.position[0];
		auto colDrawNormal = &colBuffers.normal[0];
		auto colDrawColor = &colBuffers.color[0];
		auto colDrawUV = &colBuffers.uv[0];

		auto colDrawFlipPosition = &colBuffersFlip.position[0];

		for (int i = 0; i < collisions->size(); i++) {
			auto in = &(*collisions)[i];

			auto pt0 = NyaVec3({in->fPt0[0],in->fPt0[1],in->fPt0[2]});
			auto pt1 = NyaVec3({in->fPt1[0],in->fPt1[1],in->fPt1[2]});
			auto pt2 = NyaVec3({in->fPt2[0],in->fPt2[1],in->fPt2[2]});

			if (i < colBuffers.numTrianglesUsed) {
				colDrawPosition[0] = pt0[0];
				colDrawPosition[1] = pt0[1];
				colDrawPosition[2] = pt0[2];
				colDrawPosition[3] = pt1[0];
				colDrawPosition[4] = pt1[1];
				colDrawPosition[5] = pt1[2];
				colDrawPosition[6] = pt2[0];
				colDrawPosition[7] = pt2[1];
				colDrawPosition[8] = pt2[2];
				colDrawPosition += 9;

				colDrawFlipPosition[0] = pt2[0];
				colDrawFlipPosition[1] = pt2[1];
				colDrawFlipPosition[2] = pt2[2];
				colDrawFlipPosition[3] = pt1[0];
				colDrawFlipPosition[4] = pt1[1];
				colDrawFlipPosition[5] = pt1[2];
				colDrawFlipPosition[6] = pt0[0];
				colDrawFlipPosition[7] = pt0[1];
				colDrawFlipPosition[8] = pt0[2];
				colDrawFlipPosition += 9;

				// normal
				auto faceNormal = (pt1 - pt0).Cross(pt2 - pt0);
				faceNormal.Normalize();

				colDrawNormal[0] = faceNormal[0];
				colDrawNormal[1] = faceNormal[1];
				colDrawNormal[2] = faceNormal[2];
				colDrawNormal += 3;
				colDrawNormal[0] = faceNormal[0];
				colDrawNormal[1] = faceNormal[1];
				colDrawNormal[2] = faceNormal[2];
				colDrawNormal += 3;
				colDrawNormal[0] = faceNormal[0];
				colDrawNormal[1] = faceNormal[1];
				colDrawNormal[2] = faceNormal[2];
				colDrawNormal += 3;

				if (in->fSurfaceRef) {
					auto color = (float*)in->fSurfaceRef->GetData(0x740D3125, 0);

					colDrawColor[0] = color[0];
					colDrawColor[1] = color[1];
					colDrawColor[2] = color[2];
					colDrawColor += 3;
					colDrawColor[0] = color[0];
					colDrawColor[1] = color[1];
					colDrawColor[2] = color[2];
					colDrawColor += 3;
					colDrawColor[0] = color[0];
					colDrawColor[1] = color[1];
					colDrawColor[2] = color[2];
					colDrawColor += 3;
				}
				else {
					colDrawColor[0] = colR / 255.0;
					colDrawColor[1] = colG / 255.0;
					colDrawColor[2] = colB / 255.0;
					colDrawColor += 3;
					colDrawColor[0] = colR / 255.0;
					colDrawColor[1] = colG / 255.0;
					colDrawColor[2] = colB / 255.0;
					colDrawColor += 3;
					colDrawColor[0] = colR / 255.0;
					colDrawColor[1] = colG / 255.0;
					colDrawColor[2] = colB / 255.0;
					colDrawColor += 3;
				}

				colDrawUV[0] = 0;
				colDrawUV[1] = 0;
				colDrawUV += 2;
				colDrawUV[0] = 0;
				colDrawUV[1] = 0;
				colDrawUV += 2;
				colDrawUV[0] = 0;
				colDrawUV[1] = 0;
				colDrawUV += 2;
			}
		}

		RenderMario<debugRenderId>(colBuffers);
		if (doubleSided) {
			RenderMario<debugRenderId+100>(colBuffersFlip);
		}
	}

	void UpdateMarioCollision() {
		g_pd3dDevice->SetRenderState(D3DRS_FILLMODE, bWireframeGround ? D3DFILL_WIREFRAME : D3DFILL_SOLID);

		AddMarioStaticObject<1>(&aCollisionTris, true);

		g_pd3dDevice->SetRenderState(D3DRS_FILLMODE, bWireframeBarriers ? D3DFILL_WIREFRAME : D3DFILL_SOLID);

		AddMarioStaticObject<2>(&aCollisionBarriers, true);
	}

	bool bEnabled = true;
	bool bExportEnabled = false;

	void DoCollisionCapture() {
		for (int i = 0; i < 2700; i++) {
			auto pack = WCollisionAssets::mCollisionPackList[i];
			if (!pack) continue;

			bool doExport = aCapturedArticles[i].aInstances.empty();
			for (int j = 0; j < pack->mInstanceNum; j++) {
				if (doExport) {
					CaptureCollisionArticle(&pack->mInstanceList[j], i);
				}
			}
		}
	}

	void OnTick() {
		if (!bEnabled) return;
		
		if (auto ply = GetLocalPlayerInterface<IRigidBody>()) {			
			aCollisionTris.clear();
			aCollisionBarriers.clear();

			if (bExportEnabled) {
				DoCollisionCapture();
			}

			for (int i = 0; i < 2700; i++) {
				auto pack = WCollisionAssets::mCollisionPackList[i];
				if (!pack) continue;

				for (int j = 0; j < pack->mInstanceNum; j++) {
					ProcessCollisionArticle(&pack->mInstanceList[j]);
				}
			}

			UpdateMarioCollision();
		}
	}

	ChloeHook Init([](){
		aDrawing3DLoopFunctions.push_back(OnTick);
	});
}