
#include <juce_audio_utils/juce_audio_utils.h>

class DJStudioRB : public juce::Component
{
public:
    DJStudioRB()
    {
        setSize(1100, 700);

        title.setText("DJ STUDIO RB", juce::dontSendNotification);
        title.setFont(juce::Font(30.0f, juce::Font::bold));
        title.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(title);

        setupDeck(deckA, "DECK A");
        setupDeck(deckB, "DECK B");

        crossfader.setRange(0.0, 1.0);
        crossfader.setValue(0.5);
        crossfader.setTextBoxStyle(
            juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(crossfader);

        mixerLabel.setText("CROSSFADER", juce::dontSendNotification);
        mixerLabel.setJustificationType(
            juce::Justification::centred);
        addAndMakeVisible(mixerLabel);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(18, 20, 27));
        g.setColour(juce::Colour(35, 39, 49));
        g.fillRoundedRectangle(
            getLocalBounds().toFloat().reduced(15), 15.0f);
    }

    void resized() override
    {
        title.setBounds(0, 20, getWidth(), 55);

        deckA.setBounds(35, 100, 480, 420);
        deckB.setBounds(585, 100, 480, 420);

        mixerLabel.setBounds(400, 550, 300, 30);
        crossfader.setBounds(350, 580, 400, 45);
    }

private:
    juce::Label title;
    juce::Label mixerLabel;
    juce::Slider crossfader;
    juce::GroupComponent deckA;
    juce::GroupComponent deckB;

    void setupDeck(juce::GroupComponent& deck,
                   const juce::String& name)
    {
        deck.setText(name);
        addAndMakeVisible(deck);
    }
};

class DJStudioRBApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override
    {
        return "DJ Studio RB";
    }

    const juce::String getApplicationVersion() override
    {
        return "0.1.0";
    }

    void initialise(const juce::String&) override
    {
        window.reset(new MainWindow());
    }

    void shutdown() override
    {
        window = nullptr;
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow()
            : DocumentWindow(
                "DJ Studio RB",
                juce::Colour(18, 20, 27),
                DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new DJStudioRB(), true);
            centreWithSize(1100, 700);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION(DJStudioRBApplication)
