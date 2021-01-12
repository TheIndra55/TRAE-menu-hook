#pragma once
#include <d3d9.h>

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

class Menu
{
public:
	Menu(LPDIRECT3DDEVICE9 pd3dDevice, HWND hwnd);

	void OnDeviceReleased();
	void OnPresent();
	void Process(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	void ToggleFlight(bool flight);
	void ProcessFlight(UINT msg, WPARAM wparam);
	void SetDevice(LPDIRECT3DDEVICE9 pd3dDevice);
	void Log(const char* fmt, ...);

	// TODO private
	bool m_focus = false;
private:
	void Draw();

	bool m_flight = false;
	ImGuiTextBuffer logBuffer;

	LPDIRECT3DDEVICE9 m_pd3dDevice;
	HWND m_hwnd;
};

static int(__cdecl* original_SIGNAL_FindSignal)(DWORD* level, int signal);
int hooked_SIGNAL_FindSignal(DWORD* level, int signal);