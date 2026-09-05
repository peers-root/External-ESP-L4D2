#include <windows.h>
#include <tlhelp32.h> // snapshot process memory
#include <stdio.h>
#include <string.h>
#include "math.h"

DWORD engineBase; // target module
DWORD serverBase; // target module

int main(){

    HWND gameHandle = FindWindowA(NULL, "Left 4 Dead 2 - Direct3D 9");
    DWORD pid;
    
    GetWindowThreadProcessId(gameHandle, &pid);
    
    if (gameHandle == NULL){
        printf("Process not found!\n");
        printf("Check if the process is running & try again.\n\n");
        Sleep(3000);
        return 1;
    }
    
    else{
        printf("Process ID found!\n");
        printf("PID: %d\n", pid); 
        Sleep(3000); 
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid); // "TH32CS_SNAPMODULE" OR "TH32CS_SNAPMODULE32" 32bit OR 64bit process

    if (snapshot == INVALID_HANDLE_VALUE){
            printf("Snapshot failed!");
            return 1;
        }

    MODULEENTRY32 m32;
    m32.dwSize = sizeof(MODULEENTRY32);
    
    if (Module32First(snapshot, &m32)) {
        do{
            printf("\nModule Found! --> %s", m32.szModule);
            
            if (strcmp(m32.szModule, "engine.dll") == 0){ 
                engineBase = (DWORD)m32.modBaseAddr;
                printf("\nTarget Module Found! --> %p", m32.modBaseAddr);
            }
            if (strcmp(m32.szModule, "server.dll") == 0){
                serverBase = (DWORD)m32.modBaseAddr;
                printf("\nTarget Module Found! --> %p", m32.modBaseAddr);
            }

        }while (Module32Next(snapshot, &m32));
    }

    CloseHandle(snapshot); // end ptr itteration once target .dll's are found.

    Sleep(3000); 

    DWORD entityList = serverBase + 0x7E0774; // entityList Pointer = target.dll + 0x00
    DWORD viewMatrix = engineBase + 0x601F9C; // viewMatrix Pointer = target2.dll + 0x00
    DWORD teamOffset = serverBase + 0x238;
    DWORD entityPtr;

    int wteam;

    HANDLE hprocess;

    hprocess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    
    if (hprocess == NULL) {
        printf("\nFailed to open process!\n");
        CloseHandle(snapshot); 
        return 1;
    }

    HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN hPen = CreatePen(PS_SOLID, 4, RGB(0, 255, 1));

    if (hPen == NULL){
        printf("Failed to draw ESP. . [HPEN ERROR]");
        CloseHandle(hprocess);
        return 1;
    }
    
    while (TRUE){
        HDC hdc = GetDC(gameHandle);

        HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, hBrush);

        RECT rect;
        GetClientRect(gameHandle, &rect);

        struct vector screenRes = { (float)(rect.right - rect.left), (float)(rect.bottom - rect.top), 0 }; // get screen res
        struct matrix_4x4 viewMatrixData; 
        
        // Read view matrix once per frame
        ReadProcessMemory(hprocess, (LPCVOID)viewMatrix, &viewMatrixData, sizeof(viewMatrixData), NULL);

        for (int i = 0; i < 65; i++) { // Entity Itteration loop

            DWORD entityPtr;

            if (!ReadProcessMemory(hprocess, (LPCVOID)(entityList + (i * 0x10)), &entityPtr, sizeof(entityPtr), NULL)) continue;

            if (entityPtr < 0x10000)
                continue;

            if (!ReadProcessMemory(hprocess, (LPCVOID)(entityPtr + 0x238), &wteam, sizeof(int), NULL)) continue;

            if (wteam != 2 && wteam != 3) continue;

            float x, y, z;

            if (!ReadProcessMemory(hprocess, (LPCVOID)(entityPtr + 0x02CC), &x, sizeof(float), NULL)) continue;
            if (!ReadProcessMemory(hprocess, (LPCVOID)(entityPtr + 0x02D0), &y, sizeof(float), NULL)) continue; 
            if (!ReadProcessMemory(hprocess, (LPCVOID)(entityPtr + 0x02D4), &z, sizeof(float), NULL)) continue;  

            // Entity's feet/origin
            struct vector entityPos = {
                x,
                y,
                z
            };

            // Point above the entity
            struct vector entityTop = {
                x,
                y,
                z + 72.0f
            };

            struct vector screenFeet;
            struct vector screenTop;

            // Project feet
            if (!world_to_screen(
                &entityPos,
                &viewMatrixData,
                &screenFeet,
                &screenRes
            ))
                continue;

            // Project top
            if (!world_to_screen(
                &entityTop,
                &viewMatrixData,
                &screenTop,
                &screenRes
            ))
                continue;

            // Calculate box dimensions from projected points
            float height = screenFeet.y - screenTop.y;
            float width = height * 0.5f;

            Rectangle(
                hdc,
                (int)(screenTop.x - width / 2),
                (int)screenTop.y,
                (int)(screenTop.x + width / 2),
                (int)screenFeet.y
            );
        }

        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);

        ReleaseDC(gameHandle, hdc);

        Sleep(0);
    }

    DeleteObject(hPen);
    CloseHandle(hprocess);

    return 0;
}