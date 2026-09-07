// src/tray-app/ui/custom_look_and_feel.h
//
// Neon/glow LookAndFeel for GlobalVSTHost tray UI.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace jyglobalvst::tray {

// Default NeonBlue colour constants (kept for backward compatibility as fallback).
inline const juce::Colour kBgDeep       {0xFF0A0E1A};
inline const juce::Colour kBgPanel      {0xFF12182B};
inline const juce::Colour kBgPanelBorder{0xFF1E2A4A};
inline const juce::Colour kAccentCyan   {0xFF00E5FF};
inline const juce::Colour kAccentBlue   {0xFF2979FF};
inline const juce::Colour kAccentGlow   {0x4000E5FF};
inline const juce::Colour kTextPrimary  {0xFFE0E6F1};
inline const juce::Colour kTextDim      {0xFF8A96B8};
inline const juce::Colour kControlBg    {0xFF1A2340};
inline const juce::Colour kControlHover {0xFF243560};
inline const juce::Colour kTrackBg      {0xFF0F1525};
inline const juce::Colour kMeterBg      {0xFF080C14};
inline const juce::Colour kMagenta      {0xFFD500F9};
inline const juce::Colour kRedNeon      {0xFFFF1744};

// Shared active/inactive icon colours. The leaf button established this
// language (green = active, grey = inactive); the power icon reuses the exact
// same colours so state-toggle icons read consistently across the UI.
inline const juce::Colour kIconActiveGreen  {0xFF00E676};
inline const juce::Colour kIconInactiveGrey {0xFF5A6472};

// All per-theme colours grouped together.
struct ThemeColors
{
    juce::Colour bgDeep;
    juce::Colour bgPanel;
    juce::Colour bgPanelBorder;
    juce::Colour accentCyan;
    juce::Colour accentBlue;
    juce::Colour accentGlow;
    juce::Colour textPrimary;
    juce::Colour textDim;
    juce::Colour controlBg;
    juce::Colour controlHover;
    juce::Colour trackBg;
    juce::Colour meterBg;
    juce::Colour magenta;
    juce::Colour redNeon;
};

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    enum class ThemeId : int
    {
        NeonBlue   = 1,
        NeonPurple = 2,
        NeonGreen  = 3,
        NeonOrange = 4,
        Mono       = 5,
        NeonRed    = 6,
        Light      = 7,
    };

    CustomLookAndFeel()
    {
        applyTheme(ThemeId::NeonBlue);
    }

    void applyTheme(ThemeId id)
    {
        current_theme_ = id;
        colors_ = colorsForTheme(id);
        applyColors();
    }

    ThemeId currentTheme() const { return current_theme_; }
    const ThemeColors& colors() const { return colors_; }

    // Glass panel helper -------------------------------------------------------
    void drawGlassPanel(juce::Graphics& g, juce::Rectangle<float> bounds,
                        float cornerRadius = 8.0f) const
    {
        g.setColour(colors_.bgPanel);
        g.fillRoundedRectangle(bounds, cornerRadius);

        g.setColour(colors_.accentGlow.withAlpha(0.08f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), cornerRadius, 2.0f);

        g.setColour(colors_.bgPanelBorder);
        g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);

        auto topLine = bounds.removeFromTop(1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.fillRect(topLine);
    }

    // Glow outline helper ------------------------------------------------------
    void drawGlowOutline(juce::Graphics& g, juce::Rectangle<float> bounds,
                         juce::Colour glowColour, float cornerRadius = 8.0f) const
    {
        for (int i = 3; i >= 0; --i)
        {
            float alpha = 0.10f - i * 0.025f;
            float inset = static_cast<float>(i);
            g.setColour(glowColour.withAlpha(alpha));
            g.drawRoundedRectangle(bounds.reduced(inset), cornerRadius, 1.0f);
        }
    }

    // Button background --------------------------------------------------------
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& /*backgroundColour*/,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        constexpr float radius = 6.0f;

        if (shouldDrawButtonAsDown)
        {
            g.setColour(colors_.accentBlue.darker(0.2f));
            g.fillRoundedRectangle(bounds, radius);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(colors_.controlHover);
            g.fillRoundedRectangle(bounds, radius);
            drawGlowOutline(g, bounds, colors_.accentCyan, radius);
        }
        else
        {
            g.setColour(colors_.controlBg);
            g.fillRoundedRectangle(bounds, radius);
        }

        // State buttons (power / mute / EQ / NT) convey on/off through their
        // green/grey drawing, so they skip the cyan toggle frame.
        if (button.getToggleState() && !isStateButton(button))
        {
            g.setColour(colors_.accentCyan);
            g.drawRoundedRectangle(bounds, radius, 1.5f);
            drawGlowOutline(g, bounds, colors_.accentCyan, radius);
        }
    }

    // Toggle button (override to prevent checkbox rendering) -------------------
    void drawToggleButton(juce::Graphics& g,
                         juce::ToggleButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override
    {
        // Use the standard button background rendering instead of checkbox
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        constexpr float radius = 6.0f;

        if (shouldDrawButtonAsDown)
        {
            g.setColour(colors_.accentBlue.darker(0.2f));
            g.fillRoundedRectangle(bounds, radius);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(colors_.controlHover);
            g.fillRoundedRectangle(bounds, radius);
            drawGlowOutline(g, bounds, colors_.accentCyan, radius);
        }
        else
        {
            g.setColour(colors_.controlBg);
            g.fillRoundedRectangle(bounds, radius);
        }

        // State buttons (power / mute / EQ / NT) — e.g. the tray popup toggles —
        // show on/off via green/grey, so they skip the cyan toggle frame.
        if (button.getToggleState() && !isStateButton(button))
        {
            g.setColour(colors_.accentCyan);
            g.drawRoundedRectangle(bounds, radius, 1.5f);
            drawGlowOutline(g, bounds, colors_.accentCyan, radius);
        }

        // Draw the text
        drawButtonTextForButton(g, button);
    }

    // Button text with icon drawing -------------------------------------------
    void drawButtonText(juce::Graphics& g,
                       juce::TextButton& button,
                       bool shouldDrawButtonAsHighlighted,
                       bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        drawButtonTextForButton(g, button);
    }

private:
    void drawButtonTextForButton(juce::Graphics& g, juce::Button& button)
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto text = button.getButtonText();
        auto textColour = button.findColour(button.getToggleState()
                                          ? juce::TextButton::textColourOnId
                                          : juce::TextButton::textColourOffId);

        g.setColour(textColour);

        if (text == "X")
        {
            drawTrashIcon(g, bounds, textColour);
        }
        else if (text == "E")
        {
            drawPencilIcon(g, bounds, textColour);
        }
        else if (text == "ON")
        {
            drawOnOffIcon(g, bounds, true, textColour);
        }
        else if (text == "OFF")
        {
            drawOnOffIcon(g, bounds, false, textColour);
        }
        else if (text == "SPEAKER_ON")
        {
            drawSpeakerIcon(g, bounds, true, textColour);
        }
        else if (text == "SPEAKER_OFF")
        {
            drawSpeakerIcon(g, bounds, false, textColour);
        }
        else if (text == "TAG")
        {
            drawTagIcon(g, bounds, false, textColour);
        }
        else if (text == "TAG_SET")
        {
            drawTagIcon(g, bounds, true, textColour);
        }
        else if (text == "SHORTCUT")
        {
            drawShortcutIcon(g, bounds, false, textColour);
        }
        else if (text == "SHORTCUT_SET")
        {
            drawShortcutIcon(g, bounds, true, textColour);
        }
        else if (text == "?")
        {
            drawQuestionIcon(g, bounds, textColour);
        }
        else if (text == "+")
        {
            drawPlusIcon(g, bounds, textColour);
        }
        else if (isStateTextButton(button))
        {
            // Text state buttons: green when active (toggled on), grey when off —
            // same language as the power/leaf icons, no cyan frame.
            g.setColour(button.getToggleState() ? kIconActiveGreen : kIconInactiveGrey);
            g.setFont(juce::Font{juce::FontOptions{}}.withHeight(14.0f).boldened());
            g.drawFittedText(text, bounds.toNearestInt(),
                           juce::Justification::centred, 1);
        }
        else
        {
            // Render regular text for all other buttons.
            g.setColour(textColour);
            g.setFont(14.0f);
            g.drawFittedText(text, bounds.toNearestInt(),
                           juce::Justification::centred, 1);
        }
    }

public:
    // Signal-direction marker drawn under the tray popup's meter pairs: an arrow
    // meeting a baseline (pointing down = signal coming in, pointing up = signal
    // going out). Stroked with the same rounded pen and filled arrowhead as the
    // power / mute / pencil marks so it reads as part of the same icon family.
    static void drawSignalDirectionIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                                        bool isInput, juce::Colour colour)
    {
        const auto c = bounds.getCentre();
        const float s = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

        const float stemTop = c.y - s * 0.74f;
        const float stemBot = c.y + s * 0.26f;
        const float headHalfW = s * 0.36f;
        const float headH = s * 0.38f;
        const float baseHalfW = s * 0.62f;
        const float baseY = c.y + s * 0.74f;

        g.setColour(colour);

        // Shaft, stopping short of the arrowhead so the filled tip stays crisp.
        juce::Path stem;
        if (isInput)
        {
            stem.startNewSubPath(c.x, stemTop);
            stem.lineTo(c.x, stemBot - headH * 0.55f);
        }
        else
        {
            stem.startNewSubPath(c.x, stemTop + headH * 0.55f);
            stem.lineTo(c.x, stemBot);
        }
        g.strokePath(stem, iconStroke(1.5f));

        // Arrowhead: down at the baseline for input, up and away for output.
        juce::Path head;
        if (isInput)
        {
            head.startNewSubPath(c.x - headHalfW, stemBot - headH);
            head.lineTo(c.x + headHalfW, stemBot - headH);
            head.lineTo(c.x, stemBot);
        }
        else
        {
            head.startNewSubPath(c.x - headHalfW, stemTop + headH);
            head.lineTo(c.x + headHalfW, stemTop + headH);
            head.lineTo(c.x, stemTop);
        }
        head.closeSubPath();
        g.fillPath(head);

        // Baseline the signal arrives at / departs from.
        juce::Path base;
        base.startNewSubPath(c.x - baseHalfW, baseY);
        base.lineTo(c.x + baseHalfW, baseY);
        g.strokePath(base, iconStroke(1.5f));
    }

private:
    // Buttons whose text is one of these tokens convey their on/off state through
    // colour (green = active, grey = inactive) like the leaf button, so they skip
    // the cyan toggle frame that ordinary toggle buttons draw.
    static bool isStateButtonText(const juce::String& t)
    {
        return t == "ON" || t == "OFF"
            || t == "SPEAKER_ON" || t == "SPEAKER_OFF"
            || t == "TAG" || t == "TAG_SET"
            || t == "SHORTCUT" || t == "SHORTCUT_SET"
            || t == "EQ" || t == "NT" || t == "VL";
    }

    // Buttons whose label is user-supplied (the tray popup's shortcut toggles
    // carry two letters taken from a plugin name or tag) cannot be recognised by
    // their text, so they flag themselves with this component property instead.
    static bool isStateTextButton(const juce::Button& b)
    {
        const auto t = b.getButtonText();
        return static_cast<bool>(b.getProperties().getWithDefault("stateText", false))
            || t == "EQ" || t == "NT" || t == "VL";
    }

    static bool isStateButton(const juce::Button& b)
    {
        return isStateButtonText(b.getButtonText()) || isStateTextButton(b);
    }

    // Shared stroke for all vector icons: rounded caps + joints and a common
    // weight, so the trash / pencil / power / question / leaf marks read as one
    // family (matching the LeafButton in main_window.cpp).
    static juce::PathStrokeType iconStroke(float width = 1.8f)
    {
        return juce::PathStrokeType(width, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded);
    }

    // Subtle translucent fill sat under an icon's outline, echoing the leaf.
    static constexpr float kIconFillAlpha = 0.20f;

    void drawTrashIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                       juce::Colour colour) const
    {
        const auto c = bounds.getCentre();
        const float s = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

        // Gently tapering can body (trapezoid) with a soft fill + outline.
        const float topHalf = s * 0.38f;
        const float botHalf = s * 0.30f;
        const float bodyTop = c.y - s * 0.20f;
        const float bodyBot = c.y + s * 0.52f;

        juce::Path body;
        body.startNewSubPath(c.x - topHalf, bodyTop);
        body.lineTo(c.x + topHalf, bodyTop);
        body.lineTo(c.x + botHalf, bodyBot);
        body.lineTo(c.x - botHalf, bodyBot);
        body.closeSubPath();

        g.setColour(colour.withAlpha(kIconFillAlpha));
        g.fillPath(body);
        g.setColour(colour);
        g.strokePath(body, iconStroke());

        // Lid.
        juce::Path lid;
        const float lidY = bodyTop - s * 0.16f;
        lid.startNewSubPath(c.x - s * 0.52f, lidY);
        lid.lineTo(c.x + s * 0.52f, lidY);
        g.strokePath(lid, iconStroke());

        // Handle: a small bump arc above the lid.
        juce::Path handle;
        handle.addCentredArc(c.x, lidY, s * 0.18f, s * 0.18f, 0.0f,
                             -juce::MathConstants<float>::halfPi,
                             juce::MathConstants<float>::halfPi, true);
        g.strokePath(handle, iconStroke());

        // Two ribs inside the can (echoes the leaf's midrib).
        juce::Path ribs;
        for (int i = -1; i <= 1; i += 2)
        {
            const float x = c.x + i * s * 0.15f;
            ribs.startNewSubPath(x, bodyTop + s * 0.14f);
            ribs.lineTo(x, bodyBot - s * 0.12f);
        }
        g.strokePath(ribs, iconStroke(1.4f));
    }

    void drawPencilIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                        juce::Colour colour) const
    {
        const auto c = bounds.getCentre();
        const float s = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float half = s * 0.20f;

        const float topY = c.y - s * 0.5f;   // eraser end
        const float bandY = c.y + s * 0.24f;  // wood/paint band
        const float tipY = c.y + s * 0.56f;   // graphite point

        // Barrel with a soft fill + outline.
        juce::Path barrel;
        barrel.startNewSubPath(c.x - half, topY);
        barrel.lineTo(c.x + half, topY);
        barrel.lineTo(c.x + half, bandY);
        barrel.lineTo(c.x - half, bandY);
        barrel.closeSubPath();

        g.setColour(colour.withAlpha(kIconFillAlpha));
        g.fillPath(barrel);
        g.setColour(colour);
        g.strokePath(barrel, iconStroke());

        // Wood tip (triangle) with a filled graphite point.
        juce::Path tip;
        tip.startNewSubPath(c.x - half, bandY);
        tip.lineTo(c.x + half, bandY);
        tip.lineTo(c.x, tipY);
        tip.closeSubPath();
        g.strokePath(tip, iconStroke());

        juce::Path point;
        point.startNewSubPath(c.x - half * 0.42f, tipY - s * 0.14f);
        point.lineTo(c.x + half * 0.42f, tipY - s * 0.14f);
        point.lineTo(c.x, tipY);
        point.closeSubPath();
        g.fillPath(point);

        // Midrib down the barrel + the band line.
        juce::Path detail;
        detail.startNewSubPath(c.x, topY + s * 0.1f);
        detail.lineTo(c.x, bandY - s * 0.04f);
        g.strokePath(detail, iconStroke(1.4f));
    }

    // Shopping / price tag. Outlined grey while the slot carries no tag; green
    // and scribbled on ("scratched") once a tag has been written. Like the power
    // icon, state reads from colour, so the passed-in text colour is ignored.
    void drawTagIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool hasTag,
                     juce::Colour /*colour*/) const
    {
        const auto c = bounds.getCentre();
        const float s = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float w = s * 0.40f;
        const float h = s * 0.58f;

        const juce::Colour markColour = hasTag ? kIconActiveGreen : kIconInactiveGrey;

        // Square-ended body tapering to a point, drawn upright then rotated 45°
        // so it hangs like a price tag.
        juce::Path tag;
        tag.startNewSubPath(-w, -h);
        tag.lineTo(w, -h);
        tag.lineTo(w, h * 0.30f);
        tag.lineTo(0.0f, h);
        tag.lineTo(-w, h * 0.30f);
        tag.closeSubPath();

        const float hole = w * 0.30f;
        juce::Path holePath;
        holePath.addEllipse(-hole, -h + w * 0.45f - hole, hole * 2.0f, hole * 2.0f);

        const auto place = juce::AffineTransform::rotation(juce::MathConstants<float>::pi * 0.25f)
                               .translated(c.x, c.y);
        tag.applyTransform(place);
        holePath.applyTransform(place);

        g.setColour(markColour.withAlpha(hasTag ? 0.30f : kIconFillAlpha));
        g.fillPath(tag);
        g.setColour(markColour);
        g.strokePath(tag, iconStroke());
        g.strokePath(holePath, iconStroke(1.4f));

        if (hasTag)
        {
            // Two scored lines across the body: the tag has been written on.
            juce::Path scribble;
            scribble.startNewSubPath(-w * 0.62f, -h * 0.18f);
            scribble.lineTo(w * 0.62f, -h * 0.34f);
            scribble.startNewSubPath(-w * 0.62f, h * 0.22f);
            scribble.lineTo(w * 0.62f, h * 0.06f);
            scribble.applyTransform(place);
            g.strokePath(scribble, iconStroke(1.4f));
        }
    }

    // Shortcut mark: a rounded frame with a diagonal arrow springing out of its
    // top-right corner, echoing the familiar desktop shortcut overlay. Outlined
    // grey while the slot owns no tray button; green and filled once assigned.
    // State reads from colour, so the passed-in text colour is ignored.
    void drawShortcutIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool assigned,
                          juce::Colour /*colour*/) const
    {
        const auto c = bounds.getCentre();
        const float s = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float r = s * 0.52f;

        const juce::Colour markColour = assigned ? kIconActiveGreen : kIconInactiveGrey;

        // Frame, open at the top-right so the arrow reads as leaving it.
        juce::Path frame;
        frame.startNewSubPath(c.x + r * 0.15f, c.y - r);
        frame.lineTo(c.x - r * 0.72f, c.y - r);
        frame.quadraticTo(c.x - r, c.y - r, c.x - r, c.y - r * 0.72f);
        frame.lineTo(c.x - r, c.y + r * 0.72f);
        frame.quadraticTo(c.x - r, c.y + r, c.x - r * 0.72f, c.y + r);
        frame.lineTo(c.x + r * 0.72f, c.y + r);
        frame.quadraticTo(c.x + r, c.y + r, c.x + r, c.y + r * 0.72f);
        frame.lineTo(c.x + r, c.y - r * 0.15f);

        g.setColour(markColour.withAlpha(assigned ? 0.30f : kIconFillAlpha));
        g.fillPath(frame);
        g.setColour(markColour);
        g.strokePath(frame, iconStroke());

        // Diagonal arrow breaking out through the open corner.
        const float tip_x = c.x + r * 0.92f;
        const float tip_y = c.y - r * 0.92f;
        juce::Path shaft;
        shaft.startNewSubPath(c.x - r * 0.12f, c.y + r * 0.12f);
        shaft.lineTo(tip_x, tip_y);
        g.strokePath(shaft, iconStroke());

        juce::Path head;
        head.startNewSubPath(tip_x - r * 0.62f, tip_y);
        head.lineTo(tip_x, tip_y);
        head.lineTo(tip_x, tip_y + r * 0.62f);
        if (assigned)
        {
            head.closeSubPath();
            g.fillPath(head);
        }
        g.strokePath(head, iconStroke(1.4f));
    }

    // Universal power symbol (IEC 5009): a ring broken at the top with a stem
    // through the gap. Coloured like the leaf button — green when active (on),
    // grey when inactive (off) — so state reads from colour, not a frame. The
    // passed-in text colour is intentionally ignored here.
    void drawOnOffIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isOn,
                       juce::Colour /*colour*/) const
    {
        const auto c = bounds.getCentre();
        const float s = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float r = s * 0.48f;
        const float gap = 0.62f;  // half-angle of the top gap, radians

        const juce::Colour markColour = isOn ? kIconActiveGreen : kIconInactiveGrey;

        if (isOn)
        {
            g.setColour(kIconActiveGreen.withAlpha(0.26f));
            g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        }

        juce::Path ring;
        ring.addCentredArc(c.x, c.y, r, r, 0.0f, gap,
                           juce::MathConstants<float>::twoPi - gap, true);
        g.setColour(markColour);
        g.strokePath(ring, iconStroke());

        // Stem through the gap.
        juce::Path stem;
        stem.startNewSubPath(c.x, c.y - r - s * 0.14f);
        stem.lineTo(c.x, c.y - s * 0.02f);
        g.strokePath(stem, iconStroke());
    }

    // Circular badge with a soft fill + "?" glyph — same outline+fill DNA as the
    // leaf and power icons.
    void drawQuestionIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                          juce::Colour colour) const
    {
        const auto c = bounds.getCentre();
        const float s = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float r = s * 0.62f;

        g.setColour(colour.withAlpha(kIconFillAlpha));
        g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);

        juce::Path ring;
        ring.addEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour(colour);
        g.strokePath(ring, iconStroke());

        juce::Font glyph{juce::FontOptions{}};
        glyph.setHeight(r * 1.5f);
        glyph.setBold(true);
        g.setFont(glyph);
        g.drawText("?", bounds.toNearestInt(), juce::Justification::centred, false);
    }

    // Plus icon inside a circular badge — same outline+fill family as the trash,
    // pencil, power and question icons.
    void drawPlusIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                      juce::Colour colour) const
    {
        const auto c = bounds.getCentre();
        const float s = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float r = s * 0.55f;

        g.setColour(colour.withAlpha(kIconFillAlpha));
        g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);

        juce::Path ring;
        ring.addEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour(colour);
        g.strokePath(ring, iconStroke());

        const float arm = r * 0.50f;

        juce::Path cross;
        cross.startNewSubPath(c.x - arm, c.y);
        cross.lineTo(c.x + arm, c.y);
        cross.startNewSubPath(c.x, c.y - arm);
        cross.lineTo(c.x, c.y + arm);
        g.strokePath(cross, iconStroke(2.0f));
    }

    // Coloured like the rest of the state icons: green when active (unmuted),
    // grey when inactive (muted). The passed-in text colour is ignored.
    void drawSpeakerIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isOn,
                         juce::Colour /*colour*/) const
    {
        const auto center = bounds.getCentre();
        const float size = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;
        const float iconLeft = center.x - size * 0.9f;

        const juce::Colour markColour = isOn ? kIconActiveGreen : kIconInactiveGrey;

        // Speaker body + cone as one outlined shape with a soft fill, matching
        // the rest of the icon family (leaf / trash / pencil / power).
        const float bodyW = size * 0.35f;
        const float bodyH = size * 0.55f;
        const juce::Rectangle<float> body(iconLeft, center.y - bodyH * 0.5f, bodyW, bodyH);

        juce::Path speaker;
        speaker.startNewSubPath(body.getX(), body.getY());
        speaker.lineTo(body.getRight(), body.getY());
        speaker.lineTo(body.getRight() + size * 0.3f, center.y - size * 0.7f);
        speaker.lineTo(body.getRight() + size * 0.3f, center.y + size * 0.7f);
        speaker.lineTo(body.getRight(), body.getBottom());
        speaker.lineTo(body.getX(), body.getBottom());
        speaker.closeSubPath();

        g.setColour(markColour.withAlpha(kIconFillAlpha));
        g.fillPath(speaker);
        g.setColour(markColour);
        g.strokePath(speaker, iconStroke());

        const float markX = body.getRight() + size * 0.55f;
        if (isOn)
        {
            // Sound waves emanating to the right.
            const float startAngle = juce::MathConstants<float>::halfPi - juce::MathConstants<float>::pi * 0.35f;
            const float endAngle   = juce::MathConstants<float>::halfPi + juce::MathConstants<float>::pi * 0.35f;
            for (int i = 0; i < 2; ++i)
            {
                const float r = size * (0.3f + i * 0.28f);
                juce::Path arc;
                arc.addCentredArc(markX, center.y, r, r, 0.0f, startAngle, endAngle, true);
                g.strokePath(arc, iconStroke(1.6f));
            }
        }
        else
        {
            // Mute X mark.
            juce::Path x;
            x.startNewSubPath(markX, center.y - size * 0.35f);
            x.lineTo(markX + size * 0.5f, center.y + size * 0.35f);
            x.startNewSubPath(markX, center.y + size * 0.35f);
            x.lineTo(markX + size * 0.5f, center.y - size * 0.35f);
            g.strokePath(x, iconStroke(1.6f));
        }
    }

public:

    // ComboBox -----------------------------------------------------------------
    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool isButtonDown,
                      int buttonX,
                      int buttonY,
                      int buttonW,
                      int buttonH,
                      juce::ComboBox& box) override
    {
        juce::ignoreUnused(isButtonDown, buttonX, buttonY, buttonW, buttonH);
        auto bounds = juce::Rectangle<float>(1.0f, 1.0f,
                                              static_cast<float>(width) - 2.0f,
                                              static_cast<float>(height) - 2.0f);
        constexpr float radius = 6.0f;

        g.setColour(findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(bounds, radius);
        g.setColour(findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds, radius, 1.0f);

        if (box.isMouseOver())
        {
            drawGlowOutline(g, bounds, colors_.accentCyan, radius);
        }

        auto arrowArea = bounds.removeFromRight(static_cast<float>(height) * 0.8f).reduced(4.0f);
        juce::Path p;
        p.addTriangle(arrowArea.getX(), arrowArea.getY(),
                      arrowArea.getRight(), arrowArea.getY(),
                      arrowArea.getCentreX(), arrowArea.getBottom());
        g.setColour(findColour(juce::ComboBox::arrowColourId));
        g.fillPath(p);
    }

    // Linear slider ------------------------------------------------------------
    void drawLinearSlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float /*minSliderPos*/,
                          float /*maxSliderPos*/,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
                                                    0.f, 0.f, style, slider);
            return;
        }

        const float trackY = static_cast<float>(y + height / 2);
        const float trackH = 4.0f;
        const float thumbRadius = 7.0f;
        const float trackRadius = trackH * 0.5f;

        g.setColour(colors_.trackBg);
        g.fillRoundedRectangle(static_cast<float>(x), trackY - trackH * 0.5f,
                               static_cast<float>(width), trackH, trackRadius);

        const float fillWidth = sliderPos - static_cast<float>(x);
        if (fillWidth > 0.0f)
        {
            juce::ColourGradient grad(colors_.accentBlue, static_cast<float>(x), trackY,
                                      colors_.accentCyan, sliderPos, trackY, false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(static_cast<float>(x), trackY - trackH * 0.5f,
                                   fillWidth, trackH, trackRadius);
        }

        g.setColour(colors_.accentCyan);
        g.fillEllipse(sliderPos - thumbRadius, trackY - thumbRadius,
                      thumbRadius * 2.0f, thumbRadius * 2.0f);

        for (int i = 2; i >= 0; --i)
        {
            float r = thumbRadius + static_cast<float>(i) * 3.0f;
            g.setColour(colors_.accentCyan.withAlpha(0.12f - i * 0.04f));
            g.drawEllipse(sliderPos - r, trackY - r, r * 2.0f, r * 2.0f, 1.0f);
        }
    }

    // ScrollBar ----------------------------------------------------------------
    void drawScrollbar(juce::Graphics& g,
                       juce::ScrollBar& scrollbar,
                       int x, int y, int width, int height,
                       bool isScrollbarVertical,
                       int thumbStartPosition,
                       int thumbSize,
                       bool isMouseOver,
                       bool isMouseDown) override
    {
        juce::ignoreUnused(scrollbar, isMouseDown);
        auto bounds = juce::Rectangle<float>(static_cast<float>(x),
                                              static_cast<float>(y),
                                              static_cast<float>(width),
                                              static_cast<float>(height));

        g.setColour(colors_.trackBg);
        g.fillRect(bounds);

        if (thumbSize <= 0)
            return;

        juce::Rectangle<float> thumb;
        constexpr float radius = 4.0f;
        if (isScrollbarVertical)
        {
            thumb = bounds.withX(bounds.getX() + 2.0f)
                           .withWidth(bounds.getWidth() - 4.0f)
                           .withY(static_cast<float>(thumbStartPosition))
                           .withHeight(static_cast<float>(thumbSize));
        }
        else
        {
            thumb = bounds.withY(bounds.getY() + 2.0f)
                           .withHeight(bounds.getHeight() - 4.0f)
                           .withX(static_cast<float>(thumbStartPosition))
                           .withWidth(static_cast<float>(thumbSize));
        }

        g.setColour(colors_.accentBlue);
        g.fillRoundedRectangle(thumb, radius);

        if (isMouseOver)
        {
            drawGlowOutline(g, thumb, colors_.accentBlue, radius);
        }
    }

    // Popup menu background ----------------------------------------------------
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                              static_cast<float>(width),
                                              static_cast<float>(height));
        g.setColour(findColour(juce::PopupMenu::backgroundColourId));
        g.fillRect(bounds);
        g.setColour(colors_.bgPanelBorder);
        g.drawRect(bounds, 1.0f);
    }

    void drawPopupMenuItem(juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           bool isSeparator,
                           bool isActive,
                           bool isHighlighted,
                           bool isTicked,
                           bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColour) override
    {
        juce::ignoreUnused(isTicked, hasSubMenu, shortcutKeyText, icon);

        if (isSeparator)
        {
            auto r = area.reduced(5, 0);
            r.removeFromTop(juce::roundToInt(((float)r.getHeight() - 1.0f) * 0.5f));
            g.setColour(colors_.bgPanelBorder);
            g.fillRect(r.withHeight(1));
            return;
        }

        if (isHighlighted && isActive)
        {
            g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRect(area);
            auto highlightBar = area;
            g.setColour(colors_.accentCyan);
            g.fillRect(highlightBar.removeFromLeft(3));
        }

        g.setColour(textColour != nullptr ? *textColour
                                          : (isActive ? findColour(juce::PopupMenu::textColourId)
                                                      : findColour(juce::PopupMenu::headerTextColourId)));
        g.drawText(text, area.reduced(8, 0), juce::Justification::centredLeft, true);
    }

private:
    ThemeColors colors_ {};
    ThemeId current_theme_ {ThemeId::NeonBlue};

    void applyColors()
    {
        setColour(juce::ResizableWindow::backgroundColourId, colors_.bgDeep);
        setColour(juce::DocumentWindow::backgroundColourId, colors_.bgDeep);

        setColour(juce::TextButton::buttonColourId, colors_.controlBg);
        setColour(juce::TextButton::buttonOnColourId, colors_.accentBlue);
        setColour(juce::TextButton::textColourOffId, colors_.textPrimary);
        setColour(juce::TextButton::textColourOnId, colors_.textPrimary);

        setColour(juce::ComboBox::backgroundColourId, colors_.controlBg);
        setColour(juce::ComboBox::textColourId, colors_.textPrimary);
        setColour(juce::ComboBox::outlineColourId, colors_.bgPanelBorder);
        setColour(juce::ComboBox::arrowColourId, colors_.accentCyan);
        setColour(juce::ComboBox::focusedOutlineColourId, colors_.accentCyan);

        setColour(juce::Label::textColourId, colors_.textPrimary);
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);

        setColour(juce::ScrollBar::thumbColourId, colors_.accentBlue);

        setColour(juce::Slider::thumbColourId, colors_.accentCyan);
        setColour(juce::Slider::trackColourId, colors_.trackBg);
        setColour(juce::Slider::textBoxTextColourId, colors_.textPrimary);
        setColour(juce::Slider::textBoxBackgroundColourId, colors_.controlBg);
        setColour(juce::Slider::textBoxOutlineColourId, colors_.bgPanelBorder);

        setColour(juce::PopupMenu::backgroundColourId, colors_.bgPanel);
        setColour(juce::PopupMenu::textColourId, colors_.textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, colors_.controlHover);
        setColour(juce::PopupMenu::highlightedTextColourId, colors_.accentCyan);
        setColour(juce::PopupMenu::headerTextColourId, colors_.textDim);

        setColour(juce::ListBox::backgroundColourId, colors_.bgPanel);
        setColour(juce::ListBox::outlineColourId, colors_.bgPanelBorder);

        setColour(juce::TextEditor::backgroundColourId, colors_.bgPanel);
        setColour(juce::TextEditor::textColourId, colors_.textPrimary);
        setColour(juce::TextEditor::highlightColourId, colors_.controlHover);
        setColour(juce::TextEditor::focusedOutlineColourId, colors_.accentCyan);
        setColour(juce::TextEditor::outlineColourId, colors_.bgPanelBorder);
    }

    static ThemeColors colorsForTheme(ThemeId id)
    {
        using C = juce::Colour;
        switch (id)
        {
        case ThemeId::NeonBlue:
            return { C(0xFF0A0E1A), C(0xFF12182B), C(0xFF1E2A4A),
                     C(0xFF00E5FF), C(0xFF2979FF), C(0x4000E5FF),
                     C(0xFFE0E6F1), C(0xFF8A96B8),
                     C(0xFF1A2340), C(0xFF243560), C(0xFF0F1525),
                     C(0xFF080C14), C(0xFFD500F9), C(0xFFFF1744) };

        case ThemeId::NeonPurple:
            return { C(0xFF0D0A1A), C(0xFF1A1228), C(0xFF2E1A4A),
                     C(0xFFBB00FF), C(0xFF7B00D4), C(0x40BB00FF),
                     C(0xFFE6E0F1), C(0xFF9A8AB8),
                     C(0xFF1E1A38), C(0xFF2E2858), C(0xFF120F22),
                     C(0xFF0A080E), C(0xFFFF00CC), C(0xFFFF1744) };

        case ThemeId::NeonGreen:
            return { C(0xFF0A140A), C(0xFF0F1E0F), C(0xFF1A3A1A),
                     C(0xFF00FF88), C(0xFF00C455), C(0x4000FF88),
                     C(0xFFE0F1E4), C(0xFF8AB896),
                     C(0xFF112211), C(0xFF1A3A1A), C(0xFF080F08),
                     C(0xFF040804), C(0xFF00FFCC), C(0xFFFF3D00) };

        case ThemeId::NeonOrange:
            return { C(0xFF140A00), C(0xFF1E1200), C(0xFF3A2200),
                     C(0xFFFF8C00), C(0xFFE65100), C(0x40FF8C00),
                     C(0xFFF1E8E0), C(0xFFB8A08A),
                     C(0xFF281800), C(0xFF3A2400), C(0xFF0F0800),
                     C(0xFF080400), C(0xFFFFAB00), C(0xFFFF1744) };

        case ThemeId::NeonRed:
            return { C(0xFF140505), C(0xFF1E0B0B), C(0xFF3A1414),
                     C(0xFFFF1744), C(0xFFD50032), C(0x40FF1744),
                     C(0xFFF1E0E0), C(0xFFB88A8A),
                     C(0xFF281010), C(0xFF3A1818), C(0xFF150808),
                     C(0xFF0A0404), C(0xFFFF5252), C(0xFFFF8A00) };

        case ThemeId::Mono:
            return { C(0xFF0D0D0D), C(0xFF1A1A1A), C(0xFF2E2E2E),
                     C(0xFFE0E0E0), C(0xFF808080), C(0x40E0E0E0),
                     C(0xFFE8E8E8), C(0xFF888888),
                     C(0xFF222222), C(0xFF333333), C(0xFF111111),
                     C(0xFF080808), C(0xFFCCCCCC), C(0xFFAAAAAA) };

        case ThemeId::Light:
            return { C(0xFFF2F4F8), C(0xFFFFFFFF), C(0xFFD3D9E3),
                     C(0xFF0077C2), C(0xFF2962FF), C(0x400077C2),
                     C(0xFF1A1F2B), C(0xFF5F6B80),
                     C(0xFFE7EBF2), C(0xFFD5DCE8), C(0xFFDDE2EA),
                     C(0xFFE9ECF2), C(0xFFB0009E), C(0xFFD50032) };

        default:
            return colorsForTheme(ThemeId::NeonBlue);
        }
    }
};

}  // namespace jyglobalvst::tray
