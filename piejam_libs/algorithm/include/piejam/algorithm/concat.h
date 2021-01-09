// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <iterator>

namespace piejam::algorithm
{

template <class Result, class... Range>
auto
concat(Range&&... rng) -> Result
{
    Result result;
    result.reserve((std::ranges::size(rng) + ...));
    (std::ranges::copy(rng, std::back_inserter(result)), ...);
    return result;
}

} // namespace piejam::algorithm
