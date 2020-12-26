#include <dds/dym.hpp>

#include <dds/error/errors.hpp>
#include <dds/util/log.hpp>

#include <range/v3/algorithm/min_element.hpp>
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/iota.hpp>

#include <cassert>

using namespace dds;

thread_local dym_target* dym_target::_tls_current = nullptr;

std::size_t dds::lev_edit_distance(std::string_view a, std::string_view b) noexcept {
    const auto n_rows    = b.size() + 1;
    const auto n_columns = a.size() + 1;

    const auto empty_row = std::vector<std::size_t>(n_columns, 0);

    std::vector<std::vector<std::size_t>> matrix(n_rows, empty_row);

    auto row_iter = ranges::views::iota(1u, n_rows);
    auto col_iter = ranges::views::iota(1u, n_columns);

    for (auto n : col_iter) {
        matrix[0][n] = n;
    }
    for (auto n : row_iter) {
        matrix[n][0] = n;
    }

    auto prod = ranges::views::cartesian_product(row_iter, col_iter);

    for (auto [row, col] : prod) {
        auto cost = a[col - 1] == b[row - 1] ? 0 : 1;

        auto t1  = matrix[row - 1][col] + 1;
        auto t2  = matrix[row][col - 1] + 1;
        auto t3  = matrix[row - 1][col - 1] + cost;
        auto arr = std::array{t1, t2, t3};

        matrix[row][col] = *ranges::min_element(arr);
    }

    return matrix.back().back();
}

void dds::e_did_you_mean::log_as_error() const noexcept {
    if (value) {
        dds_log(error, "  (Did you mean \"{}\"?)", *value);
    }
}
