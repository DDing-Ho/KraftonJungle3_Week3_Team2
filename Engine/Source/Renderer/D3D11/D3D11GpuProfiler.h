#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Core/HAL/PlatformTypes.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/Types/GpuProfilerTypes.h"

#include <memory>

class FD3D11RHI;
class FManualMemoryCategoryHandle;

class FD3D11GpuProfiler
{
  public:
    static constexpr uint32 CaptureBufferCount = 8;
    static constexpr uint32 MaxSamplesPerCapture = 2048;
    static constexpr uint32 MaxHistoryCount = 120;

  public:
    FD3D11GpuProfiler();
    ~FD3D11GpuProfiler();

    bool Initialize(FD3D11RHI* InRHI);
    void Shutdown();

    void SetEnabled(bool bInEnabled);
    bool IsEnabled() const { return bEnabled; }

    void SetPaused(bool bInPaused);
    bool IsPaused() const { return bPaused; }

    bool IsCapturing() const { return bEnabled && !bPaused && ActiveCapture != nullptr; }
    bool CanCapture() const { return bEnabled && !bPaused && RHI != nullptr; }

    bool BeginCapture(const FString& InCaptureLabel, uint64 InCaptureId);
    void EndCapture();

    bool BeginScope(const FString& InName, const FString& InCategory,
                    EGpuProfileDrawType InDrawType = EGpuProfileDrawType::None,
                    uint32 InElementCount = 0, uint32 InInstanceCount = 0);
    void EndScope();

    void ResolveCompletedCaptures();
    void Clear();

    const FGpuProfileFrameSnapshot& GetLatestSnapshot() const { return LatestSnapshot; }
    const TArray<float>& GetFrameHistoryMs() const { return FrameHistoryMs; }

  private:
    struct FPendingSample
    {
        FString Name;
        FString Category;
        int32   ParentIndex = -1;

        EGpuProfileDrawType DrawType = EGpuProfileDrawType::None;
        uint32              ElementCount = 0;
        uint32              InstanceCount = 0;

        bool bEnded = false;

        TComPtr<ID3D11Query> StartQuery;
        TComPtr<ID3D11Query> EndQuery;
    };

    struct FCapture
    {
        FString CaptureLabel;
        uint64  CaptureId = 0;
        uint32  OverflowCount = 0;

        bool bActive = false;
        bool bReadyForResolve = false;

        TComPtr<ID3D11Query> DisjointQuery;
        TArray<FPendingSample> Samples;
        TArray<int32>          ScopeStack;

        void Reset()
        {
            CaptureLabel.clear();
            CaptureId = 0;
            OverflowCount = 0;
            bActive = false;
            bReadyForResolve = false;
            DisjointQuery.Reset();
            Samples.clear();
            ScopeStack.clear();
        }
    };

  private:
    bool CreateTimestampQuery(TComPtr<ID3D11Query>& OutQuery) const;
    bool CreateDisjointQuery(TComPtr<ID3D11Query>& OutQuery) const;
    int32 ResolveParentIndex(const FCapture& InCapture) const;
    void  PushFrameHistory(float InGpuTimeMs);

  private:
    std::unique_ptr<FManualMemoryCategoryHandle> MemoryTrackHandle;
    FD3D11RHI* RHI = nullptr;

    bool bEnabled = false;
    bool bPaused = false;

    FCapture  Captures[CaptureBufferCount];
    FCapture* ActiveCapture = nullptr;
    uint32    NextCaptureSlot = 0;
    uint64    LastIssuedCaptureId = 0;

    FGpuProfileFrameSnapshot LatestSnapshot;
    TArray<float>            FrameHistoryMs;
};

class FGpuProfileScopeGuard
{
  public:
    FGpuProfileScopeGuard(FD3D11GpuProfiler* InProfiler, const FString& InName,
                          const FString& InCategory,
                          EGpuProfileDrawType InDrawType = EGpuProfileDrawType::None,
                          uint32 InElementCount = 0, uint32 InInstanceCount = 0)
        : Profiler(InProfiler)
    {
        if (Profiler != nullptr)
        {
            bScopePushed = Profiler->BeginScope(InName, InCategory, InDrawType, InElementCount,
                                                InInstanceCount);
        }
    }

    ~FGpuProfileScopeGuard()
    {
        if (Profiler != nullptr && bScopePushed)
        {
            Profiler->EndScope();
        }
    }

    FGpuProfileScopeGuard(const FGpuProfileScopeGuard&) = delete;
    FGpuProfileScopeGuard& operator=(const FGpuProfileScopeGuard&) = delete;

  private:
    FD3D11GpuProfiler* Profiler = nullptr;
    bool               bScopePushed = false;
};
