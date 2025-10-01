#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * PerformanceUI is a JUCE Component that implements a responsive UI with a top button row
 * and a lower slider region, using FlexBox for layout. Designed for horizontal orientation
 * on iPad/iPhone, it divides the vertical space by 8 and horizontal by 12 to define a grid.
 * The top 1/8th contains scrollable buttons, and the bottom 7/8th contains scrollable sliders.
 */
class PerformanceUI : public juce::Component, public juce::Button::Listener, public juce::Slider::Listener {
    public:
        //==============================================================================
        PerformanceUI();
        ~PerformanceUI() override;

        //==============================================================================
        void paint(juce::Graphics &g) override;
        void resized() override;

        //==============================================================================
        void buttonClicked(juce::Button *button) override;
        void sliderValueChanged(juce::Slider *slider) override;

    private:
        //==============================================================================
        // Calculate the unit size based on device dimensions (height/8, width/12)
        juce::Rectangle<float> calculateUnitSize() const;

        // Set up the button row with FlexBox
        void setupButtonRow();

        // Set up the slider region with FlexBox
        void setupSliderRegion();

        // Handle scrolling for button row and slider region
        void mouseDrag(const juce::MouseEvent &event) override;
        void mouseDown(const juce::MouseEvent &event) override;

        //==============================================================================
        // Main containers for the button row and slider region
        juce::Component buttonRowContainer;
        juce::Component sliderRegionContainer;

        // Viewports for scrolling
        juce::Viewport buttonRowViewport;
        juce::Viewport sliderRegionViewport;

        // Buttons in the top row (scrollable)
        juce::OwnedArray<juce::TextButton> buttons;

        // Sliders in the lower region (scrollable)
        juce::OwnedArray<juce::Slider> sliders;

        // Store drag start positions for scrolling
        juce::Point<float> buttonRowDragStart;
        juce::Point<float> sliderRegionDragStart;

        // FlexBox layouts
        juce::FlexBox mainFlexBox;         // Vertical layout for button row and slider region
        juce::FlexBox buttonRowFlexBox;    // Horizontal layout for buttons
        juce::FlexBox sliderRegionFlexBox; // Horizontal layout for sliders

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceUI)
};
