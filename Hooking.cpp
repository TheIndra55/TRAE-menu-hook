#include "Hooking.hpp"
#include "Game.hpp"
#include "ControlHooks.hpp"

extern Hooking* g_hooking;

LPDIRECT3DDEVICE9 pDevice;
HWND pHwnd;

void Hooking::Initialize()
{
	MH_Initialize();
	
	// hook into d3d9 creation function and wait for a device
#if TRAE
	MH_CreateHook(reinterpret_cast<void*>(0xC5C175), hooked_Direct3DInit, reinterpret_cast<void**>(&original_Direct3DInit));
#elif TR8
	MH_CreateHook(reinterpret_cast<void*>(0x00478640), hooked_Direct3DInit, reinterpret_cast<void**>(&original_Direct3DInit));
#endif

	InstallControlHooks();
	Game::Initialize();

	MH_EnableHook(MH_ALL_HOOKS);
}

void Hooking::Uninitialize()
{
	MH_Uninitialize();
}

char(__thiscall* original__PCDeviceManager__CreateDevice)(DWORD* _this, DWORD a2);

char __fastcall PCDeviceManager__CreateDevice(DWORD* _this, DWORD _, DWORD a2)
{
	auto val = original__PCDeviceManager__CreateDevice(_this, a2);

#if TRAE
	auto address = *reinterpret_cast<DWORD*>(0xA6669C);
#elif TR8
	auto address = *reinterpret_cast<DWORD*>(0xAD75E4);
#endif
	auto device = *reinterpret_cast<DWORD*>(address + 0x20);
	pDevice = reinterpret_cast<IDirect3DDevice9*>(device);

	g_hooking->menu->SetDevice(pDevice);

	return val;
}

void(__thiscall* orginal_PCDeviceManager__ReleaseDevice)(DWORD* _this, int status);

void __fastcall PCDeviceManager__ReleaseDevice(DWORD* _this, DWORD _, int status)
{
	g_hooking->menu->OnDeviceReleased();
	ImGui_ImplDX9_InvalidateDeviceObjects();

	orginal_PCDeviceManager__ReleaseDevice(_this, status);
}

int(__thiscall* origTerrainDrawable_TerrainDrawable)(DWORD _this, int* a2, int a3, int a4, int a5);
int __fastcall TerrainDrawable_TerrainDrawable(DWORD _this, DWORD _, int* a2, int a3, int a4, int a5)
{
	auto ret = origTerrainDrawable_TerrainDrawable(_this, a2, a3, a4, a5);

	if (*(bool*)0x7C7CD4 /* wire frame */)
	{
		*(unsigned int*)(_this + 0x1C) |= 0x800;
	}

	return ret;
}

int(__cdecl* origGetDrawListByTpageId)(unsigned int tpageid, bool reflect);
int __cdecl GetDrawListByTpageId(unsigned int tpageid, bool reflect)
{
	if (*(bool*)0x7C7CD4 /* wire frame */)
	{
		tpageid |= 0x800;
	}

	return origGetDrawListByTpageId(tpageid, reflect);
}

float* (__cdecl* TRANS_RotTransPersVectorf)(DWORD a1, DWORD a2);
void(__cdecl* Font__Print)(DWORD font, const char* a2, ...);
void(__cdecl* org_Font__Flush)();

void SetCursor(float x, float y)
{
	/* CursorX */ *(float*)0x007D180C = x;
	/* CursorY */ *(float*)0x007D1810 = y;
}

#if TRAE
void __cdecl EVENT_DisplayString(char* str, int time)
{
	g_hooking->menu->Log("%s\n", str);
}

void __cdecl EVENT_DisplayStringXY(char* str, int time, int x, int y)
{
	if (!g_hooking->menu->m_drawSettings.drawDebug) return;

	SetCursor((float)x, (float)y);
	Font__Print(*(DWORD*)0x007D1800, str);
}

void __cdecl EVENT_FontPrint(char* fmt, ...)
{
	if (!g_hooking->menu->m_drawSettings.drawDebug) return;

	va_list vl;
	va_start(vl, fmt);
	char str[1024]; // size same as game buffer
	vsprintf(str, fmt, vl);

	Font__Print(*(DWORD*)0x007D1800, str);
}

void __cdecl EVENT_PrintScalarExpression(int val, int time)
{
	if (!g_hooking->menu->m_drawSettings.drawDebug) return;

	char v3[11];
	sprintf(v3, "%d", val);
	Font__Print(*(DWORD*)0x007D1800, v3);
}
#endif

#if TR8
void __stdcall DisplayString(int a1, int a2, bool newline)
{
	g_hooking->menu->Log("%s%s", (char*)*(DWORD*)a1, newline ? "\n" : "");
}

void __cdecl DisplayInt(int a1, int a2, int a3)
{
	g_hooking->menu->Log("%f%s", *(float*)(a3 + 4), *(char*)(a3 + 8) != 0 ? "\n" : "");
}
#endif

bool(__cdecl* objCheckFamily)(DWORD instance, unsigned __int16 family);

void __cdecl Font__Flush()
{
	auto instance = *(DWORD*)0x817D64;

	if (g_hooking->menu->m_drawSettings.draw && instance)
	{
		auto settings = g_hooking->menu->m_drawSettings;

		// loop trough all instances
		while (1)
		{
			auto next = *(DWORD*)(instance + 8);
			auto object = *(DWORD*)(instance + 0x94);

			auto instanceObj = (Instance*)instance;

			auto data = *(DWORD*)(instance + 448);
			auto extraData = *(DWORD*)(instance + 572);

			// TODO filter only pickups
			auto show = [](DrawSettings settings, DWORD instance, DWORD data)
			{
				if (!settings.filter) return true;

				// if selected 'draw enemy health' and instance is an enemy continue
				if (settings.drawHealth && data && *(unsigned __int16*)(data + 2) == 56048) return true;

				return objCheckFamily(instance, 35) /* keys, healthpacks stuff */ || objCheckFamily(instance, 39) /* ammo */;
			};

			auto srcVector = cdc::Vector{};
			srcVector = instanceObj->position;
			TRANS_RotTransPersVectorf((DWORD)&srcVector, (DWORD)&srcVector);

			if (show(settings, instance, data) && srcVector.z > 16.f /* only draw when on screen */)
			{
				SetCursor(srcVector.x, srcVector.y);
				Font__Print(*(DWORD*)0x007D1800, "%s", (char*)*(DWORD*)(object + 0x48));

				if (settings.drawHealth && extraData && data && *(unsigned __int16*)(data + 2) == 56048)
				{
					srcVector.y += 15.f;
					SetCursor(srcVector.x, srcVector.y);
					Font__Print(*(DWORD*)0x007D1800, "%8.2f", *(float*)(extraData + 5280));
				}

				if (settings.drawIntro)
				{
					srcVector.y += 15.f;
					SetCursor(srcVector.x, srcVector.y);
					Font__Print(*(DWORD*)0x007D1800, "%d", *(int*)(instance + 0x1D0));
				}

				if (settings.drawAddress)
				{
					srcVector.y += 15.f;
					SetCursor(srcVector.x, srcVector.y);
					Font__Print(*(DWORD*)0x007D1800, "%p", instance);
				}

				if (settings.drawFamily && data)
				{
					srcVector.y += 15.f;
					SetCursor(srcVector.x, srcVector.y);
					Font__Print(*(DWORD*)0x007D1800, "%d", *(unsigned __int16*)(data + 2));
				}
			}

			if (!next)
				break;

			instance = next;
		}
	}

	org_Font__Flush();
}

void Hooking::GotDevice()
{
	this->menu = new Menu(pDevice, pHwnd);

	// hook game's d3d9 present function and wndproc function
#if TRAE
	MH_CreateHook(reinterpret_cast<void*>(0x61BB80), hooked_PCRenderContext_Present, reinterpret_cast<void**>(&original_PCRenderContext_Present));

	MH_CreateHook(reinterpret_cast<void*>(0x4040B0), hooked_RegularWndProc, reinterpret_cast<void**>(&original_RegularWndProc));
#elif TR8
	MH_CreateHook(reinterpret_cast<void*>(0x519360), hooked_PCRenderContext_Present, reinterpret_cast<void**>(&original_PCRenderContext_Present));

	MH_CreateHook(reinterpret_cast<void*>(0x478BC0), hooked_RegularWndProc, reinterpret_cast<void**>(&original_RegularWndProc));
#endif

	// hook SetCursorPos to prevent the game from resetting the cursor position
	MH_CreateHookApi(L"user32", "SetCursorPos", hooked_SetCursorPos, reinterpret_cast<void**>(&original_SetCursorPos));

#if TRAE
	MH_CreateHook((void*)0x00617F50, PCDeviceManager__ReleaseDevice, (void**)&orginal_PCDeviceManager__ReleaseDevice);
	MH_CreateHook((void*)0x00617BE0, PCDeviceManager__CreateDevice, (void**)&original__PCDeviceManager__CreateDevice);
#elif TR8
	MH_CreateHook((void*)0x005223F0, PCDeviceManager__ReleaseDevice, (void**)&orginal_PCDeviceManager__ReleaseDevice);
	MH_CreateHook((void*)0x00522580, PCDeviceManager__CreateDevice, (void**)&original__PCDeviceManager__CreateDevice);
#endif

#if TRAE
	// patch debug print nullsubs to our functions
	*(DWORD*)(0x7C8A50 + 528) = (DWORD)EVENT_DisplayString;
	*(DWORD*)(0x7C8A50 + 304) = (DWORD)EVENT_DisplayString;

	// draw debug
	*(DWORD*)(0x7C8A50 + 1400) = (DWORD)EVENT_DisplayStringXY;
	*(DWORD*)(0x7C8A50 + 464) = (DWORD)EVENT_FontPrint;
	*(DWORD*)(0x7C8A50 + 1292) = (DWORD)EVENT_PrintScalarExpression;

	objCheckFamily = reinterpret_cast<bool(__cdecl*)(DWORD instance, unsigned __int16 family)>(0x534660);

	MH_CreateHook((void*)0x00434C40, Font__Flush, (void**)&org_Font__Flush);
	Font__Print = reinterpret_cast<void(__cdecl*)(DWORD, const char*, ...)>(0x00C5F83D);
	TRANS_RotTransPersVectorf = reinterpret_cast<float*(__cdecl*)(DWORD, DWORD)>(0x00402B50);

	MH_CreateHook((void*)0xC5B896, TerrainDrawable_TerrainDrawable, (void**)&origTerrainDrawable_TerrainDrawable);
	MH_CreateHook((void*)0xC5C280, GetDrawListByTpageId, (void**)&origGetDrawListByTpageId);
#endif
#if TR8
	// debug print nullsub in TR8
	MH_CreateHook((void*)0x574BE0, DisplayString, nullptr);
	MH_CreateHook((void*)0x795D50, DisplayInt, nullptr);
#endif

	MH_EnableHook(MH_ALL_HOOKS);
}

int hooked_Direct3DInit()
{
	// call orginal game function to init d3direct device
	auto val = original_Direct3DInit();

	// get the d3d9 device and hwnd
#if TRAE
	pHwnd = *reinterpret_cast<HWND*>(0x6926C8);
#elif TR8
	pHwnd = *reinterpret_cast<HWND*>(0x9EEDE8);
#endif

	// (IDirect3DDevice*)devicemanager->d3device
#if TRAE
	auto address = *reinterpret_cast<DWORD*>(0xA6669C);
#elif TR8
	auto address = *reinterpret_cast<DWORD*>(0xAD75E4);
#endif
	auto device = *reinterpret_cast<DWORD*>(address + 0x20);
	pDevice = reinterpret_cast<IDirect3DDevice9*>(device);

	g_hooking->GotDevice();

	return val;
}

#if TRAE
void __fastcall hooked_PCRenderContext_Present(DWORD* _this, void* _, int a2, int a3, int a4)
{
	g_hooking->menu->OnPresent();

	// call orginal game present function to draw on the screen
	original_PCRenderContext_Present(_this, a2, a3, a4);
}
#elif TR8
void __fastcall hooked_PCRenderContext_Present(DWORD* _this, void* _, int a2)
{

	// call orginal game present function to draw on the screen
	original_PCRenderContext_Present(_this, a2);
	
	g_hooking->menu->OnPresent();
}
#endif

LRESULT hooked_RegularWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_KEYUP && wparam == VK_F8)
	{
		g_hooking->menu->m_focus = !g_hooking->menu->m_focus;

		// disable game input
#if TRAE
		*(bool*)0x8551A9 = g_hooking->menu->m_focus;
#elif TR8
		*(bool*)0xA02B79 = g_hooking->menu->m_focus;
#endif
	}

	// pass input to menu
	g_hooking->menu->Process(hwnd, msg, wparam, lparam);

	// pass input to orginal game wndproc
	return original_RegularWndProc(hwnd, msg, wparam, lparam);
}

BOOL WINAPI hooked_SetCursorPos(int x, int y)
{
	// prevent game from reseting cursor position
	if (g_hooking->menu->m_focus)
	{
		return 1;
	}

	return original_SetCursorPos(x, y);
}