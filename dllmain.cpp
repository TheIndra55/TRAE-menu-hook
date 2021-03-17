#include "Hooking.hpp"

Hooking* g_hooking;

DWORD WINAPI Hook(LPVOID lpParam)
{
    g_hooking = new Hooking();
    g_hooking->Initialize();

    while (true) Sleep(0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            CreateThread(nullptr, 0, Hook, NULL, 0, NULL);
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}

