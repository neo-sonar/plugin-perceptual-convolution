#include "upols_convolver.hpp"

namespace neo::fft
{

static auto shift_left(Kokkos::mdspan<float, Kokkos::dextents<size_t, 2>> buffer, std::ptrdiff_t shift) -> void
{
    for (auto ch{0UL}; ch < buffer.extent(0); ++ch)
    {
        auto channel = std::span{std::addressof(buffer(ch, 0)), buffer.extent(1)};
        std::shift_left(channel.begin(), channel.end(), shift);
    }
}

static auto copy(Kokkos::mdspan<float const, Kokkos::dextents<size_t, 2>> src,
                 Kokkos::mdspan<float, Kokkos::dextents<size_t, 2>> dest) -> void
{
    assert(src.extent(0) == dest.extent(0));
    assert(src.extent(1) * 2 == dest.extent(1));

    auto const numChannels = src.extent(0);
    auto const numSamples  = src.extent(1);

    for (auto ch{0UL}; ch < numChannels; ++ch)
    {
        auto source      = std::span{std::addressof(src(ch, 0)), numSamples};
        auto destination = std::span{std::addressof(dest(ch, numSamples)), numSamples};
        std::copy(source.begin(), source.end(), destination.begin());
    }
}

static auto multiply_and_accumulate_row(std::span<std::complex<float> const> lhs,
                                        std::span<std::complex<float> const> rhs,
                                        std::span<std::complex<float>> accumulator)
{
    auto* NEO_FFT_RESTRICT acc         = accumulator.data();
    auto const* NEO_FFT_RESTRICT left  = lhs.data();
    auto const* NEO_FFT_RESTRICT right = rhs.data();
    for (decltype(lhs.size()) i{0}; i < lhs.size(); ++i) { acc[i] += left[i] * right[i]; }
}

static auto multiply_and_accumulate(Kokkos::mdspan<std::complex<float> const, Kokkos::dextents<std::size_t, 2>> lhs,
                                    Kokkos::mdspan<std::complex<float> const, Kokkos::dextents<std::size_t, 2>> rhs,
                                    std::span<std::complex<float>> accumulator, std::size_t shift)
{
    assert(lhs.extents() == rhs.extents());
    assert(lhs.extent(1) > 0);
    assert(shift < lhs.extent(0));

    auto getRow = [](auto const& matrix, size_t row) {
        return std::span<std::complex<float> const>{std::addressof(matrix(row, 0)), matrix.extent(1)};
    };

    for (auto row{0U}; row <= shift; ++row)
    {
        multiply_and_accumulate_row(getRow(lhs, row), getRow(rhs, shift - row), accumulator);
    }

    for (auto row{shift + 1}; row < lhs.extent(0); ++row)
    {
        auto const offset    = row - shift;
        auto const offsetRow = lhs.extent(0) - offset;
        multiply_and_accumulate_row(getRow(lhs, row), getRow(rhs, offsetRow), accumulator);
    }
}

static auto multiply_and_accumulate(Kokkos::mdspan<std::complex<float> const, Kokkos::dextents<std::size_t, 3>> lhs,
                                    Kokkos::mdspan<std::complex<float> const, Kokkos::dextents<std::size_t, 3>> rhs,
                                    Kokkos::mdspan<std::complex<float>, Kokkos::dextents<std::size_t, 2>> accumulator,
                                    std::size_t shift)
{
    multiply_and_accumulate(KokkosEx::submdspan(lhs, 0, Kokkos::full_extent, Kokkos::full_extent),
                            KokkosEx::submdspan(rhs, 0, Kokkos::full_extent, Kokkos::full_extent),
                            std::span{std::addressof(accumulator(0, 0)), accumulator.extent(0)}, shift);

    multiply_and_accumulate(KokkosEx::submdspan(lhs, 1, Kokkos::full_extent, Kokkos::full_extent),
                            KokkosEx::submdspan(rhs, 1, Kokkos::full_extent, Kokkos::full_extent),
                            std::span{std::addressof(accumulator(1, 0)), accumulator.extent(0)}, shift);
}

auto upols_convolver::filter(KokkosEx::mdspan<std::complex<float> const, Kokkos::dextents<size_t, 2>> filter) -> void
{
    auto const K = std::bit_ceil((filter.extent(1) - 1U) * 2U);

    _fdl    = KokkosEx::mdarray<std::complex<float>, Kokkos::dextents<size_t, 2>>{filter.extents()};
    _filter = filter;

    _rfft = std::make_unique<rfft_radix2_plan<float>>(ilog2(K));
    _window.resize(K);
    _rfftBuf.resize(_window.size());
    _irfftBuf.resize(_window.size());
    _accumulator.resize(_filter.extent(1));

    _fdlIndex = 0;
}

auto upols_convolver::operator()(std::span<float> block) -> void
{
    assert(block.size() * 2U == _window.size());

    auto const blockSize = std::ssize(block);

    // Time domain input buffer
    std::shift_left(_window.begin(), _window.end(), blockSize);
    std::copy(block.begin(), block.end(), std::prev(_window.end(), blockSize));

    // 2B-point R2C-FFT
    std::invoke(*_rfft, _window, _rfftBuf);

    // Copy to FDL
    for (auto i{0U}; i < _fdl.extent(1); ++i) { _fdl(_fdlIndex, i) = _rfftBuf[i] / float(_rfft->size()); }

    // DFT-spectrum additions
    std::fill(_accumulator.begin(), _accumulator.end(), 0.0F);
    multiply_and_accumulate(_fdl, _filter, _accumulator, _fdlIndex);

    // All contents (DFT spectra) in the FDL are shifted up by one slot.
    ++_fdlIndex;
    if (_fdlIndex == _fdl.extent(0)) { _fdlIndex = 0; }

    // 2B-point C2R-IFFT
    std::invoke(*_rfft, _accumulator, _irfftBuf);

    // Copy blockSize samples to output
    std::copy(std::prev(_irfftBuf.end(), blockSize), _irfftBuf.end(), block.begin());
}

auto stereo_upols_convolver::filter(KokkosEx::mdspan<std::complex<float> const, Kokkos::dextents<size_t, 3>> filter)
    -> void
{
    auto const numChannels = filter.extent(0);
    auto const numBins     = filter.extent(2);
    auto const fftSize     = std::bit_ceil((numBins - 1U) * 2U);

    _fdlIndex    = 0;
    _filter      = filter;
    _fdl         = KokkosEx::mdarray<std::complex<float>, Kokkos::dextents<size_t, 3>>{filter.extents()};
    _fft         = std::make_unique<rfft_radix2_plan<float>>(ilog2(fftSize));
    _window      = KokkosEx::mdarray<float, Kokkos::dextents<size_t, 2>>(numChannels, fftSize);
    _accumulator = KokkosEx::mdarray<std::complex<float>, Kokkos::dextents<size_t, 2>>{numChannels, numBins};

    _rfftBuf.resize(fftSize);
    _irfftBuf.resize(fftSize);
}

auto stereo_upols_convolver::operator()(KokkosEx::mdspan<float, Kokkos::dextents<size_t, 2>> block) -> void
{
    auto const blockSize   = static_cast<ptrdiff_t>(block.extent(1));
    auto const numChannels = block.extent(0);
    auto const numSegments = _fdl.extent(1);
    auto const numBins     = _fdl.extent(2);

    assert(numChannels == 2);
    assert(block.extent(0) == _window.extent(0));
    assert(block.extent(1) * 2U == _window.extent(1));

    // Time domain input buffer
    shift_left(_window, blockSize);
    copy(block, _window);

    // 2B-point R2C-FFT
    // Copy to FDL
    for (auto ch{0U}; ch < numChannels; ++ch)
    {
        std::invoke(*_fft, std::span{std::addressof(_window(ch, 0)), _window.extent(1)}, _rfftBuf);
        for (auto i{0U}; i < numBins; ++i) { _fdl(ch, _fdlIndex, i) = _rfftBuf[i] / float(_fft->size()); }
    }

    // // DFT-spectrum additions
    std::fill(_accumulator.data(), _accumulator.data() + _accumulator.size(), 0.0F);
    multiply_and_accumulate(_fdl, _filter, _accumulator, _fdlIndex);

    // All contents (DFT spectra) in the FDL are shifted up by one slot.
    ++_fdlIndex;
    if (_fdlIndex == numSegments) { _fdlIndex = 0; }

    // 2B-point C2R-IFFT
    // Copy blockSize samples to output
    for (auto ch{0U}; ch < numChannels; ++ch)
    {
        auto channel = std::span{std::addressof(block(ch, 0)), block.extent(1)};
        std::invoke(*_fft, std::span{std::addressof(_accumulator(ch, 0)), _accumulator.extent(1)}, _irfftBuf);
        std::copy(std::prev(_irfftBuf.end(), blockSize), _irfftBuf.end(), channel.begin());
    }
}

}  // namespace neo::fft
