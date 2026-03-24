#include "MemoryProfilerPanel.h"

#include "Engine/MemoryProfiler.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>

namespace
{
    FString FormatBytes(uint64 InBytes)
    {
        static constexpr const char* Units[] = {"B", "KB", "MB", "GB", "TB"};

        double Value = static_cast<double>(InBytes);
        size_t UnitIndex = 0;
        while (Value >= 1024.0 && UnitIndex < std::size(Units) - 1)
        {
            Value /= 1024.0;
            ++UnitIndex;
        }

        char Buffer[64] = {};
        std::snprintf(Buffer, std::size(Buffer), "%.2f %s", Value, Units[UnitIndex]);
        return Buffer;
    }

    void DrawSummaryStat(const char* InLabel, uint64 InBytes)
    {
        ImGui::Text("%s", InLabel);
        ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.93f, 1.0f), "%s", FormatBytes(InBytes).c_str());
    }

    void DrawHistoryPlot(const char* InLabel, const TArray<float>& InHistory)
    {
        if (InHistory.empty())
        {
            ImGui::Text("%s: no samples yet.", InLabel);
            return;
        }

        float MaxValue = 1.0f;
        for (const float Value : InHistory)
        {
            MaxValue = std::max(MaxValue, Value);
        }

        ImGui::PlotLines(InLabel, InHistory.data(), static_cast<int32>(InHistory.size()), 0, nullptr,
                         0.0f, MaxValue * 1.1f, ImVec2(0.0f, 56.0f));
    }

    void DrawMemoryTable(const char* InTableId, const TArray<FMemoryStatRow>& InRows)
    {
        if (!ImGui::BeginTable(InTableId, 6,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
        {
            return;
        }

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.3f);
        ImGui::TableSetupColumn("Live Count", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Live Bytes", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Peak Count", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Peak Bytes", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Avg Live Size", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableHeadersRow();

        for (const FMemoryStatRow& Row : InRows)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(Row.Name.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%llu", static_cast<unsigned long long>(Row.LiveCount));

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(FormatBytes(Row.LiveBytes).c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%llu", static_cast<unsigned long long>(Row.PeakCount));

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(FormatBytes(Row.PeakBytes).c_str());

            ImGui::TableSetColumnIndex(5);
            const uint64 AvgBytes = Row.LiveCount > 0 ? (Row.LiveBytes / Row.LiveCount) : 0;
            ImGui::TextUnformatted(FormatBytes(AvgBytes).c_str());
        }

        ImGui::EndTable();
    }

    void DrawGpuTable(const char* InTableId, const TArray<FGpuMemoryStatRow>& InRows)
    {
        if (!ImGui::BeginTable(InTableId, 6,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
        {
            return;
        }

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.3f);
        ImGui::TableSetupColumn("Live Count", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Live Bytes", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Peak Count", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Peak Bytes", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Avg Live Size", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableHeadersRow();

        for (const FGpuMemoryStatRow& Row : InRows)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(Row.Name.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%llu", static_cast<unsigned long long>(Row.LiveCount));

            ImGui::TableSetColumnIndex(2);
            if (Row.bTracksBytes)
            {
                ImGui::TextUnformatted(FormatBytes(Row.LiveBytes).c_str());
            }
            else
            {
                ImGui::TextUnformatted("-");
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%llu", static_cast<unsigned long long>(Row.PeakCount));

            ImGui::TableSetColumnIndex(4);
            if (Row.bTracksBytes)
            {
                ImGui::TextUnformatted(FormatBytes(Row.PeakBytes).c_str());
            }
            else
            {
                ImGui::TextUnformatted("-");
            }

            ImGui::TableSetColumnIndex(5);
            if (Row.bTracksBytes && Row.LiveCount > 0)
            {
                ImGui::TextUnformatted(FormatBytes(Row.LiveBytes / Row.LiveCount).c_str());
            }
            else
            {
                ImGui::TextUnformatted("-");
            }
        }

        ImGui::EndTable();
    }
} // namespace

const wchar_t* FMemoryProfilerPanel::GetPanelID() const
{
    return L"MemoryProfilerPanel";
}

const wchar_t* FMemoryProfilerPanel::GetDisplayName() const
{
    return L"Memory Profiler";
}

void FMemoryProfilerPanel::Draw()
{
    if (!ImGui::Begin("Memory Profiler", nullptr))
    {
        ImGui::End();
        return;
    }

    FMemoryProfiler& Profiler = FMemoryProfiler::Get();
    const FMemoryProfilerSnapshot Snapshot = Profiler.BuildSnapshot();

    if (ImGui::Button("Clear History"))
    {
        Profiler.ClearHistory();
    }

    ImGui::Separator();

    if (ImGui::BeginTable("##MemorySummary", 6,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        DrawSummaryStat("CPU Live", Snapshot.CpuLiveBytes);
        ImGui::TableSetColumnIndex(1);
        DrawSummaryStat("CPU Peak", Snapshot.CpuPeakBytes);
        ImGui::TableSetColumnIndex(2);
        DrawSummaryStat("GPU Live", Snapshot.GpuLiveBytes);
        ImGui::TableSetColumnIndex(3);
        DrawSummaryStat("GPU Peak", Snapshot.GpuPeakBytes);
        ImGui::TableSetColumnIndex(4);
        DrawSummaryStat("Combined Live", Snapshot.CombinedLiveBytes);
        ImGui::TableSetColumnIndex(5);
        DrawSummaryStat("Combined Peak", Snapshot.CombinedPeakBytes);
        ImGui::EndTable();
    }

    ImGui::Spacing();
    DrawHistoryPlot("CPU History (MB)", Profiler.GetCpuHistoryMb());
    DrawHistoryPlot("GPU History (MB)", Profiler.GetGpuHistoryMb());
    DrawHistoryPlot("Combined History (MB)", Profiler.GetCombinedHistoryMb());

    ImGui::Spacing();
    ImGui::SeparatorText("UObject Classes");
    if (ImGui::BeginChild("##UObjectMemoryRows", ImVec2(0.0f, 180.0f), false))
    {
        DrawMemoryTable("##UObjectMemoryTable", Snapshot.UObjectClasses);
    }
    ImGui::EndChild();

    ImGui::SeparatorText("Manual Categories");
    if (ImGui::BeginChild("##ManualMemoryRows", ImVec2(0.0f, 160.0f), false))
    {
        DrawMemoryTable("##ManualMemoryTable", Snapshot.ManualCategories);
    }
    ImGui::EndChild();

    ImGui::SeparatorText("GPU Resources");
    if (ImGui::BeginChild("##GpuMemoryRows", ImVec2(0.0f, 180.0f), false))
    {
        DrawGpuTable("##GpuMemoryTable", Snapshot.GpuResources);
    }
    ImGui::EndChild();

    ImGui::End();
}
