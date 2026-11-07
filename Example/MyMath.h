#pragma once
#include <cmath>
#define PI 3.14159265358979323846

struct Vector2 {
    double x, y;
    Vector2() : x(0), y(0) {}
    Vector2(double _x, double _y) : x(_x), y(_y) {}
};

struct Vector3 { // Pitch, Yaw, Roll
    double x, y, z;
    Vector3() : x(0), y(0), z(0) {}
    Vector3(double _x, double _y, double _z) : x(_x), y(_y), z(_z) {}
};

bool worldToScreen(
    Vector3 worldPos, Vector3 camPos,
    float pitch, float yaw, float roll,
    float fovDeg, float screenW, float screenH,
    Vector2& out
);

