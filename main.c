#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <tlhelp32.h>
#include <stdint.h>

#include "math.h"

uintptr_t engineBase = 0;
uintptr_t clientBase = 0;

int main()
{
    HWND gameHandle = FindWindowA(NULL, "Left 4 Dead 2 - Direct3D 9");

    if (gameHandle == NULL)
    {
        printf("Process not found!\n");
        printf("Check if Left 4 Dead 2 is running.\n\n");
        Sleep(3000);
        return 1;
    }

    DWORD pid = 0;

    GetWindowThreadProcessId(gameHandle, &pid);

    printf("Process ID found!\n");
    printf("PID: %lu\n", pid);

    HANDLE snapshot =
        CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);

    if (snapshot == INVALID_HANDLE_VALUE)
    {
        printf("Snapshot failed! Error: %lu\n", GetLastError());
        return 1;
    }

    MODULEENTRY32 m32;
    memset(&m32, 0, sizeof(m32));
    m32.dwSize = sizeof(MODULEENTRY32);

    if (Module32First(snapshot, &m32))
    {
        do
        {
            printf("\nModule Found! --> %s", m32.szModule);

            if (_stricmp(m32.szModule, "engine.dll") == 0)
            {
                engineBase = (uintptr_t)m32.modBaseAddr;

                printf(
                    "\nengine.dll Found --> %p",
                    m32.modBaseAddr
                );
            }

            if (_stricmp(m32.szModule, "client.dll") == 0)
            {
                clientBase = (uintptr_t)m32.modBaseAddr;

                printf(
                    "\nclient.dll Found --> %p",
                    m32.modBaseAddr
                );
            }

        } while (Module32Next(snapshot, &m32));
    }

    CloseHandle(snapshot);

    if (engineBase == 0)
    {
        printf("\nengine.dll was not found!\n");
        return 1;
    }

    if (clientBase == 0)
    {
        printf("\nclient.dll was not found!\n");
        return 1;
    }

    /*
        Offsets from the list you posted.
    */

    uintptr_t entityList =
        clientBase + 0x73A574;

    uintptr_t viewMatrix =
        engineBase + 0x4268EC;

    printf("\n\nEntity list: %p\n", (void *)entityList);
    printf("View matrix: %p\n", (void *)viewMatrix);

    /*
        Read-only access.
    */

    HANDLE hprocess =
        OpenProcess(
            PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
            FALSE,
            pid
        );

    if (hprocess == NULL)
    {
        printf(
            "\nFailed to open process! Error: %lu\n",
            GetLastError()
        );

        return 1;
    }

    HBRUSH hBrush =
        (HBRUSH)GetStockObject(NULL_BRUSH);

    HPEN hPen =
        CreatePen(
            PS_SOLID,
            3,
            RGB(0, 255, 1)
        );

    if (hPen == NULL)
    {
        printf("Failed to create drawing pen.\n");
        CloseHandle(hprocess);
        return 1;
    }

    while (TRUE)
    {
        HDC hdc = GetDC(gameHandle);

        if (hdc == NULL)
            break;

        HPEN oldPen =
            (HPEN)SelectObject(hdc, hPen);

        HBRUSH oldBrush =
            (HBRUSH)SelectObject(hdc, hBrush);

        RECT rect;

        memset(&rect, 0, sizeof(rect));

        GetClientRect(
            gameHandle,
            &rect
        );

        struct vector screenRes =
        {
            (float)(rect.right - rect.left),
            (float)(rect.bottom - rect.top),
            0.0f
        };

        struct matrix_4x4 viewMatrixData;

        memset(
            &viewMatrixData,
            0,
            sizeof(viewMatrixData)
        );

        SIZE_T bytesRead = 0;

        /*
            Read view matrix.
        */

        if (!ReadProcessMemory(
                hprocess,
                (LPCVOID)viewMatrix,
                &viewMatrixData,
                sizeof(viewMatrixData),
                &bytesRead))
        {
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);

            ReleaseDC(
                gameHandle,
                hdc
            );

            continue;
        }

        /*
            Entity loop.
        */

        for (int i = 0; i < 65; i++)
        {
            uintptr_t entityPtr = 0;

            uintptr_t entityAddress =
                entityList + (i * 0x10);

            /*
                Read entity pointer.
            */

            if (!ReadProcessMemory(
                    hprocess,
                    (LPCVOID)entityAddress,
                    &entityPtr,
                    sizeof(entityPtr),
                    NULL))
            {
                continue;
            }

            if (entityPtr < 0x10000)
                continue;

            /*
                m_iTeamNum = 0xE4
            */

            int team = 0;

            if (!ReadProcessMemory(
                    hprocess,
                    (LPCVOID)(entityPtr + 0xE4),
                    &team,
                    sizeof(team),
                    NULL))
            {
                continue;
            }

            if (team != 2 && team != 3)
                continue;

            /*
                m_vecOrigin = 0x124
            */

            struct vector entityPos;

            memset(
                &entityPos,
                0,
                sizeof(entityPos)
            );

            if (!ReadProcessMemory(
                    hprocess,
                    (LPCVOID)(entityPtr + 0x124),
                    &entityPos,
                    sizeof(entityPos),
                    NULL))
            {
                continue;
            }

            /*
                Approximate head position.
            */

            struct vector entityTop =
            {
                entityPos.x,
                entityPos.y,
                entityPos.z + 72.0f
            };

            struct vector screenFeet;
            struct vector screenTop;

            memset(
                &screenFeet,
                0,
                sizeof(screenFeet)
            );

            memset(
                &screenTop,
                0,
                sizeof(screenTop)
            );

            if (!world_to_screen(
                    &entityPos,
                    &viewMatrixData,
                    &screenFeet,
                    &screenRes))
            {
                continue;
            }

            if (!world_to_screen(
                    &entityTop,
                    &viewMatrixData,
                    &screenTop,
                    &screenRes))
            {
                continue;
            }

            float height =
                screenFeet.y - screenTop.y;

            if (height <= 0.0f)
                continue;

            float width =
                height * 0.5f;

            Rectangle(
                hdc,
                (int)(screenTop.x - width / 2.0f),
                (int)screenTop.y,
                (int)(screenTop.x + width / 2.0f),
                (int)screenFeet.y
            );
        }

        SelectObject(
            hdc,
            oldPen
        );

        SelectObject(
            hdc,
            oldBrush
        );

        ReleaseDC(
            gameHandle,
            hdc
        );

        Sleep(1);
    }

    DeleteObject(hPen);
    CloseHandle(hprocess);

    return 0;
}