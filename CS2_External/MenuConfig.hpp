#pragma once
#include <string>
#include <array>
#include "OS-ImGui/OS-ImGui.h"
#include "KeyBind.hpp"
#include "Bone.h"

namespace MenuConfig
{
    inline std::string path;

    inline bool ShowMenu = true;
    inline Keybind::Bind MenuKey{ VK_INSERT, Keybind::Mode::Hold };
    inline bool PanicVisuals = false;
    inline Keybind::Bind PanicKey{ VK_HOME, Keybind::Mode::Hold };
    inline ImColor MenuAccentColor = ImColor(0, 205, 145, 255);
    inline bool ShowStatusCounter = true;
    inline bool StatusShowBrand = true;
    inline bool StatusShowUser = true;
    inline bool StatusShowFps = true;
    inline bool StatusShowPing = true;
    inline bool ShowSpectatorList = true;
    inline int OverlayFpsLimit = 240;

    // Standalone visual crosshair overlay. This is UI-only and does not read game state.
    inline bool SniperCrosshair = false;
    inline ImColor SniperCrosshairColor = ImColor(255, 255, 255, 255);
    inline float SniperCrosshairSize = 10.0f;
    inline float SniperCrosshairGap = 4.0f;
    inline float SniperCrosshairThickness = 1.0f;
    inline float SniperCrosshairOpacity = 1.0f;
    inline bool SniperCrosshairCenterDot = false;
    inline float SniperCrosshairDotSize = 2.0f;
    // UI-only visibility preference. This does not inspect CS2 visibility state.
    inline int ESPVisibilityMode = 0; // 0 all, 1 visible only, 2 occluded only

    inline bool ESP = true;
    inline bool TeamCheck = true;
    inline bool ShowBoxESP = true;
    inline bool ShowBoneESP = true;
    inline bool ShowHealthBar = true;
    inline bool ShowWeaponESP = true;
    inline bool ShowDistance = false;
    inline bool ShowPlayerName = true;
    
    // =========================================================================
    // Visibility-Aware ESP Colors
    // =========================================================================
    // Separate colors for visible vs occluded entities.
    // Updated immediately when visibility state changes (per-frame).
    // =========================================================================
    inline ImColor BoxColor = ImColor(255, 255, 255, 255);              // Visible box
    inline ImColor OccludedBoxColor = ImColor(100, 100, 100, 180);      // Occluded box (muted)
    inline ImColor BoneColor = ImColor(220, 220, 220, 255);             // Visible skeleton
    inline ImColor OccludedBoneColor = ImColor(80, 80, 80, 140);        // Occluded skeleton (very muted)
    inline ImColor TextColor = ImColor(255, 255, 255, 255);             // Visible text (name/weapon/distance)
    inline ImColor OccludedTextColor = ImColor(120, 120, 120, 180);     // Occluded text (muted)

    inline bool AimBot = true;
    inline Keybind::Bind AimKey{ VK_LBUTTON, Keybind::Mode::Hold };
    inline float AimFov = 4.0f;
    inline float AimSmooth = 11.0f;
    inline int AimPosition = 0; // 0 head, 1 neck, 2 chest
    inline bool AimSpottedOnly = true;  // NOTE: Changed to "VisibleOnly" with new system
    inline bool ShowAimFovRange = true;
    inline ImColor AimFovRangeColor = ImColor(255, 255, 255, 190);

    inline bool RCS = true;
    inline int RCSStartBullet = 1;
    // 1.0 = strongest/full compensation. Higher values intentionally reduce
    // how much of each recoil step is corrected, giving the single UI control
    // the same direction as normal smoothing sliders.
    inline float RCSSmooth = 1.0f;
    inline float RCSPitch = 1.0f; // legacy config compatibility
    inline float RCSYaw = 1.0f;   // legacy config compatibility
    inline bool RCSWeaponAware = true; // legacy config key; live punch is inherently weapon-aware

    inline bool TriggerBot = true;
    inline Keybind::Bind TriggerKey{ VK_XBUTTON1, Keybind::Mode::Hold };
    inline int TriggerDelay = 35;
    inline int TriggerHitchance = 100;
    inline bool TriggerSnipersScopedOnly = true;
    // Head, neck, chest, stomach, pelvis. Multiple selections are supported.
    inline std::array<bool, 5> TriggerHitboxes{ true, true, true, true, true };

    inline bool BunnyHop = false;
    inline Keybind::Bind BunnyHopKey{ VK_SPACE, Keybind::Mode::Hold };

    inline int MaxPlayers = 64;
    inline int FrameSleepMs = 0;

    inline void ResetToDefaults()
    {
        ShowMenu = true;
        MenuKey = { VK_INSERT, Keybind::Mode::Hold };
        PanicVisuals = false;
        PanicKey = { VK_HOME, Keybind::Mode::Hold };
        MenuAccentColor = ImColor(0, 205, 145, 255);
        ShowStatusCounter = true;
        StatusShowBrand = true; StatusShowUser = true; StatusShowFps = true; StatusShowPing = true;
        ShowSpectatorList = true;
        OverlayFpsLimit = 240;
        SniperCrosshair = false;
        SniperCrosshairColor = ImColor(255,255,255,255);
        SniperCrosshairSize = 10.f;
        SniperCrosshairGap = 4.f;
        SniperCrosshairThickness = 1.f;
        SniperCrosshairOpacity = 1.f;
        SniperCrosshairCenterDot = false;
        SniperCrosshairDotSize = 2.f;
        ESPVisibilityMode = 0;

        ESP = true; TeamCheck = true; ShowBoxESP = true; ShowBoneESP = true;
        ShowHealthBar = true; ShowWeaponESP = true; ShowDistance = false; ShowPlayerName = true;
        BoxColor = ImColor(255,255,255,255);
        OccludedBoxColor = ImColor(100,100,100,180);
        BoneColor = ImColor(220,220,220,255);
        OccludedBoneColor = ImColor(80,80,80,140);
        TextColor = ImColor(255,255,255,255);
        OccludedTextColor = ImColor(120,120,120,180);

        AimBot = true; AimKey = { VK_LBUTTON, Keybind::Mode::Hold }; AimFov = 4.f; AimSmooth = 11.f;
        AimPosition = 0; AimSpottedOnly = true; ShowAimFovRange = true; AimFovRangeColor = ImColor(255,255,255,190);

        RCS = true; RCSStartBullet = 1; RCSSmooth = 1.f; RCSPitch = 1.f; RCSYaw = 1.f; RCSWeaponAware = true;
        TriggerBot = true; TriggerKey = { VK_XBUTTON1, Keybind::Mode::Hold };
        TriggerDelay = 35; TriggerHitchance = 100; TriggerSnipersScopedOnly = true;
        TriggerHitboxes = { true, true, true, true, true };
        BunnyHop = false; BunnyHopKey = { VK_SPACE, Keybind::Mode::Hold };
        MaxPlayers = 64; FrameSleepMs = 0;
    }
}
