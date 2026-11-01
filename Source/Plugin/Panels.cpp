#include "Panels.h"
#include "Parameters.h"
#include "Presets.h"
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
constexpr int kSweepCurvePoints = 160;
constexpr int kRefreshHz = 30;
constexpr float kResonanceDragPixels = 150.0f;
constexpr float kSlopeDragPixels = 90.0f;
constexpr double kFallbackSampleRate = 44100.0;
constexpr int kCaptionHeight = 14;
constexpr int kSectionGap = 10;

} // namespace

ResponseView::ResponseView(OpenEdmFilterProcessor& processor)
    : processor_(processor) {
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    startTimerHz(kRefreshHz);
}

juce::Rectangle<float> ResponseView::plotArea() const {
    return getLocalBounds().toFloat().reduced(14.0f, 12.0f);
}

float ResponseView::xForHz(double hz) const {
    const auto plot = plotArea();
    return plot.getX() + plot.getWidth() * static_cast<float>(std::log(hz / kMinHz) / std::log(kMaxHz / kMinHz));
}

// Before the first processed block there is no modulated cutoff yet, so show the sweep or knob value.
double ResponseView::displayCutoff(int band) const {
    const double modulated = processor_.displayCutoffs[static_cast<size_t>(band)].load();
    if (modulated > 0.0) {
        return modulated;
    }
    const auto sweep = processor_.currentSweepSettings();
    if (core::AutoSweep::appliesToBand(sweep, band)) {
        return core::AutoSweep::valueAt(sweep, band, processor_.displaySweepPhase.load()).cutoffHz;
    }
    return processor_.parameters.getRawParameterValue(params::bandId(band, params::kCutoff))->load();
}

float ResponseView::displayResonance(int band) const {
    const auto sweep = processor_.currentSweepSettings();
    if (core::AutoSweep::appliesToBand(sweep, band)) {
        const float modulated = processor_.displayResonances[static_cast<size_t>(band)].load();
        if (modulated >= 0.0f) {
            return modulated;
        }
        return core::AutoSweep::valueAt(sweep, band, processor_.displaySweepPhase.load()).resonance;
    }
    return processor_.parameters.getRawParameterValue(params::bandId(band, params::kResonance))->load();
}

void ResponseView::mouseDown(const juce::MouseEvent& event) {
    const auto distanceTo = [&](int band) { return std::abs(event.position.x - xForHz(displayCutoff(band))); };
    const int band = distanceTo(0) <= distanceTo(1) ? 0 : 1;
    dragCutoff_ = processor_.parameters.getParameter(params::bandId(band, params::kCutoff));
    dragResonance_ = processor_.parameters.getParameter(params::bandId(band, params::kResonance));
    resonanceAtDragStart_ = dragResonance_->getValue();
    dragCutoff_->beginChangeGesture();
    dragResonance_->beginChangeGesture();
    mouseDrag(event);
}

void ResponseView::mouseDrag(const juce::MouseEvent& event) {
    if (dragCutoff_ == nullptr) {
        return;
    }
    const auto plot = plotArea();
    const float proportion = juce::jlimit(0.0f, 1.0f, (event.position.x - plot.getX()) / plot.getWidth());
    const auto hz = static_cast<float>(kMinHz * std::pow(kMaxHz / kMinHz, static_cast<double>(proportion)));
    dragCutoff_->setValueNotifyingHost(dragCutoff_->convertTo0to1(hz));
    const float resonance = resonanceAtDragStart_ - static_cast<float>(event.getDistanceFromDragStartY()) / kResonanceDragPixels;
    dragResonance_->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, resonance));
}

void ResponseView::mouseUp(const juce::MouseEvent&) {
    if (dragCutoff_ == nullptr) {
        return;
    }
    dragCutoff_->endChangeGesture();
    dragResonance_->endChangeGesture();
    dragCutoff_ = nullptr;
    dragResonance_ = nullptr;
}

void ResponseView::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kPanel);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    const auto plot = plotArea();
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
        g.setColour(theme::kTextDim.withAlpha(0.6f));
        g.drawText((db > 0.0f ? "+" : "") + juce::String(juce::roundToInt(db)) + " dB",
                   juce::Rectangle<float>(plot.getX() + 3.0f, y + 1.0f, 50.0f, 12.0f), juce::Justification::centredLeft);
    });

    const auto& state = processor_.parameters;
    const auto value = [&](size_t band, const char* suffix) {
        return state.getRawParameterValue(params::bandId(static_cast<int>(band), suffix))->load();
    };
    const auto shapeFor = [&](size_t band) {
        return core::FilterShape{ value(band, params::kBypass) < 0.5f,
                                  static_cast<core::FilterType>(static_cast<int>(value(band, params::kType))),
                                  static_cast<core::FilterSlope>(static_cast<int>(value(band, params::kSlope))),
                                  displayResonance(static_cast<int>(band)), value(band, params::kWidth) };
    };
    const std::array<core::FilterShape, 2> shapes{ shapeFor(0), shapeFor(1) };
    const std::array<double, 2> displayCutoffs{ displayCutoff(0), displayCutoff(1) };
    const std::array<juce::Colour, 2> accents{ theme::kAccentA, theme::kAccentB };
    const double hostRate = processor_.getSampleRate() > 0.0 ? processor_.getSampleRate() : kFallbackSampleRate;
    const double filterRate = hostRate * core::Oversampler::kFactor;

    const auto bandMagnitude = [&](size_t band, double hz) {
        return core::MultiModeFilter::magnitudeAt(shapes[band], displayCutoffs[band], hz, filterRate);
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
        const int resPercent = juce::roundToInt(shapes[band].resonance * 100.0f);
        const juce::String label = juce::String(band == 0 ? "A  " : "B  ")
            + params::formatHz(static_cast<float>(cutoff)) + "  (" + juce::String(resPercent) + "%)";
        const auto justification = band == 0 ? juce::Justification::centredLeft : juce::Justification::centredRight;
        g.setColour(accents[band].withAlpha(shapes[band].enabled ? 1.0f : 0.3f));
        g.fillEllipse(juce::Rectangle<float>(9.0f, 9.0f).withCentre(marker));
        g.drawText(label, plot.withHeight(16.0f).reduced(4.0f, 0.0f), justification);
    });
}

SweepSlopeEditor::SweepSlopeEditor(OpenEdmFilterProcessor& processor)
    : processor_(processor) {
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    startTimerHz(kRefreshHz);
}

const char* SweepSlopeEditor::parameterIdForHandle(Handle handle) noexcept {
    switch (handle) {
        case Handle::CutoffStart: return params::kSweepCutoffStart;
        case Handle::CutoffSlope: return params::kSweepCutoffSlope;
        case Handle::CutoffEnd: return params::kSweepCutoffEnd;
        case Handle::ResStart: return params::kSweepResStart;
        case Handle::ResSlope: return params::kSweepResSlope;
        case Handle::ResEnd: return params::kSweepResEnd;
        case Handle::None: break;
    }
    return nullptr;
}

juce::Rectangle<float> SweepSlopeEditor::plotArea() const {
    return getLocalBounds().toFloat().reduced(10.0f, 18.0f).withTrimmedTop(4.0f);
}

float SweepSlopeEditor::yForHz(float hz) const {
    const double clamped = std::clamp(static_cast<double>(hz), kMinHz, kMaxHz);
    const auto norm = static_cast<float>(std::log(clamped / kMinHz) / std::log(kMaxHz / kMinHz));
    return yForNormalized(norm);
}

float SweepSlopeEditor::hzForY(float y) const {
    const float norm = normalizedForY(y);
    return static_cast<float>(kMinHz * std::pow(kMaxHz / kMinHz, static_cast<double>(norm)));
}

float SweepSlopeEditor::yForNormalized(float value) const {
    const auto plot = plotArea();
    return plot.getBottom() - std::clamp(value, 0.0f, 1.0f) * plot.getHeight();
}

float SweepSlopeEditor::normalizedForY(float y) const {
    const auto plot = plotArea();
    return std::clamp((plot.getBottom() - y) / std::max(1.0f, plot.getHeight()), 0.0f, 1.0f);
}

std::pair<float, float> SweepSlopeEditor::handlePhases(core::SweepMode mode, int bars) const {
    switch (mode) {
        case core::SweepMode::BarWindow:
        case core::SweepMode::BarMacro:
            return { 0.5f, 1.0f };
        case core::SweepMode::UpDown:
            return { 0.25f, 0.5f };
        case core::SweepMode::EveryBar: {
            const float oneBar = 1.0f / static_cast<float>(std::max(1, bars));
            return { 0.5f * oneBar, 1.0f };
        }
    }
    return { 0.5f, 1.0f };
}

juce::Point<float> SweepSlopeEditor::handlePosition(Handle handle, const core::AutoSweepSettings& settings) const {
    const auto plot = plotArea();
    const auto [midPhase, endPhase] = handlePhases(settings.mode, settings.bars);
    const auto xAt = [&](float phase) { return plot.getX() + phase * plot.getWidth(); };
    const auto samplePhase = [&](float phase) {
        return core::AutoSweep::valueAt(settings, 0, static_cast<double>( std::min(0.9999f, phase) ));
    };

    switch (handle) {
        case Handle::CutoffStart: return { xAt(0.0f), yForHz(settings.cutoffStartHz) };
        case Handle::CutoffSlope: return { xAt(midPhase), yForHz(samplePhase(midPhase).cutoffHz) };
        case Handle::CutoffEnd: return { xAt(endPhase), yForHz(settings.cutoffEndHz) };
        case Handle::ResStart: return { xAt(0.0f), yForNormalized(settings.resonanceStart) };
        case Handle::ResSlope: return { xAt(midPhase), yForNormalized(samplePhase(midPhase).resonance) };
        case Handle::ResEnd: return { xAt(endPhase), yForNormalized(settings.resonanceEnd) };
        case Handle::None: break;
    }
    return plot.getCentre();
}

void SweepSlopeEditor::mouseDown(const juce::MouseEvent& event) {
    const auto settings = processor_.currentSweepSettings();
    const std::array<Handle, 6> handles{
        Handle::CutoffStart, Handle::CutoffSlope, Handle::CutoffEnd,
        Handle::ResStart, Handle::ResSlope, Handle::ResEnd
    };
    activeHandle_ = *std::ranges::min_element(handles, [&](Handle a, Handle b) {
        return event.position.getDistanceFrom(handlePosition(a, settings))
             < event.position.getDistanceFrom(handlePosition(b, settings));
    });
    if (const char* id = parameterIdForHandle(activeHandle_)) {
        activeParameter_ = processor_.parameters.getParameter(id);
        if (activeParameter_ != nullptr) {
            slopeAtDragStart_ = activeParameter_->convertFrom0to1(activeParameter_->getValue());
            activeParameter_->beginChangeGesture();
            mouseDrag(event);
        }
    }
}

void SweepSlopeEditor::mouseDrag(const juce::MouseEvent& event) {
    if (activeParameter_ == nullptr) {
        return;
    }
    const auto settings = processor_.currentSweepSettings();
    switch (activeHandle_) {
        case Handle::CutoffStart:
        case Handle::CutoffEnd: {
            const float hz = hzForY(event.position.y);
            activeParameter_->setValueNotifyingHost(activeParameter_->convertTo0to1(hz));
            break;
        }
        case Handle::ResStart:
        case Handle::ResEnd: {
            const float res = normalizedForY(event.position.y);
            activeParameter_->setValueNotifyingHost(activeParameter_->convertTo0to1(res));
            break;
        }
        case Handle::CutoffSlope: {
            const float normStart = normalizedForY(yForHz(settings.cutoffStartHz));
            const float normEnd = normalizedForY(yForHz(settings.cutoffEndHz));
            const float span = normEnd - normStart;
            float slope = slopeAtDragStart_ + static_cast<float>(event.getDistanceFromDragStartY()) / kSlopeDragPixels;
            if (std::abs(span) > 0.04f) {
                const float fraction = (normalizedForY(event.position.y) - normStart) / span;
                slope = core::AutoSweep::slopeFromMidpoint(fraction);
            }
            activeParameter_->setValueNotifyingHost(activeParameter_->convertTo0to1(std::clamp(slope, -1.0f, 1.0f)));
            break;
        }
        case Handle::ResSlope: {
            const float span = settings.resonanceEnd - settings.resonanceStart;
            float slope = slopeAtDragStart_ + static_cast<float>(event.getDistanceFromDragStartY()) / kSlopeDragPixels;
            if (std::abs(span) > 0.04f) {
                const float fraction = (normalizedForY(event.position.y) - settings.resonanceStart) / span;
                slope = core::AutoSweep::slopeFromMidpoint(fraction);
            }
            activeParameter_->setValueNotifyingHost(activeParameter_->convertTo0to1(std::clamp(slope, -1.0f, 1.0f)));
            break;
        }
        case Handle::None:
            break;
    }
}

void SweepSlopeEditor::mouseUp(const juce::MouseEvent&) {
    if (activeParameter_ != nullptr) {
        activeParameter_->endChangeGesture();
        activeParameter_ = nullptr;
    }
    activeHandle_ = Handle::None;
}

void SweepSlopeEditor::mouseDoubleClick(const juce::MouseEvent& event) {
    const auto settings = processor_.currentSweepSettings();
    const float distCut = event.position.getDistanceFrom(handlePosition(Handle::CutoffSlope, settings));
    const float distRes = event.position.getDistanceFrom(handlePosition(Handle::ResSlope, settings));
    const char* id = distCut <= distRes ? params::kSweepCutoffSlope : params::kSweepResSlope;
    if (auto* param = processor_.parameters.getParameter(id)) {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(0.0f));
        param->endChangeGesture();
    }
}

void SweepSlopeEditor::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kBackground.brighter(0.04f));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    const auto plot = plotArea();
    const auto settings = processor_.currentSweepSettings();
    const float activeAlpha = settings.enabled ? 1.0f : 0.45f;

    // Horizontal reference grid (25%, 50%, 75%)
    std::ranges::for_each(std::array{ 0.25f, 0.5f, 0.75f }, [&](float norm) {
        const float y = yForNormalized(norm);
        g.setColour(norm == 0.5f ? theme::kTrack.withAlpha(0.7f) : theme::kPanelEdge.withAlpha(0.5f));
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
    });

    // Vertical bar grid across the 4, 8, 16, 32 or 64 bar window
    const int bars = std::max(1, settings.bars);
    const int divisions = std::min(bars, 8);
    const int barsPerDivision = std::max(1, bars / divisions);
    g.setFont(juce::FontOptions(9.5f));
    std::ranges::for_each(std::views::iota(0, divisions + 1), [&](int div) {
        const float fraction = static_cast<float>(div) / static_cast<float>(divisions);
        const float x = plot.getX() + fraction * plot.getWidth();
        g.setColour(theme::kPanelEdge.withAlpha(0.65f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        if (div < divisions) {
            const int barNumber = div * barsPerDivision + 1;
            g.setColour(theme::kTextDim.withAlpha(0.65f));
            g.drawText("B" + juce::String(barNumber),
                       juce::Rectangle<float>(x + 2.0f, plot.getBottom() + 1.0f, 32.0f, 12.0f),
                       juce::Justification::centredLeft);
        }
    });

    const auto buildCurve = [&](const auto& yAtPhase) {
        juce::Path path;
        std::ranges::for_each(std::views::iota(0, kSweepCurvePoints), [&](int i) {
            const float phase = static_cast<float>(i) / static_cast<float>(kSweepCurvePoints - 1);
            const double samplePhase = std::min(0.9999, static_cast<double>(phase));
            const juce::Point<float> pt{ plot.getX() + phase * plot.getWidth(), yAtPhase(samplePhase) };
            if (i == 0) {
                path.startNewSubPath(pt);
                return;
            }
            path.lineTo(pt);
        });
        return path;
    };

    const juce::Path cutoffPath = buildCurve([&](double phase) {
        return yForHz(core::AutoSweep::valueAt(settings, 0, phase).cutoffHz);
    });
    juce::Path cutoffFill(cutoffPath);
    cutoffFill.lineTo(plot.getRight(), plot.getBottom());
    cutoffFill.lineTo(plot.getX(), plot.getBottom());
    cutoffFill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(theme::kAccentA.withAlpha(0.18f * activeAlpha), 0.0f, plot.getY(),
                                           theme::kAccentA.withAlpha(0.0f), 0.0f, plot.getBottom(), false));
    g.fillPath(cutoffFill);
    g.setColour(theme::kAccentA.withAlpha(activeAlpha));
    g.strokePath(cutoffPath, juce::PathStrokeType(2.0f));

    const juce::Path resPath = buildCurve([&](double phase) {
        return yForNormalized(core::AutoSweep::valueAt(settings, 0, phase).resonance);
    });
    g.setColour(theme::kAccentB.withAlpha(0.9f * activeAlpha));
    g.strokePath(resPath, juce::PathStrokeType(1.75f));

    // Live playhead across the multi-bar loop
    const float phase = std::clamp(processor_.displaySweepPhase.load(), 0.0f, 0.9999f);
    const float playheadX = plot.getX() + phase * plot.getWidth();
    const auto liveValue = core::AutoSweep::valueAt(settings, 0, static_cast<double>(phase));
    g.setColour(theme::kText.withAlpha(settings.enabled ? 0.65f : 0.2f));
    g.drawVerticalLine(juce::roundToInt(playheadX), plot.getY(), plot.getBottom());
    if (settings.enabled) {
        g.setColour(theme::kAccentA);
        g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre({ playheadX, yForHz(liveValue.cutoffHz) }));
        g.setColour(theme::kAccentB);
        g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre({ playheadX, yForNormalized(liveValue.resonance) }));
    }

    // Draw the 6 draggable handles (Start, Slope, End for Cutoff and Resonance)
    const auto drawHandle = [&](Handle handle, juce::Colour colour, bool isMidpoint) {
        const auto pos = handlePosition(handle, settings);
        const float radius = activeHandle_ == handle ? 5.5f : 4.5f;
        g.setColour(theme::kBackground);
        g.fillEllipse(juce::Rectangle<float>(radius * 2.0f + 2.0f, radius * 2.0f + 2.0f).withCentre(pos));
        g.setColour(colour.withAlpha(activeAlpha));
        if (isMidpoint) {
            g.drawEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(pos), 1.8f);
            g.fillEllipse(juce::Rectangle<float>(4.0f, 4.0f).withCentre(pos));
        } else {
            g.fillEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(pos));
        }
    };
    drawHandle(Handle::CutoffStart, theme::kAccentA, false);
    drawHandle(Handle::CutoffSlope, theme::kAccentA, true);
    drawHandle(Handle::CutoffEnd, theme::kAccentA, false);
    drawHandle(Handle::ResStart, theme::kAccentB, false);
    drawHandle(Handle::ResSlope, theme::kAccentB, true);
    drawHandle(Handle::ResEnd, theme::kAccentB, false);

    // Header overlay showing live bar, time signature, BPM and slope endpoints
    const auto headerBar = getLocalBounds().toFloat().reduced(8.0f, 2.0f).removeFromTop(14.0f);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(theme::kAccentA);
    const juce::String cutSummary = "CUT " + params::formatHz(settings.cutoffStartHz) + " -> " + params::formatHz(settings.cutoffEndHz);
    g.drawText(cutSummary, headerBar.withWidth(155.0f), juce::Justification::centredLeft);

    g.setColour(theme::kAccentB);
    const juce::String resSummary = "RES " + juce::String(juce::roundToInt(settings.resonanceStart * 100.0f)) + "% -> "
        + juce::String(juce::roundToInt(settings.resonanceEnd * 100.0f)) + "%";
    g.drawText(resSummary, headerBar.withTrimmedLeft(155.0f).withWidth(105.0f), juce::Justification::centredLeft);

    const int sigIndex = static_cast<int>(processor_.parameters.getRawParameterValue(params::kTimeSignature)->load());
    const auto [num, denom] = params::kTimeSignatures[static_cast<size_t>(std::clamp(sigIndex, 0, static_cast<int>(params::kTimeSignatures.size()) - 1))];
    const float currentBar = phase * static_cast<float>(bars) + 1.0f;
    const float bpm = processor_.displayBpm.load();
    g.setColour(theme::kTextDim);
    const juce::String clockSummary = "BAR " + juce::String(currentBar, 1) + "/" + juce::String(bars)
        + "  " + juce::String(num) + "/" + juce::String(denom)
        + "  " + juce::String(bpm, 0) + " BPM";
    g.drawText(clockSummary, headerBar, juce::Justification::centredRight);
}

SweepPanel::SweepPanel(OpenEdmFilterProcessor& processor)
    : processor_(processor),
      enabled_(processor.parameters, params::kSweepEnabled, "AUTO SWEEP", theme::kAccentA),
      clockSync_(processor.parameters, params::kClockSync, "HOST SYNC", theme::kAccentOut),
      timeSig_(processor.parameters, params::kTimeSignature, {}, theme::kAccentOut),
      bars_(processor.parameters, params::kSweepBars, {}, theme::kAccentA),
      mode_(processor.parameters, params::kSweepMode, {}, theme::kAccentA),
      target_(processor.parameters, params::kSweepTarget, {}, theme::kAccentA),
      tempo_(processor.parameters, params::kTempoBpm, "Tempo", theme::kAccentOut),
      slopeEditor_(processor),
      cutoffStart_(processor.parameters, params::kSweepCutoffStart, "Start", theme::kAccentA),
      cutoffEnd_(processor.parameters, params::kSweepCutoffEnd, "End", theme::kAccentA),
      cutoffSlope_(processor.parameters, params::kSweepCutoffSlope, "Slope", theme::kAccentA),
      resStart_(processor.parameters, params::kSweepResStart, "Start", theme::kAccentB),
      resEnd_(processor.parameters, params::kSweepResEnd, "End", theme::kAccentB),
      resSlope_(processor.parameters, params::kSweepResSlope, "Slope", theme::kAccentB) {
    slopePresetBox_.setTextWhenNothingSelected("Sweep Preset...");
    const auto& slopePresets = openedm::presets::sweepSlopePresets();
    std::ranges::for_each(slopePresets, [&, id = 1](const auto& preset) mutable {
        slopePresetBox_.addItem(preset.name, id++);
    });
    slopePresetBox_.setColour(juce::ComboBox::outlineColourId, theme::kAccentA);
    slopePresetBox_.onChange = [this] {
        const int idx = slopePresetBox_.getSelectedId() - 1;
        const auto& presets = openedm::presets::sweepSlopePresets();
        if (idx >= 0 && idx < static_cast<int>(presets.size())) {
            openedm::presets::applyPartial(presets[static_cast<size_t>(idx)], processor_.parameters);
        }
    };

    std::ranges::for_each(std::initializer_list<juce::Component*>{
                              &enabled_, &clockSync_, &timeSig_, &bars_, &mode_, &target_, &slopePresetBox_,
                              &tempo_, &slopeEditor_, &cutoffStart_, &cutoffEnd_, &cutoffSlope_,
                              &resStart_, &resEnd_, &resSlope_ },
                          [this](juce::Component* control) { addAndMakeVisible(control); });
}

void SweepPanel::paint(juce::Graphics& g) {
    drawPanel(g, getLocalBounds(), "Auto Sweep & Tempo Sync", theme::kAccentA);
    drawCaption(g, clockArea_, "Clock");
    drawCaption(g, editorArea_, "Slope editor (drag start / curve / end)");
    drawCaption(g, cutoffArea_, "Cutoff sweep");
    drawCaption(g, resArea_, "Resonance sweep");
}

void SweepPanel::resized() {
    auto bounds = getLocalBounds().reduced(12, 6);
    auto header = bounds.removeFromTop(28);
    slopePresetBox_.setBounds(header.removeFromRight(146).withSizeKeepingCentre(146, 24));
    header.removeFromRight(6);
    target_.setBounds(header.removeFromRight(92));
    header.removeFromRight(6);
    mode_.setBounds(header.removeFromRight(104));
    header.removeFromRight(6);
    bars_.setBounds(header.removeFromRight(80));
    header.removeFromRight(6);
    timeSig_.setBounds(header.removeFromRight(64));
    header.removeFromRight(6);
    clockSync_.setBounds(header.removeFromRight(82));
    header.removeFromRight(6);
    enabled_.setBounds(header.removeFromRight(92));

    bounds.removeFromTop(4);
    clockArea_ = bounds.removeFromLeft(78);
    bounds.removeFromLeft(kSectionGap);
    editorArea_ = bounds.removeFromLeft(380);
    bounds.removeFromLeft(kSectionGap);
    cutoffArea_ = bounds.removeFromLeft((bounds.getWidth() - kSectionGap) / 2);
    bounds.removeFromLeft(kSectionGap);
    resArea_ = bounds;

    tempo_.setBounds(clockArea_.withTrimmedTop(kCaptionHeight));
    slopeEditor_.setBounds(editorArea_.withTrimmedTop(kCaptionHeight));
    layoutRow(cutoffArea_.withTrimmedTop(kCaptionHeight), { &cutoffStart_, &cutoffEnd_, &cutoffSlope_ });
    layoutRow(resArea_.withTrimmedTop(kCaptionHeight), { &resStart_, &resEnd_, &resSlope_ });
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
    g.setFont(juce::FontOptions(10.0f));
    g.setColour(theme::kTextDim);
    g.drawText("v1.2.0", getLocalBounds().reduced(8, 2), juce::Justification::bottomRight);
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
