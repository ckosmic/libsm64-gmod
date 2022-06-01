#pragma once
#include "GarrysMod/Lua/Interface.h"

struct float4
{
    float4()
        : x(0.f)
        , y(0.f)
        , z(0.f)
        , w(0.f)
    {}

    float4(const float4& src)
        : x(src.x)
        , y(src.y)
        , z(src.z)
        , w(src.w)
    {}

    float4(const float sX, const float sY, const float sZ, const float sW)
        : x(sX)
        , y(sY)
        , z(sZ)
        , w(sW)
    {}

    float4& operator=(const float4& src)
    {
        x = src.x;
        y = src.y;
        z = src.z;
        w = src.w;
        return *this;
    }

    float x, y, z, w;
};

QAngle angleFromQuat(float4 q);
float4 quatFromAngle(QAngle ang);