#include "MyMath.h"

// ============================================================
// worldToScreen — теперь возвращает false только если точка
// ЗА камерой. Выход за границы экрана — проблема вызывающего.
// ============================================================
bool worldToScreen(
    Vector3 worldPos, Vector3 camPos,
    float pitch, float yaw, float roll,
    float fovDeg, float screenW, float screenH,
    Vector2& out)
{
    float P = pitch * (PI / 180.f);
    float Y = yaw * (PI / 180.f);
    float R = roll * (PI / 180.f);

    float SP = sinf(P), CP = cosf(P);
    float SY = sinf(Y), CY = cosf(Y);
    float SR = sinf(R), CR = cosf(R);

    Vector3 axisX = { CP * CY, CP * SY, SP };
    Vector3 axisY = { SR * SP * CY - CR * SY,
                      SR * SP * SY + CR * CY,
                      -SR * CP };

    Vector3 axisZ = {
        axisX.y * axisY.z - axisX.z * axisY.y,
        axisX.z * axisY.x - axisX.x * axisY.z,
        axisX.x * axisY.y - axisX.y * axisY.x
    };

    Vector3 delta = {
        worldPos.x - camPos.x,
        worldPos.y - camPos.y,
        worldPos.z - camPos.z
    };

    float depth = (float)(delta.x * axisX.x + delta.y * axisX.y + delta.z * axisX.z);
    float right = (float)(delta.x * axisY.x + delta.y * axisY.y + delta.z * axisY.z);
    float up = (float)(delta.x * axisZ.x + delta.y * axisZ.y + delta.z * axisZ.z);

    // Только одна причина для false — точка ЗА камерой
    if (depth <= 0.f) return false;

    float focal = (screenW * 0.5f) / tanf(fovDeg * (PI / 360.f));

    out.x = (screenW * 0.5f) + focal * (right / depth);
    out.y = (screenH * 0.5f) - focal * (up / depth);

    // true даже если off-screen — клипать будет вызывающий
    return true;
}