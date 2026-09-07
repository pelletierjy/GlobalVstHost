// src/tray-app/ui/chain_editor.cpp
//
// T062 — Chain editor implementation with neon hover glow.

#include "chain_editor.h"
#include "custom_look_and_feel.h"

#include <windows.h>
#include <cstdarg>

namespace jyglobalvst::tray {

namespace {

void LogDebug(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
    va_end(args);

    OutputDebugStringA("[JyGlobalVST-UI] ");
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
}

float linearToDb(float linear)
{
    if (linear <= 0.0f)
        return -120.0f;
    return 20.0f * std::log10(linear);
}

}  // namespace

// =========================================================================
// ChainSlotRow
// =========================================================================

ChainSlotRow::ChainSlotRow(IAudioEngine* engine, int position, const ChainSlotSnapshot& slot)
    : engine_(engine)
    , position_(position)
    , slot_(slot)
{
    LogDebug("ChainSlotRow::ctor: position=%d, kind=%s, is_bypassed=%s, is_failed=%s, name='%s'",
             position,
             (slot_.kind == PluginSlotKind::Placeholder) ? "Placeholder" : "Plugin",
             slot_.is_bypassed ? "true" : "false",
             slot_.is_failed ? "true" : "false",
             slot_.ref.name.c_str());

    const bool has_tag = !slot_.tag.empty();

    name_label_ = std::make_unique<juce::Label>();
    // A tagged slot reads as "Equalizer - Volume drop": the separator hangs off
    // the name so the tag label holds nothing but the editable tag text.
    name_label_->setText(juce::String(slot_.ref.name.empty() ? "Placeholder" : slot_.ref.name)
                             + (has_tag ? " -" : ""),
                         juce::dontSendNotification);
    // The label is display-only; let mouse drags fall through to the row so the
    // row can be grabbed and dragged by its title.
    name_label_->setInterceptsMouseClicks(false, false);
    addAndMakeVisible(name_label_.get());

    tag_label_ = std::make_unique<juce::Label>();
    tag_label_->setText(juce::String(slot_.tag), juce::dontSendNotification);
    tag_label_->setColour(juce::Label::textColourId, kAccentCyan);
    tag_label_->setEditable(true, true, false);
    tag_label_->onEditorShow = [this]() {
        if (auto* ed = tag_label_->getCurrentTextEditor())
        {
            ed->setInputRestrictions(kMaxTagChars);
        }
    };
    // Fires for both commit and Escape (which restores the previous text, making
    // the commit a no-op). Deferred because the editor's text has not been copied
    // into the label yet at this point.
    tag_label_->onEditorHide = [this]() {
        juce::Component::SafePointer<ChainSlotRow> safe {this};
        juce::MessageManager::callAsync([safe]() {
            if (safe != nullptr)
            {
                safe->commitTag(safe->tag_label_->getText());
            }
        });
    };
    addChildComponent(tag_label_.get());
    tag_label_->setVisible(has_tag);

    bypass_button_ = std::make_unique<juce::TextButton>(slot_.is_bypassed ? "OFF" : "ON");
    bypass_button_->setClickingTogglesState(true);
    // Toggle state tracks "active" (not bypassed) so the look-and-feel highlights
    // the button when the plugin is on, matching the Start Engine button's behavior.
    bypass_button_->setToggleState(!slot_.is_bypassed, juce::dontSendNotification);
    bypass_button_->addListener(this);
    addAndMakeVisible(bypass_button_.get());

    editor_button_ = std::make_unique<juce::TextButton>("E");
    editor_button_->addListener(this);
    addAndMakeVisible(editor_button_.get());

    tag_button_ = std::make_unique<juce::TextButton>(has_tag ? "TAG_SET" : "TAG");
    tag_button_->setTooltip(has_tag ? "Clear tag" : "Add a tag");
    tag_button_->addListener(this);
    addAndMakeVisible(tag_button_.get());

    remove_button_ = std::make_unique<juce::TextButton>("X");
    remove_button_->addListener(this);
    addAndMakeVisible(remove_button_.get());

    // Per-plugin output meter: only for actual (non-failed) plugin slots.
    if (slot_.kind == PluginSlotKind::Plugin && !slot_.is_failed)
    {
        meter_ = std::make_unique<HorizontalMeterPanel>();
        addAndMakeVisible(meter_.get());
    }

    if (slot_.kind == PluginSlotKind::Placeholder)
    {
        LogDebug("ChainSlotRow::ctor: Disabling buttons for Placeholder at position %d", position);
        name_label_->setColour(juce::Label::textColourId, kTextDim);
        bypass_button_->setEnabled(false);
        editor_button_->setEnabled(false);
        remove_button_->setEnabled(false);
    }
    if (slot_.is_failed)
    {
        LogDebug("ChainSlotRow::ctor: Plugin FAILED at position %d - disabling buttons", position);
        name_label_->setColour(juce::Label::textColourId, juce::Colour(0xFFFF1744));
        bypass_button_->setEnabled(false);
        editor_button_->setEnabled(false);
        remove_button_->setEnabled(false);
    }

    if (!isDraggable())
    {
        // Placeholder / failed slots keep showing an existing tag but cannot be
        // retagged, matching the other per-slot controls.
        tag_button_->setEnabled(false);
        tag_label_->setEditable(false, false, false);
        tag_label_->setColour(juce::Label::textColourId, kTextDim);
    }

    setWantsKeyboardFocus(true);

    if (isDraggable())
    {
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
}

bool ChainSlotRow::isDraggable() const noexcept
{
    return slot_.kind != PluginSlotKind::Placeholder && !slot_.is_failed;
}

void ChainSlotRow::setMeterLevels(float peakDb, float rmsDb)
{
    if (meter_ != nullptr)
    {
        meter_->setLevels(peakDb, rmsDb);
    }
}

void ChainSlotRow::buttonClicked(juce::Button* button)
{
    LogDebug("ChainSlotRow::buttonClicked at position %d", position_);

    if (!engine_)
    {
        LogDebug("  -> Engine is null, ignoring button click");
        return;
    }

    auto snapshot = engine_->snapshotChain();
    if (position_ < 0 || position_ >= static_cast<int>(snapshot.slots.size()))
    {
        LogDebug("  -> Position %d is out of bounds (chain size: %zu), ignoring click",
                 position_, snapshot.slots.size());
        return;
    }

    if (button == bypass_button_.get())
    {
        LogDebug("  -> Bypass button clicked");
        const bool now_active = bypass_button_->getToggleState();
        bypass_button_->setButtonText(now_active ? "ON" : "OFF");
        engine_->setBypass(position_, !now_active);
    }
    else if (button == editor_button_.get())
    {
        LogDebug("  -> Editor button clicked for position %d, calling engine_->openEditor()", position_);
        engine_->openEditor(position_);
    }
    else if (button == tag_button_.get())
    {
        if (slot_.tag.empty())
        {
            LogDebug("  -> Tag button clicked, starting inline edit");
            beginTagEdit();
        }
        else
        {
            LogDebug("  -> Tag button clicked, clearing tag");
            engine_->setSlotTag(position_, "");
        }
    }
    else if (button == remove_button_.get())
    {
        LogDebug("  -> Remove button clicked");
        engine_->removeSlot(position_);
    }
}

void ChainSlotRow::beginTagEdit()
{
    tag_label_->setVisible(true);
    resized();
    tag_label_->showEditor();
}

void ChainSlotRow::commitTag(const juce::String& text)
{
    const auto trimmed = text.trim().substring(0, kMaxTagChars);

    if (trimmed.toStdString() == slot_.tag)
    {
        // Unchanged (including an abandoned edit on an untagged slot): just drop
        // the temporary label instead of churning the chain revision.
        tag_label_->setText(juce::String(slot_.tag), juce::dontSendNotification);
        tag_label_->setVisible(!slot_.tag.empty());
        resized();
        return;
    }

    if (engine_ != nullptr)
    {
        engine_->setSlotTag(position_, trimmed.toStdString());
    }
}

void ChainSlotRow::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto* lnf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel());
    const auto bgPanel      = lnf ? lnf->colors().bgPanel      : kBgPanel;
    const auto bgBorder     = lnf ? lnf->colors().bgPanelBorder : kBgPanelBorder;
    const auto controlHover = lnf ? lnf->colors().controlHover  : kControlHover;
    const auto accentCyan   = lnf ? lnf->colors().accentCyan    : kAccentCyan;
    const auto accentGlow   = lnf ? lnf->colors().accentGlow    : kAccentGlow;

    if (isMouseOver())
    {
        g.setColour(controlHover);
        g.fillRect(bounds);

        // 3px cyan left edge.
        g.setColour(accentCyan);
        g.fillRect(bounds.removeFromLeft(3.0f));

        // Glow.
        g.setColour(accentGlow);
        g.fillRect(bounds.removeFromLeft(6.0f));
    }
    else
    {
        g.setColour(bgPanel);
        g.fillRect(bounds);
    }

    // Bottom separator.
    g.setColour(bgBorder);
    g.fillRect(bounds.removeFromBottom(1.0f));

    // Insertion indicator while another row is dragged over this one.
    if (drag_over_)
    {
        auto full = getLocalBounds().toFloat();
        g.setColour(accentCyan);
        if (insert_above_)
        {
            g.fillRect(full.removeFromTop(2.0f));
        }
        else
        {
            g.fillRect(full.removeFromBottom(2.0f));
        }
    }
}

void ChainSlotRow::resized()
{
    auto b = getLocalBounds().reduced(4, 0);
    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.alignItems = juce::FlexBox::AlignItems::stretch;
    fb.items.add(juce::FlexItem(*name_label_).withFlex(3.0f));
    fb.items.add(juce::FlexItem().withWidth(8));
    if (meter_ != nullptr)
    {
        fb.items.add(juce::FlexItem(*meter_).withMinWidth(80).withWidth(80));
        fb.items.add(juce::FlexItem().withWidth(8));
    }
    fb.items.add(juce::FlexItem(*bypass_button_).withMinWidth(72).withWidth(72));
    fb.items.add(juce::FlexItem().withWidth(8));
    fb.items.add(juce::FlexItem(*editor_button_).withMinWidth(60).withWidth(60));
    fb.items.add(juce::FlexItem().withWidth(8));
    fb.items.add(juce::FlexItem(*tag_button_).withMinWidth(44).withWidth(44));
    fb.items.add(juce::FlexItem().withWidth(8));
    fb.items.add(juce::FlexItem(*remove_button_).withMinWidth(60).withWidth(60));
    fb.performLayout(b);

    // Split the title area so the tag sits immediately after the plugin name
    // rather than at the far end of the flexible column.
    if (tag_label_->isVisible())
    {
        auto title = name_label_->getBounds();
        const int text_w =
            juce::GlyphArrangement::getStringWidthInt(name_label_->getFont(), name_label_->getText()) + 12;
        const int name_w = juce::jlimit(24, juce::jmax(24, title.getWidth() - 40), text_w);
        name_label_->setBounds(title.removeFromLeft(name_w));
        tag_label_->setBounds(title);
    }

    // Keep the horizontal meter vertically centred within the row (max 16 px tall).
    if (meter_ != nullptr)
    {
        auto mb = meter_->getBounds();
        meter_->setBounds(mb.withSizeKeepingCentre(mb.getWidth(), 16));
    }
}

bool ChainSlotRow::keyPressed(const juce::KeyPress& key)
{
    if (!engine_)
    {
        return false;
    }

    if (key == juce::KeyPress::returnKey)
    {
        if (slot_.kind != PluginSlotKind::Placeholder && !slot_.is_failed)
        {
            auto snapshot = engine_->snapshotChain();
            if (position_ >= 0 && position_ < static_cast<int>(snapshot.slots.size()))
            {
                engine_->openEditor(position_);
            }
        }
        return true;
    }

    if (key == juce::KeyPress('b', juce::ModifierKeys::ctrlModifier, 0))
    {
        if (slot_.kind != PluginSlotKind::Placeholder && !slot_.is_failed)
        {
            auto snapshot = engine_->snapshotChain();
            if (position_ >= 0 && position_ < static_cast<int>(snapshot.slots.size()))
            {
                bool current = snapshot.slots[position_].is_bypassed;
                engine_->setBypass(position_, !current);
            }
        }
        return true;
    }

    return false;
}

void ChainSlotRow::mouseEnter(const juce::MouseEvent&)
{
    repaint();
}

void ChainSlotRow::mouseExit(const juce::MouseEvent&)
{
    repaint();
}

void ChainSlotRow::mouseDrag(const juce::MouseEvent& event)
{
    if (!isDraggable())
    {
        return;
    }

    // Only start a real drag once the pointer has moved a little, so a plain
    // click on the row title is not mistaken for a drag.
    if (event.getDistanceFromDragStart() < 5)
    {
        return;
    }

    auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this);
    if (container != nullptr && !container->isDragAndDropActive())
    {
        LogDebug("ChainSlotRow::mouseDrag: starting drag from position %d", position_);
        container->startDragging(juce::var(position_), this);
    }
}

bool ChainSlotRow::isInterestedInDragSource(const SourceDetails& details)
{
    // Only accept rows dragged from this chain editor (integer position payload),
    // and never a row dropping onto itself's no-op handled at drop time.
    return details.description.isInt();
}

void ChainSlotRow::itemDragEnter(const SourceDetails& details)
{
    insert_above_ = details.localPosition.y < getHeight() / 2;
    drag_over_ = true;
    repaint();
}

void ChainSlotRow::itemDragMove(const SourceDetails& details)
{
    const bool above = details.localPosition.y < getHeight() / 2;
    if (above != insert_above_ || !drag_over_)
    {
        insert_above_ = above;
        drag_over_ = true;
        repaint();
    }
}

void ChainSlotRow::itemDragExit(const SourceDetails&)
{
    drag_over_ = false;
    repaint();
}

void ChainSlotRow::itemDropped(const SourceDetails& details)
{
    drag_over_ = false;
    repaint();

    if (!details.description.isInt() || !engine_)
    {
        return;
    }

    const int from = static_cast<int>(details.description);
    auto snapshot = engine_->snapshotChain();

    if (from < 0 || from >= static_cast<int>(snapshot.slots.size()) ||
        position_ < 0 || position_ >= static_cast<int>(snapshot.slots.size()))
    {
        LogDebug("ChainSlotRow::itemDropped: Invalid positions (from=%d, to=%d, chain_size=%zu)",
                 from, position_, snapshot.slots.size());
        return;
    }

    const bool after = details.localPosition.y >= getHeight() / 2;

    // Translate "insert before/after this row" into the final index expected by
    // IAudioEngine::moveSlot (which erases at `from` then inserts at `to`).
    int to;
    if (from < position_)
    {
        to = after ? position_ : position_ - 1;
    }
    else
    {
        to = after ? position_ + 1 : position_;
    }

    if (from == to)
    {
        return;
    }

    LogDebug("ChainSlotRow::itemDropped: reorder from %d to %d", from, to);
    if (onReorderRequest)
    {
        onReorderRequest(from, to);
    }
}

// =========================================================================
// ChainEditor
// =========================================================================

ChainEditor::ChainEditor(IAudioEngine* engine)
    : engine_(engine)
{
    content_ = std::make_unique<juce::Component>();
    setViewedComponent(content_.get(), false);

    add_plugin_button_ = std::make_unique<juce::TextButton>("+");
    add_plugin_button_->addListener(this);
    content_->addAndMakeVisible(add_plugin_button_.get());
}

void ChainEditor::refreshFromEngine()
{
    if (!engine_)
    {
        LogDebug("ChainEditor::refreshFromEngine: Engine is null");
        rows_.clear();
        return;
    }

    rows_.clear();

    const auto snapshot = engine_->snapshotChain();
    const int w = std::max(getWidth(), 400);

    int y = 0;
    for (std::size_t i = 0; i < snapshot.slots.size(); ++i)
    {
        auto row = std::make_unique<ChainSlotRow>(engine_, static_cast<int>(i), snapshot.slots[i]);
        row->setBounds(0, y, w, kRowHeight);
        row->onReorderRequest = [this](int from, int to) {
            LogDebug("ChainEditor: moveSlot(%d, %d)", from, to);
            if (engine_)
            {
                engine_->moveSlot(from, to);
                refreshFromEngine();
            }
        };
        content_->addAndMakeVisible(row.get());
        rows_.push_back(std::move(row));
        y += kRowHeight;
    }

    // Add the "+" button at the end of the chain.
    add_plugin_button_->setBounds(0, y, w, kRowHeight);
    content_->setSize(w, y + kRowHeight);
}

void ChainEditor::resized()
{
    juce::Viewport::resized();
    const int w = std::max(getWidth(), 400);
    for (int i = 0; i < static_cast<int>(rows_.size()); ++i)
        rows_[i]->setBounds(0, i * kRowHeight, w, kRowHeight);
    if (add_plugin_button_)
        add_plugin_button_->setBounds(0, static_cast<int>(rows_.size()) * kRowHeight, w, kRowHeight);
    int total_height = (static_cast<int>(rows_.size()) + 1) * kRowHeight;
    content_->setSize(w, std::max(total_height, 100));
}

void ChainEditor::setPluginMeterLevels(const std::vector<float>& peaks,
                                        const std::vector<float>& rms)
{
    const int count = static_cast<int>(std::min(peaks.size(), rms.size()));
    const int rows = static_cast<int>(rows_.size());
    for (int i = 0; i < count && i < rows; ++i)
    {
        rows_[i]->setMeterLevels(linearToDb(peaks[i]), linearToDb(rms[i]));
    }
}

void ChainEditor::buttonClicked(juce::Button* button)
{
    if (button == add_plugin_button_.get())
    {
        if (onAddPluginRequested)
        {
            onAddPluginRequested();
        }
    }
}

}  // namespace jyglobalvst::tray
