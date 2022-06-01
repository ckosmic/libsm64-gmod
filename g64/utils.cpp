#include "utils.h"

//Quat library provided by potatoOS, turns a euler angle into a quaterion
#define _PI 3.14159265358979323846
float rad(float degree) {
    return (degree * (_PI / 180));
}
float deg(float radian) {
    return (radian * (180 / _PI));
}

float4 getQuatMul(float4 lhs, float4 rhs) {
    return float4(
        lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z - lhs.w * rhs.w,
        lhs.x * rhs.y + lhs.y * rhs.x + lhs.z * rhs.w - lhs.w * rhs.z,
        lhs.x * rhs.z + lhs.z * rhs.x + lhs.w * rhs.y - lhs.y * rhs.w,
        lhs.x * rhs.w + lhs.w * rhs.x + lhs.y * rhs.z - lhs.z * rhs.y
    );
}

float4 quatFromAngleComponents(float p, float y, float r) {
    p = rad(p) * 0.5f;
    y = rad(y) * 0.5f;
    r = rad(r) * 0.5f;

    float4 q_x = float4((float)cos(y), 0, 0, (float)sin(y));
    float4 q_y = float4((float)cos(p), 0, (float)sin(p), 0);
    float4 q_z = float4((float)cos(r), (float)sin(r), 0, 0);

    return getQuatMul(q_x, getQuatMul(q_y, q_z));
}

float4 unfuckQuat(float4 q) {
    return float4(q.y, q.z, q.w, q.x);
}

float4 quatFromAngle(QAngle ang) {
    return unfuckQuat(quatFromAngleComponents(ang.x, ang.y, ang.z));
}

QAngle angleFromQuat(float4 q) {
    QAngle angles;

    float sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
    angles.z = deg(atan2(sinr_cosp, cosr_cosp));

    float sinp = 2 * (q.w * q.y - q.z * q.x);
    if (abs(sinp) >= 1)
    {
        angles.x = deg(copysign(_PI / 2, sinp));
    }
    else
    {
        angles.x = deg(asin(sinp));
    }

    float siny_cosp = 2 * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
    angles.y = deg(atan2(siny_cosp, cosy_cosp));

    return angles;
}