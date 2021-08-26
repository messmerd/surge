/*
 ** Surge Synthesizer is Free and Open Source Software
 **
 ** Surge is made available under the Gnu General Public License, v3.0
 ** https://www.gnu.org/licenses/gpl-3.0.en.html
 **
 ** Copyright 2004-2021 by various individuals as described by the Git transaction log
 **
 ** All source at: https://github.com/surge-synthesizer/surge.git
 **
 ** Surge was a commercial product from 2004-2018, with Copyright and ownership
 ** in that period held by Claes Johanson at Vember Audio. Claes made Surge
 ** open source in September 2018.
 */

#include "LuaEditors.h"
#include "SurgeGUIEditor.h"
#include "RuntimeFont.h"
#include "SkinSupport.h"
#include "SkinColors.h"
#include "WavetableScriptEvaluator.h"

namespace Surge
{
namespace Overlays
{
struct EditorColors
{
    static void setColorsFromSkin(juce::CodeEditorComponent *comp,
                                  const Surge::GUI::Skin::ptr_t &skin)
    {
        auto cs = comp->getColourScheme();

        cs.set("Bracket", skin->getColor(Colors::FormulaEditor::Lua::Bracket));
        cs.set("Comment", skin->getColor(Colors::FormulaEditor::Lua::Comment));
        cs.set("Error", skin->getColor(Colors::FormulaEditor::Lua::Error));
        cs.set("Float", skin->getColor(Colors::FormulaEditor::Lua::Number));
        cs.set("Integer", skin->getColor(Colors::FormulaEditor::Lua::Number));
        cs.set("Identifier", skin->getColor(Colors::FormulaEditor::Lua::Identifier));
        cs.set("Keyword", skin->getColor(Colors::FormulaEditor::Lua::Keyword));
        cs.set("Operator", skin->getColor(Colors::FormulaEditor::Lua::Interpunction));
        cs.set("Punctuation", skin->getColor(Colors::FormulaEditor::Lua::Interpunction));
        cs.set("String", skin->getColor(Colors::FormulaEditor::Lua::String));

        comp->setColourScheme(cs);

        comp->setColour(juce::CodeEditorComponent::backgroundColourId,
                        skin->getColor(Colors::FormulaEditor::Background));
        comp->setColour(juce::CodeEditorComponent::highlightColourId,
                        skin->getColor(Colors::FormulaEditor::Highlight));
        comp->setColour(juce::CodeEditorComponent::defaultTextColourId,
                        skin->getColor(Colors::FormulaEditor::Text));
        comp->setColour(juce::CodeEditorComponent::lineNumberBackgroundId,
                        skin->getColor(Colors::FormulaEditor::LineNumBackground));
        comp->setColour(juce::CodeEditorComponent::lineNumberTextId,
                        skin->getColor(Colors::FormulaEditor::LineNumText));

        comp->retokenise(0, -1);
    }
};

CodeEditorContainerWithApply::CodeEditorContainerWithApply(SurgeGUIEditor *ed, SurgeStorage *s,
                                                           Surge::GUI::Skin::ptr_t skin,
                                                           bool addComponents)
    : editor(ed), storage(s)
{
    applyButton = std::make_unique<juce::TextButton>("Apply");
    applyButton->setButtonText("Apply");
    applyButton->addListener(this);

    mainDocument = std::make_unique<juce::CodeDocument>();
    mainDocument->addListener(this);
    tokenizer = std::make_unique<juce::LuaTokeniser>();

    mainEditor = std::make_unique<juce::CodeEditorComponent>(*mainDocument, tokenizer.get());
    mainEditor->setFont(Surge::GUI::getFontManager()->getFiraMonoAtSize(10));
    mainEditor->setTabSize(4, true);
    mainEditor->addKeyListener(this);
    EditorColors::setColorsFromSkin(mainEditor.get(), skin);
    if (addComponents)
    {
        addAndMakeVisible(applyButton.get());
        addAndMakeVisible(mainEditor.get());
    }
    applyButton->setEnabled(false);
}

void CodeEditorContainerWithApply::buttonClicked(juce::Button *button)
{
    if (button == applyButton.get())
    {
        applyCode();
    }
}

void CodeEditorContainerWithApply::codeDocumentTextInserted(const juce::String &newText,
                                                            int insertIndex)
{
    applyButton->setEnabled(true);
}
void CodeEditorContainerWithApply::codeDocumentTextDeleted(int startIndex, int endIndex)
{
    applyButton->setEnabled(true);
}
bool CodeEditorContainerWithApply::keyPressed(const juce::KeyPress &key, juce::Component *o)
{
    if (key.getKeyCode() == juce::KeyPress::returnKey &&
        (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()))
    {
        applyCode();
        return true;
    }
    else
    {
        return Component::keyPressed(key);
    }
}

void CodeEditorContainerWithApply::paint(juce::Graphics &g) { g.fillAll(juce::Colours::black); }

struct ExpandingFormulaDebugger : public juce::Component
{
    bool isOpen{false};
    ExpandingFormulaDebugger(FormulaModulatorEditor *ed) : editor(ed) {}
    FormulaModulatorEditor *editor{nullptr};
    void paint(juce::Graphics &g) override
    {
        if (isOpen)
        {
            g.fillAll(juce::Colours::orchid);
            g.setColour(juce::Colours::white);
            g.drawText("Debugger", getLocalBounds(), juce::Justification::centredTop);
        }
        else
        {
            g.fillAll(juce::Colours::steelblue);

            g.setColour(juce::Colours::white);
            auto h = 15, y = 10;
            for (auto c : "Debugger")
            {
                g.drawText(std::string("") + c, 0, y, getWidth(), h, juce::Justification::centred);
                y += h;
            }
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        isOpen = !isOpen;
        editor->resized();
    }
};

FormulaModulatorEditor::FormulaModulatorEditor(SurgeGUIEditor *ed, SurgeStorage *s,
                                               FormulaModulatorStorage *fs,
                                               Surge::GUI::Skin::ptr_t skin)
    : CodeEditorContainerWithApply(ed, s, skin, false), formulastorage(fs)
{
    mainDocument->insertText(0, fs->formulaString);
    tabs =
        std::make_unique<juce::TabbedComponent>(juce::TabbedButtonBar::Orientation::TabsAtBottom);
    tabs->addTab("Formula", juce::Colours::red, mainEditor.get(), false);
    tabs->addTab("Prelude", juce::Colours::yellow, new juce::Label("Prelude", "Coming Soon"), true);
    tabs->addTab("Help", juce::Colours::green, new juce::Label("Help", "Coming Soon"), true);
    addAndMakeVisible(*tabs);

    tabs->getTabbedButtonBar().addAndMakeVisible(*applyButton);

    efd = std::make_unique<ExpandingFormulaDebugger>(this);
    addAndMakeVisible(*efd);
}

FormulaModulatorEditor::~FormulaModulatorEditor() = default;

void FormulaModulatorEditor::applyCode()
{
    formulastorage->setFormula(mainDocument->getAllContent().toStdString());
    applyButton->setEnabled(false);
    editor->repaintFrame();
    juce::SystemClipboard::copyTextToClipboard(formulastorage->formulaString);
}

void FormulaModulatorEditor::resized()
{
    int efdWidth = 14;
    if (efd->isOpen)
    {
        efdWidth = 200;
    }
    tabs->setTabBarDepth(18);
    tabs->setBounds(2, 2, getWidth() - 8 - efdWidth, getHeight() - 4);
    auto b = tabs->getTabbedButtonBar().getLocalBounds();
    applyButton->setBounds(b.getWidth() - 80, 2, 80 - 2, b.getHeight() - 4);
    efd->setBounds(getWidth() - 4 - efdWidth, 2, efdWidth, getHeight() - 4);
}

struct WavetablePreviewComponent : juce::Component
{
    WavetablePreviewComponent(SurgeStorage *s, OscillatorStorage *os, Surge::GUI::Skin::ptr_t skin)
        : storage(s), osc(os), skin(skin)
    {
    }

    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colour(0, 0, 0));
        g.setFont(Surge::GUI::getFontManager()->getFiraMonoAtSize(9));

        g.setColour(juce::Colour(230, 230, 255)); // could be a skin->getColor of course
        auto s1 = std::string("Frame : ") + std::to_string(frameNumber);
        auto s2 = std::string("Res   : ") + std::to_string(points.size());
        g.drawSingleLineText(s1, 3, 18);
        g.drawSingleLineText(s2, 3, 30);

        g.setColour(juce::Colour(160, 160, 160));
        auto h = getHeight();
        auto w = getWidth();

        auto t = h * 0.05;
        auto m = h * 0.5;
        auto b = h * 0.95;
        g.drawLine(0, t, w, t);
        g.drawLine(0, m, w, m);
        g.drawLine(0, b, w, b);

        auto p = juce::Path();
        auto dx = 1.0 / (points.size() - 1);
        for (int i = 0; i < points.size(); ++i)
        {
            float xp = dx * i * w;
            float yp = -((points[i] + 1) * 0.5) * (b - t) + b;
            if (i == 0)
                p.startNewSubPath(xp, yp);
            else
                p.lineTo(xp, yp);
        }
        g.setColour(juce::Colour(255, 180, 0));
        g.strokePath(p, juce::PathStrokeType(1.0));
    }

    int frameNumber = 0;
    std::vector<float> points;

    SurgeStorage *storage;
    OscillatorStorage *osc;
    Surge::GUI::Skin::ptr_t skin;
};

WavetableEquationEditor::WavetableEquationEditor(SurgeGUIEditor *ed, SurgeStorage *s,
                                                 OscillatorStorage *os, Surge::GUI::Skin::ptr_t sk)
    : CodeEditorContainerWithApply(ed, s, sk), osc(os)
{
    if (osc->wavetable_formula == "")
    {
        mainDocument->insertText(0, Surge::WavetableScript::defaultWavetableFormula());
    }
    else
    {
        mainDocument->insertText(0, osc->wavetable_formula);
    }

    resolutionLabel = std::make_unique<juce::Label>("resLabl");
    resolutionLabel->setFont(Surge::GUI::getFontManager()->getLatoAtSize(10));
    resolutionLabel->setText("Resolution:", juce::NotificationType::dontSendNotification);
    addAndMakeVisible(resolutionLabel.get());

    resolution = std::make_unique<juce::ComboBox>("res");
    int id = 1, grid = 32;
    while (grid <= 4096)
    {
        resolution->addItem(std::to_string(grid), id);
        id++;
        grid *= 2;
    }
    resolution->setSelectedId(os->wavetable_formula_res_base,
                              juce::NotificationType::dontSendNotification);
    addAndMakeVisible(resolution.get());

    framesLabel = std::make_unique<juce::Label>("frmLabl");
    framesLabel->setFont(Surge::GUI::getFontManager()->getLatoAtSize(10));
    framesLabel->setText("Frames:", juce::NotificationType::dontSendNotification);
    addAndMakeVisible(framesLabel.get());

    frames = std::make_unique<juce::TextEditor>("frm");
    frames->setFont(Surge::GUI::getFontManager()->getLatoAtSize(10));
    frames->setText(std::to_string(osc->wavetable_formula_nframes),
                    juce::NotificationType::dontSendNotification);
    addAndMakeVisible(frames.get());

    generate = std::make_unique<juce::TextButton>("gen");
    generate->setButtonText("Generate");
    generate->addListener(this);
    addAndMakeVisible(generate.get());

    renderer = std::make_unique<WavetablePreviewComponent>(storage, osc, sk);
    addAndMakeVisible(renderer.get());

    currentFrame = std::make_unique<juce::Slider>("currF");
    currentFrame->setSliderStyle(juce::Slider::LinearVertical);
    currentFrame->setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    currentFrame->setRange(0.0, 10.0);
    currentFrame->addListener(this);
    addAndMakeVisible(currentFrame.get());
}

WavetableEquationEditor::~WavetableEquationEditor() noexcept = default;

void WavetableEquationEditor::resized()
{
    auto w = getWidth() - 5;
    auto h = getHeight() - 5; // this is a hack obvs
    int m = 3;

    int topH = 20;
    int itemW = 100;

    int renderH = 150;

    resolutionLabel->setBounds(m, m, itemW, topH);
    resolution->setBounds(m * 2 + itemW, m, itemW, topH);
    framesLabel->setBounds(m * 3 + 2 * itemW, m, itemW, topH);
    frames->setBounds(m * 4 + 3 * itemW, m, itemW, topH);

    generate->setBounds(w - m - itemW, m, itemW, topH);
    applyButton->setBounds(w - 2 * m - 2 * itemW, m, itemW, topH);

    mainEditor->setBounds(m, m * 2 + topH, w - 2 * m, h - topH - renderH - 3 * m);

    currentFrame->setBounds(m, h - m - renderH, 32, renderH);
    renderer->setBounds(m + 32, h - m - renderH, w - 2 * m - 32, renderH);

    rerenderFromUIState();
}

void WavetableEquationEditor::applyCode()
{
    osc->wavetable_formula = mainDocument->getAllContent().toStdString();
    osc->wavetable_formula_res_base = resolution->getSelectedId();
    osc->wavetable_formula_nframes = std::atoi(frames->getText().toRawUTF8());

    applyButton->setEnabled(false);
    rerenderFromUIState();

    editor->repaintFrame();
}

void WavetableEquationEditor::rerenderFromUIState()
{
    auto resi = resolution->getSelectedId();
    auto nfr = std::atoi(frames->getText().toRawUTF8());
    auto cfr = (int)round(nfr * currentFrame->getValue() / 10.0);

    auto respt = 32;
    for (int i = 1; i < resi; ++i)
        respt *= 2;

    renderer->points = Surge::WavetableScript::evaluateScriptAtFrame(
        mainDocument->getAllContent().toStdString(), respt, cfr, nfr);
    renderer->frameNumber = cfr;
    renderer->repaint();
}

void WavetableEquationEditor::comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged)
{
    rerenderFromUIState();
}
void WavetableEquationEditor::sliderValueChanged(juce::Slider *slider) { rerenderFromUIState(); }
void WavetableEquationEditor::buttonClicked(juce::Button *button)
{
    if (button == generate.get())
    {
        std::cout << "GENERATE" << std::endl;
        auto resi = resolution->getSelectedId();
        auto nfr = std::atoi(frames->getText().toRawUTF8());
        auto respt = 32;
        for (int i = 1; i < resi; ++i)
            respt *= 2;

        wt_header wh;
        float *wd = nullptr;
        Surge::WavetableScript::constructWavetable(mainDocument->getAllContent().toStdString(),
                                                   respt, nfr, wh, &wd);
        storage->waveTableDataMutex.lock();
        osc->wt.BuildWT(wd, wh, wh.flags & wtf_is_sample);
        snprintf(osc->wavetable_display_name, 256, "Scripted Wavetable");
        storage->waveTableDataMutex.unlock();

        delete[] wd;
        editor->repaintFrame();

        return;
    }
    CodeEditorContainerWithApply::buttonClicked(button);
}

} // namespace Overlays
} // namespace Surge
