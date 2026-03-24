#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Core/HAL/PlatformTypes.h"

enum class EGpuProfileDrawType : uint8
{
    None,
    Draw,
    DrawIndexed,
    DrawIndexedInstanced,
    Flush
};

struct FGpuProfileSample
{
    FString Name;
    FString Category;
    int32   ParentIndex = -1;

    EGpuProfileDrawType DrawType = EGpuProfileDrawType::None;
    float               GpuTimeMs = 0.0f;
    uint32              ElementCount = 0;
    uint32              InstanceCount = 0;
};

struct FGpuProfileFrameSnapshot
{
    FString CaptureLabel;
    uint64  CaptureId = 0;
    uint32  CaptureLatency = 0;
    uint32  OverflowCount = 0;
    float   TotalGpuTimeMs = 0.0f;

    TArray<FGpuProfileSample> Samples;

    bool IsValid() const { return !Samples.empty(); }
    void Reset()
    {
        CaptureLabel.clear();
        CaptureId = 0;
        CaptureLatency = 0;
        OverflowCount = 0;
        TotalGpuTimeMs = 0.0f;
        Samples.clear();
    }
};
