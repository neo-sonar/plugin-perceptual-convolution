#include "overlap_add.hpp"
#include "overlap_save.hpp"

#include <neo/algorithm/allclose.hpp>
#include <neo/algorithm/rms_error.hpp>
#include <neo/fft/convolution/uniform_partition.hpp>
#include <neo/testing/testing.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_get_random_seed.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <tuple>

TEMPLATE_TEST_CASE("neo/fft/convolution: overlap_add", "", float, double, long double)
{
    using Float   = TestType;
    using Overlap = neo::fft::overlap_add<Float>;

    auto num_overlaps = [](std::size_t block_size, std::size_t filter_size) {
        auto overlap = Overlap{block_size, filter_size};
        return overlap.num_overlaps();
    };

    REQUIRE(num_overlaps(128, 127) == 2);
    REQUIRE(num_overlaps(128, 128) == 2);
    REQUIRE(num_overlaps(128, 129) == 2);
    REQUIRE(num_overlaps(128, 130) == 3);

    REQUIRE(num_overlaps(128, 255) == 3);
    REQUIRE(num_overlaps(128, 256) == 3);
    REQUIRE(num_overlaps(128, 257) == 3);
    REQUIRE(num_overlaps(128, 258) == 4);

    REQUIRE(num_overlaps(128, 511) == 5);
    REQUIRE(num_overlaps(128, 512) == 5);
    REQUIRE(num_overlaps(128, 513) == 5);
    REQUIRE(num_overlaps(128, 514) == 6);
}

TEMPLATE_PRODUCT_TEST_CASE(
    "neo/fft/convolution: overlap",
    "",
    (neo::fft::overlap_add, neo::fft::overlap_save),
    (float, double, long double)
)
{
    using Overlap = TestType;
    using Float   = typename Overlap::real_type;

    auto const block_size  = GENERATE(as<std::size_t>{}, 128, 256, 512);
    auto const filter_size = GENERATE(as<std::size_t>{}, 8, 9, 10, 11, 13, 17, 127, 128, 129, 130, 256, 512, 1024);
    auto const signal      = neo::generate_noise_signal<Float>(block_size * 8UL, Catch::getSeed());

    auto overlap = Overlap{block_size, filter_size};
    REQUIRE(overlap.block_size() == block_size);
    REQUIRE(overlap.filter_size() == filter_size);
    REQUIRE(overlap.transform_size() >= block_size + filter_size - 1);

    auto output = signal;
    auto blocks = stdex::mdspan{output.data(), stdex::extents{output.size()}};

    for (auto i{0U}; i < output.size(); i += block_size) {
        auto block = stdex::submdspan(blocks, std::tuple{i, i + block_size});
        overlap(block, [=](neo::inout_vector auto io) {
            REQUIRE(io.extent(0) == overlap.transform_size() / 2UL + 1UL);
        });
    }

    auto const sig = signal.to_mdspan();
    auto const out = output.to_mdspan();

    REQUIRE(neo::allclose(out, sig));
    REQUIRE_THAT(neo::rms_error(sig, out), Catch::Matchers::WithinAbs(0.0, 0.00001));

    for (auto i{0ULL}; i < output.size(); ++i) {
        CAPTURE(i);
        REQUIRE_THAT(out[i], Catch::Matchers::WithinAbs(sig[i], 0.00001));
    }
}
