#include "ConfigMenu.hpp"
#include "ConfigSaver.hpp"
#include "../MenuConfig.hpp"
#include "../MenuWidgets.hpp"
#include <Windows.h>
#include <shellapi.h>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <array>
#include <sstream>

#pragma comment(lib, "shell32.lib")

namespace
{
    std::string StatusPreview()
    {
        std::vector<std::string> parts;
        if (MenuConfig::StatusShowBrand) parts.emplace_back("amor.rip");
        if (MenuConfig::StatusShowUser) parts.emplace_back("Admin");
        if (MenuConfig::StatusShowFps) parts.emplace_back("fps");
        if (MenuConfig::StatusShowPing) parts.emplace_back("ping");
        if (parts.empty()) return "none";

        std::ostringstream out;
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            if (i) out << ", ";
            out << parts[i];
        }
        return out.str();
    }

    void StatusElementsCombo()
    {
        ImGui::TextUnformatted("status elements");
        const std::string preview = StatusPreview();
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::BeginCombo("##status_elements", preview.c_str()))
        {
            ImGui::Selectable("brand (amor.rip)", &MenuConfig::StatusShowBrand, ImGuiSelectableFlags_DontClosePopups);
            ImGui::Selectable("user (Admin)", &MenuConfig::StatusShowUser, ImGuiSelectableFlags_DontClosePopups);
            ImGui::Selectable("fps", &MenuConfig::StatusShowFps, ImGuiSelectableFlags_DontClosePopups);
            ImGui::Selectable("ping", &MenuConfig::StatusShowPing, ImGuiSelectableFlags_DontClosePopups);
            ImGui::EndCombo();
        }
    }
}

namespace ConfigMenu
{
    void RenderSettingsMenu()
    {
        MenuWidgets::KeyBind("menu key", MenuConfig::MenuKey);
        MenuWidgets::KeyBind("panic button", MenuConfig::PanicKey);

        ImGui::TextUnformatted("menu color");
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - 28.f);
        ImGui::ColorEdit3("##menu_accent_color", &MenuConfig::MenuAccentColor.Value.x,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
            ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoDragDrop);

        ImGui::Spacing();
        ImGui::Checkbox("status counter", &MenuConfig::ShowStatusCounter);
        if (MenuConfig::ShowStatusCounter)
            StatusElementsCombo();
        ImGui::Checkbox("spectator list", &MenuConfig::ShowSpectatorList);
        ImGui::TextUnformatted("overlay fps limit");
        ImGui::SetNextItemWidth(-1.f);
        ImGui::SliderInt("##overlay_fps_limit", &MenuConfig::OverlayFpsLimit, 30, 400, "%d fps", ImGuiSliderFlags_AlwaysClamp);
    }

    void RenderConfigFiles()
    {
        static char newName[64] = "";
        static int selected = -1;
        static std::vector<std::string> files;

        // Fit the name field and create button to the current group width.
        // This keeps "create" fully visible even when the menu is at its minimum size.
        const float createWidth = ImGui::CalcTextSize("create").x + ImGui::GetStyle().FramePadding.x * 2.f;
        const float createGap = ImGui::GetStyle().ItemSpacing.x;
        const float inputWidth = std::max(60.f, ImGui::GetContentRegionAvail().x - createWidth - createGap);
        ImGui::SetNextItemWidth(inputWidth);
        ImGui::InputTextWithHint("##newconfig", "config name", newName, sizeof(newName));
        ImGui::SameLine();
        if (ImGui::Button("create", ImVec2(createWidth, 0.f)))
        {
            std::string name = newName;
            if (!name.empty())
            {
                if (name.find('.') == std::string::npos)
                    name += ".cfg";
                MyConfigSaver::SaveConfig(name);
                newName[0] = '\0';
            }
        }

        files.clear();
        std::error_code ec;
        std::filesystem::create_directories(MenuConfig::path, ec);
        for (const auto& entry : std::filesystem::directory_iterator(MenuConfig::path, ec))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".cfg")
                files.push_back(entry.path().filename().string());
        }
        std::sort(files.begin(), files.end());
        if (selected >= static_cast<int>(files.size())) selected = -1;

        if (ImGui::BeginListBox("##configs", ImVec2(-FLT_MIN, 185.f)))
        {
            for (int i = 0; i < static_cast<int>(files.size()); ++i)
                if (ImGui::Selectable(files[i].c_str(), selected == i)) selected = i;
            ImGui::EndListBox();
        }

        const bool has = selected >= 0 && selected < static_cast<int>(files.size());
        if (!has) ImGui::BeginDisabled();
        if (ImGui::Button("load") && has) MyConfigSaver::LoadConfig(files[selected]);
        ImGui::SameLine();
        if (ImGui::Button("save") && has) MyConfigSaver::SaveConfig(files[selected]);
        ImGui::SameLine();
        if (ImGui::Button("delete") && has)
        {
            std::filesystem::remove(std::filesystem::path(MenuConfig::path) / files[selected], ec);
            selected = -1;
        }
        if (!has) ImGui::EndDisabled();

        if (ImGui::Button("reset defaults"))
            MenuConfig::ResetToDefaults();
        if (ImGui::Button("open config folder"))
            ShellExecuteA(nullptr, "open", MenuConfig::path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    void RenderConfigMenu()
    {
        RenderSettingsMenu();
        ImGui::Separator();
        RenderConfigFiles();
    }
}
