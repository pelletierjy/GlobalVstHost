// src/tray-app/ui/scan_dialog.cpp
//
// T063 — Scan progress dialog with glass panel styling.

#include "scan_dialog.h"
#include "custom_look_and_feel.h"

#include <chrono>
#include <fstream>
#include <iomanip>

namespace jyglobalvst::tray {

namespace {

void ScanLog(const char* msg)
{
    try
    {
        auto path = std::filesystem::path(std::getenv("LOCALAPPDATA")) / "JyGlobalVST" / "startup.log";
        std::filesystem::create_directories(path.parent_path());
        std::ofstream ofs(path, std::ios::app);
        if (ofs)
        {
            auto now = std::chrono::system_clock::now();
            auto t = std::chrono::system_clock::to_time_t(now);
            ofs << std::put_time(std::gmtime(&t), "%H:%M:%S") << " [ScanDialog] " << msg << "\n";
        }
    }
    catch (...)
    {
    }
}



class GlassContent : public juce::Component
{
public:
    void paint(juce::Graphics& g) override
    {
        auto* laf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel());
        if (laf != nullptr)
            laf->drawGlassPanel(g, getLocalBounds().toFloat().reduced(0.5f));
        else
            g.fillAll(kBgPanel);
    }
};

} // namespace

ScanDialog::ScanDialog(IAudioEngine* engine)
    : juce::DialogWindow("Scanning VST3 Plugins", kBgDeep, true, true)
    , engine_(engine)
{
    ScanLog("constructor start");
    buildUI();
    centreWithSize(420, 160);
    startTimerHz(10);
    setVisible(true);
    ScanLog("constructor end");
}

ScanDialog::~ScanDialog()
{
    ScanLog("destructor");
    stopTimer();
}

void ScanDialog::buildUI()
{
    auto* content = new GlassContent();

    status_label_ = std::make_unique<juce::Label>();
    status_label_->setText("Starting scan...", juce::dontSendNotification);
    status_label_->setColour(juce::Label::textColourId, kTextPrimary);
    content->addAndMakeVisible(status_label_.get());

    count_label_ = std::make_unique<juce::Label>();
    count_label_->setText("Plugins found: 0", juce::dontSendNotification);
    count_label_->setColour(juce::Label::textColourId, kTextDim);
    content->addAndMakeVisible(count_label_.get());

    cancel_button_ = std::make_unique<juce::TextButton>("Cancel");
    cancel_button_->addListener(this);
    content->addAndMakeVisible(cancel_button_.get());

    setContentOwned(content, true);
}

void ScanDialog::resized()
{
    DialogWindow::resized();

    if (auto* content = getContentComponent())
    {
        auto b = content->getLocalBounds().reduced(16);

        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::column;
        fb.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        fb.alignItems = juce::FlexBox::AlignItems::stretch;

        fb.items.add(juce::FlexItem(*status_label_).withMinHeight(24).withHeight(24));
        fb.items.add(juce::FlexItem().withHeight(8));
        fb.items.add(juce::FlexItem(*count_label_).withMinHeight(24).withHeight(24));
        fb.items.add(juce::FlexItem().withHeight(12));
        fb.items.add(juce::FlexItem(*cancel_button_).withMinHeight(28).withHeight(28).withMaxWidth(100));

        fb.performLayout(b);
    }
}

void ScanDialog::buttonClicked(juce::Button* button)
{
    if (button == cancel_button_.get())
    {
        engine_->cancelScan();
        cancel_button_->setEnabled(false);
    }
}

void ScanDialog::onScanStarted(int /*total_paths*/)
{
    ScanLog("onScanStarted");
    juce::ScopedLock lock(status_lock_);
    pending_status_ = "Scanning...";
    plugins_discovered_.store(0);
}

void ScanDialog::onPathStarted(const std::filesystem::path& path)
{
    juce::ScopedLock lock(status_lock_);
    pending_status_ = juce::String("Scanning: ") + juce::String(path.string());
}

void ScanDialog::onPluginDiscovered(const PluginCatalogEntry& /*entry*/)
{
    plugins_discovered_.fetch_add(1);
}

void ScanDialog::onScanFinished(int /*plugins_discovered*/)
{
    ScanLog("onScanFinished");
    juce::ScopedLock lock(status_lock_);
    pending_status_ = "Scan complete.";
    finished_.store(true);
}

void ScanDialog::onScanCancelled()
{
    ScanLog("onScanCancelled");
    juce::ScopedLock lock(status_lock_);
    pending_status_ = "Scan cancelled.";
    finished_.store(true);
}

void ScanDialog::timerCallback()
{
    if (finished_.load())
    {
        ScanLog("timerCallback: finished detected, stopping timer");
        stopTimer();
        cancel_button_->setButtonText("Close");
        cancel_button_->setEnabled(true);
        return;
    }

    {
        juce::ScopedLock lock(status_lock_);
        status_label_->setText(pending_status_, juce::dontSendNotification);
    }
    count_label_->setText(juce::String("Plugins found: ") + juce::String(plugins_discovered_.load()),
                           juce::dontSendNotification);
}

void ScanDialog::closeButtonPressed()
{
    engine_->cancelScan();
    setVisible(false);
}

}  // namespace jyglobalvst::tray
