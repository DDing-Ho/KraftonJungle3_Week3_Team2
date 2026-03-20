#pragma once

#include <Core/CoreMinimal.h>
#include "Core/HAL/PlatformTime.h"
/*
    Interface for FEditorEngineLoop, FClientEngineLoop
*/
class ENGINE_API FEngineLoop
{
protected :
    virtual bool PreInit(HINSTANCE HInstance, uint32 NCmdShow) = 0;
    virtual int32 Run() = 0;
    virtual void ShutDown() = 0;

protected:
    virtual void Tick() = 0;
    virtual void InitializeForTime() = 0;

};