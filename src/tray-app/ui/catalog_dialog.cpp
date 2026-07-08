// src/tray-app/ui/catalog_dialog.cpp
//
// Plugin catalog selection dialog.

#include "catalog_dialog.h"
#include "custom_look_and_feel.h"

namespace jyglobalvst::tray {

namespace {

class CatalogListContent : public juce::Component
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

}  // namespace

CatalogDialog::CatalogDialog(const std::vector<PluginCatalogEntry>& catalog, OnAction on_action)
    : juce::DialogWindow("Select Plugin", kBgDeep, true, true)
    , catalog_(catalog)
    , on_action_(on_action)
{
    buildUI();
    centreWithSize(500, 400);
    setVisible(true);
}

CatalogDialog::~CatalogDialog() = default;

void CatalogDialog::buildUI()
{
    auto* content = new CatalogListContent();

    list_box_ = std::make_unique<juce::ListBox>("plugins", this);
    list_box_->setColour(juce::ListBox::backgroundColourId, kBgPanel);
    list_box_->setColour(juce::ListBox::outlineColourId, kBgPanelBorder);
    list_box_->setOutlineThickness(1);
    content->addAndMakeVisible(list_box_.get());

    add_button_ = std::make_unique<juce::TextButton>("Add");
    add_button_->addListener(this);
    add_button_->setEnabled(false);
    content->addAndMakeVisible(add_button_.get());

    browse_button_ = std::make_unique<juce::TextButton>("Browse...");
    browse_button_->addListener(this);
    content->addAndMakeVisible(browse_button_.get());

    auto* cancel_button = new juce::TextButton("Cancel");
    cancel_button->addListener(this);
    cancel_button_ = std::unique_ptr<juce::TextButton>(cancel_button);
    content->addAndMakeVisible(cancel_button_.get());

    setContentOwned(content, true);
}

void CatalogDialog::resized()
{
    DialogWindow::resized();

    if (auto* content = getContentComponent())
    {
        auto b = content->getLocalBounds().reduced(16);

        auto list_area = b.removeFromTop(b.getHeight() - 44);
        b.removeFromTop(12);  // gap

        list_box_->setBounds(list_area);

        // Button row
        int button_y = b.getY();
        int button_h = 32;
        int button_w = 100;
        int cancel_x = b.getRight() - button_w;
        int browse_x = cancel_x - button_w - 8;
        int add_x = browse_x - button_w - 8;

        add_button_->setBounds(add_x, button_y, button_w, button_h);
        browse_button_->setBounds(browse_x, button_y, button_w, button_h);
        cancel_button_->setBounds(cancel_x, button_y, button_w, button_h);
    }
}

void CatalogDialog::buttonClicked(juce::Button* button)
{
    if (button == add_button_.get())
    {
        onAddClicked();
    }
    else if (button == browse_button_.get())
    {
        onBrowseClicked();
    }
    else if (button == cancel_button_.get())
    {
        if (on_action_)
            on_action_(Action::None);
        setVisible(false);
    }
}

int CatalogDialog::getNumRows()
{
    return static_cast<int>(catalog_.size());
}

void CatalogDialog::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                                     bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(catalog_.size()))
        return;

    const auto& entry = catalog_[rowNumber];

    auto bg_color = rowIsSelected ? kAccentBlue.withAlpha(0.3f) : kBgPanel;
    auto text_color = kTextPrimary;

    g.fillAll(bg_color);

    g.setColour(text_color);

    juce::FontOptions font_opts;
    g.setFont(juce::Font(font_opts.withHeight(13)));

    const auto& ref = entry.ref;
    juce::String display_name = juce::String(ref.vendor) + " " + juce::String(ref.name);

    // Add "Built-in" badge for built-in effects
    if (ref.vendor == "JyGlobalVST" && entry.file_path.empty())
        display_name += " [Built-in]";

    juce::String version_str = juce::String(" v") + juce::String(entry.version);
    juce::String full_text = display_name + version_str;

    g.drawText(full_text, 8, 0, width - 16, height, juce::Justification::centredLeft);
}

void CatalogDialog::listBoxItemClicked(int row, const juce::MouseEvent& /*e*/)
{
    if (row >= 0 && row < static_cast<int>(catalog_.size()))
    {
        add_button_->setEnabled(true);
    }
}

const PluginCatalogEntry* CatalogDialog::getSelectedEntry() const
{
    const int selected_row = getSelectedRowIndex();
    if (selected_row >= 0 && selected_row < static_cast<int>(catalog_.size()))
    {
        return &catalog_[selected_row];
    }
    return nullptr;
}

int CatalogDialog::getSelectedRowIndex() const
{
    return list_box_->getSelectedRow();
}

void CatalogDialog::onAddClicked()
{
    if (on_action_)
        on_action_(Action::Selected);
    setVisible(false);
}

void CatalogDialog::onBrowseClicked()
{
    if (on_action_)
        on_action_(Action::Browse);
    setVisible(false);
}

void CatalogDialog::closeButtonPressed()
{
    if (on_action_)
        on_action_(Action::None);
    setVisible(false);
}

}  // namespace jyglobalvst::tray
