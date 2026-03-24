#include "Renderer/D3D11/D3D11GpuProfiler.h"

#include "Renderer/D3D11/D3D11RHI.h"
#include "Engine/MemoryProfiler.h"

#include <algorithm>

namespace
{
    template <typename T> void UntrackComResource(TComPtr<T>& InResource)
    {
        FMemoryProfiler::Get().UnregisterGpuResource(InResource.Get());
        InResource.Reset();
    }
}

FD3D11GpuProfiler::FD3D11GpuProfiler()
    : MemoryTrackHandle(std::make_unique<FManualMemoryCategoryHandle>(
          "Renderer/FD3D11GpuProfiler", sizeof(FD3D11GpuProfiler)))
{
}

FD3D11GpuProfiler::~FD3D11GpuProfiler() = default;

bool FD3D11GpuProfiler::Initialize(FD3D11RHI* InRHI)
{
    if (InRHI == nullptr)
    {
        return false;
    }

    RHI = InRHI;
    bEnabled = false;
    bPaused = false;
    ActiveCapture = nullptr;
    NextCaptureSlot = 0;
    LastIssuedCaptureId = 0;
    LatestSnapshot.Reset();
    FrameHistoryMs.clear();

    for (FCapture& Capture : Captures)
    {
        UntrackComResource(Capture.DisjointQuery);
        for (FPendingSample& Sample : Capture.Samples)
        {
            UntrackComResource(Sample.StartQuery);
            UntrackComResource(Sample.EndQuery);
        }
        Capture.Reset();
    }

    return true;
}

void FD3D11GpuProfiler::Shutdown()
{
    Clear();
    RHI = nullptr;
}

void FD3D11GpuProfiler::SetEnabled(bool bInEnabled)
{
    if (bEnabled == bInEnabled)
    {
        return;
    }

    if (!bInEnabled && ActiveCapture != nullptr)
    {
        EndCapture();
    }

    bEnabled = bInEnabled;
}

void FD3D11GpuProfiler::SetPaused(bool bInPaused)
{
    if (bPaused == bInPaused)
    {
        return;
    }

    if (bInPaused && ActiveCapture != nullptr)
    {
        EndCapture();
    }

    bPaused = bInPaused;
}

bool FD3D11GpuProfiler::BeginCapture(const FString& InCaptureLabel, uint64 InCaptureId)
{
    if (!CanCapture())
    {
        return false;
    }

    if (ActiveCapture != nullptr)
    {
        EndCapture();
    }

    FCapture& Capture = Captures[NextCaptureSlot];
    NextCaptureSlot = (NextCaptureSlot + 1) % CaptureBufferCount;

    Capture.Reset();
    Capture.CaptureLabel = InCaptureLabel;
    Capture.CaptureId = InCaptureId;
    Capture.bActive = true;

    if (!CreateDisjointQuery(Capture.DisjointQuery))
    {
        UntrackComResource(Capture.DisjointQuery);
        Capture.Reset();
        return false;
    }

    RHI->BeginQuery(Capture.DisjointQuery.Get());
    ActiveCapture = &Capture;
    LastIssuedCaptureId = InCaptureId;
    return true;
}

void FD3D11GpuProfiler::EndCapture()
{
    if (ActiveCapture == nullptr || RHI == nullptr)
    {
        ActiveCapture = nullptr;
        return;
    }

    while (!ActiveCapture->ScopeStack.empty())
    {
        EndScope();
    }

    RHI->EndQuery(ActiveCapture->DisjointQuery.Get());
    ActiveCapture->bActive = false;
    ActiveCapture->bReadyForResolve = true;
    ActiveCapture = nullptr;
}

bool FD3D11GpuProfiler::BeginScope(const FString& InName, const FString& InCategory,
                                   EGpuProfileDrawType InDrawType, uint32 InElementCount,
                                   uint32 InInstanceCount)
{
    if (ActiveCapture == nullptr || RHI == nullptr)
    {
        return false;
    }

    if (ActiveCapture->Samples.size() >= MaxSamplesPerCapture)
    {
        ++ActiveCapture->OverflowCount;
        ActiveCapture->ScopeStack.push_back(-1);
        return true;
    }

    FPendingSample Sample;
    Sample.Name = InName;
    Sample.Category = InCategory;
    Sample.ParentIndex = ResolveParentIndex(*ActiveCapture);
    Sample.DrawType = InDrawType;
    Sample.ElementCount = InElementCount;
    Sample.InstanceCount = InInstanceCount;

    if (!CreateTimestampQuery(Sample.StartQuery) || !CreateTimestampQuery(Sample.EndQuery))
    {
        ++ActiveCapture->OverflowCount;
        ActiveCapture->ScopeStack.push_back(-1);
        return true;
    }

    const int32 SampleIndex = static_cast<int32>(ActiveCapture->Samples.size());
    ActiveCapture->Samples.push_back(std::move(Sample));
    ActiveCapture->ScopeStack.push_back(SampleIndex);

    RHI->EndQuery(ActiveCapture->Samples[SampleIndex].StartQuery.Get());
    return true;
}

void FD3D11GpuProfiler::EndScope()
{
    if (ActiveCapture == nullptr || RHI == nullptr || ActiveCapture->ScopeStack.empty())
    {
        return;
    }

    const int32 SampleIndex = ActiveCapture->ScopeStack.back();
    ActiveCapture->ScopeStack.pop_back();

    if (SampleIndex < 0 || SampleIndex >= static_cast<int32>(ActiveCapture->Samples.size()))
    {
        return;
    }

    FPendingSample& Sample = ActiveCapture->Samples[SampleIndex];
    RHI->EndQuery(Sample.EndQuery.Get());
    Sample.bEnded = true;
}

void FD3D11GpuProfiler::ResolveCompletedCaptures()
{
    if (RHI == nullptr)
    {
        return;
    }

    for (FCapture& Capture : Captures)
    {
        if (!Capture.bReadyForResolve || Capture.bActive || Capture.DisjointQuery == nullptr)
        {
            continue;
        }

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointData = {};
        if (RHI->GetQueryData(Capture.DisjointQuery.Get(), &DisjointData, sizeof(DisjointData), 0) !=
            S_OK)
        {
            continue;
        }

        bool            bAllSamplesReady = true;
        uint64          StartTimestamp = 0;
        uint64          EndTimestamp = 0;
        FGpuProfileFrameSnapshot Snapshot;
        Snapshot.CaptureLabel = Capture.CaptureLabel;
        Snapshot.CaptureId = Capture.CaptureId;
        Snapshot.CaptureLatency =
            (LastIssuedCaptureId > Capture.CaptureId)
                ? static_cast<uint32>(LastIssuedCaptureId - Capture.CaptureId)
                : 0;
        Snapshot.OverflowCount = Capture.OverflowCount;
        Snapshot.Samples.reserve(Capture.Samples.size());

        for (const FPendingSample& Sample : Capture.Samples)
        {
            if (!Sample.bEnded || Sample.StartQuery == nullptr || Sample.EndQuery == nullptr)
            {
                bAllSamplesReady = false;
                break;
            }

            if (RHI->GetQueryData(Sample.StartQuery.Get(), &StartTimestamp, sizeof(StartTimestamp),
                                  0) != S_OK ||
                RHI->GetQueryData(Sample.EndQuery.Get(), &EndTimestamp, sizeof(EndTimestamp), 0) !=
                    S_OK)
            {
                bAllSamplesReady = false;
                break;
            }

            FGpuProfileSample ResolvedSample;
            ResolvedSample.Name = Sample.Name;
            ResolvedSample.Category = Sample.Category;
            ResolvedSample.ParentIndex = Sample.ParentIndex;
            ResolvedSample.DrawType = Sample.DrawType;
            ResolvedSample.ElementCount = Sample.ElementCount;
            ResolvedSample.InstanceCount = Sample.InstanceCount;

            if (!DisjointData.Disjoint && EndTimestamp >= StartTimestamp &&
                DisjointData.Frequency > 0)
            {
                const double Delta = static_cast<double>(EndTimestamp - StartTimestamp);
                ResolvedSample.GpuTimeMs =
                    static_cast<float>((Delta * 1000.0) / static_cast<double>(DisjointData.Frequency));
            }

            Snapshot.TotalGpuTimeMs += ResolvedSample.GpuTimeMs;
            Snapshot.Samples.push_back(std::move(ResolvedSample));
        }

        if (!bAllSamplesReady)
        {
            continue;
        }

        LatestSnapshot = std::move(Snapshot);
        if (LatestSnapshot.CaptureLabel == "Frame")
        {
            PushFrameHistory(LatestSnapshot.TotalGpuTimeMs);
        }

        UntrackComResource(Capture.DisjointQuery);
        for (FPendingSample& Sample : Capture.Samples)
        {
            UntrackComResource(Sample.StartQuery);
            UntrackComResource(Sample.EndQuery);
        }
        Capture.Reset();
    }
}

void FD3D11GpuProfiler::Clear()
{
    if (ActiveCapture != nullptr)
    {
        EndCapture();
    }

    for (FCapture& Capture : Captures)
    {
        UntrackComResource(Capture.DisjointQuery);
        for (FPendingSample& Sample : Capture.Samples)
        {
            UntrackComResource(Sample.StartQuery);
            UntrackComResource(Sample.EndQuery);
        }
        Capture.Reset();
    }

    LatestSnapshot.Reset();
    FrameHistoryMs.clear();
    NextCaptureSlot = 0;
}

bool FD3D11GpuProfiler::CreateTimestampQuery(TComPtr<ID3D11Query>& OutQuery) const
{
    if (RHI == nullptr)
    {
        return false;
    }

    D3D11_QUERY_DESC Desc = {};
    Desc.Query = D3D11_QUERY_TIMESTAMP;
    Desc.MiscFlags = 0;
    return RHI->CreateQuery(Desc, OutQuery.GetAddressOf());
}

bool FD3D11GpuProfiler::CreateDisjointQuery(TComPtr<ID3D11Query>& OutQuery) const
{
    if (RHI == nullptr)
    {
        return false;
    }

    D3D11_QUERY_DESC Desc = {};
    Desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    Desc.MiscFlags = 0;
    return RHI->CreateQuery(Desc, OutQuery.GetAddressOf());
}

int32 FD3D11GpuProfiler::ResolveParentIndex(const FCapture& InCapture) const
{
    for (auto Iterator = InCapture.ScopeStack.rbegin(); Iterator != InCapture.ScopeStack.rend();
         ++Iterator)
    {
        if (*Iterator >= 0)
        {
            return *Iterator;
        }
    }

    return -1;
}

void FD3D11GpuProfiler::PushFrameHistory(float InGpuTimeMs)
{
    FrameHistoryMs.push_back(InGpuTimeMs);
    if (FrameHistoryMs.size() > MaxHistoryCount)
    {
        FrameHistoryMs.erase(FrameHistoryMs.begin());
    }
}
