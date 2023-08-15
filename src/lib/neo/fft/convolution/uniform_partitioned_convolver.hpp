#pragma once

#include <neo/algorithm/copy.hpp>
#include <neo/algorithm/fill.hpp>
#include <neo/algorithm/multiply_sum_columns.hpp>
#include <neo/container/mdspan.hpp>
#include <neo/container/sparse_matrix.hpp>
#include <neo/fft/convolution/overlap_add.hpp>
#include <neo/fft/convolution/overlap_save.hpp>
#include <neo/math/complex.hpp>

namespace neo::fft {

template<std::floating_point Float, typename Overlap = overlap_save<Float>>
struct uniform_partitioned_convolver
{
    uniform_partitioned_convolver() = default;

    auto filter(in_matrix auto filter) -> void;
    auto operator()(in_vector auto block) -> void;

private:
    Overlap _overlap{1, 1};

    stdex::mdspan<std::complex<Float> const, stdex::dextents<size_t, 2>> _filter;
    stdex::mdarray<std::complex<Float>, stdex::dextents<size_t, 1>> _accumulator;
    stdex::mdarray<std::complex<Float>, stdex::dextents<size_t, 2>> _fdl;
};

template<std::floating_point Float, typename Overlap>
auto uniform_partitioned_convolver<Float, Overlap>::filter(in_matrix auto filter) -> void
{
    _overlap     = Overlap{filter.extent(1) - 1, filter.extent(1) - 1};
    _fdl         = stdex::mdarray<std::complex<Float>, stdex::dextents<size_t, 2>>{filter.extents()};
    _accumulator = stdex::mdarray<std::complex<Float>, stdex::dextents<size_t, 1>>{filter.extent(1)};
    _filter      = filter;
}

template<std::floating_point Float, typename Overlap>
auto uniform_partitioned_convolver<Float, Overlap>::operator()(in_vector auto block) -> void
{
    _overlap(block, [this](inout_vector auto inout) {
        auto const fdl         = _fdl.to_mdspan();
        auto const accumulator = _accumulator.to_mdspan();

        NEO_EXPECTS(inout.extent(0) == fdl.extent(1));
        NEO_EXPECTS(inout.extent(0) == accumulator.extent(0));

        shift_rows_up(fdl);
        copy(inout, stdex::submdspan(fdl, 0, stdex::full_extent));

        fill(accumulator, std::complex<Float>{});
        multiply_sum_columns(fdl, _filter, accumulator);

        copy(accumulator, inout);
    });
}

template<std::floating_point Float>
using upols_convolver = uniform_partitioned_convolver<Float, overlap_save<Float>>;

template<std::floating_point Float>
using upola_convolver = uniform_partitioned_convolver<Float, overlap_add<Float>>;

}  // namespace neo::fft
