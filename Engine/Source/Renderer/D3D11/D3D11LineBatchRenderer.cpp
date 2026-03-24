#include "Renderer/D3D11/D3D11LineBatchRenderer.h"

#include "Renderer/D3D11/D3D11GpuProfiler.h"
#include "Renderer/D3D11/D3D11RHI.h"
#include "Renderer/SceneView.h"
#include "Renderer/Types/ShaderConstants.h"
#include "Engine/MemoryProfiler.h"

namespace
{
    template <typename T> void UntrackComResource(TComPtr<T>& InResource)
    {
        FMemoryProfiler::Get().UnregisterGpuResource(InResource.Get());
        InResource.Reset();
    }
}

FD3D11LineBatchRenderer::FD3D11LineBatchRenderer()
    : MemoryTrackHandle(std::make_unique<FManualMemoryCategoryHandle>(
          "Renderer/FD3D11LineBatchRenderer", sizeof(FD3D11LineBatchRenderer)))
{
}

FD3D11LineBatchRenderer::~FD3D11LineBatchRenderer() = default;

bool FD3D11LineBatchRenderer::Initialize(FD3D11RHI* InRHI)
{
    if (InRHI == nullptr)
    {
        return false;
    }

    RHI = InRHI;
    GpuProfiler = nullptr;
    CurrentSceneView = nullptr;

    Vertices.reserve(MaxVertexCount);

    if (!CreateShaders())
    {
        Shutdown();
        return false;
    }

    if (!CreateConstantBuffer())
    {
        Shutdown();
        return false;
    }

    if (!CreateDynamicVertexBuffer(MaxVertexCount))
    {
        Shutdown();
        return false;
    }

    return true;
}

void FD3D11LineBatchRenderer::Shutdown()
{
    UntrackComResource(DynamicVertexBuffer);
    UntrackComResource(ConstantBuffer);
    UntrackComResource(InputLayout);
    UntrackComResource(PixelShader);
    UntrackComResource(VertexShader);

    Vertices.clear();
    GpuProfiler = nullptr;
    CurrentSceneView = nullptr;
    RHI = nullptr;
}

void FD3D11LineBatchRenderer::BeginFrame(const FSceneView* InSceneView)
{
    CurrentSceneView = InSceneView;
    Vertices.clear();
}

void FD3D11LineBatchRenderer::AddLine(const FVector& InStart, const FVector& InEnd,
                                      const FColor& InColor)
{
    if (RHI == nullptr || CurrentSceneView == nullptr)
    {
        return;
    }

    if (Vertices.size() + 2 > MaxVertexCount)
    {
        Flush();
    }

    if (Vertices.size() + 2 > MaxVertexCount)
    {
        return;
    }

    FLineVertex V0 = {};
    V0.Position = InStart;
    V0.Color = InColor;

    FLineVertex V1 = {};
    V1.Position = InEnd;
    V1.Color = InColor;

    Vertices.push_back(V0);
    Vertices.push_back(V1);
}

void FD3D11LineBatchRenderer::EndFrame()
{
    if (RHI == nullptr)
    {
        return;
    }

    Flush();
    CurrentSceneView = nullptr;
}

bool FD3D11LineBatchRenderer::CreateShaders()
{
    if (RHI == nullptr)
    {
        return false;
    }

    static const D3D11_INPUT_ELEMENT_DESC InputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         static_cast<UINT>(offsetof(FLineVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         static_cast<UINT>(offsetof(FLineVertex, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    if (!RHI->CreateVertexShaderAndInputLayout(DefaultShaderPath, "VSMain", InputElements,
                                               static_cast<uint32>(ARRAYSIZE((InputElements))),
                                               VertexShader.GetAddressOf(),
                                               InputLayout.GetAddressOf()))
    {
        return false;
    }

    if (!RHI->CreatePixelShader(DefaultShaderPath, "PSMain", PixelShader.GetAddressOf()))
    {
        VertexShader.Reset();
        InputLayout.Reset();
        return false;
    }

    return true;
}

bool FD3D11LineBatchRenderer::CreateConstantBuffer()
{
    if (RHI == nullptr)
    {
        return false;
    }

    return RHI->CreateConstantBuffer(sizeof(FLineConstants), ConstantBuffer.GetAddressOf());
}

bool FD3D11LineBatchRenderer::CreateDynamicVertexBuffer(uint32 InMaxVertexCount)
{
    if (RHI == nullptr)
    {
        return false;
    }

    return RHI->CreateVertexBuffer(nullptr, sizeof(FLineVertex) * InMaxVertexCount,
                                   sizeof(FLineVertex), true,
                                   DynamicVertexBuffer.GetAddressOf());
}

void FD3D11LineBatchRenderer::Flush()
{
    if (RHI == nullptr || CurrentSceneView == nullptr)
    {
        return;
    }

    if (Vertices.empty())
    {
        return;
    }

    if (!RHI->UpdateDynamicBuffer(DynamicVertexBuffer.Get(), Vertices.data(),
                                  static_cast<uint32>(sizeof(FLineVertex) * Vertices.size())))
    {
        return;
    }

    FLineConstants Constants = {};
    Constants.VP = CurrentSceneView->GetViewProjectionMatrix();

    if (!RHI->UpdateConstantBuffer(ConstantBuffer.Get(), &Constants, sizeof(Constants)))
    {
        return;
    }

    const UINT    Stride = sizeof(FLineVertex);
    const UINT    Offset = 0;
    ID3D11Buffer* VertexBuffer = DynamicVertexBuffer.Get();

    RHI->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    RHI->SetInputLayout(InputLayout.Get());
    RHI->SetVertexBuffer(0, VertexBuffer, Stride, Offset);

    RHI->SetVertexShader(VertexShader.Get());
    RHI->SetVSConstantBuffer(0, ConstantBuffer.Get());

    RHI->SetPixelShader(PixelShader.Get());

    const FGpuProfileScopeGuard ScopeGuard(GpuProfiler, "Line Flush", "Lines",
                                           EGpuProfileDrawType::Draw,
                                           static_cast<uint32>(Vertices.size()), 1);
    RHI->Draw(static_cast<uint32>(Vertices.size()), 0);

    Vertices.clear();
}
