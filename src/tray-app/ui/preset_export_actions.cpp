// src/tray-app/ui/preset_export_actions.cpp
//
// T079 — Export Preset action + Reveal in Explorer.
// Stub for testable-dev.

#include "main_window.h"

#include <windows.h>
#include <shellapi.h>

namespace jyglobalvst::tray {

void MainWindow::revealPresetInExplorer(const std::filesystem::path& path)
{
    if (std::filesystem::exists(path))
    {
        ShellExecuteW(nullptr, L"open", L"explorer.exe",
                      (L"/select," + path.wstring()).c_str(),
                      nullptr, SW_SHOWNORMAL);
    }
}

}  // namespace jyglobalvst::tray
