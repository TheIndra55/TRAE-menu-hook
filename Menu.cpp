#include "Menu.hpp"
#include "Game.hpp"
#include "Hooking.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool shouldInstance = true;
static bool shouldReloc = true;

Instance*(__cdecl* INSTANCE_NewInstance)();

Instance* newinstance()
{
    if (shouldInstance)
        return INSTANCE_NewInstance();

    return 0;
}

// most of this code is written in 2 minutes to confirm finds
char(__cdecl* pushscreen)(int, bool);

char pushscreenhooked(int screen, bool unk2)
{
    Hooking::GetInstance().GetMenu()->Log("screen pushed: %d\n", screen);
    return pushscreen(screen, unk2);
}

void(__thiscall* orginal_Subtitle_Add)(DWORD*, char* str, int duration);
void __fastcall hooked_Subtitle_Add(DWORD* _this, void* _, char* str, int duration)
{
    Hooking::GetInstance().GetMenu()->Log("%s %d\n", str, duration);

    orginal_Subtitle_Add(_this, str, duration);
}

char IsPs2()
{
    // somewhat the zoom code for binoculars is inside an 'if IsPs2()' in TRAE so we have to hook that function
    return Game::m_binoculars;
}

bool isDiskFS = false;
bool switchPlayerNextFrame = false;

extern int(__thiscall* MSFileSystem_FileExists)(int _this, const char* file);

int(__cdecl* origIMAGE_LoadImage)(char* name);
int __cdecl IMAGE_LoadImage(char* name)
{
    char string[256];
    sprintf_s(string, "\\" CONFIGNAME "\\units\\%s.raw", name);

    if (MSFileSystem_FileExists(*(int*)DISKFS, string))
    {
        Hooking::GetInstance().GetMenu()->Log("%s exists, loading that one instead\n", string);
        isDiskFS = true;
    }

    auto ret = origIMAGE_LoadImage(name);
    isDiskFS = false;

    return ret;
}

int(__cdecl* origInsertGlobalObject)(int a1, char a2);
int __cdecl InsertGlobalObject(int a1, char a2)
{
#if TRAE
    auto objects = *(int*)0x842C70;
#elif TR7
    auto objects = *(int*)0x10F9110;
#elif TR8
    auto objects = *(int*)0xDB94D0;
#endif

#if TRAE || TR7
    auto name = (char*)*(int*)(objects + 8 * a1 - 4);
#elif TR8
    auto name = (char*)*(int*)(objects + 8 * a1);
#endif

    char string[256];
    sprintf_s(string, "\\" CONFIGNAME "\\units\\%s.drm", name);

    //Hooking::GetInstance().GetMenu()->Log("InsertGlobalObject: %s\n", name);
    if (MSFileSystem_FileExists(*(int*)DISKFS, string))
    {
        Hooking::GetInstance().GetMenu()->Log("%s exists, loading that one instead\n", string);
        isDiskFS = true;
    }

    auto ret = origInsertGlobalObject(a1, a2);
    isDiskFS = false;

    return ret;
}

void(__cdecl* origUnitFileName)(char*, char*, char*);
void unitFileName(char* name, char* unit, char* ext)
{
    if (isDiskFS)
    {
        sprintf(name, "\\" CONFIGNAME "\\units\\%s.drm", unit);
        return;
    }

    origUnitFileName(name, unit, ext);
}

void(__cdecl* origImageFileName)(char*, char*);
void __cdecl imageFileName(char* name, char* image)
{
    if (isDiskFS)
    {
        sprintf(name, "\\" CONFIGNAME "\\units\\%s.raw", image);
        return;
    }

    origImageFileName(name, image);
}

int getFS()
{
    if (isDiskFS)
    {
        return *(int*)DISKFS;
    }

#if TRAE
    return *(int*)0x83888C;
#elif TR8
    return *(int*)0x9CE278;
#elif TR7
    return *(int*)0x10EEC7C;
#endif
}

int(__cdecl* origSTREAM_LoadLevel)(char* a1, int a2, char a3);

int __cdecl STREAM_LoadLevel(char* a1, int a2, char a3)
{
    Hooking::GetInstance().GetMenu()->Log(__FUNCTION__ " Loading %s\n", a1);

    char string[256];
    sprintf_s(string, "\\" CONFIGNAME "\\units\\%s.drm", a1);

    if (MSFileSystem_FileExists(*(int*)DISKFS, string))
    {
        // load this unit from disk
        Hooking::GetInstance().GetMenu()->Log("%s exists, loading this unit from disk\n", string);
        isDiskFS = true;
    }

    auto unit = origSTREAM_LoadLevel(a1, a2, a3);
    isDiskFS = false;

    return unit;
}

void(__cdecl* origSTREAM_FinishLoad)(StreamUnit* unit);
void STREAM_FinishLoad(StreamUnit* unit)
{
    if (!shouldReloc)
    {
        Level* level = unit->level;
        *(DWORD*)(level + 156) = 0; // set reloc module ptr to 0
    }

    origSTREAM_FinishLoad(unit);
}

int(__cdecl* INSTANCE_ReallyRemoveInstance)(Instance* instance, int a2, char a3);
void(__cdecl* INSTANCE_SetModel)(Instance* instance, int model);

void(__cdecl* G2EmulationInstanceSetAnimation)(Instance* instance, int section, int anim, int frame, int frames);
void(__cdecl* G2EmulationInstanceSetMode)(Instance* instance, int section, int mode);

void(__thiscall* origCinematicHandlerImpl_NextFrame)(int _this);
void __fastcall CinematicHandlerImpl_NextFrame(int _this, int)
{
    auto streamFlags = (int*)0x8383F4;
    if (*streamFlags & 0x1000)
    {
        // dont progress cine
        return;
    }

    origCinematicHandlerImpl_NextFrame(_this);
}

#if TR8
void(__cdecl* CAMERA_SetMode)(int mode);
int(__thiscall* _setToGameplayCamera)(int _this);
#endif

Menu::Menu(LPDIRECT3DDEVICE9 pd3dDevice, HWND hwnd)
{
	m_pd3dDevice = pd3dDevice;
	m_hwnd = hwnd;

	OnLayoutChange();

	ImGui::CreateContext();

	ImGui_ImplWin32_Init(m_hwnd);
	ImGui_ImplDX9_Init(m_pd3dDevice);

#if TRAE
    MH_CreateHook((void*)0x00457580, newinstance, (void**)&INSTANCE_NewInstance);
    MH_CreateHook((void*)0x4FCB60, pushscreenhooked, (void**)&pushscreen);
    MH_CreateHook((void*)0x0046F080, hooked_Subtitle_Add, (void**)&orginal_Subtitle_Add);

    MH_CreateHook((void*)0x004E6EC0, IsPs2, nullptr);
    MH_CreateHook((void*)0x005DBBA0, STREAM_LoadLevel, (void**)&origSTREAM_LoadLevel);
    MH_CreateHook((void*)0x005DB680, STREAM_FinishLoad, (void**)&origSTREAM_FinishLoad);

    MH_CreateHook((void*)0x00424FE0, CinematicHandlerImpl_NextFrame, (void**)&origCinematicHandlerImpl_NextFrame);
#endif

#if TRAE
    MH_CreateHook((void*)0x0045F640, getFS, nullptr);
    MSFileSystem_FileExists = reinterpret_cast<int(__thiscall*)(int _this, const char* file)>(0x005E52C0);

    MH_CreateHook((void*)0x00401480, IMAGE_LoadImage, (void**)&origIMAGE_LoadImage);

    MH_CreateHook((void*)0x45F650, unitFileName, (void**)&origUnitFileName);
    MH_CreateHook((void*)0x5DB360, InsertGlobalObject, (void**)&origInsertGlobalObject);
    MH_CreateHook((void*)0x0045F6A0, imageFileName, (void**)&origImageFileName);

    INSTANCE_ReallyRemoveInstance = reinterpret_cast<int(__cdecl*)(Instance*, int, char)>(0x0045A3A0);
    INSTANCE_SetModel = reinterpret_cast<void(__cdecl*)(Instance * instance, int model)>(0x00458A90);

    G2EmulationInstanceSetAnimation = reinterpret_cast<void(__cdecl*)(Instance*, int, int, int, int)>(0x004DE690);
    G2EmulationInstanceSetMode = reinterpret_cast<void(__cdecl*)(Instance*, int, int)>(0x004DE7F0);
#elif TR7
    MH_CreateHook((void*)0x0045F420, getFS, nullptr);
    MH_CreateHook((void*)0x0045F4D0, unitFileName, (void**)&origUnitFileName);

    MH_CreateHook((void*)0x005DB550, InsertGlobalObject, (void**)&origInsertGlobalObject);
    MSFileSystem_FileExists = reinterpret_cast<int(__thiscall*)(int _this, const char* file)>(0x0047DC70);

    MH_CreateHook((void*)0x005DBD20, STREAM_LoadLevel, (void**)&origSTREAM_LoadLevel);

    MH_CreateHook((void*)0x00401480, IMAGE_LoadImage, (void**)&origIMAGE_LoadImage);
    MH_CreateHook((void*)0x0045F520, imageFileName, (void**)&origImageFileName);

    INSTANCE_ReallyRemoveInstance = reinterpret_cast<int(__cdecl*)(Instance*, int, char)>(0x0045A330);
    INSTANCE_SetModel = reinterpret_cast<void(__cdecl*)(Instance * instance, int model)>(0x00458C80);

    G2EmulationInstanceSetAnimation = reinterpret_cast<void(__cdecl*)(Instance*, int, int, int, int)>(0x004E1F00);
    G2EmulationInstanceSetMode = reinterpret_cast<void(__cdecl*)(Instance*, int, int)>(0x004E2060);
#endif

#if TR8
    MH_CreateHook((void*)0x00472B50, getFS, nullptr);
    MH_CreateHook((void*)0x00477970, unitFileName, (void**)&origUnitFileName);

    MH_CreateHook((void*)0x005C1550, InsertGlobalObject, (void**)&origInsertGlobalObject);
    MSFileSystem_FileExists = reinterpret_cast<int(__thiscall*)(int _this, const char* file)>(0x00473FB0);

    MH_CreateHook((void*)0x005D23F0, STREAM_LoadLevel, (void**)&origSTREAM_LoadLevel);

    INSTANCE_ReallyRemoveInstance = reinterpret_cast<int(__cdecl*)(Instance*, int, char)>(0x005BC4E0);
    INSTANCE_SetModel = reinterpret_cast<void(__cdecl*)(Instance * instance, int model)>(0x005B9170);

    G2EmulationInstanceSetAnimation = reinterpret_cast<void(__cdecl*)(Instance*, int, int, int, int)>(0x005B1EA0);
    G2EmulationInstanceSetMode = reinterpret_cast<void(__cdecl*)(Instance*, int, int)>(0x005B1F50);

    CAMERA_SetMode = reinterpret_cast<void(__cdecl*)(int mode)>(0x005F39F0);
    _setToGameplayCamera = reinterpret_cast<int(__thiscall*)(int _this)>(0x5EDF50);
#endif
}

void Menu::OnDeviceReleased()
{
    m_pd3dDevice->Release();
}

void Menu::SetDevice(LPDIRECT3DDEVICE9 pd3dDevice)
{
    m_pd3dDevice = pd3dDevice;
    ImGui_ImplDX9_Init(m_pd3dDevice);
}

void Menu::OnPresent()
{
	ImGui::GetIO().MouseDrawCursor = m_focus;

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();
	// draw menu

    if (m_visible)
    {
        this->Draw();
    }

	ImGui::EndFrame();

	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void Menu::Process(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    auto shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (msg == WM_KEYUP && wparam == VK_F2)
    {
        ToggleFlight(!m_flight);
    }

    if (msg == WM_KEYUP && wparam == VK_F9)
    {
        Game::SwitchPlayerCharacter();
    }

    if (msg == WM_KEYUP && wparam == VK_F7)
    {
        m_visible = !m_visible;
    }

    if (msg == WM_KEYUP && wparam == VK_F5)
    {
#if TR8
        auto gPlayerControl_Enabled = (bool*)0x8AB4E6;
#elif TRAE
        auto gPlayerControl_Enabled = (bool*)0x666C34;
#elif TR7
        auto gPlayerControl_Enabled = (bool*)0xF1E40C;
#endif

        *gPlayerControl_Enabled = !*gPlayerControl_Enabled;
    }

    if (msg == WM_KEYUP && wparam == VK_F3 && shift)
    {
#if TRAE || TR7
        auto debugTimeMult = (float*)(GAMETRACKER + 0x13C);
#elif TR8
        auto debugTimeMult = (float*)0xE7F1D0;
#endif

        if (*debugTimeMult < 0.06f)
        {
            *debugTimeMult = 1.0f;
        }
        else if (*debugTimeMult < 0.3f)
        {
            *debugTimeMult = 0.05f;
        }
        else
        {
            *debugTimeMult = 0.2f;
        }
    }

    if (msg == WM_KEYUP && wparam == VK_F4)
    {
#if TRAE // current legend supported exe (debug exe) has already fly on F4 so only switch the mode on TRAE
        auto cameraMode = (int*)0x850984;
        *cameraMode = *cameraMode == 7 ? 2 : 7;
#endif

#if TRAE || TR7
        *(int*)(GAMETRACKER + 0x1C) ^= 0x8000; // hide hud
#endif

#if TR8
        m_drawSettings.flight = !m_drawSettings.flight;
        if (m_drawSettings.flight)
        {
            CAMERA_SetMode(11);

            auto camera = *(int*)0xE80534;
            *(cdc::Vector*)(camera + 0x40) = (*(Instance**)PLAYERINSTANCE)->position;
        }
        else
        {
            //*(int*)0xE80534 = 0xE83860 /* AVLaraCameraRewrite */;

            _setToGameplayCamera(0xE804F0 /* AVCameraManager */);
        }
#endif
    }

    // pause the game with F3
    if (msg == WM_KEYUP && wparam == VK_F3 && !shift)
    {
#if TRAE || TR7
        auto streamFlags = (int*)(GAMETRACKER + 0xC4);
#elif TR8
        auto streamFlags = (int*)0xE7F0B8;
#endif
        if (*streamFlags & 0x1000)
        {
            *streamFlags &= 0xFFFFEFFF;
        }
        else
        {
            *streamFlags |= 0x1000u;
        }
    }

#if TRAE
    if (msg == WM_KEYUP && wparam == VK_DELETE)
    {
        Game::ToggleBinoculars();
    }

    if (switchPlayerNextFrame)
    {
        switchPlayerNextFrame = false;

        Game::SwitchPlayerCharacter();
    }
#endif

    if (msg == WM_INPUTLANGCHANGE)
    {
        OnLayoutChange();
    }

    if (m_flight)
    {
        ProcessFlight(msg, wparam);
    }

	if (m_focus)
	{
		ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
	}
}

void Menu::ToggleFlight(bool flight)
{
    m_flight = flight;

#if TRAE || TR7
    auto ptr = reinterpret_cast<int*>(GAMETRACKER + 227);
#elif TR8
    auto ptr = reinterpret_cast<int*>(0xE7F143);
#endif

    if (m_flight)
    {
        //*ptr |= 256;
        *ptr |= 256;
    }
    else
    {
        //*ptr ^= 256;
        *ptr = 0;

        // drop player when flight disabled
#if TRAE || TR7
        Game::InstancePost(*reinterpret_cast<Instance**>(PLAYERINSTANCE), 1048592, 0);
#endif
    }
}

void Menu::ProcessFlight(UINT msg, WPARAM wparam)
{
    auto base = *reinterpret_cast<DWORD*>(PLAYERINSTANCE);

#if TRAE || TR7
    auto z = reinterpret_cast<float*>(base + 24);
#elif TR8
    auto z = reinterpret_cast<float*>(base + 40);
#endif

    auto upKey = m_isAzertyLayout ? 0x41 /* A */ : 0x51 /* Q */;
    auto downKey = m_isAzertyLayout ? 0x57 /* W */ : 0x5A /* Z */;

    if (msg == WM_KEYDOWN && wparam == upKey)
    {
        *z += m_flightSpeed;
    }

    if (msg == WM_KEYDOWN && wparam == downKey)
    {
        *z -= m_flightSpeed;
    }
}

// used for PushOkDialog so it pops the dialog away after pressing OK
void __cdecl DialogFn()
{
    Game::PopScreen();
}

void Menu::Draw()
{
    static char chapter[32] = "";
    static char unit[32] = "";

    static bool show_instance_viewer = false;
    if (show_instance_viewer) DrawInstanceViewer();

    ImGui::Begin("Menu", nullptr, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Tools"))
        {
            ImGui::MenuItem("Instance viewer", NULL, &show_instance_viewer);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::Text("F2 = Flight, F8 = Toggle menu focus, F9 = Switch player character");
    ImGui::Text("Unit = %s, Flight = %s", (char*)GAMETRACKER_BASE_AREA, m_flight ? "true" : "false");

    ImGui::SliderFloat("Z speed", &m_flightSpeed, 0, 500);
#if TRAE

    if (ImGui::Button("Fill 'er Up"))
    {
        // pointers everywhere!
        auto missingHealth = *(float*)(*(DWORD*)((*(DWORD*)PLAYERINSTANCE) + 448) + 164);

        Game::IncreaseHealth(missingHealth);
        Game::TriggerUiFadeGroup(1);
        // TODO ammo
    }

    ImGui::Checkbox("Should instance?", &shouldInstance);
    ImGui::Checkbox("Load unit script", &shouldReloc);
    ImGui::Checkbox("Enable debug keypad", (bool*)0x7C8A3C);

    if (ImGui::Checkbox("Enable debug draw", &m_drawSettings.drawDebug))
    {
        *(int*)0x838348/*debugFlags2*/ |= 0x100;
    }
#endif

#if TRAE
    ImGui::Checkbox("Wireframe", (bool*)0x7C7CD4);
#elif TR7
    ImGui::Checkbox("Wireframe", (bool*)0x107E580);
#endif

#if TRAE || TR7
    ImGui::Checkbox("Draw instances", &m_drawSettings.draw);
    ImGui::Checkbox("Draw markup", &m_drawSettings.drawMarkup);
    ImGui::Checkbox("Draw enemy path", &m_drawSettings.drawPath);

    if (ImGui::CollapsingHeader("Draw settings"))
    {
        ImGui::Checkbox("Filter", &m_drawSettings.filter);
        ImGui::Checkbox("Draw intro id", &m_drawSettings.drawIntro);
        ImGui::Checkbox("Draw pointer address", &m_drawSettings.drawAddress);
        ImGui::Checkbox("Draw 'family'", &m_drawSettings.drawFamily);
        ImGui::Checkbox("Draw enemy health", &m_drawSettings.drawHealth);
    }
#endif

#if TRAE
    ImGui::InputText("chapter", chapter, 32);
#endif
    ImGui::InputText("unit", unit, MAX_UNIT_LEN);

#if TRAE
    if (ImGui::Button("Load chapter"))
    {
        // load chapter (chapter0, chapter1..)
        // chapter0 = croft manor
        if(Game::CheckChapter(chapter))
        {
            Game::SwitchChapter(chapter);
        }
        else
        {
            Game::PushOkDialog("Chapter does not exist.", DialogFn, 0, 0);
        }
    }
#endif
    if (ImGui::Button("Load unit"))
    {
        // change current unit
        strcpy_s((char*)GAMETRACKER_BASE_AREA, MAX_UNIT_LEN, unit);
        Game::ResetGame(4);
    }

#if TRAE
    ImGui::Text("screen: %d", Game::GetTopScreenID());

    static char screenId[8] = "";
    ImGui::InputText("screen Id", screenId, 8);
    if (ImGui::Button("Push screen"))
    {
        int screen = std::atoi(screenId);

        Game::PushScreen(screen, 0);
    }
#endif

    if (ImGui::Button("Clear")) {
        this->logBuffer.clear();
    }

    if (ImGui::Button("List units"))
    {
#if TRAE
        auto unitList = *(DWORD*)0x8AF44C;
#elif TR8
        auto unitList = *(DWORD*)0xDBA188;
#elif TR7
        auto unitList = *(DWORD*)0x1162C54;
#endif
        auto numUnits = *(int*)unitList;
        Log("numUnits: %d\n", numUnits);

        int offset = 0;
        int count = 0;
        while (true)
        {
            Log("%s\n", (char*)(unitList + 4 + offset));

#if TRAE || TR7
            offset += 20;
#elif TR8
            offset += 132;
#endif
            count++;
            if (count >= numUnits) break;
        }

        ImGui::SetClipboardText(this->logBuffer.begin());
    }

    if (ImGui::Button("List instances"))
    {
#if TRAE
        auto instance = *(DWORD*)0x817D64;
#elif TR7
        auto instance = *(DWORD*)0x10CEE64;
#elif TR8
        auto instance = *(DWORD*)0xD98D54;
#endif
        while (1)
        {
            auto next = *(DWORD*)(instance + 8);

#if TRAE || TR7
            auto object = *(DWORD*)(instance + 0x94);
            auto name = (char*)*(DWORD*)(object + 0x48);
            auto intro = *(int*)(instance + 0x1D0);
#elif TR8
            auto object = *(DWORD*)(instance + 0x10);
            auto name = (char*)*(DWORD*)(object + 0x60);
            auto intro = *(int*)(instance + 0x58);
#endif
            Log("%s - %d\n", name, intro);

            if (!next)
                break;

            instance = next;
        }

        ImGui::SetClipboardText(this->logBuffer.begin());
    }

    auto player = *reinterpret_cast<Instance**>(PLAYERINSTANCE);
#if TRAE
    if (ImGui::Button("Trigger All Fade Groups"))
    {
        for (int i = 0; i <= 28; i++)
        {
            Game::TriggerUiFadeGroup(i);
        }

        Game::SetGameValue(126, 10.0, 1);
    }

    if (ImGui::Button("Turn Lara into gold anim"))
    {
        auto player = *reinterpret_cast<Instance**>(PLAYERINSTANCE);
        int v11 = 52;

        Game::InstancePost(player, 262158, 1);

        auto anim = Game::AnimDataSomething((int)player, (int)player, (int)&v11);
        Game::InstancePost(player, 262147, anim);
        Game::InstanceSetEventAnimPlaying(player, 0);
        Game::PlayerTurnGold();
    }

    if (ImGui::Button("Real gold lara"))
    {
        Game::PlayerTurnGold();
    }

    if (ImGui::Button("Give all weapons"))
    {
        for (int i = 0; i < 3; i++)
        {
            Game::InstancePost(player, 262256, i);
        }
    }

    static int item;
    ImGui::InputInt("itemId", &item);
    if (ImGui::Button("Give item"))
    {
        Game::InstancePost(player, 262256, item );
    }
#endif

    static char name[100] = "";
    ImGui::InputText("name", name, 100);
    if (ImGui::Button("Birth instance"))
    {
        if (Game::GetObjectID(name) == 0)
        {
            Log("Failed to load '%s', make sure it exists.\nIf you are trying to birth a new object make sure you added it to objectlist.txt\nSee README FAQ for more info\n", name);
        }
        else
        {
            auto position = player->position;
            auto rotation = player->rotation;

#if TRAE
            auto unitId = *(int*)0x838418;
#elif TR7
            auto unitId = *(int*)(GAMETRACKER + 0xE8);
#elif TR8
            auto unitId = *(int*)((*(int*)PLAYERINSTANCE) + 224);
#endif

            auto tracker = Stream::GetObjectTrackerByName(name);
            while (tracker->status != 2 && Stream::PollLoadQueue());

            Game::BirthObjectNoParent(unitId, &position, &rotation, nullptr, tracker->object, 0, 1);
        }
    }

#if TRAE
    static char outfit[100] = "";
    ImGui::InputText("outfit", outfit, 100);
    if (ImGui::Button("Load outfit"))
    {
        switchPlayerNextFrame = true;

        auto obj = Game::GetObjectID(outfit);
        *(int*)0x838768 /* alt player object */ = obj;
    }
#endif

    ImGui::End();

    ImGui::Begin("Log", nullptr);

    // log window (based on imgui_demo log window)
    ImGui::BeginChild("LogRegion", ImVec2(), true);

    ImGui::TextUnformatted(this->logBuffer.begin());
    ImGui::SetScrollHere(1.0f);
    ImGui::EndChild();

    ImGui::End();
}

void DrawInstanceViewer()
{
    static DWORD clickedInstance;

    ImGui::Begin("Instances");

    ImGui::Columns(2, "instances");

#if TRAE
    auto instance = *(DWORD*)0x817D64;
#elif TR7
    auto instance = *(DWORD*)0x10CEE64;
#elif TR8
    auto instance = *(DWORD*)0xD98D54;
#endif

    ImGui::BeginChild("InstanceListTree");
    if (instance)
    {
        while (1)
        {
            auto next = *(DWORD*)(instance + 8);

#if TRAE || TR7
            auto object = *(DWORD*)(instance + 0x94);
            auto name = (char*)*(DWORD*)(object + 0x48);
            auto intro = *(int*)(instance + 0x1D0);
#elif TR8
            auto object = *(DWORD*)(instance + 0x10);
            auto name = (char*)*(DWORD*)(object + 0x60);
            auto intro = *(int*)(instance + 0x58);
#endif

            if (ImGui::TreeNodeEx((void*)object, ImGuiTreeNodeFlags_Leaf, "%d %s", intro, name))
            {
                if (ImGui::IsItemClicked())
                {
                    clickedInstance = instance;
                }

                ImGui::TreePop();
            }

            if (!next)
                break;

            instance = next;
        }
    }
    ImGui::EndChild();

    ImGui::NextColumn();

    if (clickedInstance && *(int*)clickedInstance == 0)
    {
        // points to nullptr remove
        clickedInstance = 0;
    }

    if (clickedInstance)
    {
        auto oInstance = (Instance*)clickedInstance;

#if TRAE || TR7
        auto object = *(DWORD*)(clickedInstance + 0x94);
        auto data = *(DWORD*)(clickedInstance + 448);
        auto extraData = *(DWORD*)(clickedInstance + 572);

        auto numAnims = *(__int16*)(object + 0x1A);
        auto numModels = *(__int16*)(object + 0x18);

        auto name = (char*)*(DWORD*)(object + 0x48);
        auto intro = *(int*)(clickedInstance + 0x1D0);
#elif TR8
        auto object = *(DWORD*)(clickedInstance + 0x10);
        auto data = *(DWORD*)(clickedInstance + 80);

        auto numAnims = *(__int16*)(object + 0x38);
        auto numModels = *(__int16*)(object + 0x3A);

        auto name = (char*)*(DWORD*)(object + 0x60);
        auto intro = *(int*)(clickedInstance + 0x58);
#endif

        ImGui::Text("%s", name);

        auto coords = oInstance->position;
        auto rotation = oInstance->rotation;

        ImGui::Text("Position: %f %f %f", coords.x, coords.y, coords.z);
        ImGui::Text("Rotation: %f %f %f", rotation.x, rotation.y, rotation.z);

        ImGui::Text("Intro: %d", intro);

        if (data)
        {
            ImGui::Text("Family %d", *(unsigned __int16*)(data + 2));
        }

        ImGui::Text("Address %p", clickedInstance);

#if TRAE || TR7

        if (extraData)
        {
#if TRAE
            auto health = *(float*)(extraData + 5280);
#elif TR7
            auto health = *(float*)(extraData + 5040);
#endif
            ImGui::Text("Health: %8.2f", health);
        }

        if (ImGui::Button("Switch down"))
        {
            Game::InstancePost((Instance*)clickedInstance, 8388753, 1);
        }
        if (ImGui::Button("Switch up"))
        {
            Game::InstancePost((Instance*)clickedInstance, 8388753, 2);
        }
        ImGui::Text("Switch status: %d", Game::InstanceQuery((Instance*)clickedInstance, 233));
#endif

        auto player = *(Instance**)PLAYERINSTANCE;
        if (ImGui::Button("Goto"))
        {
            player->position = oInstance->position;
        }

        if (ImGui::Button("Bring"))
        {
            oInstance->position = player->position;
        }

        if (ImGui::Button("Delete"))
        {
            INSTANCE_ReallyRemoveInstance((Instance*)clickedInstance, 0, 0);
        }

        ImGui::Text("numModels %d", numModels);

        static int model;
        ImGui::InputInt("model index", &model);
        if (ImGui::Button("Set model"))
        {
            INSTANCE_SetModel(oInstance, model);
        }

        ImGui::Text("numAnims %d", numAnims);

        static int anim;
        ImGui::InputInt("anim", &anim);
        if (ImGui::Button("Play anim"))
        {
            G2EmulationInstanceSetAnimation(oInstance, 0, anim, 0, 0);
            G2EmulationInstanceSetMode(oInstance, 0, 2);
        }

#if TR8
        static int maxList;

        if (ImGui::Button("List anims"))
        {
            auto animList = *(int*)(object + 0x44);
            for (int i = 0; i < maxList; i++)
            {
                auto anim = *(char**)(animList + (i * 0x0c) + 0x08);

                Hooking::GetInstance().GetMenu()->Log("%d %s\n", i, anim);
            }
        }
        ImGui::SameLine();
        ImGui::InputInt("num anims", &maxList);
#endif

#if TRAE || TR7
        auto modelList = *(int*)(object + 0x20);
        for (int i = 0; i < numModels; i++)
        {
            auto model = **(Model**)(modelList + (i * 4));

            char label[10];
            sprintf_s(label, "model %d", i);

            if (ImGui::CollapsingHeader(label))
            {
                ImGui::Text("version %d", model.version);
                ImGui::Text("numSegments %d", model.numSegments);
                ImGui::Text("numVirtSegments %d", model.numVirtSegments);
                ImGui::Text("numVertices %d", model.numVertices);
                ImGui::Text("numNormals %d", model.numNormals);
                ImGui::Text("numFaces %d", model.numFaces);
            }
        }
#endif
    }

    ImGui::End();
}

void Menu::Log(const char* fmt, ...) IM_FMTARGS(2)
{
    va_list args;
    va_start(args, fmt);
    this->logBuffer.appendfv(fmt, args);
    va_end(args);
}

bool Menu::IsFocus() const noexcept
{
    return m_focus;
}

void Menu::SetFocus(bool value) noexcept
{
    m_focus = value;
}

void Menu::OnLayoutChange() noexcept
{
    // thanks to Xwilarg
    auto hkl = GetKeyboardLayout(0);
    auto id = (int)((unsigned int)hkl & 0x0000FFFF);
    m_isAzertyLayout = id == 2060 || id == 1036;
}
