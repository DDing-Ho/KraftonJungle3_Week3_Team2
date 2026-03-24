#pragma once

#include "Panel.h"

class FGpuProfilerPanel : public IPanel
{
  public:
    const wchar_t* GetPanelID() const override;
    const wchar_t* GetDisplayName() const override;
    int32          GetWindowMenuOrder() const override { return 50; }
    void           OnClose() override;
    void           Draw() override;

  private:
    bool bAutoScroll = true;
};
