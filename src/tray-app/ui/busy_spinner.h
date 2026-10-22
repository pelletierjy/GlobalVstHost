// src/tray-app/ui/busy_spinner.h
//
// Busy indicator for slow operations: engine stop -> mutation -> restart around
// audio-settings changes (EngineSettingsGuard scopes) and startup restore.
//
// Two layers work together:
//  - BusySpinner: animated arc in the status bar. It renders whenever the
//    message loop pumps (startup frames, gaps between mutations).
//  - UiBusyScope: shows the spinner AND raises the system busy cursor
//    (MouseCursor::showWaitCursor). Windows draws its animated spinner around
//    the pointer even while the message thread is blocked inside stop()/start()
//    -- exactly the case where an in-app spinner cannot animate. The window
//    cursor is also pinned to Wait so hovering keeps signalling "busy".

#pragma once

#include "custom_look_and_feel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace jyglobalvst::tray {

class BusySpinner : public juce::Component,
                    private juce::Timer
{
public:
    BusySpinner()
    {
        setInterceptsMouseClicks(false, false);
    }

    ~BusySpinner() override
    {
        stopTimer();
    }

    void setActive(bool active)
    {
        if (active_ == active)
            return;
        active_ = active;
        if (active_)
        {
            angle_ = 0.0f;
            startTimerHz(30);
        }
        else
        {
            stopTimer();
        }
        repaint();
    }

    bool isActive() const { return active_; }

    void paint(juce::Graphics& g) override
    {
        if (!active_)
            return;

        const auto area = getLocalBounds().toFloat().reduced(3.0f);
        const auto centre = area.getCentre();
        const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f - 1.0f;
        if (radius <= 1.0f)
            return;

        g.setColour(kTextDim.withAlpha(0.35f));
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);

        const float arc = juce::degreesToRadians(110.0f);
        juce::Path ring;
        ring.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, angle_, angle_ + arc, true);
        g.setColour(kAccentCyan);
        g.strokePath(ring, juce::PathStrokeType(2.0f));
    }

private:
    void timerCallback() override
    {
        angle_ += juce::degreesToRadians(12.0f);
        if (angle_ >= juce::MathConstants<float>::twoPi)
            angle_ -= juce::MathConstants<float>::twoPi;
        repaint();
    }

    bool active_ {false};
    float angle_ {0.0f};
};

// RAII busy scope: spinner + system busy cursor for the lifetime of the scope.
// Nested/reentrant use (e.g. a scope during startup) restores the previous
// state instead of forcing idle.
class UiBusyScope
{
public:
    explicit UiBusyScope(BusySpinner& spinner)
        : spinner_(spinner), was_active_(spinner.isActive())
    {
        spinner_.setActive(true);
        juce::MouseCursor::showWaitCursor();
        if (auto* top = spinner_.getTopLevelComponent())
            top->setMouseCursor(juce::MouseCursor::WaitCursor);
    }

    ~UiBusyScope()
    {
        juce::MouseCursor::hideWaitCursor();
        if (!was_active_)
        {
            spinner_.setActive(false);
            if (auto* top = spinner_.getTopLevelComponent())
                top->setMouseCursor(juce::MouseCursor::NormalCursor);
        }
    }

    UiBusyScope(const UiBusyScope&) = delete;
    UiBusyScope& operator=(const UiBusyScope&) = delete;

private:
    BusySpinner& spinner_;
    bool was_active_ {false};
};

}  // namespace jyglobalvst::tray