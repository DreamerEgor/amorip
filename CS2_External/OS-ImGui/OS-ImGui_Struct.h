#pragma once
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_internal.h"
#include <d3d11.h>
#include <cmath>
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"dxgi.lib")

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Vec2
{
public:
    float x = 0.f, y = 0.f;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2(ImVec2 v) : x(v.x), y(v.y) {}
    Vec2& operator=(ImVec2 v) { x = v.x; y = v.y; return *this; }
    Vec2 operator+(const Vec2& v) const { return { x + v.x, y + v.y }; }
    Vec2 operator-(const Vec2& v) const { return { x - v.x, y - v.y }; }
    Vec2 operator*(const Vec2& v) const { return { x * v.x, y * v.y }; }
    Vec2 operator/(const Vec2& v) const { return { x / v.x, y / v.y }; }
    Vec2 operator*(float n) const { return { x * n, y * n }; }
    Vec2 operator/(float n) const { return { x / n, y / n }; }
    bool operator==(const Vec2& v) const { return x == v.x && y == v.y; }
    bool operator!=(const Vec2& v) const { return !(*this == v); }
    ImVec2 ToImVec2() const { return ImVec2(x, y); }
    float Length() const { return std::sqrt(x * x + y * y); }
    float DistanceTo(const Vec2& p) const { return (*this - p).Length(); }
};

class Vec3
{
public:
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& v) const { return { x + v.x, y + v.y, z + v.z }; }
    Vec3 operator-(const Vec3& v) const { return { x - v.x, y - v.y, z - v.z }; }
    Vec3 operator*(const Vec3& v) const { return { x * v.x, y * v.y, z * v.z }; }
    Vec3 operator/(const Vec3& v) const { return { x / v.x, y / v.y, z / v.z }; }
    Vec3 operator*(float n) const { return { x * n, y * n, z * n }; }
    Vec3 operator/(float n) const { return { x / n, y / n, z / n }; }
    bool operator==(const Vec3& v) const { return x == v.x && y == v.y && z == v.z; }
    bool operator!=(const Vec3& v) const { return !(*this == v); }
    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    float DistanceTo(const Vec3& p) const { return (*this - p).Length(); }
};

template <typename T>
class Singleton
{
public:
    static T& get() { static T instance; return instance; }
};
