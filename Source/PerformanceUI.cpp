#include "PerformanceUI.h"

//==============================================================================
PerformanceUI::PerformanceUI() {
    // Initialize the main container and viewports
    addAndMakeVisible(buttonRowViewport);
    addAndMakeVisible(sliderRegionViewport);

    buttonRowViewport.setViewedComponent(&buttonRowContainer, false);
    sliderRegionViewport.setViewedComponent(&sliderRegionContainer, false);

    // Create 12 buttons for the button row
    for (int i = 0; i < 12; ++i) {
        auto *button = buttons.add(new juce::TextButton("Btn " + juce::String(i + 1)));
        button->addListener(this);
        buttonRowContainer.addAndMakeVisible(*button);
        button->setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        button->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }

    // Create 24 sliders for the slider region, styled like aviation trim sliders
    for (int i = 0; i < 24; ++i) {
        auto *slider = sliders.add(new juce::Slider(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow));
        slider->setRange(0.0, 100.0, 0.1);
        slider->setValue(50.0);
        slider->addListener(this);
        sliderRegionContainer.addAndMakeVisible(*slider);

        // Style sliders to resemble aviation trim sliders
        slider->setColour(juce::Slider::thumbColourId, juce::Colours::silver);
        slider->setColour(juce::Slider::trackColourId, juce::Colours::darkgrey);
        slider->setColour(juce::Slider::backgroundColourId, juce::Colours::grey);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        slider->setTextValueSuffix("%");
        slider->setName("Slider " + juce::String(i + 1));
    }

    // Enable mouse dragging for scrolling
    setWantsKeyboardFocus(true);
}

PerformanceUI::~PerformanceUI() {}

//==============================================================================
void PerformanceUI::paint(juce::Graphics &g) {
    // Paint the background
    g.fillAll(juce::Colours::black);

    // Optional: Draw grid lines for debugging
    /*
    auto unit = calculateUnitSize();
    g.setColour(juce::Colours::grey.withAlpha(0.2f));
    for (int i = 1; i < 8; ++i)
        g.drawHorizontalLine(i * unit.getHeight(), 0, getWidth());
    for (int i = 1; i < 12; ++i)
        g.drawVerticalLine(i * unit.getWidth(), 0, getHeight());
    */
}

void PerformanceUI::resized() {
    // Calculate the unit size based on device dimensions
    auto unit = calculateUnitSize();

    // Set up the main FlexBox (vertical layout: 1/8 for button row, 7/8 for slider region)
    mainFlexBox.flexDirection = juce::FlexBox::Direction::column;
    mainFlexBox.items.clear();
    mainFlexBox.items.add(juce::FlexItem(buttonRowViewport)
                              .withFlex(1.0f) // 1/8 of height
                              .withMinHeight(unit.getHeight()));
    mainFlexBox.items.add(juce::FlexItem(sliderRegionViewport)
                              .withFlex(7.0f) // 7/8 of height
                              .withMinHeight(7.0f * unit.getHeight()));

    // Perform the main layout
    mainFlexBox.performLayout(getLocalBounds().toFloat());

    // Set up button row and slider region layouts
    setupButtonRow();
    setupSliderRegion();
}

//==============================================================================
juce::Rectangle<float> PerformanceUI::calculateUnitSize() const {
    // Divide vertical dimension by 8 and horizontal by 12 to define the grid unit
    float unitHeight = getHeight() / 8.0f;
    float unitWidth = getWidth() / 12.0f;
    return juce::Rectangle<float>(0, 0, unitWidth, unitHeight);
}

void PerformanceUI::setupButtonRow() {
    auto unit = calculateUnitSize();
    float buttonWidth = unit.getWidth() * 2.0f;   // Each button spans 2 grid units horizontally
    float buttonHeight = unit.getHeight() * 0.8f; // Slightly less than unit height for padding

    // Configure button row FlexBox
    buttonRowFlexBox.flexDirection = juce::FlexBox::Direction::row;
    buttonRowFlexBox.items.clear();

    for (auto *button : buttons) {
        buttonRowFlexBox.items.add(
            juce::FlexItem(*button)
                .withWidth(buttonWidth)
                .withHeight(buttonHeight)
                .withMargin(juce::FlexItem::Margin(0, unit.getWidth() * 0.1f, 0, unit.getWidth() * 0.1f)));
    }

    // Calculate total width of buttons
    float totalButtonWidth = buttons.size() * buttonWidth + (buttons.size() - 1) * unit.getWidth() * 0.2f;
    buttonRowContainer.setSize(totalButtonWidth, buttonRowViewport.getHeight());

    // Perform button row layout
    buttonRowFlexBox.performLayout(buttonRowContainer.getLocalBounds().toFloat());

    // Ensure viewport scroll is within bounds
    buttonRowViewport.setViewPosition(0, 0);
}

void PerformanceUI::setupSliderRegion() {
    auto unit = calculateUnitSize();
    float sliderWidth = unit.getWidth() * 0.8f;                   // Each slider is slightly narrower than a grid unit
    float sliderHeight = sliderRegionViewport.getHeight() * 0.9f; // Use most of the viewport height

    // Configure slider region FlexBox
    sliderRegionFlexBox.flexDirection = juce::FlexBox::Direction::row;
    sliderRegionFlexBox.items.clear();

    for (auto *slider : sliders) {
        sliderRegionFlexBox.items.add(
            juce::FlexItem(*slider)
                .withWidth(sliderWidth)
                .withHeight(sliderHeight)
                .withMargin(juce::FlexItem::Margin(0, unit.getWidth() * 0.1f, 0, unit.getWidth() * 0.1f)));
    }

    // Calculate total width of sliders
    float totalSliderWidth = sliders.size() * sliderWidth + (sliders.size() - 1) * unit.getWidth() * 0.2f;
    sliderRegionContainer.setSize(totalSliderWidth, sliderRegionViewport.getHeight());

    // Perform slider region layout
    sliderRegionFlexBox.performLayout(sliderRegionContainer.getLocalBounds().toFloat());

    // Ensure viewport scroll is within bounds
    sliderRegionViewport.setViewPosition(0, 0);
}

//==============================================================================
void PerformanceUI::buttonClicked(juce::Button *button) {
    // Handle button clicks (for testing purposes, toggle button state)
    button->setToggleState(!button->getToggleState(), juce::dontSendNotification);
    DBG("Button clicked: " << button->getName());
}

void PerformanceUI::sliderValueChanged(juce::Slider *slider) {
    // Handle slider value changes (for testing purposes, log the value)
    DBG("Slider " << slider->getName() << " value: " << slider->getValue());
}

//==============================================================================
void PerformanceUI::mouseDown(const juce::MouseEvent &event) {
    // Store the drag start position for scrolling
    if (buttonRowViewport.getBounds().contains(event.getPosition()))
        buttonRowDragStart = event.getPosition().toFloat();
    else if (sliderRegionViewport.getBounds().contains(event.getPosition()))
        sliderRegionDragStart = event.getPosition().toFloat();
}

void PerformanceUI::mouseDrag(const juce::MouseEvent &event) {
    // Handle scrolling for button row
    if (buttonRowViewport.getBounds().contains(event.getPosition())) {
        auto delta = event.getPosition().toFloat() - buttonRowDragStart;
        auto currentPos = buttonRowViewport.getViewPosition();
        int newX = currentPos.x - delta.x;

        // Clamp the scroll position
        newX = juce::jlimit(0, juce::jmax(0, buttonRowContainer.getWidth() - buttonRowViewport.getWidth()), newX);
        buttonRowViewport.setViewPosition(newX, 0);

        buttonRowDragStart = event.getPosition().toFloat();
    }

    // Handle scrolling for slider region
    if (sliderRegionViewport.getBounds().contains(event.getPosition())) {
        auto delta = event.getPosition().toFloat() - sliderRegionDragStart;
        auto currentPos = sliderRegionViewport.getViewPosition();
        int newX = currentPos.x - delta.x;

        // Clamp the scroll position
        newX = juce::jlimit(0, juce::jmax(0, sliderRegionContainer.getWidth() - sliderRegionViewport.getWidth()), newX);
        sliderRegionViewport.setViewPosition(newX, 0);

        sliderRegionDragStart = event.getPosition().toFloat();
    }
}
