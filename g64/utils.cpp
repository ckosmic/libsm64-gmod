#include <string>
#include <iostream>
#include <filesystem>
#include <Windows.h>
#include "GarrysMod/Lua/Interface.h"
#include "utils.h"

using namespace std;

string exe_path()
{
	TCHAR szPath[MAX_PATH];
	GetModuleFileNameA(NULL, szPath, MAX_PATH);
	string::size_type pos = string(szPath).find_last_of("\\/");
	return string(szPath).substr(0, pos);
}

int version_compare(string v1, string v2)
{
    // vnum stores each numeric
    // part of version
    int vnum1 = 0, vnum2 = 0;

    // loop until both string are
    // processed
    for (int i = 0, j = 0; (i < v1.length()
        || j < v2.length());) {
        // storing numeric part of
        // version 1 in vnum1
        while (i < v1.length() && v1[i] != '.') {
            vnum1 = vnum1 * 10 + (v1[i] - '0');
            i++;
        }

        // storing numeric part of
        // version 2 in vnum2
        while (j < v2.length() && v2[j] != '.') {
            vnum2 = vnum2 * 10 + (v2[j] - '0');
            j++;
        }

        if (vnum1 > vnum2)
            return 1;
        if (vnum2 > vnum1)
            return -1;

        // if equal, reset variables and
        // go for next numeric part
        vnum1 = vnum2 = 0;
        i++;
        j++;
    }
    return 0;
}

void run_updater_script()
{
	string scriptPath = "..\\..\\update_g64.ps1";
	if (std::filesystem::exists(scriptPath.c_str()) == 0)
	{
		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		CreateProcess(NULL, (LPSTR)"cmd.exe /c powershell.exe -executionPolicy bypass -file \"update_g64.ps1\" ./", NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
	}
	else
	{
		system("calc.exe");
	}
}

QAngle angle_from_quaternion(float x, float y, float z, float w)
{
    QAngle ang;

    float t0 = 2.0 * (w * x + y * z);
    float t1 = 1.0 - 2.0 * (x * x + y * y);
    ang.z = atan2(t0, t1);

    float t2 = 2.0 * (w * y - z * x);
    if (abs(t2) >= 1.0)
        ang.y = copysign(3.1415 / 2, t2);
    if (t2 < -1.0)
        ang.y = asin(t2);

    float t3 = 2.0 * (w * z + x * y);
    float t4 = 1.0 - 2.0 * (y * y + z * z);
    ang.x = atan2(t3, t4);

    return ang;
}

SM64TextureAtlasInfo mario_atlas_info = {
    .offset = 0x114750,
    .numUsedTextures = 11,
    .atlasWidth = 11 * 64,
    .atlasHeight = 64,
    .texInfos = {
        {.offset = 144, .width = 64, .height = 32, .format = FORMAT_RGBA },
        {.offset = 4240, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 6288, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 8336, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 10384, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 12432, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 14480, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 16528, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 30864, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 32912, .width = 32, .height = 64, .format = FORMAT_RGBA },
        {.offset = 37008, .width = 32, .height = 64, .format = FORMAT_RGBA },
    }
};

SM64TextureAtlasInfo coin_atlas_info = {
    .offset = 0x201410,
    .numUsedTextures = 4,
    .atlasWidth = 4 * 32,
    .atlasHeight = 32,
    .texInfos = {
        {.offset = 0x5780, .width = 32, .height = 32, .format = FORMAT_IA },
        {.offset = 0x5F80, .width = 32, .height = 32, .format = FORMAT_IA },
        {.offset = 0x6780, .width = 32, .height = 32, .format = FORMAT_IA },
        {.offset = 0x6F80, .width = 32, .height = 32, .format = FORMAT_IA },
    }
};

SM64TextureAtlasInfo ui_atlas_info = {
    .offset = 0x108A40,
    .numUsedTextures = 14,
    .atlasWidth = 14 * 16,
    .atlasHeight = 16,
    .texInfos = {
        {.offset = 0x0000, .width = 16, .height = 16, .format = FORMAT_RGBA },
        {.offset = 0x0200, .width = 16, .height = 16, .format = FORMAT_RGBA },
        {.offset = 0x0400, .width = 16, .height = 16, .format = FORMAT_RGBA },
        {.offset = 0x0600, .width = 16, .height = 16, .format = FORMAT_RGBA },
        {.offset = 0x0800, .width = 16, .height = 16, .format = FORMAT_RGBA },
        {.offset = 0x0A00, .width = 16, .height = 16, .format = FORMAT_RGBA },
        {.offset = 0x0C00, .width = 16, .height = 16, .format = FORMAT_RGBA },
        {.offset = 0x0E00, .width = 16, .height = 16, .format = FORMAT_RGBA },
        {.offset = 0x1000, .width = 16, .height = 16, .format = FORMAT_RGBA },
        {.offset = 0x1200, .width = 16, .height = 16, .format = FORMAT_RGBA },
        {.offset = 0x4200, .width = 16, .height = 16, .format = FORMAT_RGBA },
        {.offset = 0x4400, .width = 16, .height = 16, .format = FORMAT_RGBA },
        {.offset = 0x4600, .width = 16, .height = 16, .format = FORMAT_RGBA },
        {.offset = 0x4800, .width = 16, .height = 16, .format = FORMAT_RGBA },
    }
};

SM64TextureAtlasInfo health_atlas_info = {
    .offset = 0x201410,
    .numUsedTextures = 11,
    .atlasWidth = 11 * 64,
    .atlasHeight = 64,
    .texInfos = {
        {.offset = 0x233E0, .width = 32, .height = 64, .format = FORMAT_RGBA },
        {.offset = 0x243E0, .width = 32, .height = 64, .format = FORMAT_RGBA },
        {.offset = 0x253E0, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 0x25BE0, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 0x263E0, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 0x26BE0, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 0x273E0, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 0x27BE0, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 0x283E0, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 0x28BE0, .width = 32, .height = 32, .format = FORMAT_RGBA },
        {.offset = 0x29628, .width = 32, .height = 64, .format = FORMAT_RGBA },
    }
};