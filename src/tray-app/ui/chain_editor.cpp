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

    name_label_ = std::make_unique<juce::Label>();
    name_label_->setText(juce::String(slot_.ref.name.empty() ? "Placeholder" : slot_.ref.name),
                         juce::dontSendNotification);
    // The label is display-only; let mouse drags fall through to the row so the
    // row can be grabbed and dragged by its title.
    name_label_->setInterceptsMouseClicks(false, false);
    addAndMakeVisible(name_label_.get());

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

    remove_button_ = std::make_unique<juce::TextButton>("X");
    remove_button_->addListener(this);
    addAndMakeVisible(remove_button_.get());

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
    else if (button == remove_button_.get())
    {
        LogDebug("  -> Remove button clicked");
        engine_->removeSlot(position_);
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
    fb.items.add(juce::FlexItem(*bypass_button_).withMinWidth(72).withWidth(72));
    fb.items.add(juce::FlexItem().withWidth(8));
    fb.items.add(juce::FlexItem(*editor_button_).withMinWidth(60).withWidth(60));
    fb.items.add(juce::FlexItem().withWidth(8));
    fb.items.add(juce::FlexItem(*remove_button_).withMinWidth(60).withWidth(60));
    fb.performLayout(b);
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
