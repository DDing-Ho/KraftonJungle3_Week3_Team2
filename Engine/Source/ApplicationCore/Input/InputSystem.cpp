#include "InputSystem.h"

void Engine::ApplicationCore::InputSystem::BeginFrame()
{
    State.BeginFrame();
}

void Engine::ApplicationCore::InputSystem::ProcessingWin32Message(UINT Msg, WPARAM WParam,
    LPARAM LParam)
{
    //  Windows에 있는거 옮길지 고민해보기.
}

bool Engine::ApplicationCore::InputSystem::PollEvent(FInputEvent& OutEvent)
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

void Engine::ApplicationCore::InputSystem::UpdateModifiers()
{
    // State.Modifiers.bCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    // State.Modifiers.bShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    // State.Modifiers.bAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
}