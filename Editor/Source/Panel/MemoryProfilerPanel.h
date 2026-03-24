#pragma once

#include "Panel.h"

class FMemoryProfilerPanel : public IPanel
{
  public:
    const wchar_t* GetPanelID() const override;
    const wchar_t* GetDisplayName() const override;
    int32          GetWindowMenuOrder() const override { return 60; }
    void           Draw() override;
};
