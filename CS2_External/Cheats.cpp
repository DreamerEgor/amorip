#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "Cheats.h"
#include "Game.h"
#include "Entity.h"
#include "AimBot.hpp"
#include "TriggerBot.h"
#include "BunnyHop.hpp"
#include "Render.hpp"
#include "MenuConfig.hpp"
#include "MenuWidgets.hpp"
#include "Utils/ConfigMenu.hpp"
#include "OS-ImGui/OS-ImGui.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>
#include <chrono>
#include <string>
#include <cstdio>
#include <cstring>
#include <vector>
#include <sstream>
#include <unordered_set>
#include <utility>


namespace
{
    int g_tab = 0;
    bool g_tabScrubActive = false;
    int g_tabScrubCandidate = -1;
    double g_tabScrubCandidateSince = 0.0;
    float g_tabVisual = 0.0f;
    std::vector<std::string> gSpectatorPreviewNames;
    int gCurrentPing = -1;

    constexpr float kUiRounding = 1.0f;

    constexpr float kMenuWidth = 680.f;
    constexpr float kMenuHeight = 460.f;
    constexpr float kHeaderHeight = 35.f;
    constexpr float kFooterHeight = 25.f;
    constexpr float kOuterPad = 5.f;
    constexpr float kGroupGap = 5.f;
    constexpr float kSidebarWidth = 90.f;
    constexpr float kContentX = kSidebarWidth + kOuterPad;

    ImVec4 CurrentAccent()
    {
        ImVec4 accent = MenuConfig::MenuAccentColor.Value;
        accent.w = 1.f;
        return accent;
    }

    ImU32 CurrentAccentU32()
    {
        return ImGui::ColorConvertFloat4ToU32(CurrentAccent());
    }

    void DrawCenteredAccentGradient(ImDrawList* draw, const ImVec2& min, const ImVec2& max, float height = 2.f)
    {
        if (!draw || max.x <= min.x || height <= 0.f)
            return;

        ImVec4 accent = CurrentAccent();
        accent.w = 1.f;
        ImVec4 edge = accent;
        edge.w = 0.f;
        const ImU32 strong = ImGui::ColorConvertFloat4ToU32(accent);
        const ImU32 transparent = ImGui::ColorConvertFloat4ToU32(edge);
        const float centerX = (min.x + max.x) * 0.5f;
        const float y2 = std::min(max.y, min.y + height);

        // Fade in toward the center, then fade back out toward the right edge.
        draw->AddRectFilledMultiColor(
            min, ImVec2(centerX, y2),
            transparent, strong, strong, transparent);
        draw->AddRectFilledMultiColor(
            ImVec2(centerX, min.y), ImVec2(max.x, y2),
            strong, transparent, transparent, strong);
    }
    const ImU32 kBodyU32 = IM_COL32(20, 20, 20, 255);
    const ImU32 kHeaderU32 = IM_COL32(25, 25, 25, 255);
    const ImU32 kOutlineU32 = IM_COL32(16, 16, 16, 255);
    const ImU32 kGroupU32 = IM_COL32(22, 22, 22, 255);
    const ImU32 kWidgetU32 = IM_COL32(19, 19, 19, 255);
    const ImU32 kWidgetDarkU32 = IM_COL32(14, 14, 14, 255);
    const ImU32 kTextU32 = IM_COL32(220, 220, 220, 255);
    const ImU32 kMutedU32 = IM_COL32(110, 110, 110, 255);

    void DrawStatusCounter()
    {
        if (!MenuConfig::ShowStatusCounter || MenuConfig::PanicVisuals)
            return;

        const ImGuiIO& io = ImGui::GetIO();
        const int fps = io.Framerate > 0.f ? static_cast<int>(std::lround(io.Framerate)) : 0;

        std::vector<std::string> parts;
        if (MenuConfig::StatusShowBrand)
            parts.emplace_back("amor.rip");
        if (MenuConfig::StatusShowUser)
            parts.emplace_back("Admin");
        if (MenuConfig::StatusShowFps)
            parts.emplace_back("fps: " + std::to_string(fps));
        if (MenuConfig::StatusShowPing)
            parts.emplace_back(gCurrentPing >= 0 ? ("ping: " + std::to_string(gCurrentPing) + " ms") : "ping: --");

        if (parts.empty())
            return;

        std::ostringstream textBuilder;
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            if (i) textBuilder << " | ";
            textBuilder << parts[i];
        }
        const std::string text = textBuilder.str();

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        // Slightly larger than the old compact counter so it reads more like a
        // proper status badge without becoming visually dominant.
        const float fontSize = std::max(12.f, ImGui::GetFontSize() * 1.05f);
        const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text.c_str());
        const ImVec2 pad(8.f, 3.5f);
        const ImVec2 display = io.DisplaySize;
        const ImVec2 max(display.x - 10.f, 10.f + textSize.y + pad.y * 2.f);
        const ImVec2 min(max.x - textSize.x - pad.x * 2.f, 10.f);

        draw->AddRectFilled(min, max, IM_COL32(12, 16, 16, 225), kUiRounding);
        DrawCenteredAccentGradient(draw, min, max, 2.f);
        draw->AddText(font, fontSize, ImVec2(min.x + pad.x, min.y + pad.y), kTextU32, text.c_str());
    }

    void DrawSpectatorPanel()
    {
        if (!MenuConfig::ShowSpectatorList || MenuConfig::PanicVisuals)
            return;

        // Live spectator names are refreshed from Source 2 observer services in
        // the player scan below. The preview hook remains useful for UI testing.
        const ImGuiIO& io = ImGui::GetIO();
        const float headerH = 18.f;
        const float rowH = 17.f;
        const float sidePad = 7.f;
        const float defaultTop = MenuConfig::ShowStatusCounter ? 42.f : 10.f;

        float width = 160.f;
        for (const std::string& name : gSpectatorPreviewNames)
            width = std::max(width, ImGui::CalcTextSize(name.c_str()).x + sidePad * 2.f);
        width = std::clamp(width, 160.f, 280.f);

        const float bodyH = gSpectatorPreviewNames.empty() ? 0.f : (4.f + rowH * static_cast<float>(gSpectatorPreviewNames.size()) + 3.f);
        const float height = headerH + bodyH;

        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 10.f - width, defaultTop), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing;
        if (!MenuConfig::ShowMenu)
            flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        if (ImGui::Begin("##amor_spectator_panel", nullptr, flags))
        {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 min = ImGui::GetWindowPos();
            const ImVec2 max(min.x + width, min.y + height);
            const ImVec2 headerMax(min.x + width, min.y + headerH);

            draw->AddRectFilled(min, max, IM_COL32(12, 16, 16, 225), kUiRounding);
            DrawCenteredAccentGradient(draw, min, headerMax, 2.f);

            const char* label = "spectators";
            const ImVec2 ts = ImGui::CalcTextSize(label);
            draw->AddText(ImVec2(min.x + (width - ts.x) * 0.5f, min.y + (headerH - ts.y) * 0.5f), kTextU32, label);

            float y = min.y + headerH + 4.f;
            for (const std::string& name : gSpectatorPreviewNames)
            {
                const char* text = name.empty() ? "(unnamed)" : name.c_str();
                draw->AddText(ImVec2(min.x + sidePad, y), kTextU32, text);
                y += rowH;
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void DrawSniperCrosshairOverlay()
    {
        if (!MenuConfig::SniperCrosshair || MenuConfig::PanicVisuals)
            return;

        const ImGuiIO& io = ImGui::GetIO();
        const ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        const float size = std::clamp(MenuConfig::SniperCrosshairSize, 1.f, 40.f);
        const float gap = std::clamp(MenuConfig::SniperCrosshairGap, 0.f, 30.f);
        const float thickness = std::clamp(MenuConfig::SniperCrosshairThickness, 0.5f, 8.f);

        ImVec4 color = MenuConfig::SniperCrosshairColor.Value;
        color.w *= std::clamp(MenuConfig::SniperCrosshairOpacity, 0.f, 1.f);
        const ImU32 u32 = ImGui::ColorConvertFloat4ToU32(color);
        ImDrawList* draw = ImGui::GetForegroundDrawList();

        draw->AddLine(ImVec2(c.x - gap - size, c.y), ImVec2(c.x - gap, c.y), u32, thickness);
        draw->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + gap + size, c.y), u32, thickness);
        draw->AddLine(ImVec2(c.x, c.y - gap - size), ImVec2(c.x, c.y - gap), u32, thickness);
        draw->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + gap + size), u32, thickness);

        if (MenuConfig::SniperCrosshairCenterDot)
        {
            const float radius = std::clamp(MenuConfig::SniperCrosshairDotSize, 1.f, 12.f) * 0.5f;
            draw->AddCircleFilled(c, radius, u32);
        }
    }

    bool PressedOnce(int vk, bool& previous, int& previousKey)
    {
        if (vk <= 0 || vk > 255)
        {
            previous = false;
            previousKey = vk;
            return false;
        }
        if (previousKey != vk)
        {
            previousKey = vk;
            previous = (GetAsyncKeyState(vk) & 0x8000) != 0;
            return false;
        }
        const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool pressed = down && !previous;
        previous = down;
        return pressed;
    }

    const char* VisibleLabel(const char* label, std::string& storage)
    {
        const char* hash = std::strstr(label, "##");
        if (!hash)
            return label;
        storage.assign(label, hash);
        return storage.c_str();
    }

    void ApplyMenuStyle()
    {
        static bool initialized = false;
        ImGuiStyle& s = ImGui::GetStyle();

        if (!initialized)
        {
            initialized = true;
            ImGui::GetIO().ConfigWindowsResizeFromEdges = true;
            s.WindowRounding = 5.f;
            s.ChildRounding = 4.f;
            s.FrameRounding = 4.f;
            s.PopupRounding = 4.f;
            s.ScrollbarRounding = 4.f;
            s.GrabRounding = 4.f;
            s.WindowPadding = ImVec2(0.f, 0.f);
            s.FramePadding = ImVec2(7.f, 4.f);
            s.ItemSpacing = ImVec2(5.f, 5.f);
            s.ItemInnerSpacing = ImVec2(5.f, 4.f);
            s.WindowBorderSize = 1.f;
            s.ChildBorderSize = 0.f;
            s.FrameBorderSize = 0.f;

            auto& c = s.Colors;
            c[ImGuiCol_WindowBg] = ImVec4(20.f / 255.f, 20.f / 255.f, 20.f / 255.f, 1.f);
            c[ImGuiCol_ChildBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
            c[ImGuiCol_PopupBg] = ImVec4(22.f / 255.f, 22.f / 255.f, 22.f / 255.f, 0.99f);
            c[ImGuiCol_Border] = ImVec4(16.f / 255.f, 16.f / 255.f, 16.f / 255.f, 1.f);
            c[ImGuiCol_FrameBg] = ImVec4(19.f / 255.f, 19.f / 255.f, 19.f / 255.f, 1.f);
            c[ImGuiCol_FrameBgHovered] = ImVec4(24.f / 255.f, 24.f / 255.f, 24.f / 255.f, 1.f);
            c[ImGuiCol_FrameBgActive] = ImVec4(32.f / 255.f, 32.f / 255.f, 32.f / 255.f, 1.f);
            c[ImGuiCol_Button] = ImVec4(19.f / 255.f, 19.f / 255.f, 19.f / 255.f, 1.f);
            c[ImGuiCol_ButtonHovered] = ImVec4(32.f / 255.f, 32.f / 255.f, 32.f / 255.f, 1.f);
            c[ImGuiCol_ButtonActive] = ImVec4(38.f / 255.f, 38.f / 255.f, 38.f / 255.f, 1.f);
            c[ImGuiCol_Header] = ImVec4(32.f / 255.f, 32.f / 255.f, 32.f / 255.f, 1.f);
            c[ImGuiCol_HeaderHovered] = ImVec4(38.f / 255.f, 38.f / 255.f, 38.f / 255.f, 1.f);
            c[ImGuiCol_HeaderActive] = ImVec4(42.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f);
            c[ImGuiCol_Text] = ImVec4(220.f / 255.f, 220.f / 255.f, 220.f / 255.f, 1.f);
            c[ImGuiCol_TextDisabled] = ImVec4(110.f / 255.f, 110.f / 255.f, 110.f / 255.f, 1.f);
            c[ImGuiCol_Separator] = ImVec4(16.f / 255.f, 16.f / 255.f, 16.f / 255.f, 1.f);
        }

        // Accent-backed ImGui widgets update live when the color picker changes.
        auto& c = s.Colors;
        const ImVec4 accent = CurrentAccent();
        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accent;
    }

    bool ClassicTab(const char* label, int id, float y)
    {
        constexpr float tabX = 5.f;
        constexpr float tabWidth = kSidebarWidth - 10.f;
        constexpr float tabHeight = 29.f;

        const ImVec2 textSize = ImGui::CalcTextSize(label);
        ImGui::SetCursorPos(ImVec2(tabX, y));
        ImGui::PushID(id);
        const bool clicked = ImGui::InvisibleButton("##tab", ImVec2(tabWidth, tabHeight));
        const ImVec2 p = ImGui::GetItemRectMin();
        const ImVec2 pMax(p.x + tabWidth, p.y + tabHeight);

        // Once M1 is pressed on a sidebar tab, keep the tab strip in scrub
        // mode until release. Moving onto a different tab now has a very short
        // dwell before the page changes so fast mouse jitter does not snap
        // through pages instantly. The active highlight itself is animated in
        // Menu() below.
        const bool mouseOver = ImGui::IsMouseHoveringRect(p, pMax, false);
        const bool pressedHere = mouseOver && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        if (pressedHere)
        {
            g_tab = id;
            g_tabScrubActive = true;
            g_tabScrubCandidate = id;
            g_tabScrubCandidateSince = ImGui::GetTime();
        }
        else if (g_tabScrubActive && ImGui::IsMouseDown(ImGuiMouseButton_Left) && mouseOver)
        {
            if (g_tabScrubCandidate != id)
            {
                g_tabScrubCandidate = id;
                g_tabScrubCandidateSince = ImGui::GetTime();
            }
            else if (id != g_tab && (ImGui::GetTime() - g_tabScrubCandidateSince) >= 0.075)
            {
                g_tab = id;
            }
        }
        else if (clicked && !g_tabScrubActive)
        {
            g_tab = id;
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (mouseOver && g_tab != id)
        {
            draw->AddRectFilled(p, pMax, IM_COL32(27, 27, 27, 255), 4.f);
        }

        const ImU32 textColor = g_tab == id ? CurrentAccentU32() : (mouseOver ? kTextU32 : kMutedU32);
        draw->AddText(ImVec2(p.x + 10.f, p.y + (tabHeight - textSize.y) * 0.5f), textColor, label);
        ImGui::PopID();
        return clicked;
    }

    bool BeginClassicGroup(const char* id, const char* label, const ImVec2& size)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const bool hasHeader = label != nullptr && label[0] != '\0';

        draw->AddRectFilled(ImVec2(p.x + 1.f, p.y + 1.f), ImVec2(p.x + size.x + 1.f, p.y + size.y + 1.f), kWidgetDarkU32, 4.f);
        draw->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), kGroupU32, 4.f);
        if (hasHeader)
        {
            draw->AddRectFilled(p, ImVec2(p.x + size.x, p.y + 25.f), kHeaderU32, 4.f, ImDrawFlags_RoundCornersTop);
            draw->AddText(ImVec2(p.x + 8.f, p.y + 5.f), kTextU32, label);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 6.f));
        const bool open = ImGui::BeginChild(id, size, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::SetCursorPosY(hasHeader ? 31.f : 6.f);
        return open;
    }

    void EndClassicGroup()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    bool ClassicToggle(const char* label, bool* value)
    {
        std::string visibleStorage;
        const char* visible = VisibleLabel(label, visibleStorage);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;

        ImGui::PushID(label);
        ImGui::InvisibleButton("##toggle", ImVec2(width, 17.f));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            *value = !*value;

        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(ImVec2(p.x + 1.f, p.y + 1.f), ImVec2(p.x + 16.f, p.y + 16.f), kWidgetDarkU32, 4.f);
        draw->AddRectFilled(p, ImVec2(p.x + 15.f, p.y + 15.f), *value ? CurrentAccentU32() : kWidgetU32, 4.f);
        if (hovered && !*value)
            draw->AddRect(p, ImVec2(p.x + 15.f, p.y + 15.f), IM_COL32(38, 38, 38, 255), 4.f);
        draw->AddText(ImVec2(p.x + 20.f, p.y - 1.f), *value ? kTextU32 : kMutedU32, visible);
        ImGui::PopID();
        return *value;
    }

    bool ClassicToggleWithKeybind(const char* label, bool* value, const char* keyId, Keybind::Bind& bind, float keyWidth = 92.f)
    {
        std::string visibleStorage;
        const char* visible = VisibleLabel(label, visibleStorage);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const float gap = 5.f;
        const float toggleWidth = std::max(40.f, width - keyWidth - gap);
        constexpr float rowHeight = 22.f;

        ImGui::PushID(label);
        ImGui::InvisibleButton("##toggle", ImVec2(toggleWidth, rowHeight));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            *value = !*value;

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float checkY = p.y + 3.f;
        draw->AddRectFilled(ImVec2(p.x + 1.f, checkY + 1.f), ImVec2(p.x + 16.f, checkY + 16.f), kWidgetDarkU32, 4.f);
        draw->AddRectFilled(ImVec2(p.x, checkY), ImVec2(p.x + 15.f, checkY + 15.f), *value ? CurrentAccentU32() : kWidgetU32, 4.f);
        if (hovered && !*value)
            draw->AddRect(ImVec2(p.x, checkY), ImVec2(p.x + 15.f, checkY + 15.f), IM_COL32(38, 38, 38, 255), 4.f);
        draw->AddText(ImVec2(p.x + 20.f, p.y + 2.f), *value ? kTextU32 : kMutedU32, visible);
        ImGui::PopID();

        // Place the keybind button on the same row, flush right, with no separate
        // "key" label. Its capture/mode behavior is unchanged.
        ImGui::SetCursorScreenPos(ImVec2(p.x + width - keyWidth, p.y));
        MenuWidgets::KeyBindButton(keyId, bind, keyWidth);
        return *value;
    }

    bool ClassicSliderFloat(const char* label, float* value, float minValue, float maxValue, const char* format)
    {
        std::string visibleStorage;
        const char* visible = VisibleLabel(label, visibleStorage);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const float barY = p.y + 20.f;
        char valueText[64]{};
        std::snprintf(valueText, sizeof(valueText), format, *value);
        const ImVec2 valueSize = ImGui::CalcTextSize(valueText);

        ImGui::PushID(label);
        ImGui::InvisibleButton("##slider", ImVec2(width, 32.f));
        const bool active = ImGui::IsItemActive();
        const bool hovered = ImGui::IsItemHovered();
        bool changed = false;
        if (active && ImGui::GetIO().MouseDown[0])
        {
            const float t = std::clamp((ImGui::GetIO().MousePos.x - p.x) / std::max(width, 1.f), 0.f, 1.f);
            const float next = minValue + (maxValue - minValue) * t;
            if (next != *value)
            {
                *value = next;
                changed = true;
            }
        }

        const float t = std::clamp((*value - minValue) / std::max(maxValue - minValue, 0.0001f), 0.f, 1.f);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddText(p, active ? kTextU32 : kMutedU32, visible);
        draw->AddText(ImVec2(p.x + width - valueSize.x, p.y), active ? kTextU32 : kMutedU32, valueText);
        draw->AddRectFilled(ImVec2(p.x + 1.f, barY + 1.f), ImVec2(p.x + width + 1.f, barY + 11.f), kWidgetDarkU32, 4.f);
        draw->AddRectFilled(ImVec2(p.x, barY), ImVec2(p.x + width, barY + 10.f), hovered ? IM_COL32(22, 22, 22, 255) : kWidgetU32, 4.f);
        if (t > 0.f)
            draw->AddRectFilled(ImVec2(p.x, barY), ImVec2(p.x + width * t, barY + 10.f), CurrentAccentU32(), 4.f);
        ImGui::PopID();
        return changed;
    }

    bool ClassicSliderInt(const char* label, int* value, int minValue, int maxValue, const char* format)
    {
        char valueText[64]{};
        std::snprintf(valueText, sizeof(valueText), format, *value);

        std::string visibleStorage;
        const char* visible = VisibleLabel(label, visibleStorage);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const float barY = p.y + 20.f;
        const ImVec2 valueSize = ImGui::CalcTextSize(valueText);

        ImGui::PushID(label);
        ImGui::InvisibleButton("##slider", ImVec2(width, 32.f));
        const bool active = ImGui::IsItemActive();
        const bool hovered = ImGui::IsItemHovered();
        bool changed = false;
        if (active && ImGui::GetIO().MouseDown[0])
        {
            const float t = std::clamp((ImGui::GetIO().MousePos.x - p.x) / std::max(width, 1.f), 0.f, 1.f);
            const int next = static_cast<int>(std::lround(minValue + (maxValue - minValue) * t));
            if (next != *value)
            {
                *value = std::clamp(next, minValue, maxValue);
                changed = true;
            }
        }

        const float t = static_cast<float>(*value - minValue) / static_cast<float>(std::max(maxValue - minValue, 1));
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddText(p, active ? kTextU32 : kMutedU32, visible);
        draw->AddText(ImVec2(p.x + width - valueSize.x, p.y), active ? kTextU32 : kMutedU32, valueText);
        draw->AddRectFilled(ImVec2(p.x + 1.f, barY + 1.f), ImVec2(p.x + width + 1.f, barY + 11.f), kWidgetDarkU32, 4.f);
        draw->AddRectFilled(ImVec2(p.x, barY), ImVec2(p.x + width, barY + 10.f), hovered ? IM_COL32(22, 22, 22, 255) : kWidgetU32, 4.f);
        if (t > 0.f)
            draw->AddRectFilled(ImVec2(p.x, barY), ImVec2(p.x + width * std::clamp(t, 0.f, 1.f), barY + 10.f), CurrentAccentU32(), 4.f);
        ImGui::PopID();
        return changed;
    }

    void ClassicCombo(const char* label, int* value, const char* const items[], int count)
    {
        ImGui::TextDisabled("%s", label);
        ImGui::PushID(label);
        ImGui::SetNextItemWidth(-1.f);
        ImGui::Combo("##combo", value, items, count);
        ImGui::PopID();
    }

    void ClassicMultiSelectCombo(const char* label, bool* values, const char* const items[], int count)
    {
        std::string preview;
        int selectedCount = 0;
        for (int i = 0; i < count; ++i)
        {
            if (!values[i])
                continue;
            ++selectedCount;
            if (!preview.empty())
                preview += ", ";
            preview += items[i];
        }
        if (selectedCount == 0)
            preview = "none";
        else if (selectedCount == count)
            preview = "all";

        ImGui::TextDisabled("%s", label);
        ImGui::PushID(label);
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::BeginCombo("##multi_combo", preview.c_str()))
        {
            for (int i = 0; i < count; ++i)
                ImGui::Selectable(items[i], &values[i], ImGuiSelectableFlags_DontClosePopups);

            ImGui::Separator();
            if (ImGui::Selectable("select all", false, ImGuiSelectableFlags_DontClosePopups))
                for (int i = 0; i < count; ++i) values[i] = true;
            if (ImGui::Selectable("clear", false, ImGuiSelectableFlags_DontClosePopups))
                for (int i = 0; i < count; ++i) values[i] = false;
            ImGui::EndCombo();
        }
        ImGui::PopID();
    }

    bool ReadSpectatorName(std::uintptr_t controllerAddress, std::string& out)
    {
        out.clear();
        if (!controllerAddress || !Offset::Entity.iszPlayerName)
            return false;
        char name[128]{};
        if (!ProcessMgr.ReadBuffer(controllerAddress + Offset::Entity.iszPlayerName, name, sizeof(name) - 1))
            return false;
        name[sizeof(name) - 1] = '\0';
        out = name;
        if (out.empty())
            out = "player";
        return true;
    }

    bool IsSpectatingPawn(std::uintptr_t controllerAddress, std::uintptr_t targetPawnAddress)
    {
        if (!controllerAddress || !targetPawnAddress || !Offset::Entity.ObserverPawn ||
            !Offset::Pawn.pObserverServices || !Offset::Observer.hObserverTarget)
            return false;

        std::uint32_t observerPawnHandle = 0;
        if (!ProcessMgr.ReadMemory(controllerAddress + Offset::Entity.ObserverPawn, observerPawnHandle) ||
            !observerPawnHandle || observerPawnHandle == 0xFFFFFFFFu)
            return false;

        const std::uintptr_t observerPawn = gGame.ResolveEntity(observerPawnHandle);
        if (!observerPawn)
            return false;

        std::uintptr_t observerServices = 0;
        if (!ProcessMgr.ReadMemory(observerPawn + Offset::Pawn.pObserverServices, observerServices) || !observerServices)
            return false;

        if (Offset::Observer.iObserverMode)
        {
            std::uint8_t mode = 0;
            if (ProcessMgr.ReadMemory(observerServices + Offset::Observer.iObserverMode, mode) && mode == 0)
                return false;
        }

        std::uint32_t targetHandle = 0;
        if (!ProcessMgr.ReadMemory(observerServices + Offset::Observer.hObserverTarget, targetHandle) ||
            !targetHandle || targetHandle == 0xFFFFFFFFu)
            return false;

        return gGame.ResolveEntity(targetHandle) == targetPawnAddress;
    }

    void ClassicColorRow(const char* label, ImColor& color)
    {
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - 28.f);
        ImGui::PushID(label);
        ImGui::ColorEdit4("##color", &color.Value.x,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
        ImGui::PopID();
    }

    std::uint32_t AimBoneIndex()
    {
        if (MenuConfig::AimPosition == 1) return BONEINDEX::neck_0;
        if (MenuConfig::AimPosition == 2) return BONEINDEX::spine_2;
        return BONEINDEX::head;
    }
}

void Cheats::SetSpectatorPreviewNames(const std::vector<std::string>& names)
{
    gSpectatorPreviewNames = names;
}

void Cheats::Menu()
{
    ApplyMenuStyle();
    if (!MenuConfig::ShowMenu)
        return;

    // Start at the original ClassicCounter size, but allow the user to resize
    // the menu afterwards. Keep a minimum size so the two-column layout stays usable.
    ImGui::SetNextWindowSize(ImVec2(kMenuWidth, kMenuHeight), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(520.f, 400.f), ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
    ImGui::SetNextWindowPos(ImVec2(40.f, 80.f), ImGuiCond_Once);
    if (!ImGui::Begin("##cs2_external_classiccounter", &MenuConfig::ShowMenu,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::End();
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();
    const float menuWidth = ws.x;
    const float menuHeight = ws.y;

    // ClassicCounter shell: body, header, footer and 1px separators.
    // Use the live ImGui window size so the custom shell follows resizing.
    draw->AddRectFilled(wp, ImVec2(wp.x + menuWidth, wp.y + menuHeight), kBodyU32, 5.f);
    draw->AddRectFilled(wp, ImVec2(wp.x + menuWidth, wp.y + kHeaderHeight), kHeaderU32, 5.f, ImDrawFlags_RoundCornersTop);
    draw->AddRectFilled(ImVec2(wp.x, wp.y + kHeaderHeight), ImVec2(wp.x + menuWidth, wp.y + kHeaderHeight + 1.f), kOutlineU32);
    draw->AddRectFilled(ImVec2(wp.x, wp.y + menuHeight - kFooterHeight), ImVec2(wp.x + menuWidth, wp.y + menuHeight), kHeaderU32, 5.f, ImDrawFlags_RoundCornersBottom);
    draw->AddRectFilled(ImVec2(wp.x, wp.y + menuHeight - kFooterHeight), ImVec2(wp.x + menuWidth, wp.y + menuHeight - kFooterHeight + 1.f), kOutlineU32);
    draw->AddRect(wp, ImVec2(wp.x + menuWidth, wp.y + menuHeight), kOutlineU32, 5.f);

    draw->AddText(ImVec2(wp.x + 10.f, wp.y + 9.f), kTextU32, "cs2 external");

    // Vertical ClassicCounter-style navigation down the left side.
    draw->AddRectFilled(
        ImVec2(wp.x + kSidebarWidth, wp.y + kHeaderHeight),
        ImVec2(wp.x + kSidebarWidth + 1.f, wp.y + menuHeight - kFooterHeight),
        kOutlineU32);

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        g_tabScrubActive = false;
        g_tabScrubCandidate = -1;
    }

    const char* tabs[] = { "aimbot", "visuals", "misc", "config" };
    constexpr float tabStartY = kHeaderHeight + 8.f;
    constexpr float tabStep = 34.f;
    constexpr float tabX = 5.f;
    constexpr float tabWidth = kSidebarWidth - 10.f;
    constexpr float tabHeight = 29.f;

    // Smoothly slide the selected-tab background toward the newly selected
    // page. The page switch has the tiny scrub dwell above, while this gives
    // the transition a softer visual motion instead of a hard snap.
    const float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f);
    const float blend = 1.f - std::exp(-18.f * dt);
    g_tabVisual += (static_cast<float>(g_tab) - g_tabVisual) * blend;
    if (std::fabs(g_tabVisual - static_cast<float>(g_tab)) < 0.001f)
        g_tabVisual = static_cast<float>(g_tab);

    const float activeY = tabStartY + tabStep * g_tabVisual;
    const ImVec2 activeMin(wp.x + tabX, wp.y + activeY);
    const ImVec2 activeMax(activeMin.x + tabWidth, activeMin.y + tabHeight);
    draw->AddRectFilled(activeMin, activeMax, IM_COL32(32, 32, 32, 255), 4.f);
    draw->AddRectFilled(activeMin, ImVec2(activeMin.x + 2.f, activeMax.y), CurrentAccentU32(), 2.f, ImDrawFlags_RoundCornersLeft);

    float tabY = tabStartY;
    for (int i = 0; i < IM_ARRAYSIZE(tabs); ++i)
    {
        ClassicTab(tabs[i], i, tabY);
        tabY += tabStep;
    }

    // The custom background covers ImGui's default resize-grip drawing, so add
    // a subtle cue in the lower-right corner. ImGui still handles the resize hitbox.
    const ImU32 resizeCue = IM_COL32(80, 80, 80, 180);
    draw->AddLine(ImVec2(wp.x + menuWidth - 12.f, wp.y + menuHeight - 4.f), ImVec2(wp.x + menuWidth - 4.f, wp.y + menuHeight - 12.f), resizeCue, 1.f);
    draw->AddLine(ImVec2(wp.x + menuWidth - 8.f, wp.y + menuHeight - 4.f), ImVec2(wp.x + menuWidth - 4.f, wp.y + menuHeight - 8.f), resizeCue, 1.f);

    const float bodyY = kHeaderHeight + kOuterPad;
    const float bodyHeight = menuHeight - kHeaderHeight - kFooterHeight - (kOuterPad * 2.f);
    const float groupWidth = (menuWidth - kContentX - kOuterPad - kGroupGap) * 0.5f;
    const float rightX = kContentX + groupWidth + kGroupGap;

    if (g_tab == 0)
    {
        constexpr float globalGroupHeight = 82.f;
        constexpr float globalGroupGap = 5.f;

        ImGui::SetCursorPos(ImVec2(kContentX, bodyY));
        if (BeginClassicGroup("##global_group", "global", ImVec2(groupWidth, globalGroupHeight)))
        {
            ClassicToggleWithKeybind("enabled", &MenuConfig::AimBot, "aim_key_inline", MenuConfig::AimKey, 92.f);
            ClassicToggle("visible only", &MenuConfig::AimSpottedOnly);
        }
        EndClassicGroup();

        ImGui::SetCursorPos(ImVec2(kContentX, bodyY + globalGroupHeight + globalGroupGap));
        if (BeginClassicGroup("##trigger_group", "triggerbot", ImVec2(groupWidth, bodyHeight - globalGroupHeight - globalGroupGap)))
        {
            ClassicToggleWithKeybind("enabled##trigger", &MenuConfig::TriggerBot, "trigger_key_inline", MenuConfig::TriggerKey, 92.f);
            ClassicSliderInt("hitchance (%)", &MenuConfig::TriggerHitchance, 0, 100, "%d");
            ClassicSliderInt("delay (ms)", &MenuConfig::TriggerDelay, 0, 500, "%d");
            ClassicToggle("scoped only (snipers)", &MenuConfig::TriggerSnipersScopedOnly);
            ImGui::Spacing();
            const char* triggerHitboxes[] = { "head", "neck", "chest", "stomach", "pelvis" };
            ClassicMultiSelectCombo("hitboxes", MenuConfig::TriggerHitboxes.data(), triggerHitboxes, IM_ARRAYSIZE(triggerHitboxes));
        }
        EndClassicGroup();

        ImGui::SetCursorPos(ImVec2(rightX, bodyY));
        if (BeginClassicGroup("##general_group", "general", ImVec2(groupWidth, bodyHeight)))
        {
            const char* bones[] = { "head", "neck", "chest" };
            ClassicCombo("target bone", &MenuConfig::AimPosition, bones, IM_ARRAYSIZE(bones));
            ClassicSliderFloat("smoothing", &MenuConfig::AimSmooth, 1.f, 20.f, "%.1f");
            ClassicSliderFloat("field of view", &MenuConfig::AimFov, 0.25f, 15.f, "%.2f");
            ClassicToggle("show fov circle", &MenuConfig::ShowAimFovRange);
            ClassicColorRow("fov color", MenuConfig::AimFovRangeColor);
            ImGui::Spacing();
            ClassicToggle("recoil compensation", &MenuConfig::RCS);
            ClassicSliderFloat("smooth", &MenuConfig::RCSSmooth, 1.f, 12.f, "%.1f");
        }
        EndClassicGroup();
    }
    else if (g_tab == 1)
    {
        ImGui::SetCursorPos(ImVec2(kContentX, bodyY));
        if (BeginClassicGroup("##esp_group", "esp", ImVec2(groupWidth, bodyHeight)))
        {
            ClassicToggle("enable", &MenuConfig::ESP);
            ClassicToggle("team check", &MenuConfig::TeamCheck);
            const char* visibilityModes[] = { "all", "visible only", "occluded only" };
            ClassicCombo("visibility", &MenuConfig::ESPVisibilityMode, visibilityModes, IM_ARRAYSIZE(visibilityModes));
            ImGui::Spacing();
            ClassicToggle("name", &MenuConfig::ShowPlayerName);
            ClassicToggle("bounding box", &MenuConfig::ShowBoxESP);
            ClassicToggle("health bar", &MenuConfig::ShowHealthBar);
            ClassicToggle("weapon", &MenuConfig::ShowWeaponESP);
            ClassicToggle("skeleton", &MenuConfig::ShowBoneESP);
            ClassicToggle("distance", &MenuConfig::ShowDistance);
        }
        EndClassicGroup();

        ImGui::SetCursorPos(ImVec2(rightX, bodyY));
        if (BeginClassicGroup("##colors_group", "colors", ImVec2(groupWidth, 145.f)))
        {
            ClassicColorRow("box color", MenuConfig::BoxColor);
            ClassicColorRow("skeleton color", MenuConfig::BoneColor);
            ClassicColorRow("text color", MenuConfig::TextColor);
        }
        EndClassicGroup();
    }
    else if (g_tab == 2)
    {
        ImGui::SetCursorPos(ImVec2(kContentX, bodyY));
        if (BeginClassicGroup("##movement_group", "movement", ImVec2(groupWidth, 120.f)))
        {
            ClassicToggleWithKeybind("bunny hop", &MenuConfig::BunnyHop, "bhop_key_inline", MenuConfig::BunnyHopKey, 92.f);
        }
        EndClassicGroup();

        ImGui::SetCursorPos(ImVec2(rightX, bodyY));
        if (BeginClassicGroup("##crosshair_group", "sniper crosshair", ImVec2(groupWidth, bodyHeight)))
        {
            ClassicToggle("enabled", &MenuConfig::SniperCrosshair);
            ClassicColorRow("color", MenuConfig::SniperCrosshairColor);
            ClassicSliderFloat("size", &MenuConfig::SniperCrosshairSize, 1.f, 40.f, "%.1f");
            ClassicSliderFloat("gap", &MenuConfig::SniperCrosshairGap, 0.f, 30.f, "%.1f");
            ClassicSliderFloat("thickness", &MenuConfig::SniperCrosshairThickness, 0.5f, 8.f, "%.1f");
            ClassicSliderFloat("opacity", &MenuConfig::SniperCrosshairOpacity, 0.f, 1.f, "%.2f");
            ClassicToggle("center dot", &MenuConfig::SniperCrosshairCenterDot);
            ClassicSliderFloat("dot size", &MenuConfig::SniperCrosshairDotSize, 1.f, 12.f, "%.1f");
        }
        EndClassicGroup();
    }
    else
    {
        ImGui::SetCursorPos(ImVec2(kContentX, bodyY));
        if (BeginClassicGroup("##config_group", "", ImVec2(groupWidth, bodyHeight)))
        {
            ConfigMenu::RenderSettingsMenu();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::Button("uninject", ImVec2(-1.f, 0.f)))
                Gui.Quit();
        }
        EndClassicGroup();

        // The old status panel is now the actual config manager.
        ImGui::SetCursorPos(ImVec2(rightX, bodyY));
        if (BeginClassicGroup("##configs_group", "configs", ImVec2(groupWidth, bodyHeight)))
            ConfigMenu::RenderConfigFiles();
        EndClassicGroup();
    }

    ImGui::End();
}

void Cheats::Run()
{
    if (!ProcessMgr.IsActive())
    {
        Gui.Quit();
        return;
    }

    const HWND foreground = GetForegroundWindow();
    DWORD foregroundPid = 0;
    if (foreground)
        GetWindowThreadProcessId(foreground, &foregroundPid);

    // Treat any top-level window owned by CS2 as game focus and any window owned
    // by this process as overlay/menu focus. Checking only exact HWNDs can briefly
    // fail during slider/empty-menu clicks and make the overlay disappear until
    // the game is clicked again.
    const bool gameFocused = foreground == Gui.DestWindow.hWnd ||
        (foregroundPid != 0 && foregroundPid == ProcessMgr.ProcessID);
    const bool overlayFocused = foreground == Gui.Window.hWnd ||
        GetActiveWindow() == Gui.Window.hWnd ||
        (foregroundPid != 0 && foregroundPid == GetCurrentProcessId());

    // Do not draw or process global hotkeys/input while another application is
    // active. The overlay window itself counts as active only so its menu can be
    // clicked; gameplay automation still requires the actual CS2 window.
    if (!gameFocused && !overlayFocused)
    {
        TriggerBot::Reset();
        BunnyHop::Reset();
        return;
    }

    static bool menuPrev = false, panicPrev = false;
    static int menuPrevKey = MenuConfig::MenuKey.key, panicPrevKey = MenuConfig::PanicKey.key;
    const bool capturing = MenuWidgets::Capture().active != 0;
    if (!capturing && PressedOnce(MenuConfig::MenuKey.key, menuPrev, menuPrevKey))
        MenuConfig::ShowMenu = !MenuConfig::ShowMenu;
    if (!capturing && PressedOnce(MenuConfig::PanicKey.key, panicPrev, panicPrevKey))
        MenuConfig::PanicVisuals = !MenuConfig::PanicVisuals;

    Menu();
    DrawStatusCounter();
    DrawSpectatorPanel();
    DrawSniperCrosshairOverlay();

    if (!ProcessMgr.ReadBuffer(gGame.GetMatrixAddress(), gGame.View.Matrix, sizeof(gGame.View.Matrix)))
        return;

    std::uintptr_t localControllerAddress = 0, localPawnAddress = 0;
    if (!gGame.ReadLocalController(localControllerAddress) || !gGame.ReadLocalPawn(localPawnAddress))
        return;

    // Calibrate the current Source-2 entity-list slot layout against the local
    // controller pointer. This fixes ESP/aim across builds where the slot stride
    // differs (notably 0x70 vs 0x78).
    if (!gGame.GetEntityStride() && !gGame.CalibrateEntityResolver(localControllerAddress))
        return;

    CEntity local{};
    local.Controller.Address = localControllerAddress;
    ProcessMgr.ReadMemory(localControllerAddress + Offset::Entity.TeamID, local.Controller.TeamID);
    if (Offset::Entity.Ping && ProcessMgr.ReadMemory(localControllerAddress + Offset::Entity.Ping, local.Controller.Ping))
        gCurrentPing = std::clamp(local.Controller.Ping, 0, 999);
    else
        gCurrentPing = -1;
    if (!local.UpdateLocalPawn(localPawnAddress))
    {
        TriggerBot::Reset();
        return;
    }

    const bool allowAutomation = gameFocused && !MenuConfig::ShowMenu;

    // Run bhop immediately after the local pawn read; doing it before the player
    // scan avoids missing the short landing window on busy ESP frames.
    if (allowAutomation)
        BunnyHop::Run(local);
    else
        BunnyHop::Reset();

    float bestScreenDistance = std::numeric_limits<float>::max();
    Vec2 bestTargetScreen{};
    bool hasTarget = false;
    const bool needBones = (MenuConfig::ESP && MenuConfig::ShowBoneESP && !MenuConfig::PanicVisuals) || MenuConfig::AimBot || MenuConfig::TriggerBot;
    const bool needSpotted = MenuConfig::AimSpottedOnly;
    std::vector<CEntity> triggerEntities;
    if (MenuConfig::TriggerBot)
        triggerEntities.reserve(static_cast<std::size_t>(std::clamp(MenuConfig::MaxPlayers, 1, 64)));

    std::vector<std::string> liveSpectators;
    std::unordered_set<std::string> spectatorDedup;
    if (MenuConfig::ShowSpectatorList)
        liveSpectators.reserve(8);

    for (int i = 1; i <= std::clamp(MenuConfig::MaxPlayers, 1, 64); ++i)
    {
        const std::uintptr_t controllerAddress = gGame.ResolveEntity(static_cast<std::uint32_t>(i));
        if (!controllerAddress || controllerAddress == localControllerAddress)
            continue;

        if (MenuConfig::ShowSpectatorList && IsSpectatingPawn(controllerAddress, localPawnAddress))
        {
            std::string spectatorName;
            if (ReadSpectatorName(controllerAddress, spectatorName) && spectatorDedup.insert(spectatorName).second)
                liveSpectators.push_back(std::move(spectatorName));
        }

        CEntity entity{};
        if (!entity.UpdateController(controllerAddress))
            continue;
        if (MenuConfig::TeamCheck && entity.Controller.TeamID == local.Pawn.TeamID)
            continue;

        const bool needWeapon = (MenuConfig::ESP && MenuConfig::ShowWeaponESP && !MenuConfig::PanicVisuals);
        if (!entity.UpdatePawn(entity.Pawn.Address, needBones, needWeapon, needSpotted))
            continue;

        if (MenuConfig::TriggerBot)
            triggerEntities.push_back(entity);

        if (MenuConfig::ESP && !MenuConfig::PanicVisuals)
            Render::DrawPlayer(entity, local);

        if (MenuConfig::AimBot)
        {
            if (MenuConfig::AimSpottedOnly && entity.Pawn.SpottedByMask == 0)
                continue;

            Vec3 target{};
            const auto boneIndex = AimBoneIndex();
            if (entity.Pawn.BoneData.BonePosList.size() > boneIndex)
                target = entity.Pawn.BoneData.BonePosList[boneIndex].Pos;
            else
            {
                // Keep normal external aim functional if the game's bone array
                // moves between updates. These are conservative standing-height
                // fallbacks based on the pawn origin.
                const float z = MenuConfig::AimPosition == 2 ? 50.f : (MenuConfig::AimPosition == 1 ? 60.f : 66.f);
                target = entity.Pawn.Pos + Vec3(0.f, 0.f, z);
            }

            // Screen-space target selection uses the same view matrix as ESP and
            // avoids depending on a separate view-angle/sensitivity chain for aim.
            Vec2 targetScreen{};
            if (!gGame.View.WorldToScreen(target, targetScreen))
                continue;
            const Vec2 center(Gui.Window.Size.x * 0.5f, Gui.Window.Size.y * 0.5f);
            const float screenDistance = targetScreen.DistanceTo(center);
            if (screenDistance <= AimControl::AimFovRadiusPixels() && screenDistance < bestScreenDistance)
            {
                bestScreenDistance = screenDistance;
                bestTargetScreen = targetScreen;
                hasTarget = true;
            }
        }
    }

    if (MenuConfig::ShowSpectatorList)
    {
        std::sort(liveSpectators.begin(), liveSpectators.end());
        gSpectatorPreviewNames = std::move(liveSpectators);
    }
    else
        gSpectatorPreviewNames.clear();

    if (!MenuConfig::PanicVisuals && MenuConfig::AimBot)
        Render::DrawAimFov();

    if (allowAutomation && MenuConfig::AimBot && hasTarget && MenuConfig::AimKey.IsActive())
        AimControl::AimAtScreen(bestTargetScreen);

    if (allowAutomation)
        AimControl::RunRCS(local);

    if (allowAutomation && MenuConfig::TriggerBot && MenuConfig::TriggerKey.IsActive())
        TriggerBot::Run(local, triggerEntities);
    else
        TriggerBot::Reset();

    if (MenuConfig::FrameSleepMs > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(MenuConfig::FrameSleepMs));
}
