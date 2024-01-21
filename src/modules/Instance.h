#pragma once

#include <string>

#include "Module.h"

#include "instance/Instance.h"

class InstanceModule : public Module
{
private:
	bool m_show = false;
	Instance* m_selected = nullptr;
	char m_filter[64] = "";

	void DrawInstance();
	void SkewTo(Instance* instance);

	std::string GetBinary(int value);

public:
	void OnMenu();
	void OnDraw();

	Instance* GetSelected() { return m_selected; }
};