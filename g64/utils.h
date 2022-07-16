#pragma once
#include <string>
#include <iostream>
#include <filesystem>
#include <Windows.h>

extern "C"
{
#include "libsm64.h"
}

using namespace std;

struct float4 {
    float x, y, z, w;
    float4(float x1, float y1, float z1, float w1) : x(x1), y(y1), z(z1), w(w1) {};
    float4() : x(0), y(0), z(0), w(0) {};
    float4(float l) : x(l), y(l), z(l), w(l) {};
    float4(Vector l, float w1) : x(l.x), y(l.y), z(l.z), w(w1) {};
    float4 operator+(float4 other) {
        x = other.x;
        y = other.y;
        z = other.z;
        w = other.w;
    }
};

string exe_path();
int version_compare(string v1, string v2);
void run_updater_script();
QAngle angle_from_quaternion(float x, float y, float z, float w);
float4 quatFromAngle(QAngle ang);

extern SM64TextureAtlasInfo mario_atlas_info;
extern SM64TextureAtlasInfo coin_atlas_info;
extern SM64TextureAtlasInfo ui_atlas_info;
extern SM64TextureAtlasInfo health_atlas_info;