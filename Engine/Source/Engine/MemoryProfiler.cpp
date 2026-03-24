#include "Core/CoreMinimal.h"
#include "MemoryProfiler.h"

#include <algorithm>

namespace
{
    constexpr double BytesToMegabytes = 1.0 / (1024.0 * 1024.0);

    template <typename TRow>
    void SortRowsByBytesAndName(TArray<TRow>& Rows)
    {
        std::sort(Rows.begin(), Rows.end(),
                  [](const TRow& Left, const TRow& Right)
                  {
                      if (Left.LiveBytes != Right.LiveBytes)
                      {
                          return Left.LiveBytes > Right.LiveBytes;
                      }
                      if (Left.LiveCount != Right.LiveCount)
                      {
                          return Left.LiveCount > Right.LiveCount;
                      }
                      return Left.Name < Right.Name;
                  });
    }

    void PushHistorySample(TArray<float>& History, float Value)
    {
        if (History.size() >= FMemoryProfiler::MaxHistorySamples)
        {
            History.erase(History.begin());
        }
        History.push_back(Value);
    }
} // namespace

FMemoryProfiler& FMemoryProfiler::Get()
{
    static FMemoryProfiler Instance;
    return Instance;
}

void FMemoryProfiler::RegisterUObjectAllocation(const char* InTypeName, size_t InSize)
{
    const FString TypeName = GetTypeNameOrDefault(InTypeName);
    FStatBucket&  Bucket = UObjectBuckets[TypeName];
    Bucket.LiveCount += 1;
    Bucket.LiveBytes += static_cast<uint64>(InSize);
    Bucket.PeakCount = std::max(Bucket.PeakCount, Bucket.LiveCount);
    Bucket.PeakBytes = std::max(Bucket.PeakBytes, Bucket.LiveBytes);

    CpuUObjectLiveBytes += static_cast<uint64>(InSize);
    UpdateCpuPeak();
}

void FMemoryProfiler::RegisterUObjectFree(const char* InTypeName, size_t InSize)
{
    const FString TypeName = GetTypeNameOrDefault(InTypeName);
    auto          It = UObjectBuckets.find(TypeName);
    if (It != UObjectBuckets.end())
    {
        FStatBucket& Bucket = It->second;
        if (Bucket.LiveCount > 0)
        {
            Bucket.LiveCount -= 1;
        }

        const uint64 Size = static_cast<uint64>(InSize);
        Bucket.LiveBytes = (Bucket.LiveBytes > Size) ? (Bucket.LiveBytes - Size) : 0;
    }

    const uint64 Size = static_cast<uint64>(InSize);
    CpuUObjectLiveBytes = (CpuUObjectLiveBytes > Size) ? (CpuUObjectLiveBytes - Size) : 0;
}

void FMemoryProfiler::RegisterManualCategoryAllocation(const FString& InCategory, size_t InSize)
{
    if (InCategory.empty() || InSize == 0)
    {
        return;
    }

    FStatBucket& Bucket = ManualBuckets[InCategory];
    Bucket.LiveCount += 1;
    Bucket.LiveBytes += static_cast<uint64>(InSize);
    Bucket.PeakCount = std::max(Bucket.PeakCount, Bucket.LiveCount);
    Bucket.PeakBytes = std::max(Bucket.PeakBytes, Bucket.LiveBytes);

    CpuManualLiveBytes += static_cast<uint64>(InSize);
    UpdateCpuPeak();
}

void FMemoryProfiler::RegisterManualCategoryFree(const FString& InCategory, size_t InSize)
{
    if (InCategory.empty() || InSize == 0)
    {
        return;
    }

    auto It = ManualBuckets.find(InCategory);
    if (It != ManualBuckets.end())
    {
        FStatBucket& Bucket = It->second;
        if (Bucket.LiveCount > 0)
        {
            Bucket.LiveCount -= 1;
        }

        const uint64 Size = static_cast<uint64>(InSize);
        Bucket.LiveBytes = (Bucket.LiveBytes > Size) ? (Bucket.LiveBytes - Size) : 0;
    }

    const uint64 Size = static_cast<uint64>(InSize);
    CpuManualLiveBytes = (CpuManualLiveBytes > Size) ? (CpuManualLiveBytes - Size) : 0;
}

void FMemoryProfiler::RegisterGpuBuffer(ID3D11Buffer* InBuffer, EGpuResourceKind InKind,
                                        const char* InLabel)
{
    RegisterGpuResourceInternal(InBuffer, InKind, EstimateBufferBytes(InBuffer),
                                DoesGpuKindTrackBytes(InKind), InLabel);
}

void FMemoryProfiler::RegisterGpuTexture2D(ID3D11Texture2D* InTexture, EGpuResourceKind InKind,
                                           const char* InLabel)
{
    RegisterGpuResourceInternal(InTexture, InKind, EstimateTexture2DBytes(InTexture),
                                DoesGpuKindTrackBytes(InKind), InLabel);
}

void FMemoryProfiler::RegisterGpuCountOnly(const void* InResourceKey, EGpuResourceKind InKind,
                                           const char* InLabel)
{
    RegisterGpuResourceInternal(InResourceKey, InKind, 0, DoesGpuKindTrackBytes(InKind), InLabel);
}

void FMemoryProfiler::UnregisterGpuResource(const void* InResourceKey)
{
    if (InResourceKey == nullptr)
    {
        return;
    }

    const auto ResourceIt = GpuResources.find(InResourceKey);
    if (ResourceIt == GpuResources.end())
    {
        return;
    }

    const FGpuResourceEntry Entry = ResourceIt->second;
    auto BucketIt = GpuBuckets.find(GetGpuResourceKindName(Entry.Kind));
    if (BucketIt != GpuBuckets.end())
    {
        FStatBucket& Bucket = BucketIt->second;
        if (Bucket.LiveCount > 0)
        {
            Bucket.LiveCount -= 1;
        }
        Bucket.LiveBytes = (Bucket.LiveBytes > Entry.Bytes) ? (Bucket.LiveBytes - Entry.Bytes) : 0;
    }

    if (Entry.bTracksBytes)
    {
        GpuLiveBytes = (GpuLiveBytes > Entry.Bytes) ? (GpuLiveBytes - Entry.Bytes) : 0;
    }

    GpuResources.erase(ResourceIt);
}

void FMemoryProfiler::SampleHistoryFrame()
{
    const uint64 CpuLive = CpuUObjectLiveBytes + CpuManualLiveBytes;
    const uint64 GpuLive = GpuLiveBytes;
    const uint64 CombinedLive = CpuLive + GpuLive;

    PushHistorySample(CpuHistoryMb, static_cast<float>(static_cast<double>(CpuLive) * BytesToMegabytes));
    PushHistorySample(GpuHistoryMb, static_cast<float>(static_cast<double>(GpuLive) * BytesToMegabytes));
    PushHistorySample(CombinedHistoryMb,
                      static_cast<float>(static_cast<double>(CombinedLive) * BytesToMegabytes));

    UpdateCombinedPeak();
}

void FMemoryProfiler::ClearHistory()
{
    CpuHistoryMb.clear();
    GpuHistoryMb.clear();
    CombinedHistoryMb.clear();
}

FMemoryProfilerSnapshot FMemoryProfiler::BuildSnapshot() const
{
    FMemoryProfilerSnapshot Snapshot;
    Snapshot.CpuLiveBytes = CpuUObjectLiveBytes + CpuManualLiveBytes;
    Snapshot.CpuPeakBytes = CpuPeakBytes;
    Snapshot.GpuLiveBytes = GpuLiveBytes;
    Snapshot.GpuPeakBytes = GpuPeakBytes;
    Snapshot.CombinedLiveBytes = Snapshot.CpuLiveBytes + Snapshot.GpuLiveBytes;
    Snapshot.CombinedPeakBytes = CombinedPeakBytes;

    BuildStatRows(UObjectBuckets, Snapshot.UObjectClasses);
    BuildStatRows(ManualBuckets, Snapshot.ManualCategories);

    Snapshot.GpuResources.clear();
    Snapshot.GpuResources.reserve(GpuBuckets.size());
    for (const auto& [Name, Bucket] : GpuBuckets)
    {
        if (Bucket.LiveCount == 0 && Bucket.PeakCount == 0)
        {
            continue;
        }

        FGpuMemoryStatRow Row;
        Row.Name = Name;
        Row.LiveCount = Bucket.LiveCount;
        Row.LiveBytes = Bucket.LiveBytes;
        Row.PeakCount = Bucket.PeakCount;
        Row.PeakBytes = Bucket.PeakBytes;

        EGpuResourceKind Kind = EGpuResourceKind::Texture2D;
        for (const auto& [_, Resource] : GpuResources)
        {
            if (GetGpuResourceKindName(Resource.Kind) == Name)
            {
                Kind = Resource.Kind;
                break;
            }
        }
        Row.bTracksBytes = DoesGpuKindTrackBytes(Kind);
        Snapshot.GpuResources.push_back(std::move(Row));
    }

    SortRowsByBytesAndName(Snapshot.UObjectClasses);
    SortRowsByBytesAndName(Snapshot.ManualCategories);
    SortRowsByBytesAndName(Snapshot.GpuResources);
    return Snapshot;
}

void FMemoryProfiler::RegisterGpuResourceInternal(const void* InResourceKey,
                                                  EGpuResourceKind InKind, uint64 InBytes,
                                                  bool bInTracksBytes, const char* InLabel)
{
    if (InResourceKey == nullptr)
    {
        return;
    }

    const auto ExistingIt = GpuResources.find(InResourceKey);
    if (ExistingIt != GpuResources.end())
    {
        UnregisterGpuResource(InResourceKey);
    }

    const FString BucketName = GetGpuResourceKindName(InKind);
    FStatBucket&  Bucket = GpuBuckets[BucketName];
    Bucket.LiveCount += 1;
    Bucket.LiveBytes += InBytes;
    Bucket.PeakCount = std::max(Bucket.PeakCount, Bucket.LiveCount);
    Bucket.PeakBytes = std::max(Bucket.PeakBytes, Bucket.LiveBytes);

    FGpuResourceEntry Entry;
    Entry.Kind = InKind;
    Entry.Bytes = InBytes;
    Entry.Label = GetTypeNameOrDefault(InLabel);
    Entry.bTracksBytes = bInTracksBytes;
    GpuResources[InResourceKey] = std::move(Entry);

    if (bInTracksBytes)
    {
        GpuLiveBytes += InBytes;
        UpdateGpuPeak();
    }

    UpdateCombinedPeak();
}

void FMemoryProfiler::UpdateCpuPeak()
{
    CpuPeakBytes = std::max(CpuPeakBytes, CpuUObjectLiveBytes + CpuManualLiveBytes);
    UpdateCombinedPeak();
}

void FMemoryProfiler::UpdateGpuPeak()
{
    GpuPeakBytes = std::max(GpuPeakBytes, GpuLiveBytes);
    UpdateCombinedPeak();
}

void FMemoryProfiler::UpdateCombinedPeak()
{
    CombinedPeakBytes =
        std::max(CombinedPeakBytes, CpuUObjectLiveBytes + CpuManualLiveBytes + GpuLiveBytes);
}

FString FMemoryProfiler::GetTypeNameOrDefault(const char* InTypeName)
{
    return (InTypeName != nullptr && InTypeName[0] != '\0') ? FString(InTypeName)
                                                            : FString("Unknown");
}

FString FMemoryProfiler::GetGpuResourceKindName(EGpuResourceKind InKind)
{
    switch (InKind)
    {
    case EGpuResourceKind::VertexBuffer:
        return "VertexBuffer";
    case EGpuResourceKind::IndexBuffer:
        return "IndexBuffer";
    case EGpuResourceKind::ConstantBuffer:
        return "ConstantBuffer";
    case EGpuResourceKind::Texture2D:
        return "Texture2D";
    case EGpuResourceKind::BackBuffer:
        return "BackBuffer";
    case EGpuResourceKind::DepthStencilTexture:
        return "DepthStencilTexture";
    case EGpuResourceKind::StagingTexture:
        return "StagingTexture";
    case EGpuResourceKind::ShaderResourceView:
        return "ShaderResourceView";
    case EGpuResourceKind::RenderTargetView:
        return "RenderTargetView";
    case EGpuResourceKind::DepthStencilView:
        return "DepthStencilView";
    case EGpuResourceKind::SamplerState:
        return "SamplerState";
    case EGpuResourceKind::BlendState:
        return "BlendState";
    case EGpuResourceKind::RasterizerState:
        return "RasterizerState";
    case EGpuResourceKind::DepthStencilState:
        return "DepthStencilState";
    case EGpuResourceKind::InputLayout:
        return "InputLayout";
    case EGpuResourceKind::VertexShader:
        return "VertexShader";
    case EGpuResourceKind::PixelShader:
        return "PixelShader";
    case EGpuResourceKind::Query:
        return "Query";
    case EGpuResourceKind::SwapChain:
        return "SwapChain";
    default:
        return "UnknownGpuResource";
    }
}

bool FMemoryProfiler::DoesGpuKindTrackBytes(EGpuResourceKind InKind)
{
    switch (InKind)
    {
    case EGpuResourceKind::VertexBuffer:
    case EGpuResourceKind::IndexBuffer:
    case EGpuResourceKind::ConstantBuffer:
    case EGpuResourceKind::Texture2D:
    case EGpuResourceKind::BackBuffer:
    case EGpuResourceKind::DepthStencilTexture:
    case EGpuResourceKind::StagingTexture:
        return true;
    default:
        return false;
    }
}

uint64 FMemoryProfiler::EstimateBufferBytes(ID3D11Buffer* InBuffer)
{
    if (InBuffer == nullptr)
    {
        return 0;
    }

    D3D11_BUFFER_DESC Desc = {};
    InBuffer->GetDesc(&Desc);
    return static_cast<uint64>(Desc.ByteWidth);
}

uint64 FMemoryProfiler::EstimateTexture2DBytes(ID3D11Texture2D* InTexture)
{
    if (InTexture == nullptr)
    {
        return 0;
    }

    D3D11_TEXTURE2D_DESC Desc = {};
    InTexture->GetDesc(&Desc);
    return EstimateTexture2DBytes(Desc);
}

uint64 FMemoryProfiler::EstimateTexture2DBytes(const D3D11_TEXTURE2D_DESC& InDesc)
{
    const uint32 BitsPerPixel = GetBitsPerPixel(InDesc.Format);
    if (BitsPerPixel == 0)
    {
        return 0;
    }

    uint64 TotalBytes = 0;
    uint32 Width = InDesc.Width;
    uint32 Height = InDesc.Height;
    const uint32 MipLevels = InDesc.MipLevels > 0 ? InDesc.MipLevels : 1;

    for (uint32 MipIndex = 0; MipIndex < MipLevels; ++MipIndex)
    {
        const uint64 RowBytes = (static_cast<uint64>(Width) * BitsPerPixel + 7ull) / 8ull;
        TotalBytes += RowBytes * static_cast<uint64>(Height) * static_cast<uint64>(InDesc.ArraySize);
        Width = std::max(1u, Width / 2u);
        Height = std::max(1u, Height / 2u);
    }

    return TotalBytes;
}

uint32 FMemoryProfiler::GetBitsPerPixel(DXGI_FORMAT InFormat)
{
    switch (InFormat)
    {
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return 128;
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        return 96;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return 64;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return 32;
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
        return 16;
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
        return 8;
    default:
        return 0;
    }
}

FManualMemoryCategoryHandle::FManualMemoryCategoryHandle(const FString& InCategory, size_t InSize)
{
    Track(InCategory, InSize);
}

FManualMemoryCategoryHandle::~FManualMemoryCategoryHandle()
{
    Reset();
}

FManualMemoryCategoryHandle::FManualMemoryCategoryHandle(
    FManualMemoryCategoryHandle&& Other) noexcept
    : Category(std::move(Other.Category)), Size(Other.Size), bTracked(Other.bTracked)
{
    Other.Size = 0;
    Other.bTracked = false;
}

FManualMemoryCategoryHandle& FManualMemoryCategoryHandle::operator=(
    FManualMemoryCategoryHandle&& Other) noexcept
{
    if (this != &Other)
    {
        Reset();
        Category = std::move(Other.Category);
        Size = Other.Size;
        bTracked = Other.bTracked;

        Other.Size = 0;
        Other.bTracked = false;
    }
    return *this;
}

void FManualMemoryCategoryHandle::Track(const FString& InCategory, size_t InSize)
{
    Reset();
    if (InCategory.empty() || InSize == 0)
    {
        return;
    }

    Category = InCategory;
    Size = InSize;
    bTracked = true;
    FMemoryProfiler::Get().RegisterManualCategoryAllocation(Category, Size);
}

void FManualMemoryCategoryHandle::Reset()
{
    if (bTracked)
    {
        FMemoryProfiler::Get().RegisterManualCategoryFree(Category, Size);
        bTracked = false;
    }

    Size = 0;
    Category.clear();
}
