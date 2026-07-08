// src/tray-app/ui/preset_import_handler.cpp
//
// T078 — Drag-and-drop import handler + Import Preset file picker.
// Stub for testable-dev; full JUCE drag-and-drop wiring in release.

#include "main_window.h"

namespace jyglobalvst::tray {

void MainWindow::handleDragAndDropPreset(const std::filesystem::path& path)
{
    engine_->loadPreset(path);
    preset_override_flag_ = true;
}

}  // namespace jyglobalvst::tray
