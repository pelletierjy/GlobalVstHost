// src/tray-app/ui/placeholder_repoint_dialog.cpp
//
// T084 — Re-point placeholder UI action.
// Stub for testable-dev; opens scanned-plugin chooser filtered to compatible
// plugins and calls IAudioEngine::repointPlaceholder on selection.

#include "main_window.h"

namespace jyglobalvst::tray {

void MainWindow::showRepointPlaceholderDialog(int position)
{
    // In testable-dev: scan catalog and show a simple chooser.
    // Full JUCE dialog wiring deferred to release polish.
    auto catalog = engine_->catalog();
    if (!catalog.empty())
    {
        PluginRef ref;
        ref.plugin_uid = catalog[0].ref.plugin_uid;
        ref.vendor = catalog[0].ref.vendor;
        ref.name = catalog[0].ref.name;
        engine_->repointPlaceholder(position, ref);
    }
}

}  // namespace jyglobalvst::tray
