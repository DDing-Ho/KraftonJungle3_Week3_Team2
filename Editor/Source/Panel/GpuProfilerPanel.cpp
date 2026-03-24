#include "GpuProfilerPanel.h"

#include "Editor/EditorContext.h"
#include "Renderer/RendererModule.h"
#include "Renderer/Types/GpuProfilerTypes.h"

#include "imgui.h"

#include <algorithm>
#include <cstdint>

namespace
{
    const char* GetDrawTypeLabel(EGpuProfileDrawType InDrawType)
    {
        switch (InDrawType)
        {
        case EGpuProfileDrawType::Draw:
            return "Draw";
        case EGpuProfileDrawType::DrawIndexed:
            return "DrawIndexed";
        case EGpuProfileDrawType::DrawIndexedInstanced:
            return "DrawIndexedInstanced";
        case EGpuProfileDrawType::Flush:
            return "Flush";
        default:
            return "-";
        }
    }

    void BuildSampleChildren(const FGpuProfileFrameSnapshot& InSnapshot,
                             TArray<TArray<int32>>& OutChildren, TArray<int32>& OutRoots)
    {
        OutChildren = TArray<TArray<int32>>(InSnapshot.Samples.size());
        OutRoots.clear();

        for (int32 Index = 0; Index < static_cast<int32>(InSnapshot.Samples.size()); ++Index)
        {
            const int32 ParentIndex = InSnapshot.Samples[Index].ParentIndex;
            if (ParentIndex >= 0 && ParentIndex < static_cast<int32>(OutChildren.size()))
            {
                OutChildren[ParentIndex].push_back(Index);
            }
            else
            {
                OutRoots.push_back(Index);
            }
        }
    }

    void DrawSampleRowRecursive(const FGpuProfileFrameSnapshot& InSnapshot,
                                const TArray<TArray<int32>>& InChildren, int32 InSampleIndex)
    {
        const FGpuProfileSample& Sample = InSnapshot.Samples[InSampleIndex];
        const bool bHasChildren =
            InSampleIndex >= 0 && InSampleIndex < static_cast<int32>(InChildren.size()) &&
            !InChildren[InSampleIndex].empty();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        const ImGuiTreeNodeFlags Flags =
            ImGuiTreeNodeFlags_SpanAllColumns |
            (!bHasChildren ? (ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen) : 0);

        const bool bOpened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(InSampleIndex)),
                                               Flags, "%s", Sample.Name.c_str());

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(Sample.Category.c_str());

        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.3f", Sample.GpuTimeMs);

        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(GetDrawTypeLabel(Sample.DrawType));

        ImGui::TableSetColumnIndex(4);
        if (Sample.InstanceCount > 1)
        {
            ImGui::Text("%u / %u", Sample.ElementCount, Sample.InstanceCount);
        }
        else
        {
            ImGui::Text("%u", Sample.ElementCount);
        }

        if (bHasChildren && bOpened)
        {
            for (int32 ChildIndex : InChildren[InSampleIndex])
            {
                DrawSampleRowRecursive(InSnapshot, InChildren, ChildIndex);
            }
            ImGui::TreePop();
        }
    }
} // namespace

const wchar_t* FGpuProfilerPanel::GetPanelID() const
{
    return L"GpuProfilerPanel";
}

const wchar_t* FGpuProfilerPanel::GetDisplayName() const
{
    return L"GPU Profiler";
}

void FGpuProfilerPanel::OnClose()
{
    IPanel::OnClose();

    if (GetContext() != nullptr && GetContext()->Renderer != nullptr)
    {
        GetContext()->Renderer->SetGpuProfilerPaused(false);
        GetContext()->Renderer->SetGpuProfilerEnabled(false);
    }
}

void FGpuProfilerPanel::Draw()
{
    if (!ImGui::Begin("GPU Profiler", nullptr))
    {
        ImGui::End();
        return;
    }

    FRendererModule* Renderer = GetContext() != nullptr ? GetContext()->Renderer : nullptr;
    if (Renderer == nullptr)
    {
        ImGui::TextUnformatted("Renderer is unavailable.");
        ImGui::End();
        return;
    }

    bool bEnabled = Renderer->IsGpuProfilerEnabled();
    if (ImGui::Checkbox("Enable", &bEnabled))
    {
        Renderer->SetGpuProfilerEnabled(bEnabled);
        if (!bEnabled)
        {
            Renderer->SetGpuProfilerPaused(false);
        }
    }

    ImGui::SameLine();
    bool bPaused = Renderer->IsGpuProfilerPaused();
    ImGui::BeginDisabled(!bEnabled);
    if (ImGui::Checkbox("Pause", &bPaused))
    {
        Renderer->SetGpuProfilerPaused(bPaused);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        Renderer->ClearGpuProfiler();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto Scroll", &bAutoScroll);

    const FGpuProfileFrameSnapshot& Snapshot = Renderer->GetLatestGpuProfileSnapshot();
    const TArray<float>&            History = Renderer->GetGpuProfilerFrameHistory();

    if (!History.empty())
    {
        float MaxValue = 1.0f;
        for (float Value : History)
        {
            MaxValue = std::max(MaxValue, Value);
        }

        ImGui::PlotLines("Recent Frame GPU ms", History.data(), static_cast<int>(History.size()), 0,
                         nullptr, 0.0f, MaxValue * 1.1f, ImVec2(0.0f, 60.0f));
    }

    if (!Snapshot.IsValid())
    {
        ImGui::Spacing();
        ImGui::TextUnformatted("No resolved GPU capture yet.");
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::Text("Capture              : %s", Snapshot.CaptureLabel.c_str());
    ImGui::Text("Capture Id           : %llu", static_cast<unsigned long long>(Snapshot.CaptureId));
    ImGui::Text("GPU Time             : %.3f ms", Snapshot.TotalGpuTimeMs);
    ImGui::Text("Capture Latency      : %u", Snapshot.CaptureLatency);
    ImGui::Text("Sample Count         : %u", static_cast<unsigned>(Snapshot.Samples.size()));
    ImGui::Text("Overflowed Samples   : %u", Snapshot.OverflowCount);

    TArray<TArray<int32>> Children;
    TArray<int32>         Roots;
    BuildSampleChildren(Snapshot, Children, Roots);

    ImGui::Spacing();
    if (ImGui::BeginChild("##GpuProfilerRows", ImVec2(0.0f, 0.0f), false))
    {
        if (ImGui::BeginTable("##GpuProfilerTable", 5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.5f);
            ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Draw Type", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Count/Instances", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableHeadersRow();

            for (int32 RootIndex : Roots)
            {
                DrawSampleRowRecursive(Snapshot, Children, RootIndex);
            }

            if (bAutoScroll)
            {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}
