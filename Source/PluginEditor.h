#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace Palette
{
    const juce::Colour bg        { 0xff0a0806 };
    const juce::Colour panel     { 0xff14100c };
    const juce::Colour panelEdge { 0xff3d2e1d };
    const juce::Colour bronze    { 0xffc8862e };
    const juce::Colour gold      { 0xfff0c068 };
    const juce::Colour ember     { 0xffff9a3c };
    const juce::Colour deep      { 0xff6b4012 };
    const juce::Colour text      { 0xffefe2cc };
    const juce::Colour dimText   { 0xff8f7f68 };
}

// ------------------------------------------------------------------ look & feel
class AncientLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AncientLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
};

// ------------------------------------------------------------------ controls
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label, bool bipolar = false);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
private:
    juce::String label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label);
    void resized() override;
private:
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// ------------------------------------------------------------------ sample waveform
class WaveView : public juce::Component
{
public:
    void setSample (SamplePtr s, const juce::String& missingPath);
    void setDragHover (bool h) { if (hover != h) { hover = h; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    SamplePtr sample;
    juce::String missing;
    bool hover = false;
};

// ------------------------------------------------------------------ one sample layer (PUNCH / CRUNCH / KICK)
class LayerPanel : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    LayerPanel (AncientProcessor&, int layer, const juce::String& title);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { wave.setDragHover (true); }
    void fileDragExit (const juce::StringArray&) override { wave.setDragHover (false); }
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    AncientProcessor& proc;
    const int layer;
    juce::String title;
    Toggle onToggle;
    std::unique_ptr<Toggle> keyToggle;
    juce::ComboBox source;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment;
    juce::TextButton loadButton { "LOAD" };
    std::unique_ptr<juce::FileChooser> chooser;
    WaveView wave;
    std::vector<std::unique_ptr<Knob>> knobs;
    const SampleData* shown = nullptr;
    juce::String shownMissing;
};

// ------------------------------------------------------------------ last hit display
class HitView : public juce::Component
{
public:
    explicit HitView (AncientProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void refresh();
private:
    AncientProcessor& proc;
    int lastSerial = -1, lastLen = -1;
};

// ------------------------------------------------------------------ canvas (fixed 1280x760, scaled)
class Canvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1280, H = 760;
    explicit Canvas (AncientProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawSunDisc (juce::Graphics&);
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String&);
    void refreshPresetList();
    void showSaveDialog();

    struct Section { juce::String title; juce::Rectangle<int> area; };

    AncientProcessor& proc;
    AncientLookAndFeel lnf;
    juce::Image stone;

    std::array<std::unique_ptr<LayerPanel>, NumSampleLayers> layers;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<juce::Component*> subCells, rumCells, masterCells;
    Toggle* subOn = nullptr;
    Toggle* rumOn = nullptr;
    std::vector<Section> sections;
    HitView hitView;

    juce::ComboBox presetBox;
    juce::TextButton saveButton { "SAVE" }, randomButton { "RANDOMIZE" }, playButton { "PLAY" };
    juce::Array<juce::File> userPresetFiles;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::String lastPresetName;
    static constexpr int userIdBase = 1000;
    float angle = 0.f;

    Knob* knob (const juce::String& id, const juce::String& label, bool bipolar = false);
    Toggle* toggle (const juce::String& id, const juce::String& label);
};

class AncientEditor : public juce::AudioProcessorEditor
{
public:
    explicit AncientEditor (AncientProcessor&);
    void resized() override;
private:
    Canvas canvas;
};
