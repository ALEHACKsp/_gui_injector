#include <Windows.h>
#include <TlHelp32.h>
#include <vector>
#include <string>
#include <fstream>
#include "process.h"


enum ARCH getFileArch(const char* szDllFile)
{
    BYTE* pSrcData = nullptr;
    IMAGE_NT_HEADERS* pOldNtHeader = nullptr;
    IMAGE_FILE_HEADER* pOldFileHeader = nullptr;

    if (!GetFileAttributesA(szDllFile))
    {
        printf("File doesn't exist\n");
        return NONE;
    }

    std::ifstream File(szDllFile, std::ios::binary | std::ios::ate);

    if (File.fail())
    {
        printf("Opening the file failed: %X\n", (DWORD)File.rdstate());
        File.close();
        return NONE;
    }

    auto FileSize = File.tellg();
    if (FileSize < 0x1000)
    {
        printf("Filesize is invalid.\n");
        File.close();
        return NONE;
    }

    pSrcData = new BYTE[static_cast<UINT_PTR>(FileSize)];
    if (!pSrcData)
    {
        printf("Memory allocating failed\n");
        File.close();
        return NONE;
    }

    File.seekg(0, std::ios::beg);
    File.read(reinterpret_cast<char*>(pSrcData), FileSize);
    File.close();

    if (reinterpret_cast<IMAGE_DOS_HEADER*>(pSrcData)->e_magic != 0x5A4D) //"MZ"
    {
        printf("Invalid file\n");
        delete[] pSrcData;
        return NONE;
    }

    pOldNtHeader = reinterpret_cast<IMAGE_NT_HEADERS*>(pSrcData + reinterpret_cast<IMAGE_DOS_HEADER*>(pSrcData)->e_lfanew);
    pOldFileHeader = &pOldNtHeader->FileHeader;

    if (pOldFileHeader->Machine == IMAGE_FILE_MACHINE_AMD64)
    {
        delete[] pSrcData;
        return X64;
    }
    else if (pOldFileHeader->Machine == IMAGE_FILE_MACHINE_I386)
    {
        delete[] pSrcData;
        return X86;
    }

    delete[] pSrcData;
    return NONE;
}

enum ARCH getProcArch(const int pid)
{
    HANDLE hOpenProc = OpenProcess(PROCESS_QUERY_INFORMATION, NULL, pid);
    if (hOpenProc != NULL)
    {
        BOOL tempWow64 = FALSE;

        BOOL bIsWow = IsWow64Process(hOpenProc, &tempWow64);
        if (bIsWow != 0)
        {
            if (tempWow64 == TRUE)
            {
                return X86;
            }
            else
            {
                return X64;
            }
            return NONE;
        }
        CloseHandle(hOpenProc);
    }
    return NONE;
}

Process_Struct getProcessByName(const char* proc)
{
    PROCESSENTRY32 procEntry = { 0 };
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    char name[250]; 
    Process_Struct ps;
    memset(&ps, 0, sizeof(Process_Struct));

    if (hSnapshot)
    {
        procEntry.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnapshot, &procEntry))
        {
            do
            {
                int ret = wcstombs(name, procEntry.szExeFile, sizeof(name));
                if (!strcmp(name, proc))
                {

                    ps.arch = getProcArch(procEntry.th32ProcessID);
                    if (ps.arch != ARCH::NONE)
                    {
                        ps.pid = procEntry.th32ProcessID;
                        strcpy(ps.name, name);
                        break;
                    }
                }

            } while (Process32Next(hSnapshot, &procEntry));
        }
    }
    CloseHandle(hSnapshot);
    return ps;
}

Process_Struct getProcessByPID(const int pid)
{
    PROCESSENTRY32 procEntry = { 0 };
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    char name[250];
    Process_Struct ps;
    memset(&ps, 0, sizeof(Process_Struct));

    if (hSnapshot)
    {
        procEntry.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnapshot, &procEntry))
        {
            do
            {
                if(procEntry.th32ProcessID == pid)
                {

                    ps.arch = getProcArch(procEntry.th32ProcessID);
                    if (ps.arch != ARCH::NONE)
                    {
                        ps.pid = procEntry.th32ProcessID;
                        int ret = wcstombs(name, procEntry.szExeFile, sizeof(name));
                        strcpy(ps.name, name);
                        break;
                    }
                }

            } while (Process32Next(hSnapshot, &procEntry));
        }
    }
    CloseHandle(hSnapshot);
    return ps;
}

bool getProcessList(std::vector<Process_Struct>& pl)
{

    PROCESSENTRY32 procEntry = { 0 };
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    if (hSnapshot)
    {
        procEntry.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnapshot, &procEntry))
        {
            do
            {
                Process_Struct ps_item;
                memset(&ps_item, 0, sizeof(Process_Struct));

                ps_item.pid = procEntry.th32ProcessID;
                ps_item.arch = getProcArch(procEntry.th32ProcessID);
                int ret = wcstombs(ps_item.name, procEntry.szExeFile, sizeof(ps_item.name));

                pl.push_back(ps_item);

            } while (Process32Next(hSnapshot, &procEntry));
        }
    }
    CloseHandle(hSnapshot);
    return true;
}