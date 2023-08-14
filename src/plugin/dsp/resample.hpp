#pragma once

#include <neo/container/mdspan.hpp>

#include <juce_audio_formats/juce_audio_formats.h>

namespace neo {

auto resample(juce::AudioBuffer<float> const& buf, double srcSampleRate, double destSampleRate)
    -> juce::AudioBuffer<float>;

[[nodiscard]] auto loadAndResample(juce::AudioFormatManager& formats, juce::File const& file, double sampleRate)
    -> juce::AudioBuffer<float>;

[[nodiscard]] auto to_mdarray(juce::AudioBuffer<float> const& buffer)
    -> stdex::mdarray<float, stdex::dextents<std::size_t, 2>>;

}  // namespace neo
