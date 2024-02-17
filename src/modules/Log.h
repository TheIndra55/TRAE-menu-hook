#pragma once

#include <imgui.h>

#include "Module.h"

class Log : public Module
{
private:
	bool m_show = true;
	ImGuiTextBuffer m_buffer;

public:
	void OnMenu();
	void OnDraw();

	// Writes to the log
	void Write(const char* fmt, ...);
	// Writes a line to the log
	void WriteLine(const char* fmt, ...);

	// Clears the log
	void Clear();
};