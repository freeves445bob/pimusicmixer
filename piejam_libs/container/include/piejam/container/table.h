// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <piejam/container/allocator_alignment.h>
#include <piejam/range/table_view.h>

#include <boost/align/align_up.hpp>

#include <vector>

namespace piejam::container
{

namespace detail
{

template <class T, class Allocator>
constexpr auto
table_padding(std::size_t const num_columns) -> std::size_t
{
    return (boost::alignment::align_up(
                    num_columns * sizeof(T),
                    allocator_alignment_v<Allocator>) /
            sizeof(T)) -
           num_columns;
}

} // namespace detail

template <class T, class Allocator = std::allocator<T>>
class table
{
public:
    static_assert(allocator_alignment_v<Allocator> % alignof(T) == 0);

    constexpr table() noexcept = default;

    constexpr table(std::size_t const num_rows, std::size_t const num_columns)
        : m_num_rows(num_rows)
        , m_num_columns(num_columns)
        , m_padding(detail::table_padding<T, Allocator>(num_columns))
        , m_data(m_num_rows * (m_num_columns + m_padding))
    {
    }

    constexpr auto num_rows() const noexcept -> std::size_t
    {
        return m_num_rows;
    }

    constexpr auto num_columns() const noexcept -> std::size_t
    {
        return m_num_columns;
    }

    constexpr auto data() const noexcept { return m_data.data(); }

    constexpr auto rows() noexcept
    {
        return range::table_view<T>(
                m_data.data(),
                m_num_rows,
                m_num_columns,
                m_num_columns + m_padding,
                1);
    }

    constexpr auto rows() const noexcept
    {
        return range::table_view<T const>(
                m_data.data(),
                m_num_rows,
                m_num_columns,
                m_num_columns + m_padding,
                1);
    }

    constexpr auto columns() noexcept
    {
        return range::table_view<T>(
                m_data.data(),
                m_num_columns,
                m_num_rows,
                1,
                m_num_columns + m_padding);
    }

    constexpr auto columns() const noexcept
    {
        return range::table_view<T const>(
                m_data.data(),
                m_num_columns,
                m_num_rows,
                1,
                m_num_columns + m_padding);
    }

private:
    std::size_t m_num_rows{};
    std::size_t m_num_columns{};
    std::size_t m_padding{};
    std::vector<T, Allocator> m_data;
};

} // namespace piejam::container
