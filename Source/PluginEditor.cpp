#include "PluginEditor.h"

using namespace juce;

static Font uiFont (float size, bool bold = true, float kerning = 0.f)
{
    Font f (FontOptions (size).withStyle (bold ? "Bold" : "Regular"));
    return kerning > 0.f ? f.withExtraKerningFactor (kerning) : f;
}
static Font serifFont (float size, float kerning = 0.f)
{
    Font f (FontOptions (Font::getDefaultSerifFontName(), size, Font::bold));
    return kerning > 0.f ? f.withExtraKerningFactor (kerning) : f;
}

// ====================================================================== LookAndFeel
AncientLookAndFeel::AncientLookAndFeel()
{
    setColour (PopupMenu::backgroundColourId, Colour (0xff15110c));
    setColour (PopupMenu::textColourId, Palette::text);
    setColour (PopupMenu::headerTextColourId, Palette::bronze);
    setColour (PopupMenu::highlightedBackgroundColourId, Palette::deep);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ComboBox::textColourId, Palette::text);
    setColour (ComboBox::arrowColourId, Palette::bronze);
    setColour (Label::textColourId, Palette::text);
    setColour (TextEditor::backgroundColourId, Colour (0xff1c160f));
    setColour (TextEditor::textColourId, Palette::text);
    setColour (TextEditor::highlightColourId, Palette::deep);
    setColour (TextEditor::outlineColourId, Colours::transparentBlack);
    setColour (TextEditor::focusedOutlineColourId, Palette::bronze);
    setColour (CaretComponent::caretColourId, Palette::gold);
    setColour (AlertWindow::backgroundColourId, Colour (0xff15110c));
    setColour (AlertWindow::textColourId, Palette::text);
    setColour (AlertWindow::outlineColourId, Palette::bronze);
    setColour (TextButton::textColourOffId, Palette::text);
    setColour (TextButton::textColourOnId, Colours::white);
}

void AncientLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, Slider& s)
{
    const bool bipolar = (bool) s.getProperties()["bipolar"];
    const bool enabled = s.isEnabled();
    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.f);
    const float size = jmin (bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre (size, size);
    const auto c = bounds.getCentre();
    const float r = size * 0.5f;
    const float arcR = r - 5.f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float alpha = enabled ? 1.f : 0.35f;

    // engraved scale
    for (int i = 0; i < 9; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 8.f;
        g.setColour ((a <= angle + 0.001f ? Palette::gold : Palette::panelEdge).withAlpha (0.85f * alpha));
        g.drawLine ({ c.getPointOnCircumference (r - 1.f, a), c.getPointOnCircumference (r + 2.f, a) }, 1.4f);
    }

    Path track;
    track.addCentredArc (c.x, c.y, arcR - 3.f, arcR - 3.f, 0.f, startAngle, endAngle, true);
    g.setColour (Colour (0xff241b12));
    g.strokePath (track, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, arcR - 3.f, arcR - 3.f, 0.f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (Palette::ember.withAlpha (0.20f * alpha));
        g.strokePath (arc, PathStrokeType (10.f, PathStrokeType::curved, PathStrokeType::rounded));
        ColourGradient grad (Palette::deep.withAlpha (alpha), c.x - arcR, c.y + arcR, Palette::gold.withAlpha (alpha), c.x + arcR, c.y - arcR, false);
        grad.addColour (0.5, Palette::bronze.withAlpha (alpha));
        g.setGradientFill (grad);
        g.strokePath (arc, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // bronze body
    const float bodyR = arcR - 10.f;
    auto body = Rectangle<float> (bodyR * 2.f, bodyR * 2.f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.75f));
    g.fillEllipse (body.translated (0.f, 2.5f).expanded (1.f));
    ColourGradient bg (Colour (0xff5a3d1e), body.getX(), body.getY(), Colour (0xff120c07), body.getRight(), body.getBottom(), false);
    bg.addColour (0.45, Colour (0xff2c1d0f));
    g.setGradientFill (bg);
    g.fillEllipse (body);
    g.setColour (Palette::bronze.withAlpha (0.55f * alpha));
    g.drawEllipse (body.reduced (0.6f), 1.2f);
    g.setColour (Colour (0xff120c07));
    g.drawEllipse (body.reduced (bodyR * 0.22f), 1.f);

    // notch
    const auto p1 = c.getPointOnCircumference (bodyR * 0.30f, angle);
    const auto p2 = c.getPointOnCircumference (bodyR * 0.90f, angle);
    g.setColour (Palette::gold.withAlpha (enabled ? 1.f : 0.4f));
    g.drawLine ({ p1, p2 }, 2.4f);
}

void AncientLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.f);
    const bool on = b.getToggleState();
    g.setColour (on ? Palette::deep.withAlpha (0.6f) : Colour (0xff17120c));
    g.fillRoundedRectangle (r, 4.f);
    g.setColour (on ? Palette::gold : (highlighted ? Palette::dimText : Palette::panelEdge.brighter (0.2f)));
    g.drawRoundedRectangle (r, 4.f, 1.2f);

    auto led = Rectangle<float> (7.f, 7.f).withCentre ({ r.getX() + 10.f, r.getCentreY() });
    if (on) { g.setColour (Palette::ember.withAlpha (0.4f)); g.fillEllipse (led.expanded (4.f)); }
    g.setColour (on ? Palette::gold : Colour (0xff3a2d20));
    g.fillEllipse (led);
    g.setColour (on ? Palette::text : Palette::dimText);
    g.setFont (uiFont (10.5f, true, 0.06f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (17.f), Justification::centred);
}

void AncientLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
    g.setColour (Colour (0xff17120c));
    g.fillRoundedRectangle (r, 4.f);
    g.setColour (box.isMouseOver (true) ? Palette::bronze : Palette::panelEdge.brighter (0.3f));
    g.drawRoundedRectangle (r, 4.f, 1.2f);
    Path arrow;
    const float ax = (float) w - 14.f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (Palette::bronze);
    g.fillPath (arrow);
}

Font AncientLookAndFeel::getComboBoxFont (ComboBox&) { return uiFont (13.f, true, 0.05f); }
Font AncientLookAndFeel::getPopupMenuFont()          { return uiFont (14.f, false); }
void AncientLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

Label* AncientLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (11.f, false));
    l->setColour (Label::textColourId, Palette::dimText);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setJustificationType (Justification::centred);
    return l;
}

void AncientLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (highlighted) { g.setColour (Palette::ember.withAlpha (0.15f)); g.fillRoundedRectangle (r.expanded (2.f), 5.f); }
    ColourGradient grad (down ? Palette::deep : Colour (0xff221a10), r.getX(), r.getY(),
                         down ? Palette::deep.darker (0.4f) : Colour (0xff100c08), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 4.f);
    g.setColour (highlighted ? Palette::gold : Palette::panelEdge.brighter (0.35f));
    g.drawRoundedRectangle (r, 4.f, 1.2f);
}

void AncientLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool highlighted, bool)
{
    g.setFont (uiFont (11.5f, true, 0.16f));
    g.setColour (highlighted ? Colours::white : Palette::text);
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

Font AncientLookAndFeel::getAlertWindowTitleFont()   { return serifFont (18.f, 0.15f); }
Font AncientLookAndFeel::getAlertWindowMessageFont() { return uiFont (13.f, false); }

// ====================================================================== Knob / Toggle
Knob::Knob (AudioProcessorValueTreeState& apvts, const String& paramId, const String& text, bool bipolar) : label (text)
{
    slider.getProperties().set ("bipolar", bipolar);
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 90, 16);
    slider.setMouseDragSensitivity (220);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxTextColourId, Palette::dimText);
    slider.setColour (Slider::textBoxHighlightColourId, Palette::deep);
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    if (auto* p = apvts.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64));
    }
}

void Knob::paint (Graphics& g)
{
    g.setColour (isEnabled() ? Palette::text : Palette::dimText);
    g.setFont (uiFont (10.5f, true, 0.12f));
    g.drawText (label, getLocalBounds().removeFromTop (16), Justification::centred);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (16);
    slider.setBounds (r);
}

Toggle::Toggle (AudioProcessorValueTreeState& apvts, const String& paramId, const String& text)
{
    button.setButtonText (text);
    addAndMakeVisible (button);
    attachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramId, button);
    if (auto* p = apvts.getParameter (paramId)) button.setTooltip (p->getName (64));
}

void Toggle::resized() { button.setBounds (getLocalBounds().withSizeKeepingCentre (jmin (getWidth() - 2, 96), jmin (getHeight(), 26))); }

// ====================================================================== WaveView
void WaveView::setSample (SamplePtr s, const String& missingPath)
{
    sample = std::move (s);
    missing = missingPath;
    repaint();
}

void WaveView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0c0906));
    g.fillRoundedRectangle (r, 4.f);

    if (sample != nullptr && ! sample->data.empty())
    {
        const auto& d = sample->data;
        const int w = (int) r.getWidth() - 8;
        const float mid = r.getCentreY(), amp = r.getHeight() * 0.42f;
        Path p;
        const size_t per = jmax<size_t> (1, d.size() / (size_t) w);
        for (int x = 0; x < w; ++x)
        {
            float lo = 0.f, hi = 0.f;
            const size_t a = (size_t) x * per, b = jmin (d.size(), a + per);
            for (size_t i = a; i < b; ++i) { lo = jmin (lo, d[i]); hi = jmax (hi, d[i]); }
            const float xx = r.getX() + 4.f + (float) x;
            p.addRectangle (xx, mid - hi * amp, 1.f, jmax (1.f, (hi - lo) * amp));
        }
        ColourGradient grad (Palette::gold, 0.f, r.getY(), Palette::deep, 0.f, r.getBottom(), false);
        grad.addColour (0.5, Palette::bronze);
        g.setGradientFill (grad);
        g.fillPath (p);

        g.setFont (uiFont (10.f, true, 0.08f));
        const auto info = (sample->isUser ? "USER  //  " : "LIBRARY  //  ") + sample->name
                          + "  //  " + String (roundToInt ((double) d.size() / sample->sampleRate * 1000.0)) + " ms";
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRect (r.getX() + 4.f, r.getY() + 3.f, (float) GlyphArrangement::getStringWidthInt (uiFont (10.f, true, 0.08f), info) + 8.f, 14.f);
        g.setColour (sample->isUser ? Palette::gold : Palette::dimText);
        g.drawText (info, r.reduced (8.f, 3.f).removeFromTop (14.f), Justification::centredLeft);
    }
    else
    {
        g.setColour (Palette::dimText);
        g.setFont (uiFont (11.f, true, 0.06f));
        g.drawText (missing.isNotEmpty() ? "MISSING FILE: " + File (missing).getFileName()
                                         : "DROP A SAMPLE HERE OR PRESS LOAD",
                    r.toNearestInt(), Justification::centred);
    }

    g.setColour (hover ? Palette::gold : Palette::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.f, hover ? 2.f : 1.f);
}

// ====================================================================== LayerPanel
LayerPanel::LayerPanel (AncientProcessor& p, int l, const String& t)
    : proc (p), layer (l), title (t), onToggle (p.apvts, ParamIDs::id (l, ParamIDs::on), "ON")
{
    using namespace ParamIDs;
    addAndMakeVisible (onToggle);
    if (layer == Punch)
    {
        keyToggle = std::make_unique<Toggle> (p.apvts, punchKey, "KEY");
        addAndMakeVisible (*keyToggle);
    }

    source.addItemList (Library::choicesFor (layer), 1);
    addAndMakeVisible (source);
    sourceAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, id (layer, src), source);
    source.setTooltip ("Sample z wbudowanej biblioteki albo Twoj wlasny (USER SAMPLE)");

    loadButton.setTooltip ("Wczytaj wlasny sample (wav / aiff / flac / ogg / mp3)");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Load " + title + " sample", File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) proc.loadUserSample (layer, f);
                              });
    };
    addAndMakeVisible (loadButton);
    addAndMakeVisible (wave);

    const std::pair<const char*, const char*> k[] = { { level, "LEVEL" }, { pitch, "PITCH" }, { start, "START" },
                                                       { length, "LENGTH" }, { hp, "LOW CUT" }, { lp, "HIGH CUT" } };
    for (auto& [suffix, name] : k)
    {
        knobs.push_back (std::make_unique<Knob> (p.apvts, id (layer, suffix), name, String (suffix) == pitch));
        addAndMakeVisible (*knobs.back());
    }
}

void LayerPanel::refresh()
{
    auto s = proc.getLayerSample (layer);
    const bool isUserSel = source.getSelectedItemIndex() == Library::userIndex;
    const auto missing = (s == nullptr && isUserSel) ? proc.getUserSamplePath (layer) : String();
    if (s.get() != shown || missing != shownMissing)
    {
        shown = s.get();
        shownMissing = missing;
        wave.setSample (s, missing);
    }
}

bool LayerPanel::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (AncientProcessor::isSupportedAudioFile (f)) return true;
    return false;
}

void LayerPanel::filesDropped (const StringArray& files, int, int)
{
    wave.setDragHover (false);
    for (auto& f : files)
        if (AncientProcessor::isSupportedAudioFile (f)) { proc.loadUserSample (layer, File (f)); break; }
}

void LayerPanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0.f, 3.f), 6.f);
    ColourGradient pg (Colour (0xff1a140e), 0.f, 0.f, Palette::panel, 0.f, r.getBottom(), false);
    g.setGradientFill (pg);
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);

    // title with a small rune
    auto t = getLocalBounds().reduced (12, 8).removeFromTop (22);
    Path rune;
    const float rx = (float) t.getX() + 5.f, ry = (float) t.getCentreY();
    rune.startNewSubPath (rx - 4.f, ry + 5.f); rune.lineTo (rx, ry - 5.f); rune.lineTo (rx + 4.f, ry + 5.f);
    rune.startNewSubPath (rx - 2.5f, ry + 1.f); rune.lineTo (rx + 2.5f, ry + 1.f);
    g.setColour (Palette::gold);
    g.strokePath (rune, PathStrokeType (1.4f));
    g.setFont (serifFont (17.f, 0.25f));
    g.drawText (title, t.withTrimmedLeft (16), Justification::centredLeft);

    ColourGradient ul (Palette::bronze.withAlpha (0.6f), r.getX(), 0.f, Palette::bronze.withAlpha (0.f), r.getRight(), 0.f, false);
    g.setGradientFill (ul);
    g.fillRect (Rectangle<float> (r.getX() + 12.f, (float) t.getBottom() + 4.f, r.getWidth() - 24.f, 1.f));
}

void LayerPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    auto head = r.removeFromTop (22);
    onToggle.setBounds (head.removeFromRight (58));
    if (keyToggle != nullptr) { head.removeFromRight (6); keyToggle->setBounds (head.removeFromRight (62)); }
    r.removeFromTop (10);

    auto srcRow = r.removeFromTop (26);
    loadButton.setBounds (srcRow.removeFromRight (64));
    srcRow.removeFromRight (6);
    source.setBounds (srcRow);
    r.removeFromTop (6);
    wave.setBounds (r.removeFromTop (70));
    r.removeFromTop (6);

    const float cw = (float) r.getWidth() / (float) knobs.size();
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (r.getX() + (int) (cw * (float) i), r.getY(), (int) cw, r.getHeight());
}

// ====================================================================== HitView
void HitView::refresh()
{
    const int s = proc.getHitSerial(), l = proc.getHitLength();
    if (s != lastSerial || l != lastLen) { lastSerial = s; lastLen = l; repaint(); }
}

void HitView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0b0806));
    g.fillRoundedRectangle (r, 5.f);

    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    const int total = (int) (sr * 1.2);
    const int len = jmin (proc.getHitLength(), total);
    const auto& d = proc.getHitBuffer();

    // 100 ms grid
    g.setFont (uiFont (9.f, false));
    for (int ms = 100; ms < 1200; ms += 100)
    {
        const float x = r.getX() + r.getWidth() * (float) ms / 1200.f;
        g.setColour (Palette::panelEdge.withAlpha (ms % 500 == 0 ? 0.9f : 0.45f));
        g.drawVerticalLine ((int) x, r.getY() + 3.f, r.getBottom() - 3.f);
        if (ms % 200 == 0)
        {
            g.setColour (Palette::dimText.withAlpha (0.7f));
            g.drawText (String (ms) + "ms", Rectangle<float> (x + 3.f, r.getBottom() - 14.f, 44.f, 12.f), Justification::left);
        }
    }
    g.setColour (Palette::panelEdge.withAlpha (0.6f));
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 3.f, r.getRight() - 3.f);

    if (len > 0 && ! d.empty())
    {
        const int w = (int) r.getWidth();
        const float mid = r.getCentreY(), amp = r.getHeight() * 0.44f;
        const double per = (double) total / (double) w;
        Path p;
        for (int x = 0; x < w; ++x)
        {
            const int a = (int) (x * per), b = jmin (len, (int) ((x + 1) * per));
            if (a >= len) break;
            float lo = 0.f, hi = 0.f;
            for (int i = a; i < b; ++i) { const float v = d[(size_t) i]; lo = jmin (lo, v); hi = jmax (hi, v); }
            p.addRectangle (r.getX() + (float) x, mid - jlimit (-1.f, 1.f, hi) * amp, 1.f,
                            jmax (1.f, (jlimit (-1.f, 1.f, hi) - jlimit (-1.f, 1.f, lo)) * amp));
        }
        ColourGradient grad (Palette::gold, 0.f, r.getY(), Palette::ember.withAlpha (0.8f), 0.f, r.getCentreY(), false);
        grad.addColour (1.0, Palette::deep);
        g.setGradientFill (grad);
        g.fillPath (p);
    }
    else
    {
        g.setColour (Palette::dimText);
        g.setFont (uiFont (11.f, true, 0.1f));
        g.drawText ("PLAY A NOTE OR PRESS PLAY", r.toNearestInt(), Justification::centred);
    }
    g.setColour (Palette::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.f, 1.f);
}

// ====================================================================== Canvas
Knob* Canvas::knob (const String& id, const String& label, bool bipolar)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, bipolar);
    auto* raw = k.get();
    addAndMakeVisible (*raw);
    owned.push_back (std::move (k));
    return raw;
}

Toggle* Canvas::toggle (const String& id, const String& label)
{
    auto t = std::make_unique<Toggle> (proc.apvts, id, label);
    auto* raw = t.get();
    addAndMakeVisible (*raw);
    owned.push_back (std::move (t));
    return raw;
}

Canvas::Canvas (AncientProcessor& p) : proc (p), hitView (p)
{
    setLookAndFeel (&lnf);
    using namespace ParamIDs;

    // stone texture (generated once)
    stone = Image (Image::ARGB, W, H, true);
    {
        Graphics sg (stone);
        Random r (7);
        for (int i = 0; i < 26000; ++i)
        {
            const float x = r.nextFloat() * W, y = r.nextFloat() * H, s = 0.6f + r.nextFloat() * 1.8f;
            sg.setColour ((r.nextBool() ? Colours::white : Colours::black).withAlpha (0.015f + 0.035f * r.nextFloat()));
            sg.fillEllipse (x, y, s, s);
        }
        for (int i = 0; i < 40; ++i)   // faint cracks
        {
            Path crack;
            float x = r.nextFloat() * W, y = r.nextFloat() * H;
            crack.startNewSubPath (x, y);
            for (int k = 0; k < 6; ++k) { x += (r.nextFloat() - 0.5f) * 60.f; y += (r.nextFloat() - 0.3f) * 30.f; crack.lineTo (x, y); }
            sg.setColour (Colours::black.withAlpha (0.12f));
            sg.strokePath (crack, PathStrokeType (0.8f));
        }
    }

    const char* titles[] = { "PUNCH", "CRUNCH", "KICK" };
    for (int l = 0; l < NumSampleLayers; ++l)
    {
        layers[(size_t) l] = std::make_unique<LayerPanel> (proc, l, titles[l]);
        addAndMakeVisible (*layers[(size_t) l]);
    }

    subOn = toggle (ParamIDs::subOn, "ON");
    subCells = { knob (subWave, "WAVE"), knob (subOct, "OCTAVE", true), knob (subLevel, "LEVEL"), knob (subAttack, "ATTACK"),
                 knob (subLength, "LENGTH"), knob (subTone, "TONE"), knob (subDrive, "DRIVE") };
    rumOn = toggle (ParamIDs::rumOn, "ON");
    rumCells = { knob (rumLevel, "LEVEL"), knob (rumDelay, "DELAY"), knob (rumDecay, "DECAY"), knob (rumDrive, "DRIVE"),
                 knob (rumLp, "LOW PASS"), knob (rumDuck, "DUCK") };
    masterCells = { knob (root, "ROOT"), knob (preGain, "PRE-GAIN"), toggle (clipOn, "CLIP"), knob (clipCeil, "CEILING"),
                    knob (clipKnee, "SOFTNESS"), toggle (limOn, "LIMIT"), knob (limCeil, "CEILING"), knob (limRel, "RELEASE"),
                    knob (volume, "VOLUME") };
    addAndMakeVisible (hitView);

    refreshPresetList();
    presetBox.onChange = [this]
    {
        const int pid = presetBox.getSelectedId();
        if (pid <= 0) return;
        if (pid < userIdBase) proc.setCurrentProgram (pid - 1);
        else if (isPositiveAndBelow (pid - userIdBase, userPresetFiles.size())) proc.loadUserPreset (userPresetFiles[pid - userIdBase]);
        lastPresetName = {};
    };
    saveButton.onClick = [this] { showSaveDialog(); };
    randomButton.onClick = [this] { proc.randomize(); };
    playButton.onClick = [this] { proc.triggerTest(); };
    saveButton.setTooltip ("Zapisz preset (razem ze sciezkami Twoich sampli)");
    randomButton.setTooltip ("Losowy kick w rozsadnych zakresach");
    playButton.setTooltip ("Odegraj kicka na nucie ROOT");
    for (auto* c : { (Component*) &presetBox, (Component*) &saveButton, (Component*) &randomButton, (Component*) &playButton })
        addAndMakeVisible (c);

    setSize (W, H);
    timerCallback();
    startTimerHz (30);
}

Canvas::~Canvas()
{
    if (saveDialog != nullptr) saveDialog->exitModalState (0);
    saveDialog.reset();
    setLookAndFeel (nullptr);
}

void Canvas::refreshPresetList()
{
    presetBox.clear (dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    userPresetFiles = proc.getUserPresets();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), userIdBase + i);
    }
    lastPresetName = {};
}

void Canvas::showSaveDialog()
{
    if (saveDialog != nullptr) return;
    saveDialog = std::make_unique<AlertWindow> ("SAVE PRESET", "Nazwa presetu (Dokumenty/Ancient Drums/Presets):",
                                                MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&lnf);
    saveDialog->addTextEditor ("name", proc.getPresetName().startsWith ("Random") ? String ("My Kick") : proc.getPresetName());
    saveDialog->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
    saveDialog->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));

    SafePointer<Canvas> safe (this);
    saveDialog->enterModalState (true, ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->saveDialog == nullptr) return;
        const auto name = safe->saveDialog->getTextEditorContents ("name");
        MessageManager::callAsync ([safe] { if (safe != nullptr) safe->saveDialog.reset(); });
        if (result == 1)
        {
            if (safe->proc.saveUserPreset (name)) safe->refreshPresetList();
            else AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "SAVE PRESET", "Nie udalo sie zapisac presetu.");
        }
    }), false);
}

void Canvas::timerCallback()
{
    angle += 0.0025f;
    repaint (0, 0, W, 112);

    for (auto& l : layers) l->refresh();
    hitView.refresh();

    const auto name = proc.getPresetName();
    if (name != lastPresetName)
    {
        lastPresetName = name;
        int found = 0;
        for (int i = 0; i < presetBox.getNumItems(); ++i)
            if (presetBox.getItemText (i) == name) { found = presetBox.getItemId (i); break; }
        if (found != 0) presetBox.setSelectedId (found, dontSendNotification);
        else presetBox.setText (name, dontSendNotification);
    }
}

void Canvas::resized()
{
    const int m = 16, gap = 14;
    sections.clear();

    // row 1: sample layers
    const int lw = (W - 2 * m - 2 * gap) / 3;
    for (int l = 0; l < NumSampleLayers; ++l)
        layers[(size_t) l]->setBounds (m + l * (lw + gap), 122, lw, 300);

    auto placeCells = [] (Rectangle<int> area, const std::vector<Component*>& cells)
    {
        const float cw = (float) area.getWidth() / (float) cells.size();
        for (size_t i = 0; i < cells.size(); ++i)
        {
            auto cell = Rectangle<int> (area.getX() + (int) (cw * (float) i), area.getY(), (int) cw, area.getHeight());
            cells[i]->setBounds (dynamic_cast<Toggle*> (cells[i]) != nullptr ? cell.withSizeKeepingCentre (cell.getWidth(), 30)
                                                                             : cell.withSizeKeepingCentre (jmin (cell.getWidth(), 90), cell.getHeight()));
        }
    };
    auto inner = [] (Rectangle<int> a) { a = a.reduced (12, 8); a.removeFromTop (30); return a; };

    // row 2: SUB + RUMBLE
    const int avail = W - 2 * m - gap;
    const int subW = avail * 7 / 13;
    Rectangle<int> subA (m, 434, subW, 150), rumA (m + subW + gap, 434, avail - subW, 150);
    sections.push_back ({ "SUB BASS", subA });
    sections.push_back ({ "RUMBLE", rumA });
    subOn->setBounds (subA.getRight() - 12 - 58, subA.getY() + 8, 58, 22);
    rumOn->setBounds (rumA.getRight() - 12 - 58, rumA.getY() + 8, 58, 22);
    placeCells (inner (subA), subCells);
    placeCells (inner (rumA), rumCells);

    // row 3: MASTER + last hit
    const int masterW = 760;
    Rectangle<int> masA (m, 596, masterW, 150), hitA (m + masterW + gap, 596, W - 2 * m - masterW - gap, 150);
    sections.push_back ({ "MASTER", masA });
    sections.push_back ({ "LAST HIT", hitA });
    placeCells (inner (masA), masterCells);
    hitView.setBounds (inner (hitA).reduced (0, 0).withTrimmedBottom (2));

    // header
    const int px = W - 16 - 240;
    presetBox.setBounds (px, 32, 240, 30);
    randomButton.setBounds (px, 70, 120, 28);
    saveButton.setBounds (px + 126, 70, 54, 28);
    playButton.setBounds (px + 186, 70, 54, 28);
}

void Canvas::drawPanel (Graphics& g, Rectangle<int> area, const String& title)
{
    auto r = area.toFloat();
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0.f, 3.f), 6.f);
    ColourGradient pg (Colour (0xff1a140e), 0.f, r.getY(), Palette::panel, 0.f, r.getBottom(), false);
    g.setGradientFill (pg);
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);

    auto t = area.reduced (12, 8).removeFromTop (22);
    Path rune;
    const float rx = (float) t.getX() + 5.f, ry = (float) t.getCentreY();
    rune.startNewSubPath (rx, ry - 5.f); rune.lineTo (rx, ry + 5.f);
    rune.startNewSubPath (rx, ry - 2.f); rune.lineTo (rx + 4.f, ry - 5.f);
    rune.startNewSubPath (rx, ry + 1.f); rune.lineTo (rx + 4.f, ry - 2.f);
    g.setColour (Palette::gold);
    g.strokePath (rune, PathStrokeType (1.4f));
    g.setFont (serifFont (17.f, 0.25f));
    g.drawText (title, t.withTrimmedLeft (16), Justification::centredLeft);
    ColourGradient ul (Palette::bronze.withAlpha (0.6f), r.getX(), 0.f, Palette::bronze.withAlpha (0.f), r.getRight(), 0.f, false);
    g.setGradientFill (ul);
    g.fillRect (Rectangle<float> (r.getX() + 12.f, (float) t.getBottom() + 4.f, r.getWidth() - 24.f, 1.f));
}

void Canvas::drawSunDisc (Graphics& g)
{
    const Point<float> c (W * 0.5f, 56.f);
    // rays
    for (int i = 0; i < 32; ++i)
    {
        const float a = (float) i * MathConstants<float>::twoPi / 32.f + angle * 0.5f;
        const auto end = c.getPointOnCircumference (i % 2 == 0 ? 640.f : 420.f, a);
        ColourGradient lg (Palette::gold.withAlpha (0.10f), c.x, c.y, Palette::gold.withAlpha (0.f), end.x, end.y, false);
        g.setGradientFill (lg);
        g.drawLine ({ c.getPointOnCircumference (150.f, a), end }, i % 2 == 0 ? 1.4f : 0.8f);
    }
    // rings
    const float radii[] = { 48.f, 74.f, 108.f, 150.f };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (Palette::bronze.withAlpha (0.24f - 0.04f * (float) i));
        g.drawEllipse (Rectangle<float> (radii[i] * 2.f, radii[i] * 2.f).withCentre (c), i == 2 ? 2.f : 1.f);
    }
    // rotating glyph band
    for (int i = 0; i < 20; ++i)
    {
        const float a = angle + (float) i * MathConstants<float>::twoPi / 20.f;
        const auto pc = c.getPointOnCircumference (91.f, a);
        Path glyph;
        switch (i % 4)
        {
            case 0: glyph.addTriangle (-4.f, 4.f, 4.f, 4.f, 0.f, -5.f); break;
            case 1: glyph.addEllipse (-3.5f, -3.5f, 7.f, 7.f); break;
            case 2: glyph.startNewSubPath (-4.f, -4.f); glyph.lineTo (4.f, 4.f); glyph.startNewSubPath (4.f, -4.f); glyph.lineTo (-4.f, 4.f); break;
            default: glyph.startNewSubPath (0.f, -5.f); glyph.lineTo (0.f, 5.f); glyph.startNewSubPath (-4.f, 0.f); glyph.lineTo (4.f, 0.f); break;
        }
        glyph.applyTransform (AffineTransform::rotation (a + MathConstants<float>::halfPi).translated (pc));
        g.setColour (Palette::gold.withAlpha (0.30f));
        g.strokePath (glyph, PathStrokeType (1.1f));
    }
    // counter-rotating ticks
    for (int i = 0; i < 60; ++i)
    {
        const float a = -angle * 1.3f + (float) i * MathConstants<float>::twoPi / 60.f;
        g.setColour (Palette::bronze.withAlpha (i % 5 == 0 ? 0.35f : 0.15f));
        g.drawLine ({ c.getPointOnCircumference (150.f, a), c.getPointOnCircumference (i % 5 == 0 ? 138.f : 144.f, a) }, 1.f);
    }
    // pyramid
    Path pyr;
    pyr.addTriangle (c.x - 36.f, c.y + 26.f, c.x + 36.f, c.y + 26.f, c.x, c.y - 34.f);
    g.setColour (Palette::gold.withAlpha (0.16f));
    g.strokePath (pyr, PathStrokeType (1.2f));
    g.drawLine (c.x - 24.f, c.y + 10.f, c.x + 24.f, c.y + 10.f, 0.8f);
}

void Canvas::paint (Graphics& g)
{
    g.fillAll (Palette::bg);
    ColourGradient glow (Colour (0xff4a2a08).withAlpha (0.8f), W * 0.5f, 30.f, Colours::transparentBlack, W * 0.5f, 560.f, true);
    g.setGradientFill (glow);
    g.fillRect (0, 0, W, H);
    ColourGradient floor (Colours::transparentBlack, 0.f, H * 0.55f, Colour (0xff1c1206), 0.f, (float) H, false);
    g.setGradientFill (floor);
    g.fillRect (0, H / 2, W, H / 2);
    g.drawImageAt (stone, 0, 0);

    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (0, 0, W, 112);
        drawSunDisc (g);
    }

    // title
    const auto titleArea = Rectangle<int> (0, 12, W, 54);
    g.setFont (serifFont (46.f, 0.28f));
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawText ("ANCIENT DRUMS", titleArea.translated (0, 3), Justification::centred);
    g.setColour (Palette::ember.withAlpha (0.15f));
    for (int i = 1; i <= 3; ++i)
    {
        g.drawText ("ANCIENT DRUMS", titleArea.translated (i, 0), Justification::centred);
        g.drawText ("ANCIENT DRUMS", titleArea.translated (-i, 0), Justification::centred);
    }
    ColourGradient tg (Colour (0xfffff0c8), 0.f, (float) titleArea.getY() + 8.f, Palette::bronze, 0.f, (float) titleArea.getBottom() - 6.f, false);
    tg.addColour (0.55, Palette::gold);
    g.setGradientFill (tg);
    g.drawText ("ANCIENT DRUMS", titleArea, Justification::centred);

    g.setFont (uiFont (11.5f, true, 0.5f));
    g.setColour (Palette::bronze);
    g.drawText ("HARDSTYLE  KICK  GENERATOR", Rectangle<int> (0, 70, W, 18), Justification::centred);

    g.setFont (uiFont (10.f, true, 0.3f));
    g.setColour (Palette::dimText);
    g.drawText ("PRESET", Rectangle<int> (W - 16 - 240, 16, 240, 14), Justification::left);
    g.drawText ("v1.0  //  PUNCH  CRUNCH  KICK", Rectangle<int> (16, 34, 320, 14), Justification::left);
    g.drawText ("SUB  //  RUMBLE  //  CLIP  //  LIMIT", Rectangle<int> (16, 50, 320, 14), Justification::left);
    g.setColour (Palette::gold.withAlpha (0.85f));
    g.drawText ("FORGED IN THE OLD WAYS", Rectangle<int> (16, 66, 320, 14), Justification::left);

    ColourGradient rule (Palette::gold.withAlpha (0.f), 0.f, 0.f, Palette::gold.withAlpha (0.f), (float) W, 0.f, false);
    rule.addColour (0.5, Palette::gold.withAlpha (0.85f));
    g.setGradientFill (rule);
    g.fillRect (0, 108, W, 2);

    for (auto& s : sections) drawPanel (g, s.area, s.title);
}

// ====================================================================== Editor
AncientEditor::AncientEditor (AncientProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (768, 456, 1920, 1140);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (1088, 646);
}

void AncientEditor::resized()
{
    canvas.setBounds (0, 0, Canvas::W, Canvas::H);
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) Canvas::W));
}
