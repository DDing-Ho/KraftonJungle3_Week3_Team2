#pragma once

#include "ApplicationCore/Input/InputRouter.h"
#include "Core/CoreMinimal.h"
#include "Renderer/Scene.h"

class FEditor
{
public:
    /* Default Functions */
    void Create(HWND);
    void Release();
    
    void BeginPlay();
    void Tick(float DeltaTime);
    
    void OnWindowResized(float Width, float Height);
    void SetMainLoopFPS(float FPS) { CurFPS = FPS; }
    
    /* From Panel */
    void CreateNewScene();
    void ClearScene();
    
private:
    
    
public:
    
    
private:
    /* Input System */
    Engine::ApplicationCore::FInputRouter FInputRouter;
    
    /* Panel */
    
    /* Gizmo */
    
    /* Scene */
    FScene * CurScene = nullptr;
    
    /* Properties */
    float WindowWidth = 0.0f;
    float WindowHeight = 0.0f;
    
    float CurFPS = 0.0f;    //  Panel에 Display
    
};