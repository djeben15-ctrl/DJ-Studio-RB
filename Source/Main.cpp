#include <juce_audio_utils/juce_audio_utils.h>

class DJStudioRB : public juce::AudioAppComponent
{
public:
    DJStudioRB()
    {
        formatManager.registerBasicFormats();

        setSize(1100, 700);
        setAudioChannels(0, 2);

        title.setText("DJ STUDIO RB", juce::dontSendNotification);
        title.setFont(juce::Font(30.0f, juce::Font::bold));
        title.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(title);

        setupDeck(deckA, "DECK A", loadA, playA, stopA, trackA);
        setupDeck(deckB, "DECK B", loadB, playB, stopB, trackB);

        loadA.onClick = [this] { loadTrack(0); };
        loadB.onClick = [this] { loadTrack(1); };
        playA.onClick = [this] { startDeck(0); };
        playB.onClick = [this] { startDeck(1); };
        stopA.onClick = [this] { stopDeck(0); };
        stopB.onClick = [this] { stopDeck(1); };

        crossfader.setRange(0.0, 1.0, 0.001);
        crossfader.setValue(0.5);
        crossfader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        crossfader.setColour(juce::Slider::thumbColourId, juce::Colour(35, 210, 205));
        addAndMakeVisible(crossfader);

        mixerLabel.setText("CROSSFADER  A  ◀──────●──────▶  B", juce::dontSendNotification);
        mixerLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(mixerLabel);

        status.setText("Listo. Carga una canción en cada deck.", juce::dontSendNotification);
        status.setJustificationType(juce::Justification::centred);
        status.setColour(juce::Label::textColourId, juce::Colour(180, 190, 205));
        addAndMakeVisible(status);
    }

    ~DJStudioRB() override
    {
        shutdownAudio();
        transportA.setSource(nullptr);
        transportB.setSource(nullptr);
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        transportA.prepareToPlay(samplesPerBlockExpected, sampleRate);
        transportB.prepareToPlay(samplesPerBlockExpected, sampleRate);
        scratchA.setSize(2, samplesPerBlockExpected);
        scratchB.setSize(2, samplesPerBlockExpected);
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        if (info.buffer == nullptr)
            return;

        const int channels = juce::jmin(2, info.buffer->getNumChannels());
        if (info.numSamples > scratchA.getNumSamples())
        {
            scratchA.setSize(2, info.numSamples, false, false, true);
            scratchB.setSize(2, info.numSamples, false, false, true);
        }

        scratchA.clear(0, info.numSamples);
        scratchB.clear(0, info.numSamples);

        juce::AudioSourceChannelInfo blockA(&scratchA, 0, info.numSamples);
        juce::AudioSourceChannelInfo blockB(&scratchB, 0, info.numSamples);
        transportA.getNextAudioBlock(blockA);
        transportB.getNextAudioBlock(blockB);

        const float blend = static_cast<float>(crossfader.getValue());
        const float gainA = 1.0f - blend;
        const float gainB = blend;

        for (int channel = 0; channel < channels; ++channel)
        {
            info.buffer->clear(channel, info.startSample, info.numSamples);
            info.buffer->addFrom(channel, info.startSample, scratchA, channel, 0,
                                 info.numSamples, gainA);
            info.buffer->addFrom(channel, info.startSample, scratchB, channel, 0,
                                 info.numSamples, gainB);
        }
    }

    void releaseResources() override
    {
        transportA.releaseResources();
        transportB.releaseResources();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(18, 20, 27));
        g.setColour(juce::Colour(35, 39, 49));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(15), 15.0f);
    }

    void resized() override
    {
        title.setBounds(0, 18, getWidth(), 55);
        deckA.setBounds(35, 100, 480, 390);
        deckB.setBounds(585, 100, 480, 390);

        loadA.setBounds(65, 155, 120, 42);
        playA.setBounds(195, 155, 100, 42);
        stopA.setBounds(305, 155, 100, 42);
        trackA.setBounds(65, 220, 410, 90);

        loadB.setBounds(615, 155, 120, 42);
        playB.setBounds(745, 155, 100, 42);
        stopB.setBounds(855, 155, 100, 42);
        trackB.setBounds(615, 220, 410, 90);

        mixerLabel.setBounds(350, 515, 400, 30);
        crossfader.setBounds(300, 550, 500, 45);
        status.setBounds(100, 620, 900, 30);
    }

private:
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transportA, transportB;
    std::unique_ptr<juce::AudioFormatReaderSource> readerA, readerB;
    juce::AudioBuffer<float> scratchA, scratchB;
    std::unique_ptr<juce::FileChooser> chooserA, chooserB;

    juce::Label title, mixerLabel, status;
    juce::GroupComponent deckA, deckB;
    juce::TextButton loadA{"CARGAR"};
    juce::TextButton playA{"PLAY"};
    juce::TextButton stopA{"STOP"};
    juce::TextButton loadB{"CARGAR"};
    juce::TextButton playB{"PLAY"};
    juce::TextButton stopB{"STOP"};
    juce::Label trackA, trackB;
    juce::Slider crossfader;

    void setupDeck(juce::GroupComponent& deck, const juce::String& name,
                   juce::TextButton& load, juce::TextButton& play,
                   juce::TextButton& stop, juce::Label& track)
    {
        deck.setText(name);
        addAndMakeVisible(deck);

        for (auto* button : { &load, &play, &stop })
        {
            button->setColour(juce::TextButton::buttonColourId, juce::Colour(45, 55, 70));
            button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            addAndMakeVisible(button);
        }

        track.setText("Ninguna pista cargada", juce::dontSendNotification);
        track.setJustificationType(juce::Justification::centred);
        track.setColour(juce::Label::textColourId, juce::Colour(190, 200, 215));
        addAndMakeVisible(track);
    }

    void startDeck(int deckNumber)
    {
        auto* reader = (deckNumber == 0 ? readerA.get() : readerB.get());
        auto& transport = (deckNumber == 0 ? transportA : transportB);
        auto& playButton = (deckNumber == 0 ? playA : playB);

        if (reader == nullptr)
        {
            status.setText(deckNumber == 0
                ? "DECK A: primero carga una canción."
                : "DECK B: primero carga una canción.", juce::dontSendNotification);
            return;
        }

        if (transport.getLengthInSeconds() > 0.0 &&
            transport.getCurrentPosition() >= transport.getLengthInSeconds())
            transport.setPosition(0.0);

        transport.start();
        playButton.setButtonText("PLAYING");
        status.setText(deckNumber == 0
            ? "DECK A: orden de reproducción enviada al motor de audio."
            : "DECK B: orden de reproducción enviada al motor de audio.",
            juce::dontSendNotification);
    }

    void stopDeck(int deckNumber)
    {
        auto& transport = (deckNumber == 0 ? transportA : transportB);
        auto& playButton = (deckNumber == 0 ? playA : playB);
        transport.stop();
        transport.setPosition(0.0);
        playButton.setButtonText("PLAY");
        status.setText(deckNumber == 0 ? "DECK A detenido." : "DECK B detenido.",
                       juce::dontSendNotification);
    }

    void loadTrack(int deckNumber)
    {
        auto& chooser = (deckNumber == 0 ? chooserA : chooserB);
        chooser = std::make_unique<juce::FileChooser>(
            "Selecciona una canción de audio",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*.mp3;*.wav;*.aif;*.aiff;*.flac;*.ogg");

        chooser->launchAsync(
            juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
            [this, deckNumber](const juce::FileChooser& fileChooser)
            {
                const juce::File file = fileChooser.getResult();
                if (file.existsAsFile())
                    loadTrackFromFile(deckNumber, file);
            });
    }

    void loadTrackFromFile(int deckNumber, const juce::File& file)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

        if (reader == nullptr)
        {
            status.setText("No se pudo abrir ese formato de audio.", juce::dontSendNotification);
            return;
        }

        const double sourceSampleRate = reader->sampleRate;
        std::unique_ptr<juce::AudioFormatReaderSource> newSource(
            new juce::AudioFormatReaderSource(reader.release(), true));

        if (deckNumber == 0)
        {
            transportA.stop();
            transportA.setSource(nullptr);
            readerA.reset();
            readerA = std::move(newSource);
            transportA.setSource(readerA.get(), 0, nullptr, sourceSampleRate);
            trackA.setText(file.getFileName(), juce::dontSendNotification);
            playA.setButtonText("PLAY");
        }
        else
        {
            transportB.stop();
            transportB.setSource(nullptr);
            readerB.reset();
            readerB = std::move(newSource);
            transportB.setSource(readerB.get(), 0, nullptr, sourceSampleRate);
            trackB.setText(file.getFileName(), juce::dontSendNotification);
            playB.setButtonText("PLAY");
        }

        status.setText("Pista cargada. Pulsa PLAY para reproducir.", juce::dontSendNotification);
    }
};

class DJStudioRBApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "DJ Studio RB"; }
    const juce::String getApplicationVersion() override { return "0.2.0"; }

    void initialise(const juce::String&) override { window.reset(new MainWindow()); }
    void shutdown() override { window = nullptr; }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow()
            : DocumentWindow("DJ Studio RB", juce::Colour(18, 20, 27),
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
