#include "overlap_save.hpp"

#include <neo/fft/algorithm/allclose.hpp>
#include <neo/fft/convolution/uniform_partition.hpp>
#include <neo/fft/testing/testing.hpp>

#include <catch2/catch_get_random_seed.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <span>

TEMPLATE_TEST_CASE("neo/fft/convolution: overlap_save", "", float, double)
{
    using Float = TestType;

    auto const blockSize = GENERATE(as<std::size_t>{}, 128, 256, 512);

    auto const signal     = neo::fft::generate_noise_signal<Float>(blockSize * 15UL, Catch::getSeed());
    auto const partitions = neo::fft::generate_identity_impulse<Float>(blockSize, 8UL);

    auto ols = neo::fft::overlap_save<Float>{blockSize};

    auto output = signal;
    auto blocks = Kokkos::mdspan{output.data(), Kokkos::extents{output.size()}};

    for (auto i{0U}; i < output.size(); i += blockSize) {
        auto block = KokkosEx::submdspan(blocks, std::tuple{i, i + blockSize});
        ols(block, [](auto) {});
    }

    for (auto i{0ULL}; i < output.size(); ++i) {
        CAPTURE(i);
        REQUIRE_THAT(output[i], Catch::Matchers::WithinAbs(signal[i], 0.00001));
    }
}
