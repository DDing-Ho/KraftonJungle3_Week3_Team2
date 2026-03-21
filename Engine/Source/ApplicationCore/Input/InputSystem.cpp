#include "InputSystem.h"

void Engine::ApplicationCore::FInputSystem::BeginFrame()
{
    State.BeginFrame();
}

void Engine::ApplicationCore::FInputSystem::ProcessingWin32Message(UINT Msg, WPARAM WParam,
    LPARAM LParam)
{
    //  Windows에 있는거 옮길지 고민해보기.
}

bool Engine::ApplicationCore::FInputSystem::PollEvent(FInputEvent& OutEvent)
{
    if (EventQueue.empty())
    {
        return false;
    }
    
    //  미구현
    // OutEvent = EventQueue.front();
    EventQueue.pop();
    return true;
}

void Engine::ApplicationCore::FInputSystem::UpdateModifiers()
{
    // State.Modifiers.bCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    // State.Modifiers.bShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    // State.Modifiers.bAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
}