#include "Offsets.h"
#include <Windows.h>
#include <winhttp.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <vector>
#include <cctype>

#pragma comment(lib, "winhttp.lib")

namespace
{
    bool HttpGet(const wchar_t* host, const wchar_t* path, std::string& output)
    {
        output.clear();
        HINTERNET session = WinHttpOpen(L"admin-external/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session)
            return false;

        WinHttpSetTimeouts(session, 4000, 4000, 5000, 5000);
        HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connect)
        {
            WinHttpCloseHandle(session);
            return false;
        }

        HINTERNET request = WinHttpOpenRequest(connect, L"GET", path, nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!request)
        {
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return false;
        }

        bool ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr);

        if (ok)
        {
            DWORD status = 0;
            DWORD statusSize = sizeof(status);
            if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) || status != 200)
                ok = false;
        }

        while (ok)
        {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available))
            {
                ok = false;
                break;
            }
            if (!available)
                break;

            std::vector<char> buffer(available);
            DWORD read = 0;
            if (!WinHttpReadData(request, buffer.data(), available, &read))
            {
                ok = false;
                break;
            }
            output.append(buffer.data(), read);
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return ok && !output.empty();
    }

    bool ReadTextFile(const std::filesystem::path& path, std::string& output)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return false;
        std::ostringstream ss;
        ss << file.rdbuf();
        output = ss.str();
        return !output.empty();
    }

    void WriteTextFile(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (file)
            file.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    std::optional<std::uintptr_t> ParseNumberAfterKey(const std::string& text, const std::string& key, std::size_t start = 0)
    {
        const std::string marker = "\"" + key + "\"";
        std::size_t pos = text.find(marker, start);
        if (pos == std::string::npos)
            return std::nullopt;
        pos = text.find(':', pos + marker.size());
        if (pos == std::string::npos)
            return std::nullopt;
        ++pos;
        while (pos < text.size() && (std::isspace(static_cast<unsigned char>(text[pos])) || text[pos] == '"'))
            ++pos;

        int base = 10;
        if (pos + 2 <= text.size() && text[pos] == '0' && (text[pos + 1] == 'x' || text[pos + 1] == 'X'))
        {
            base = 16;
            pos += 2;
        }

        std::size_t end = pos;
        while (end < text.size())
        {
            const unsigned char c = static_cast<unsigned char>(text[end]);
            if ((base == 10 && std::isdigit(c)) || (base == 16 && std::isxdigit(c)))
                ++end;
            else
                break;
        }
        if (end == pos)
            return std::nullopt;

        try
        {
            return static_cast<std::uintptr_t>(std::stoull(text.substr(pos, end - pos), nullptr, base));
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::optional<std::uintptr_t> ParseClassField(const std::string& json, const std::string& className, const std::string& fieldName)
    {
        const std::string classMarker = "\"" + className + "\"";
        std::size_t searchFrom = 0;

        // A class name can appear earlier in the dump as a parent's value, e.g.
        //     "parent": "C_BaseEntity"
        // Only accept an occurrence that is actually a JSON object key:
        //     "C_BaseEntity": {
        while (true)
        {
            const std::size_t classPos = json.find(classMarker, searchFrom);
            if (classPos == std::string::npos)
                return std::nullopt;

            std::size_t afterClass = classPos + classMarker.size();
            while (afterClass < json.size() && std::isspace(static_cast<unsigned char>(json[afterClass])))
                ++afterClass;

            if (afterClass < json.size() && json[afterClass] == ':')
            {
                ++afterClass;
                while (afterClass < json.size() && std::isspace(static_cast<unsigned char>(json[afterClass])))
                    ++afterClass;

                if (afterClass < json.size() && json[afterClass] == '{')
                {
                    const std::size_t fieldsPos = json.find("\"fields\"", afterClass + 1);
                    if (fieldsPos == std::string::npos)
                        return std::nullopt;

                    const std::size_t fieldsOpen = json.find('{', fieldsPos);
                    if (fieldsOpen == std::string::npos)
                        return std::nullopt;
                    const std::size_t fieldsClose = json.find('}', fieldsOpen + 1);
                    if (fieldsClose == std::string::npos)
                        return std::nullopt;

                    // cs2-dumper field objects are flat maps, so the first closing brace
                    // after the fields object is its end.
                    const std::string fieldsObject = json.substr(fieldsOpen, fieldsClose - fieldsOpen + 1);
                    return ParseNumberAfterKey(fieldsObject, fieldName);
                }
            }

            searchFrom = classPos + classMarker.size();
        }
    }

    bool FetchDumperFiles(std::string& offsets, std::string& client)
    {
        if (HttpGet(L"raw.githubusercontent.com", L"/a2x/cs2-dumper/main/output/offsets.json", offsets) &&
            HttpGet(L"raw.githubusercontent.com", L"/a2x/cs2-dumper/main/output/client_dll.json", client))
            return true;

        offsets.clear();
        client.clear();
        return HttpGet(L"raw.githubusercontent.com", L"/sezzyaep/CS2-OFFSETS/main/offsets.json", offsets) &&
            HttpGet(L"raw.githubusercontent.com", L"/sezzyaep/CS2-OFFSETS/main/client_dll.json", client);
    }

    template <typename T>
    bool AssignRequired(T& destination, const std::optional<std::uintptr_t>& value)
    {
        if (!value || *value == 0)
            return false;
        destination = static_cast<T>(*value);
        return true;
    }
}

bool Offset::UpdateOffsets(const std::string& cacheDirectory, std::string* statusMessage)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(cacheDirectory, ec);

    const fs::path offsetCache = fs::path(cacheDirectory) / "offsets.cache.json";
    const fs::path clientCache = fs::path(cacheDirectory) / "client_dll.cache.json";

    std::string offsetsJson;
    std::string clientJson;
    bool online = FetchDumperFiles(offsetsJson, clientJson);
    if (online)
    {
        WriteTextFile(offsetCache, offsetsJson);
        WriteTextFile(clientCache, clientJson);
    }
    else if (!ReadTextFile(offsetCache, offsetsJson) || !ReadTextFile(clientCache, clientJson))
    {
        if (statusMessage)
            *statusMessage = "could not download current cs2-dumper offsets and no cache exists";
        return false;
    }

    const auto clientStart = offsetsJson.find("\"client.dll\"");
    if (clientStart == std::string::npos)
        return false;

    bool ok = true;
    std::vector<std::string> missing;
    auto required = [&](auto& destination, const std::optional<std::uintptr_t>& value, const char* name)
    {
        if (!AssignRequired(destination, value))
        {
            missing.emplace_back(name);
            return false;
        }
        return true;
    };

    ok &= required(EntityList, ParseNumberAfterKey(offsetsJson, "dwEntityList", clientStart), "dwEntityList");
    ok &= required(Matrix, ParseNumberAfterKey(offsetsJson, "dwViewMatrix", clientStart), "dwViewMatrix");
    ok &= required(ViewAngle, ParseNumberAfterKey(offsetsJson, "dwViewAngles", clientStart), "dwViewAngles");
    ok &= required(LocalPlayerController, ParseNumberAfterKey(offsetsJson, "dwLocalPlayerController", clientStart), "dwLocalPlayerController");
    ok &= required(LocalPlayerPawn, ParseNumberAfterKey(offsetsJson, "dwLocalPlayerPawn", clientStart), "dwLocalPlayerPawn");
    ok &= required(Sensitivity, ParseNumberAfterKey(offsetsJson, "dwSensitivity", clientStart), "dwSensitivity");
    ok &= required(SensitivityValue, ParseNumberAfterKey(offsetsJson, "dwSensitivity_sensitivity", clientStart), "dwSensitivity_sensitivity");

    ok &= required(Entity.Health, ParseClassField(clientJson, "C_BaseEntity", "m_iHealth"), "C_BaseEntity::m_iHealth");
    ok &= required(Entity.TeamID, ParseClassField(clientJson, "C_BaseEntity", "m_iTeamNum"), "C_BaseEntity::m_iTeamNum");
    ok &= required(Entity.PlayerPawn, ParseClassField(clientJson, "CCSPlayerController", "m_hPlayerPawn"), "CCSPlayerController::m_hPlayerPawn");
    ok &= required(Entity.iszPlayerName, ParseClassField(clientJson, "CBasePlayerController", "m_iszPlayerName"), "CBasePlayerController::m_iszPlayerName");
    // Ping is useful for the status overlay but is intentionally optional so a
    // schema rename cannot prevent the rest of the external from starting.
    if (const auto ping = ParseClassField(clientJson, "CCSPlayerController", "m_iPing"))
        Entity.Ping = *ping;
    else
        Entity.Ping = 0;

    // Spectator-list offsets are optional. If Valve renames one of these fields,
    // the rest of the external should still launch normally.
    Entity.ObserverPawn = ParseClassField(clientJson, "CCSPlayerController", "m_hObserverPawn").value_or(0);

    ok &= required(Pawn.Pos, ParseClassField(clientJson, "C_BasePlayerPawn", "m_vOldOrigin"), "C_BasePlayerPawn::m_vOldOrigin");
    ok &= required(Pawn.ViewOffset, ParseClassField(clientJson, "C_BaseModelEntity", "m_vecViewOffset"), "C_BaseModelEntity::m_vecViewOffset");
    ok &= required(Pawn.GameSceneNode, ParseClassField(clientJson, "C_BaseEntity", "m_pGameSceneNode"), "C_BaseEntity::m_pGameSceneNode");
    ok &= required(Pawn.Dormant, ParseClassField(clientJson, "CGameSceneNode", "m_bDormant"), "CGameSceneNode::m_bDormant");
    ok &= required(Pawn.SceneOrigin, ParseClassField(clientJson, "CGameSceneNode", "m_vecAbsOrigin"), "CGameSceneNode::m_vecAbsOrigin");
    ok &= required(Pawn.pWeaponServices, ParseClassField(clientJson, "C_BasePlayerPawn", "m_pWeaponServices"), "C_BasePlayerPawn::m_pWeaponServices");
    Pawn.pObserverServices = ParseClassField(clientJson, "C_BasePlayerPawn", "m_pObserverServices").value_or(0);
    Observer.hObserverTarget = ParseClassField(clientJson, "CPlayer_ObserverServices", "m_hObserverTarget").value_or(0);
    Observer.iObserverMode = ParseClassField(clientJson, "CPlayer_ObserverServices", "m_iObserverMode").value_or(0);

    // Recoil layout has changed across CS2 builds. Prefer the old/direct pawn
    // m_aimPunchAngle when a dump exposes it; otherwise use the current
    // CCSPlayer_AimPunchServices predictable + unpredictable base angles.
    const auto directPunch = ParseClassField(clientJson, "C_CSPlayerPawn", "m_aimPunchAngle");
    Pawn.AimPunchDirectAngle = directPunch.value_or(0);

    const auto aimPunchServices = ParseClassField(clientJson, "C_CSPlayerPawn", "m_pAimPunchServices");
    const auto predictablePunch = ParseClassField(clientJson, "CCSPlayer_AimPunchServices", "m_predictableBaseAngle");
    const auto unpredictablePunch = ParseClassField(clientJson, "CCSPlayer_AimPunchServices", "m_unpredictableBaseAngle");

    Pawn.pAimPunchServices = aimPunchServices.value_or(0);
    Pawn.AimPunchBaseAngle = predictablePunch.value_or(0);
    Pawn.AimPunchUnpredictableAngle = unpredictablePunch.value_or(0);

    if (!Pawn.AimPunchDirectAngle &&
        (!Pawn.pAimPunchServices || !Pawn.AimPunchBaseAngle || !Pawn.AimPunchUnpredictableAngle))
    {
        ok = false;
        missing.emplace_back("aim punch angle layout");
    }

    ok &= required(Pawn.iShotsFired, ParseClassField(clientJson, "C_CSPlayerPawn", "m_iShotsFired"), "C_CSPlayerPawn::m_iShotsFired");
    ok &= required(Pawn.iIDEntIndex, ParseClassField(clientJson, "C_CSPlayerPawn", "m_iIDEntIndex"), "C_CSPlayerPawn::m_iIDEntIndex");
    ok &= required(Pawn.bIsScoped, ParseClassField(clientJson, "C_CSPlayerPawn", "m_bIsScoped"), "C_CSPlayerPawn::m_bIsScoped");
    ok &= required(Pawn.OnGroundLastTick, ParseClassField(clientJson, "C_CSPlayerPawn", "m_bOnGroundLastTick"), "C_CSPlayerPawn::m_bOnGroundLastTick");

    // m_fFlags is inherited from C_BaseEntity in current CS2 schema dumps.
    auto flags = ParseClassField(clientJson, "C_BaseEntity", "m_fFlags");
    if (!flags)
        flags = ParseClassField(clientJson, "C_BasePlayerPawn", "m_fFlags");
    ok &= required(Pawn.Flags, flags, "m_fFlags");

    auto spottedState = ParseClassField(clientJson, "C_CSPlayerPawn", "m_entitySpottedState");
    auto spottedMask = ParseClassField(clientJson, "EntitySpottedState_t", "m_bSpottedByMask");
    if (!spottedState || !spottedMask)
    {
        ok = false;
        if (!spottedState) missing.emplace_back("C_CSPlayerPawn::m_entitySpottedState");
        if (!spottedMask) missing.emplace_back("EntitySpottedState_t::m_bSpottedByMask");
    }
    else
        Pawn.bSpottedByMask = *spottedState + *spottedMask;

    auto modelState = ParseClassField(clientJson, "CSkeletonInstance", "m_modelState");
    if (!modelState)
    {
        ok = false;
        missing.emplace_back("CSkeletonInstance::m_modelState");
    }
    else
        Pawn.BoneArray = *modelState + 0x80;

    ok &= required(Weapon.hActiveWeapon, ParseClassField(clientJson, "CPlayer_WeaponServices", "m_hActiveWeapon"), "CPlayer_WeaponServices::m_hActiveWeapon");
    ok &= required(Weapon.AttributeManager, ParseClassField(clientJson, "C_EconEntity", "m_AttributeManager"), "C_EconEntity::m_AttributeManager");
    ok &= required(Weapon.Item, ParseClassField(clientJson, "C_AttributeContainer", "m_Item"), "C_AttributeContainer::m_Item");
    ok &= required(Weapon.ItemDefinitionIndex, ParseClassField(clientJson, "C_EconItemView", "m_iItemDefinitionIndex"), "C_EconItemView::m_iItemDefinitionIndex");

    if (statusMessage)
    {
        if (ok)
            *statusMessage = online ? "offsets updated online" : "using cached offsets";
        else
        {
            std::ostringstream message;
            message << "current offset data was incomplete";
            if (!missing.empty())
            {
                message << "\n\nMissing:";
                for (const auto& name : missing)
                    message << "\n- " << name;
            }
            *statusMessage = message.str();
        }
    }
    return ok;
}
