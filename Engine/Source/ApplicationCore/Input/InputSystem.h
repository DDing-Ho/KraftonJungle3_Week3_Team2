#pragma once

#include "Core/CoreMinimal.h"
#include "IInputContext.h"

namespace Engine::ApplicationCore
{
    class ENGINE_API FInputSystem
    {
    public:
        FInputSystem() = default;
        ~FInputSystem() = default;
        
        void BeginFrame();
        void ProcessingWin32Message(UINT Msg, WPARAM WParam, LPARAM LParam);
        
        bool PollEvent(FInputEvent & OutEvent);
        const FInputState & GetInputState() const { return State; }
        
    private:
        void UpdateModifiers();
        
    private:
        FInputState State;
        //  TODO : Build 에러 방지
        TQueue<FInputEvent> EventQueue;
    };    
}

