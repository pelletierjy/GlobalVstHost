// src/tray-app/ui/settings_dialog_scan_paths.cpp
//
// T064 — Scan paths settings implementation with neon theme.

#include "settings_dialog_scan_paths.h"
#include "custom_look_and_feel.h"

#include <windows.h>
#include <shlobj.h>

namespace jyglobalvst::tray {

SettingsDialogScanPaths::SettingsDialogScanPaths()
{
    list_box_ = std::make_unique<juce::ListBox>("ScanPaths", this);
    addAndMakeVisible(list_box_.get());

    add_button_ = std::make_unique<juce::TextButton>("Add...");
    add_button_->addListener(this);
    addAndMakeVisible(add_button_.get());

    remove_button_ = std::make_unique<juce::TextButton>("Remove");
    remove_button_->addListener(this);
    addAndMakeVisible(remove_button_.get());
}

void SettingsDialogScanPaths::buttonClicked(juce::Button* button)
{
    if (button == add_button_.get())
    {
        BROWSEINFOW bi = {};
        bi.lpszTitle = L"Select VST3 folder";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (pidl != nullptr)
        {
            wchar_t path[MAX_PATH];
            if (SHGetPathFromIDListW(pidl, path))
            {
                addPath(juce::String(path));
            }
            CoTaskMemFree(pidl);
        }
    }
    else if (button == remove_button_.get())
    {
        removeSelectedPath();
    }
}

int SettingsDialogScanPaths::getNumRows()
{
    return static_cast<int>(items_.size());
}

void SettingsDialogScanPaths::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                                                bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(items_.size()))
        return;

    const auto& item = items_[static_cast<std::size_t>(rowNumber)];
    if (rowIsSelected)
    {
        g.fillAll(kControlHover);
        g.setColour(kAccentCyan);
        g.fillRect(0, 0, 3, height);
    }
    else
    {
        g.fillAll(kBgPanel);
    }

    g.setColour(item.enabled ? kTextPrimary : kTextDim);
    g.drawText(item.path, 8, 0, width - 16, height, juce::Justification::centredLeft);
}

void SettingsDialogScanPaths::addPath(const juce::String& path)
{
    items_.push_back({path, true});
    list_box_->updateContent();
}

void SettingsDialogScanPaths::removeSelectedPath()
{
    const int idx = list_box_->getSelectedRow();
    if (idx >= 0 && idx < static_cast<int>(items_.size()))
    {
        items_.erase(items_.begin() + idx);
        list_box_->updateContent();
    }
}

std::vector<ScanPathItem> SettingsDialogScanPaths::items() const
{
    return items_;
}

void SettingsDialogScanPaths::resized()
{
    auto bounds = getLocalBounds();
    auto button_bar = bounds.removeFromBottom(36).reduced(4);
    add_button_->setBounds(button_bar.removeFromLeft(80));
    button_bar.removeFromLeft(8);
    remove_button_->setBounds(button_bar.removeFromLeft(80));
    list_box_->setBounds(bounds.reduced(4));
}

}  // namespace jyglobalvst::tray
