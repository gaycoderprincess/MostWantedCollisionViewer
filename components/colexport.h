#include "assimp/Exporter.hpp"
#include "assimp/Logger.hpp"
#include "assimp/DefaultLogger.hpp"
#include "assimp/scene.h"

namespace CollExport {
	uint32_t GetTriSurfaceHash(WCollisionTri* tri) {
		if (tri->fSurfaceRef) {
			return tri->fSurfaceRef->mKey;
		}
		return 0;
	}

	std::vector<uint32_t> aSceneMaterials;
	int GetSceneMaterialId(uint32_t hash) {
		for (auto& mat : aSceneMaterials) {
			if (mat == hash) return &mat - &aSceneMaterials[0];
		}
		aSceneMaterials.push_back(hash);
		return aSceneMaterials[aSceneMaterials.size()-1];
	}

	std::string GetMaterialName(uint32_t hash) {
		const char* names[] = {
				"default",
				"energy",
				"explosion",
				"shield",
				"softbarrier",
				"liquid",
				"water",
				"null",
				"overrides",
				"blown_tire",
				"solid",
				"concrete",
				"asphalt",
				"asphalt_no_leaves",
				"acidslick",
				"fireslick",
				"oilslick",
				"roadfog",
				"wetpaved",
				"cobble",
				"rooftile",
				"stone",
				"glass",
				"ice",
				"metal",
				"carbody",
				"railroad",
				"rotors",
				"organic",
				"flesh",
				"wood",
				"particulate",
				"dirt",
				"mud",
				"grass",
				"golf_fairway",
				"golf_green",
				"golf_rough",
				"grass_shoulder",
				"gravel",
				"hay",
				"sand",
				"snow",
				"plastic",
				"bumper",
				"wheels",
				"unknown",
		};

		for (auto& name : names) {
			if (Attrib::StringHash32(name) == hash) return name;
		}
		return std::format("{:X}", hash);
	}

	void ExportTris(aiScene& scene, const std::string& name, int& meshId, std::vector<WCollisionTri>& tris) {
		std::vector<uint32_t> materials;
		for (auto& tri : tris) {
			bool add = true;
			for (auto& mat: materials) {
				if (GetTriSurfaceHash(&tri) == mat) {
					add = false;
				}
			}
			if (add) {
				materials.push_back(GetTriSurfaceHash(&tri));
			}
		}

		for (auto& mat : materials) {
			auto dest = scene.mMeshes[meshId++] = new aiMesh();
			dest->mName = name;
			dest->mMaterialIndex = GetSceneMaterialId(mat);

			int faceCount = 0;
			for (auto& tri : tris) {
				if (GetTriSurfaceHash(&tri) == mat) {
					faceCount++;
				}
			}
			int vertexCount = faceCount * 3;

			dest->mVertices = new aiVector3D[vertexCount];
			dest->mNumVertices = vertexCount;
			dest->mFaces = new aiFace[faceCount];
			dest->mNumFaces = faceCount;

			int faceId = 0;
			int vertexId = 0;
			for (auto& tri : tris) {
				if (GetTriSurfaceHash(&tri) != mat) continue;

				dest->mFaces[faceId].mIndices = new uint32_t[3];
				dest->mFaces[faceId].mIndices[0] = faceId*3;
				dest->mFaces[faceId].mIndices[1] = (faceId*3)+1;
				dest->mFaces[faceId].mIndices[2] = (faceId*3)+2;
				dest->mFaces[faceId].mNumIndices = 3;
				faceId++;

				dest->mVertices[vertexId].x = -tri.fPt0.x;
				dest->mVertices[vertexId].y = tri.fPt0.y;
				dest->mVertices[vertexId].z = tri.fPt0.z;
				vertexId++;
				dest->mVertices[vertexId].x = -tri.fPt1.x;
				dest->mVertices[vertexId].y = tri.fPt1.y;
				dest->mVertices[vertexId].z = tri.fPt1.z;
				vertexId++;
				dest->mVertices[vertexId].x = -tri.fPt2.x;
				dest->mVertices[vertexId].y = tri.fPt2.y;
				dest->mVertices[vertexId].z = tri.fPt2.z;
				vertexId++;
			}
		}
	}

	aiScene GenerateScene() {
		aiScene scene;
		scene.mRootNode = new aiNode();

		aSceneMaterials.clear();
		GetSceneMaterialId(0);

		int numSurfaces = 0;
		for (int i = 0; i < 2700; i++) {
			auto& article = CollView::aCapturedArticles[i];
			for (auto& inst : article.aInstances) {
				std::vector<uint32_t> materials;

				for (auto& tri : inst.aTris) {
					bool add = true;
					for (auto& mat : materials) {
						if (GetTriSurfaceHash(&tri) == mat) {
							add = false;
						}
					}
					if (add) {
						numSurfaces++;
						materials.push_back(GetTriSurfaceHash(&tri));
						GetSceneMaterialId(GetTriSurfaceHash(&tri));
					}
				}

				if (!inst.aBarriers.empty()) numSurfaces++;
			}
		}
		WriteLog(std::to_string(numSurfaces) + " surfaces will be exported");

		scene.mMaterials = new aiMaterial*[aSceneMaterials.size()];
		for (int i = 0; i < aSceneMaterials.size(); i++) {
			auto mat = aSceneMaterials[i];
			auto srcName = GetMaterialName(mat);
			scene.mMaterials[i] = new aiMaterial();
			auto dest = scene.mMaterials[i];
			aiString matName(srcName);
			dest->AddProperty(&matName, AI_MATKEY_NAME);

			if (mat) {
				auto collection = Attrib::FindCollection(Attrib::StringHash32("simsurface"), mat);
				auto color = (float*)collection->GetData(0x740D3125, 0);
				aiColor4D matColor = {color[0], color[1], color[2], color[3]};
				dest->AddProperty(&matColor, 1, AI_MATKEY_COLOR_DIFFUSE);
			}
		}
		scene.mNumMaterials = aSceneMaterials.size();

		scene.mMeshes = new aiMesh*[numSurfaces];
		scene.mNumMeshes = numSurfaces;

		int counter = 0;
		for (int i = 0; i < 2700; i++) {
			auto& article = CollView::aCapturedArticles[i];

			for (auto& inst : article.aInstances) {
				auto articleName = std::format("Article{}_Instance{}", i, &inst - &article.aInstances[0]);
				if (inst.groupId) articleName = std::format("SceneryGroup{}_{}", inst.groupId, articleName);

				if (!inst.aTris.empty()) {
					ExportTris(scene, articleName + "_TriStrip", counter, inst.aTris);
				}
				if (!inst.aBarriers.empty()) {
					ExportTris(scene, articleName + "_Barrier", counter, inst.aBarriers);
				}
			}
		}

		WriteLog(std::to_string(counter) + " surfaces processed");

		aiNode* node = nullptr;
		std::string nodeName;
		for (int i = 0; i < numSurfaces; i++) {
			if (!node || nodeName != scene.mMeshes[i]->mName.C_Str()) {
				node = new aiNode();
				node->mName = scene.mMeshes[i]->mName;
				scene.mRootNode->addChildren(1, &node);
			}

			std::vector<int> meshes;
			if (node->mMeshes) {
				for (int j = 0; j < node->mNumMeshes; j++) {
					meshes.push_back(node->mMeshes[j]);
				}
				delete[] node->mMeshes;
			}
			meshes.push_back(i);

			node->mMeshes = new uint32_t[meshes.size()];
			node->mNumMeshes = meshes.size();
			for (int j = 0; j < meshes.size(); j++) {
				node->mMeshes[j] = meshes[j];
			}
		}

		return scene;
	}

	void WriteToFBX() {
		WriteLog("Writing model file...");

		auto scene = GenerateScene();

		Assimp::Logger::LogSeverity severity = Assimp::Logger::VERBOSE;
		Assimp::DefaultLogger::create("fbx_export_log.txt", severity, aiDefaultLogStream_FILE);

		Assimp::Exporter exporter;
		if (exporter.Export(&scene, "fbx", "world.fbx") != aiReturn_SUCCESS) {
			WriteLog("ERROR: Model export failed!");
		}
		else {
			WriteLog("Model export finished");
		}
	}
}