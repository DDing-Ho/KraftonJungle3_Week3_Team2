#pragma once

#include "Core/CoreMinimal.h"
#include "IInputContext.h"

namespace Engine::ApplicationCore
{
    class ENGINE_API InputSystem
    {
    public:
        InputSystem() = default;
        ~InputSystem() = default;
        
        void BeginFrame();
        void ProcessingWin32Message(UINT Msg, WPARAM WParam, LPARAM LParam);
        
        bool PollEvent(FInputEvent & OutEvent);
        const FInputState & GetInputState() const { return State; }
        
    private:
        void UpdateModifiers();
        
    private:
        FInputState State;
        TQueue<FInputEvent> EventQueue;
    };    
}

