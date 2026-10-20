#include "Panels.h"
#include "Parameters.h"
#include "Theme.h"
#include <cmath>
#include <numeric>
#include <ranges>

namespace openedm::ui {

namespace {

namespace core = openedm::core;
namespace params = openedm::params;

constexpr double kMinHz = 20.0;
constexpr double kMaxHz = 20000.0;
constexpr float kMaxDb = 24.0f;
constexpr float kMinDb = -48.0f;
constexpr int kCurvePoints = 240;
constexpr int kRefreshHz = 30;
constexpr int kCaptionHeight = 14;
constexpr int kSectionGap = 10;

} // namespace

ResponseView::ResponseView(OpenEdmFilterProcessor& processor)
    : processor_(processor) {
    startTimerHz(kRefreshHz);
}

void ResponseView::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kPanel);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    const auto plot = bounds.reduced(14.0f, 12.0f);
    const double logSpan = std::log(kMaxHz / kMinHz);
    const auto xForHz = [&](double hz) {
        return plot.getX() + plot.getWidth() * static_cast<float>(std::log(hz / kMinHz) / logSpan);
    };
    const auto yForMagnitude = [&](double magnitude) {
        const auto db = static_cast<float>(juce::Decibels::gainToDecibels(magnitude, static_cast<double>(kMinDb)));
        return juce::jmap(juce::jlimit(kMinDb, kMaxDb, db), kMaxDb, kMinDb, plot.getY(), plot.getBottom());
    };

    g.setFont(juce::FontOptions(10.0f));
    const std::array<std::pair<double, const char*>, 8> gridLines{ {
        { 50.0, "50" }, { 100.0, "100" }, { 200.0, "200" }, { 500.0, "500" },
        { 1000.0, "1k" }, { 2000.0, "2k" }, { 5000.0, "5k" }, { 10000.0, "10k" } } };
    std::ranges::for_each(gridLines, [&](const auto& line) {
        const float x = xForHz(line.first);
        g.setColour(theme::kPanelEdge);
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        g.setColour(theme::kTextDim.withAlpha(0.6f));
        g.drawText(line.second, juce::Rectangle<float>(x + 3.0f, plot.getBottom() - 13.0f, 40.0f, 12.0f),
                   juce::Justification::centredLeft);
    });
    std::ranges::for_each(std::array{ 12.0f, 0.0f, -12.0f, -24.0f, -36.0f }, [&](float db) {
        const float y = yForMagnitude(juce::Decibels::decibelsToGain(static_cast<double>(db)));
        g.setColour(db == 0.0f ? theme::kTrack : theme::kPanelEdge);
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
    });

    const auto& state = processor_.parameters;
    const auto value = [&](size_t band, const char* suffix) {
        return state.getRawParameterValue(params::bandId(static_cast<int>(band), suffix))->load();
    };
    const auto shapeFor = [&](size_t band) {
        return core::FilterShape{ value(band, params::kBypass) < 0.5f,
                                  static_cast<core::FilterType>(static_cast<int>(value(band, params::kType))),
                                  static_cast<core::FilterSlope>(static_cast<int>(value(band, params::kSlope))),
                                  value(band, params::kResonance), value(band, params::kWidth) };
    };
    const std::array<core::FilterShape, 2> shapes{ shapeFor(0), shapeFor(1) };
    // Before the first processed block there is no modulated cutoff yet, so show the knob value.
    const auto displayCutoff = [&](size_t band) {
        const double modulated = processor_.displayCutoffs[band].load();
        if (modulated > 0.0) {
            return modulated;
        }
        return static_cast<double>(value(band, params::kCutoff));
    };
    const std::array<double, 2> displayCutoffs{ displayCutoff(0), displayCutoff(1) };
    const std::array<juce::Colour, 2> accents{ theme::kAccentA, theme::kAccentB };

    const auto bandMagnitude = [&](size_t band, double hz) {
        return core::MultiModeFilter::magnitudeAt(shapes[band], displayCutoffs[band], hz);
    };
    const auto combinedMagnitude = [&](double hz) { return bandMagnitude(0, hz) * bandMagnitude(1, hz); };
    const auto curve = [&](const auto& magnitudeAt) {
        juce::Path path;
        std::ranges::for_each(std::views::iota(0, kCurvePoints), [&](int point) {
            const double hz = kMinHz * std::pow(kMaxHz / kMinHz, point / static_cast<double>(kCurvePoints - 1));
            const juce::Point<float> position{ xForHz(hz), yForMagnitude(magnitudeAt(hz)) };
            if (point == 0) {
                path.startNewSubPath(position);
                return;
            }
            path.lineTo(position);
        });
        return path;
    };

    std::ranges::for_each(std::views::iota(size_t{0}, size_t{2}), [&](size_t band) {
        g.setColour(accents[band].withAlpha(shapes[band].enabled ? 0.5f : 0.15f));
        g.strokePath(curve([&](double hz) { return bandMagnitude(band, hz); }), juce::PathStrokeType(1.5f));
    });

    const juce::Path combined = curve(combinedMagnitude);
    juce::Path fill(combined);
    fill.lineTo(plot.getRight(), plot.getBottom());
    fill.lineTo(plot.getX(), plot.getBottom());
    fill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(theme::kText.withAlpha(0.14f), 0.0f, plot.getY(),
                                           theme::kText.withAlpha(0.0f), 0.0f, plot.getBottom(), false));
    g.fillPath(fill);
    g.setColour(theme::kText);
    g.strokePath(combined, juce::PathStrokeType(2.0f));

    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    std::ranges::for_each(std::views::iota(size_t{0}, size_t{2}), [&](size_t band) {
        const double cutoff = juce::jlimit(kMinHz, kMaxHz, displayCutoffs[band]);
        const juce::Point<float> marker{ xForHz(cutoff), yForMagnitude(combinedMagnitude(cutoff)) };
        const juce::String label = juce::String(band == 0 ? "A  " : "B  ") + params::formatHz(static_cast<float>(cutoff));
        const auto justification = band == 0 ? juce::Justification::centredLeft : juce::Justification::centredRight;
        g.setColour(accents[band].withAlpha(shapes[band].enabled ? 1.0f : 0.3f));
        g.fillEllipse(juce::Rectangle<float>(9.0f, 9.0f).withCentre(marker));
        g.drawText(label, plot.withHeight(16.0f).reduced(4.0f, 0.0f), justification);
    });
}

BandPanel::BandPanel(Apvts& state, int band)
    : title_(band == 0 ? "Filter A" : "Filter B"),
      accent_(band == 0 ? theme::kAccentA : theme::kAccentB),
      bypass_(state, params::bandId(band, params::kBypass), "BYPASS", accent_),
      type_(state, params::bandId(band, params::kType), {}, accent_),
      slope_(state, params::bandId(band, params::kSlope), {}, accent_),
      cutoff_(state, params::bandId(band, params::kCutoff), "Cutoff", accent_),
      resonance_(state, params::bandId(band, params::kResonance), "Resonance", accent_),
      width_(state, params::bandId(band, params::kWidth), "Width", accent_),
      lfoShape_(state, params::bandId(band, params::kLfoShape), "Shape", accent_),
      lfoRate_(state, params::bandId(band, params::kLfoRate), "Rate", accent_),
      lfoDepth_(state, params::bandId(band, params::kLfoDepth), "Depth", accent_),
      envSource_(state, params::bandId(band, params::kEnvSource), "Source", accent_),
      envAttack_(state, params::bandId(band, params::kEnvAttack), "Attack", accent_),
      envRelease_(state, params::bandId(band, params::kEnvRelease), "Release", accent_),
      envDepth_(state, params::bandId(band, params::kEnvDepth), "Depth", accent_),
      bypassWatcher_(*state.getParameter(params::bandId(band, params::kBypass)), [this](float isBypassed) {
          const float alpha = isBypassed > 0.5f ? 0.35f : 1.0f;
          std::ranges::for_each(getChildren(), [&](juce::Component* control) {
              if (control != &bypass_) {
                  control->setAlpha(alpha);
              }
          });
      }) {
    std::ranges::for_each(std::initializer_list<juce::Component*>{
                              &bypass_, &type_, &slope_, &cutoff_, &resonance_, &width_, &lfoShape_, &lfoRate_,
                              &lfoDepth_, &envSource_, &envAttack_, &envRelease_, &envDepth_ },
                          [this](juce::Component* control) { addAndMakeVisible(control); });
    bypassWatcher_.sendInitialUpdate();
}

void BandPanel::paint(juce::Graphics& g) {
    drawPanel(g, getLocalBounds(), title_, accent_);
    drawCaption(g, filterArea_, "Filter");
    drawCaption(g, lfoArea_, "LFO / tempo sync");
    drawCaption(g, envelopeArea_, "Envelope follower");
}

void BandPanel::resized() {
    auto bounds = getLocalBounds().reduced(12, 6);
    auto header = bounds.removeFromTop(30);
    slope_.setBounds(header.removeFromRight(76));
    header.removeFromRight(6);
    type_.setBounds(header.removeFromRight(108));
    header.removeFromRight(6);
    bypass_.setBounds(header.removeFromRight(68));

    bounds.removeFromTop(6);
    auto topRow = bounds.removeFromTop(bounds.getHeight() / 2);
    filterArea_ = topRow.removeFromLeft(topRow.getWidth() * 3 / 5);
    topRow.removeFromLeft(kSectionGap);
    lfoArea_ = topRow;
    envelopeArea_ = bounds.withTrimmedTop(4);

    layoutRow(filterArea_.withTrimmedTop(kCaptionHeight), { &cutoff_, &resonance_, &width_ });
    auto lfoControls = lfoArea_.withTrimmedTop(kCaptionHeight);
    auto lfoChoices = lfoControls.removeFromLeft(lfoControls.getWidth() / 2);
    lfoShape_.setBounds(lfoChoices.removeFromTop(lfoChoices.getHeight() / 2).reduced(0, 1));
    lfoRate_.setBounds(lfoChoices.reduced(0, 1));
    lfoDepth_.setBounds(lfoControls);
    layoutRow(envelopeArea_.withTrimmedTop(kCaptionHeight), { &envSource_, &envAttack_, &envRelease_, &envDepth_ });
}

EffectsPanel::EffectsPanel(Apvts& state)
    : drive_(state, params::kDrive, "Amount", theme::kAccentFx),
      drivePosition_(state, params::kDrivePosition, "Position", theme::kAccentFx),
      crushRate_(state, params::kCrushRate, "Rate", theme::kAccentFx),
      crushBits_(state, params::kCrushBits, "Bits", theme::kAccentFx),
      crushMix_(state, params::kCrushMix, "Mix", theme::kAccentFx),
      crushPosition_(state, params::kCrushPosition, "Position", theme::kAccentFx),
      crushLink_(state, params::kCrushLink, "LINK A", theme::kAccentFx),
      ringFrequency_(state, params::kRingFrequency, "Freq", theme::kAccentFx),
      ringMix_(state, params::kRingMix, "Mix", theme::kAccentFx),
      fmRate_(state, params::kFmRate, "Rate", theme::kAccentFx),
      fmDepth_(state, params::kFmDepth, "Depth", theme::kAccentFx),
      mix_(state, params::kMix, "Dry/Wet", theme::kAccentOut),
      output_(state, params::kOutput, "Gain", theme::kAccentOut),
      ceiling_(state, params::kCeiling, "Ceiling", theme::kAccentOut),
      limiter_(state, params::kLimiter, "LIMIT", theme::kAccentOut),
      sections_{ { { "Drive", theme::kAccentFx, 2, {} },
                   { "Crush", theme::kAccentFx, 5, {} },
                   { "Ring Mod", theme::kAccentFx, 2, {} },
                   { "Cutoff FM", theme::kAccentFx, 2, {} },
                   { "Output", theme::kAccentOut, 4, {} } } } {
    std::ranges::for_each(std::initializer_list<juce::Component*>{
                              &drive_, &drivePosition_, &crushRate_, &crushBits_, &crushMix_, &crushPosition_,
                              &crushLink_, &ringFrequency_, &ringMix_, &fmRate_, &fmDepth_, &mix_, &output_,
                              &ceiling_, &limiter_ },
                          [this](juce::Component* control) { addAndMakeVisible(control); });
}

void EffectsPanel::paint(juce::Graphics& g) {
    std::ranges::for_each(sections_, [&](const Section& section) {
        drawPanel(g, section.area, section.title, section.accent);
    });
}

void EffectsPanel::resized() {
    auto bounds = getLocalBounds();
    const int totalUnits = std::accumulate(sections_.begin(), sections_.end(), 0,
        [](int units, const Section& section) { return units + section.units; });
    const int gaps = kSectionGap * static_cast<int>(sections_.size() - 1);
    const int unitWidth = (bounds.getWidth() - gaps) / totalUnits;
    std::ranges::for_each(sections_, [&](Section& section) {
        section.area = bounds.removeFromLeft(section.units * unitWidth);
        bounds.removeFromLeft(kSectionGap);
    });
    sections_.back().area = sections_.back().area.withRight(getWidth());

    const auto content = [&](size_t section) { return sections_[section].area.reduced(10, 0).withTrimmedTop(34).withTrimmedBottom(8); };
    layoutRow(content(0), { &drive_, &drivePosition_ });
    layoutRow(content(1), { &crushRate_, &crushBits_, &crushMix_, &crushPosition_, &crushLink_ });
    layoutRow(content(2), { &ringFrequency_, &ringMix_ });
    layoutRow(content(3), { &fmRate_, &fmDepth_ });
    layoutRow(content(4), { &mix_, &output_, &ceiling_, &limiter_ });
}

} // namespace openedm::ui
