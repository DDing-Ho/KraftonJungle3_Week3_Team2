#pragma once

#include "Core/CoreMinimal.h"

class FEditorEngineLoop
{
public:
    bool PreInit(HINSTANCE HInstance, uint32 NCmdShow);
    int32 Run();
    void Shutdown();
    
private:
    static LRESULT CALLBACK StaticWndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam);
    LRESULT WndProc(HWND HWnd, uint32 Message, WPARAM WParam, LPARAM LParam);
    
    void Tick();
    
public:
    
private:
    HWND HWindow = nullptr;
    
    bool bIsExit = false;
};
