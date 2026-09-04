#pragma once
#include "OS-ImGui/imgui/imgui.h"
#include "KeyBind.hpp"
#include <array>
#include <string>

namespace MenuWidgets
{
    inline ImVec4 Accent() { return ImVec4(0.0f, 205.0f / 255.0f, 145.0f / 255.0f, 1.0f); }

    inline bool Toggle(const char* label, bool* value)
    {
        return ImGui::Checkbox(label, value);
    }

    struct CaptureState
    {
        ImGuiID active = 0;
        bool waitingForMouseRelease = false;
        std::array<bool, 256> baseline{};
    };

    inline CaptureState& Capture()
    {
        static CaptureState state;
        return state;
    }

    inline void SnapshotKeys(std::array<bool, 256>& state)
    {
        for (int i = 1; i < 256; ++i)
            state[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
    }

    inline bool KeyBindButton(const char* idLabel, Keybind::Bind& bind, float width = 110.0f)
    {
        ImGui::PushID(idLabel);

        auto& capture = Capture();
        const ImGuiID id = ImGui::GetID("##keybind");
        std::string buttonText;
        if (capture.active == id)
            buttonText = "press a key...";
        else if (bind.mode == Keybind::Mode::Always)
            buttonText = "always";
        else
            buttonText = Keybind::KeyName(bind.key);

        ImGui::SetNextItemWidth(width);
        if (ImGui::Button(buttonText.c_str(), ImVec2(width, 0)))
        {
            capture.active = id;
            capture.waitingForMouseRelease = true;
            SnapshotKeys(capture.baseline);
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("mode_popup");

        if (ImGui::BeginPopup("mode_popup"))
        {
            if (ImGui::Selectable("hold", bind.mode == Keybind::Mode::Hold))
            {
                bind.mode = Keybind::Mode::Hold;
                bind.ResetRuntime();
            }
            if (ImGui::Selectable("toggle", bind.mode == Keybind::Mode::Toggle))
            {
                bind.mode = Keybind::Mode::Toggle;
                bind.ResetRuntime();
            }
            if (ImGui::Selectable("always", bind.mode == Keybind::Mode::Always))
            {
                bind.mode = Keybind::Mode::Always;
                bind.ResetRuntime();
            }
            ImGui::Separator();
            if (ImGui::Selectable("clear"))
            {
                bind.key = 0;
                bind.mode = Keybind::Mode::Hold;
                bind.ResetRuntime();
            }
            ImGui::EndPopup();
        }

        bool changed = false;
        if (capture.active == id)
        {
            if (capture.waitingForMouseRelease)
            {
                if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
                {
                    capture.waitingForMouseRelease = false;
                    SnapshotKeys(capture.baseline);
                }
            }
            else
            {
                if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
                {
                    capture.active = 0;
                }
                else
                {
                    for (int vk = 1; vk < 256; ++vk)
                    {
                        const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
                        if (down && !capture.baseline[vk])
                        {
                            bind.key = vk;
                            if (bind.mode == Keybind::Mode::Always)
                                bind.mode = Keybind::Mode::Hold;
                            bind.ResetRuntime();
                            capture.active = 0;
                            changed = true;
                            break;
                        }
                    }
                }
            }
        }

        ImGui::PopID();
        return changed;
    }

    inline bool KeyBind(const char* label, Keybind::Bind& bind, float width = 110.0f)
    {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - width);
        const bool changed = KeyBindButton("##button", bind, width);
        ImGui::PopID();
        return changed;
    }
}
