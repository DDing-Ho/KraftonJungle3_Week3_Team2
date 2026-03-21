#pragma once

#include "Core/CoreMinimal.h"
#include "ApplicationCore/Input/IInputContext.h"

namespace Engine::ApplicationCore
{
    class ENGINE_API FInputRouter
    {
    public:
        FInputRouter() = default;
        ~FInputRouter() = default;
        
        void AddContext(IInputContext * Context);
        bool RouteEvent(const FInputEvent & Event, const FInputState & State);
        void TickContexts(const FInputState & State);
        
    private:
        TArray<IInputContext *> Contexts;
    };
}