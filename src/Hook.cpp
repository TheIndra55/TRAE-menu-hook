#include <MinHook.h>

#include "Hook.h"
#include "input/MessageHook.h"

#include "cdc/render/PCDeviceManager.h"

using namespace std::placeholders;

static bool(*s_D3D_Init)();

static bool D3D_Init()
{
	// Initialize the device
	auto ret = s_D3D_Init();

	Hook::GetInstance().OnDevice();

	return ret;
}

Hook::Hook() : m_menu(nullptr)
{
	Initialize();
}

void Hook::Initialize()
{
	MH_Initialize();
	MH_CreateHook((void*)0x4153E0, D3D_Init, (void**)&s_D3D_Init);
	MH_EnableHook(MH_ALL_HOOKS);
}

void Hook::PostInitialize()
{
	// Create the menu
	m_menu = std::make_unique<Menu>();

	// Register the message hook
	MessageHook::OnMessage(std::bind(&Hook::OnMessage, this, _1, _2, _3, _4));
}

void Hook::OnMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	m_menu->OnMessage(hWnd, msg, wParam, lParam);
}

void Hook::OnDevice()
{
	// Assign the DeviceManager instance
	cdc::PCDeviceManager::s_pInstance = *(cdc::PCDeviceManager**)0xA6669C;

	// Initialize the hook
	PostInitialize();
}

Hook& Hook::GetInstance()
{
	static Hook instance;
	return instance;
}