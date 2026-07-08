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

        if (button.getToggleState())
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

        if (button.getToggleState())
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
        else
        {
            // Render regular text for all other buttons (EQ, NT, etc.)
            g.setColour(textColour);
            g.setFont(14.0f);
            g.drawFittedText(text, bounds.toNearestInt(),
                           juce::Justification::centred, 1);
        }
    }

public:

private:
    void drawTrashIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                       juce::Colour colour) const
    {
        auto center = bounds.getCentre();
        auto size = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

        g.setColour(colour);

        // Trash can body
        auto body = juce::Rectangle<float>(center.x - size * 0.4f, center.y - size * 0.3f,
                                           size * 0.8f, size * 0.5f);
        g.drawRoundedRectangle(body, 1.0f, 1.5f);

        // Trash can lid
        g.drawLine(center.x - size * 0.5f, center.y - size * 0.35f,
                   center.x + size * 0.5f, center.y - size * 0.35f, 1.5f);

        // Handle
        g.drawLine(center.x - size * 0.2f, center.y - size * 0.4f,
                   center.x - size * 0.2f, center.y - size * 0.5f, 1.0f);
        g.drawLine(center.x + size * 0.2f, center.y - size * 0.4f,
                   center.x + size * 0.2f, center.y - size * 0.5f, 1.0f);
        g.drawLine(center.x - size * 0.2f, center.y - size * 0.5f,
                   center.x + size * 0.2f, center.y - size * 0.5f, 1.0f);
    }

    void drawPencilIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                        juce::Colour colour) const
    {
        auto center = bounds.getCentre();
        auto size = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;

        g.setColour(colour);

        // Pencil shaft
        g.drawLine(center.x - size * 0.2f, center.y + size * 0.5f,
                   center.x - size * 0.2f, center.y - size * 0.5f, 2.0f);
        g.drawLine(center.x + size * 0.2f, center.y + size * 0.5f,
                   center.x + size * 0.2f, center.y - size * 0.5f, 2.0f);

        // Pencil tip (triangle)
        juce::Path tip;
        tip.addTriangle(center.x - size * 0.3f, center.y + size * 0.4f,
                       center.x + size * 0.3f, center.y + size * 0.4f,
                       center.x, center.y + size * 0.6f);
        g.fillPath(tip);
    }

    void drawOnOffIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isOn,
                       juce::Colour colour) const
    {
        auto center = bounds.getCentre();
        auto size = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.35f;

        g.setColour(colour);

        // Draw circle
        g.drawEllipse(center.x - size, center.y - size, size * 2.0f, size * 2.0f, 1.5f);

        // Draw indicator
        if (isOn)
        {
            g.fillEllipse(center.x - size * 0.3f, center.y - size * 0.8f,
                         size * 0.6f, size * 0.6f);
        }
        else
        {
            g.fillEllipse(center.x - size * 0.3f, center.y + size * 0.2f,
                         size * 0.6f, size * 0.6f);
        }
    }

    void drawSpeakerIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isOn,
                         juce::Colour colour) const
    {
        auto center = bounds.getCentre();
        const float size = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;
        const float iconLeft = center.x - size * 0.9f;

        g.setColour(colour);

        // Speaker body + cone.
        const float bodyW = size * 0.35f;
        const float bodyH = size * 0.55f;
        juce::Rectangle<float> body(iconLeft, center.y - bodyH * 0.5f, bodyW, bodyH);
        g.fillRect(body);

        juce::Path cone;
        cone.startNewSubPath(body.getRight(), body.getY());
        cone.lineTo(body.getRight() + size * 0.3f, center.y - size * 0.7f);
        cone.lineTo(body.getRight() + size * 0.3f, center.y + size * 0.7f);
        cone.lineTo(body.getRight(), body.getBottom());
        cone.closeSubPath();
        g.fillPath(cone);

        const float markX = body.getRight() + size * 0.45f;
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
                g.strokePath(arc, juce::PathStrokeType(1.5f));
            }
        }
        else
        {
            // Mute X mark.
            g.drawLine(markX, center.y - size * 0.35f, markX + size * 0.5f, center.y + size * 0.35f, 1.5f);
            g.drawLine(markX, center.y + size * 0.35f, markX + size * 0.5f, center.y - size * 0.35f, 1.5f);
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

        default:
            return colorsForTheme(ThemeId::NeonBlue);
        }
    }
};

}  // namespace jyglobalvst::tray
