#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <mutex>
#include <random>
#include <thread>
#include <toml++/toml.hpp>

#include "nya_dx9_hookbase.h"
#include "nya_commonhooklib.h"
#include "nya_commonmath.h"
#include "nfsmw.h"

#include "include/chloemenulib.h"

std::vector<void(*)()> aDrawing3DLoopFunctions;
std::vector<void(*)()> aDrawing3DLoopFunctionsOnce;

#include "util.h"

void UpdateD3DProperties() {
	g_pd3dDevice = GameD3DDevice;
	ghWnd = GameWindow;
	GetRacingResolution(&nResX, &nResY);
}

void HookLoop() {}

#include "components/render3d.h"
#include "components/colrender.h"

void Render3DLoop() {
	UpdateD3DProperties();

	DLLDirSetter _setdir;

	static IDirect3DStateBlock9* state = nullptr;
	if (g_pd3dDevice->CreateStateBlock(D3DSBT_ALL, &state) != D3D_OK) {
		return;
	}

	if (state->Capture() < 0) {
		state->Release();
		return;
	}

	for (auto& func : aDrawing3DLoopFunctions) {
		func();
	}

	for (auto& func : aDrawing3DLoopFunctionsOnce) {
		func();
	}
	aDrawing3DLoopFunctionsOnce.clear();

	state->Apply();
	state->Release();
}

void DebugMenu() {
	ChloeMenuLib::BeginMenu();

	if (DrawMenuOption("no world draw")) {
		NyaHookLib::Patch(0x723FA0, 0x530008C2);
	}
	if (DrawMenuOption("yes world draw")) {
		NyaHookLib::Patch(0x723FA0, 0x5314EC83);
	}

	QuickValueEditor("bWireframeGround", CollView::bWireframeGround);
	QuickValueEditor("bWireframeBarriers", CollView::bWireframeBarriers);

	QuickValueEditor("colR", CollView::colR);
	QuickValueEditor("colG", CollView::colG);
	QuickValueEditor("colB", CollView::colB);

	ChloeMenuLib::EndMenu();
}

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
	switch( fdwReason ) {
		case DLL_PROCESS_ATTACH: {
			if (NyaHookLib::GetEntryPoint() != 0x3C4040) {
				MessageBoxA(nullptr, "Unsupported game version! Make sure you're using v1.3 (.exe size of 6029312 bytes)", "nya?!~", MB_ICONERROR);
				return TRUE;
			}

			srand(time(0));

			NyaHookLib::Patch(0x723FA0, 0x530008C2);

			GetCurrentDirectoryW(MAX_PATH, gDLLDir);

			ChloeMenuLib::RegisterMenu("Cwoee Collision Viewer", &DebugMenu);

			for (auto& func : ChloeHook::aHooks) {
				func();
			}

			NyaHooks::LateInitHook::Init();
			NyaHooks::LateInitHook::aFunctions.push_back([](){
				if (!GetModuleHandleA("vulkan-1.dll") && !GetModuleHandleA("winevulkan.dll")) {
					MessageBoxA(nullptr, "WARNING: DXVK is not installed properly! Make sure you've placed d3d9.dll from the mod's archive into the game folder or you WILL encounter stability issues!", "nya?!~", MB_ICONERROR);
				}
			});
			NyaHooks::RenderWorldHook::Init();
			NyaHooks::RenderWorldHook::aPostFunctions.push_back(Render3DLoop);

			//SkipFE = true;
		} break;
		default:
			break;
	}
	return TRUE;
}