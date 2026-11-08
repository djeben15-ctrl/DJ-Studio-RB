#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>

class DJStudioRB : public juce::AudioAppComponent, private juce::Timer
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

        setupDeckControls();
        startTimerHz(10);
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
        lowAlpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 250.0f / static_cast<float>(sampleRate));
        midAlpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 4000.0f / static_cast<float>(sampleRate));
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
        applyThreeBandEQ(scratchA, info.numSamples,
                         static_cast<float>(eqLowA.getValue()),
                         static_cast<float>(eqMidA.getValue()),
                         static_cast<float>(eqHighA.getValue()), lowStateA, midStateA);
        applyThreeBandEQ(scratchB, info.numSamples,
                         static_cast<float>(eqLowB.getValue()),
                         static_cast<float>(eqMidB.getValue()),
                         static_cast<float>(eqHighB.getValue()), lowStateB, midStateB);

        const float gainA = (1.0f - blend) * static_cast<float>(volumeA.getValue());
        const float gainB = blend * static_cast<float>(volumeB.getValue());

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
        trackA.setBounds(65, 210, 410, 48);
        timeA.setBounds(65, 265, 410, 24);
        seekA.setBounds(65, 290, 410, 24);
        cueA.setBounds(65, 330, 90, 34);
        volumeLabelA.setBounds(170, 330, 90, 34);
        volumeA.setBounds(260, 330, 215, 34);
        eqTitleA.setBounds(65, 372, 410, 20);
        eqLowA.setBounds(65, 398, 125, 28);
        eqMidA.setBounds(205, 398, 125, 28);
        eqHighA.setBounds(345, 398, 130, 28);

        loadB.setBounds(615, 155, 120, 42);
        playB.setBounds(745, 155, 100, 42);
        stopB.setBounds(855, 155, 100, 42);
        trackB.setBounds(615, 210, 410, 48);
        timeB.setBounds(615, 265, 410, 24);
        seekB.setBounds(615, 290, 410, 24);
        cueB.setBounds(615, 330, 90, 34);
        volumeLabelB.setBounds(720, 330, 90, 34);
        volumeB.setBounds(810, 330, 215, 34);
        eqTitleB.setBounds(615, 372, 410, 20);
        eqLowB.setBounds(615, 398, 125, 28);
        eqMidB.setBounds(755, 398, 125, 28);
        eqHighB.setBounds(895, 398, 130, 28);

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
    juce::Slider crossfader, seekA, seekB, volumeA, volumeB;
    juce::TextButton cueA{"CUE / INICIO"};
    juce::TextButton cueB{"CUE / INICIO"};
    juce::Label timeA, timeB, volumeLabelA, volumeLabelB, eqTitleA, eqTitleB;
    juce::Slider eqLowA, eqMidA, eqHighA, eqLowB, eqMidB, eqHighB;
    float lowAlpha = 0.03f, midAlpha = 0.3f;
    float lowStateA[2] = { 0.0f, 0.0f }, midStateA[2] = { 0.0f, 0.0f };
    float lowStateB[2] = { 0.0f, 0.0f }, midStateB[2] = { 0.0f, 0.0f };

    void applyThreeBandEQ(juce::AudioBuffer<float>& buffer, int numSamples,
                          float lowGain, float midGain, float highGain,
                          float* lowState, float* midState)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* samples = buffer.getWritePointer(channel);
            float low = lowState[channel];
            float midLowPass = midState[channel];

            for (int i = 0; i < numSamples; ++i)
            {
                const float input = samples[i];
                low += lowAlpha * (input - low);
                midLowPass += midAlpha * (input - midLowPass);
                const float lowBand = low;
                const float midBand = midLowPass - low;
                const float highBand = input - midLowPass;
                samples[i] = lowBand * lowGain + midBand * midGain + highBand * highGain;
            }

            lowState[channel] = low;
            midState[channel] = midLowPass;
        }
    }

    void setupDeckControls()
    {
        auto setupSeek = [this](juce::Slider& slider)
        {
            slider.setRange(0.0, 1.0, 0.001);
            slider.setValue(0.0);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            slider.setColour(juce::Slider::trackColourId, juce::Colour(35, 210, 205));
            addAndMakeVisible(slider);
        };
        setupSeek(seekA);
        setupSeek(seekB);

        auto setupVolume = [this](juce::Slider& slider)
        {
            slider.setRange(0.0, 1.0, 0.01);
            slider.setValue(1.0);
            slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 48, 24);
            addAndMakeVisible(slider);
        };
        setupVolume(volumeA);
        setupVolume(volumeB);

        auto setupEq = [this](juce::Slider& slider, juce::Label& label, const juce::String& text)
        {
            label.setText(text, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setColour(juce::Label::textColourId, juce::Colour(190, 200, 215));
            addAndMakeVisible(label);
            slider.setRange(0.0, 2.0, 0.01);
            slider.setValue(1.0);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            slider.setColour(juce::Slider::trackColourId, juce::Colour(35, 210, 205));
            addAndMakeVisible(slider);
        };
        setupEq(eqLowA, eqTitleA, "ECUALIZADOR  LOW / MID / HIGH");
        setupEq(eqMidA, eqTitleA, "ECUALIZADOR  LOW / MID / HIGH");
        setupEq(eqHighA, eqTitleA, "ECUALIZADOR  LOW / MID / HIGH");
        setupEq(eqLowB, eqTitleB, "ECUALIZADOR  LOW / MID / HIGH");
        setupEq(eqMidB, eqTitleB, "ECUALIZADOR  LOW / MID / HIGH");
        setupEq(eqHighB, eqTitleB, "ECUALIZADOR  LOW / MID / HIGH");

        cueA.onClick = [this] { cueDeck(0); };
        cueB.onClick = [this] { cueDeck(1); };
        for (auto* button : { &cueA, &cueB })
        {
            button->setColour(juce::TextButton::buttonColourId, juce::Colour(45, 55, 70));
            button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            addAndMakeVisible(button);
        }

        timeA.setJustificationType(juce::Justification::centredLeft);
        timeB.setJustificationType(juce::Justification::centredLeft);
        volumeLabelA.setText("VOLUME", juce::dontSendNotification);
        volumeLabelB.setText("VOLUME", juce::dontSendNotification);
        for (auto* label : { &timeA, &timeB, &volumeLabelA, &volumeLabelB })
        {
            label->setColour(juce::Label::textColourId, juce::Colour(190, 200, 215));
            addAndMakeVisible(label);
        }

        seekA.onValueChange = [this] { seekDeckFromSlider(0); };
        seekB.onValueChange = [this] { seekDeckFromSlider(1); };
    }

    static juce::String formatTime(double seconds)
    {
        const int total = juce::jmax(0, (int) seconds);
        return juce::String(total / 60).paddedLeft('0', 2) + ":" +
               juce::String(total % 60).paddedLeft('0', 2);
    }

    void timerCallback() override
    {
        updateDeckDisplay(0);
        updateDeckDisplay(1);
    }

    void updateDeckDisplay(int deckNumber)
    {
        auto& transport = (deckNumber == 0 ? transportA : transportB);
        auto& slider = (deckNumber == 0 ? seekA : seekB);
        auto& label = (deckNumber == 0 ? timeA : timeB);
        const double length = transport.getLengthInSeconds();
        const double position = transport.getCurrentPosition();

        if (!slider.isMouseButtonDown())
            slider.setValue(length > 0.0 ? position / length : 0.0,
                            juce::dontSendNotification);

        label.setText(formatTime(position) + " / " + formatTime(length),
                      juce::dontSendNotification);
    }

    void seekDeckFromSlider(int deckNumber)
    {
        auto& transport = (deckNumber == 0 ? transportA : transportB);
        const double length = transport.getLengthInSeconds();
        if (length > 0.0)
        {
            const auto& slider = (deckNumber == 0 ? seekA : seekB);
            transport.setPosition(slider.getValue() * length);
        }
    }

    void cueDeck(int deckNumber)
    {
        auto& transport = (deckNumber == 0 ? transportA : transportB);
        transport.stop();
        transport.setPosition(0.0);
        auto& playButton = (deckNumber == 0 ? playA : playB);
        playButton.setButtonText("PLAY");
        status.setText(deckNumber == 0 ? "DECK A: posición inicial." :
                                         "DECK B: posición inicial.",
                       juce::dontSendNotification);
    }

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
    const juce::String getApplicationVersion() override { return "0.4.0"; }

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
