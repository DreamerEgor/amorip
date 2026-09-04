#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

/*
    Read-only process helper based on the original TKazer CS2_External project.
    The process handle intentionally requests no VM_WRITE / VM_OPERATION rights.
*/

enum StatusCode
{
    SUCCEED,
    FAILE_PROCESSID,
    FAILE_HPROCESS,
    FAILE_MODULE,
};

class ProcessManager
{
private:
    bool Attached = false;

public:
    HANDLE hProcess = nullptr;
    DWORD ProcessID = 0;
    std::uintptr_t ModuleAddress = 0;

    ~ProcessManager()
    {
        Detach();
    }

    StatusCode Attach(const std::string& processName)
    {
        Detach();
        ProcessID = GetProcessID(processName);
        if (!ProcessID)
            return FAILE_PROCESSID;

        hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, ProcessID);
        if (!hProcess)
            return FAILE_HPROCESS;

        ModuleAddress = reinterpret_cast<std::uintptr_t>(GetProcessModuleHandle(processName));
        if (!ModuleAddress)
        {
            Detach();
            return FAILE_MODULE;
        }

        Attached = true;
        return SUCCEED;
    }

    void Detach()
    {
        if (hProcess)
            CloseHandle(hProcess);
        hProcess = nullptr;
        ProcessID = 0;
        ModuleAddress = 0;
        Attached = false;
    }

    bool IsActive() const
    {
        if (!Attached || !hProcess)
            return false;
        DWORD exitCode = 0;
        return GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE;
    }

    template <typename T>
    bool ReadMemory(std::uintptr_t address, T& value) const
    {
        if (!hProcess || !address)
            return false;
        SIZE_T bytesRead = 0;
        return ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address), &value, sizeof(T), &bytesRead)
            && bytesRead == sizeof(T);
    }

    template <typename T>
    bool ReadMemory(std::uintptr_t address, T& value, std::size_t size) const
    {
        if (!hProcess || !address || size > sizeof(T))
            return false;
        SIZE_T bytesRead = 0;
        return ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address), &value, size, &bytesRead)
            && bytesRead == size;
    }

    bool ReadBuffer(std::uintptr_t address, void* buffer, std::size_t size) const
    {
        if (!hProcess || !address || !buffer || !size)
            return false;
        SIZE_T bytesRead = 0;
        return ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead)
            && bytesRead == size;
    }

    std::uintptr_t TraceAddress(std::uintptr_t baseAddress, const std::vector<std::uintptr_t>& offsets) const
    {
        if (!hProcess || !baseAddress)
            return 0;
        if (offsets.empty())
            return baseAddress;

        std::uintptr_t address = 0;
        if (!ReadMemory(baseAddress, address))
            return 0;

        for (std::size_t i = 0; i + 1 < offsets.size(); ++i)
        {
            if (!ReadMemory(address + offsets[i], address))
                return 0;
        }
        return address ? address + offsets.back() : 0;
    }

    DWORD GetProcessID(const std::string& processName) const
    {
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return 0;

        DWORD pid = 0;
        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                char name[MAX_PATH]{};
                WideCharToMultiByte(CP_UTF8, 0, entry.szExeFile, -1, name, MAX_PATH, nullptr, nullptr);
                if (_stricmp(name, processName.c_str()) == 0)
                {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return pid;
    }

    HMODULE GetProcessModuleHandle(const std::string& moduleName) const
    {
        if (!ProcessID)
            return nullptr;

        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, ProcessID);
        if (snapshot == INVALID_HANDLE_VALUE)
            return nullptr;

        HMODULE result = nullptr;
        if (Module32FirstW(snapshot, &entry))
        {
            do
            {
                char name[MAX_PATH]{};
                WideCharToMultiByte(CP_UTF8, 0, entry.szModule, -1, name, MAX_PATH, nullptr, nullptr);
                if (_stricmp(name, moduleName.c_str()) == 0)
                {
                    result = entry.hModule;
                    break;
                }
            } while (Module32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return result;
    }
};

inline ProcessManager ProcessMgr;
