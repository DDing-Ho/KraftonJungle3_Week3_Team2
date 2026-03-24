#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/String.h"
#include "Core/EngineAPI.h"
#include "Core/HAL/PlatformTypes.h"
#include "Renderer/D3D11/D3D11Common.h"

#include <cstddef>

enum class EGpuResourceKind : uint8
{
    VertexBuffer,
    IndexBuffer,
    ConstantBuffer,
    Texture2D,
    BackBuffer,
    DepthStencilTexture,
    StagingTexture,
    ShaderResourceView,
    RenderTargetView,
    DepthStencilView,
    SamplerState,
    BlendState,
    RasterizerState,
    DepthStencilState,
    InputLayout,
    VertexShader,
    PixelShader,
    Query,
    SwapChain
};

struct FMemoryStatRow
{
    FString Name;
    uint64  LiveCount = 0;
    uint64  LiveBytes = 0;
    uint64  PeakCount = 0;
    uint64  PeakBytes = 0;
};

struct FGpuMemoryStatRow
{
    FString Name;
    uint64  LiveCount = 0;
    uint64  LiveBytes = 0;
    uint64  PeakCount = 0;
    uint64  PeakBytes = 0;
    bool    bTracksBytes = true;
};

struct FMemoryProfilerSnapshot
{
    uint64 CpuLiveBytes = 0;
    uint64 CpuPeakBytes = 0;
    uint64 GpuLiveBytes = 0;
    uint64 GpuPeakBytes = 0;
    uint64 CombinedLiveBytes = 0;
    uint64 CombinedPeakBytes = 0;

    TArray<FMemoryStatRow>    UObjectClasses;
    TArray<FMemoryStatRow>    ManualCategories;
    TArray<FGpuMemoryStatRow> GpuResources;
};

class ENGINE_API FMemoryProfiler
{
  public:
    static constexpr uint32 MaxHistorySamples = 300;

  public:
    static FMemoryProfiler& Get();

    void RegisterUObjectAllocation(const char* InTypeName, size_t InSize);
    void RegisterUObjectFree(const char* InTypeName, size_t InSize);

    void RegisterManualCategoryAllocation(const FString& InCategory, size_t InSize);
    void RegisterManualCategoryFree(const FString& InCategory, size_t InSize);

    void RegisterGpuBuffer(ID3D11Buffer* InBuffer, EGpuResourceKind InKind, const char* InLabel);
    void RegisterGpuTexture2D(ID3D11Texture2D* InTexture, EGpuResourceKind InKind,
                              const char* InLabel);
    void RegisterGpuCountOnly(const void* InResourceKey, EGpuResourceKind InKind,
                              const char* InLabel);
    void UnregisterGpuResource(const void* InResourceKey);

    void SampleHistoryFrame();
    void ClearHistory();

    FMemoryProfilerSnapshot BuildSnapshot() const;

    const TArray<float>& GetCpuHistoryMb() const { return CpuHistoryMb; }
    const TArray<float>& GetGpuHistoryMb() const { return GpuHistoryMb; }
    const TArray<float>& GetCombinedHistoryMb() const { return CombinedHistoryMb; }

  private:
    struct FStatBucket
    {
        uint64 LiveCount = 0;
        uint64 LiveBytes = 0;
        uint64 PeakCount = 0;
        uint64 PeakBytes = 0;
    };

    struct FGpuResourceEntry
    {
        EGpuResourceKind Kind = EGpuResourceKind::Texture2D;
        uint64           Bytes = 0;
        FString          Label;
        bool             bTracksBytes = true;
    };

  private:
    void RegisterGpuResourceInternal(const void* InResourceKey, EGpuResourceKind InKind,
                                     uint64 InBytes, bool bInTracksBytes,
                                     const char* InLabel);
    void UpdateCpuPeak();
    void UpdateGpuPeak();
    void UpdateCombinedPeak();

    static FString GetTypeNameOrDefault(const char* InTypeName);
    static FString GetGpuResourceKindName(EGpuResourceKind InKind);
    static bool    DoesGpuKindTrackBytes(EGpuResourceKind InKind);
    static uint64  EstimateBufferBytes(ID3D11Buffer* InBuffer);
    static uint64  EstimateTexture2DBytes(ID3D11Texture2D* InTexture);
    static uint64  EstimateTexture2DBytes(const D3D11_TEXTURE2D_DESC& InDesc);
    static uint32  GetBitsPerPixel(DXGI_FORMAT InFormat);

    template <typename TRowArray, typename TMapType>
    static void BuildStatRows(const TMapType& InBuckets, TRowArray& OutRows)
    {
        OutRows.clear();
        OutRows.reserve(InBuckets.size());

        for (const auto& [Name, Bucket] : InBuckets)
        {
            if (Bucket.LiveCount == 0 && Bucket.PeakCount == 0 && Bucket.LiveBytes == 0 &&
                Bucket.PeakBytes == 0)
            {
                continue;
            }

            typename TRowArray::value_type Row;
            Row.Name = Name;
            Row.LiveCount = Bucket.LiveCount;
            Row.LiveBytes = Bucket.LiveBytes;
            Row.PeakCount = Bucket.PeakCount;
            Row.PeakBytes = Bucket.PeakBytes;
            OutRows.push_back(std::move(Row));
        }
    }

  private:
    TMap<FString, FStatBucket> UObjectBuckets;
    TMap<FString, FStatBucket> ManualBuckets;
    TMap<FString, FStatBucket> GpuBuckets;
    TMap<const void*, FGpuResourceEntry> GpuResources;

    uint64 CpuUObjectLiveBytes = 0;
    uint64 CpuManualLiveBytes = 0;
    uint64 CpuPeakBytes = 0;
    uint64 GpuLiveBytes = 0;
    uint64 GpuPeakBytes = 0;
    uint64 CombinedPeakBytes = 0;

    TArray<float> CpuHistoryMb;
    TArray<float> GpuHistoryMb;
    TArray<float> CombinedHistoryMb;
};

class ENGINE_API FManualMemoryCategoryHandle
{
  public:
    FManualMemoryCategoryHandle() = default;
    FManualMemoryCategoryHandle(const FString& InCategory, size_t InSize);
    ~FManualMemoryCategoryHandle();

    FManualMemoryCategoryHandle(const FManualMemoryCategoryHandle&) = delete;
    FManualMemoryCategoryHandle& operator=(const FManualMemoryCategoryHandle&) = delete;

    FManualMemoryCategoryHandle(FManualMemoryCategoryHandle&& Other) noexcept;
    FManualMemoryCategoryHandle& operator=(FManualMemoryCategoryHandle&& Other) noexcept;

    void Track(const FString& InCategory, size_t InSize);
    void Reset();

  private:
    FString Category;
    size_t  Size = 0;
    bool    bTracked = false;
};

