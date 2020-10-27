// PieJam - An audio mixer for Raspberry Pi.
//
// Copyright (C) 2020  Dimitrij Kotrev
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <piejam/audio/engine/input_processor.h>

#include <piejam/audio/engine/process_context.h>

#include <boost/core/ignore_unused.hpp>

#include <algorithm>
#include <cassert>

namespace piejam::audio::engine
{

input_processor::input_processor(std::size_t const num_outputs)
    : m_num_outputs(num_outputs)
{
    assert(m_num_outputs > 0);
}

void
input_processor::process(process_context const& ctx)
{
    assert(m_engine_input.major_size() == m_num_outputs);

    for (std::size_t ch = 0; ch < m_num_outputs; ++ch)
    {
        assert(m_engine_input.minor_size() == ctx.outputs[ch].size());
        assert(m_engine_input.minor_step() == 1);
        ctx.results[ch] = {
                m_engine_input[ch].data(),
                m_engine_input.minor_size()};
    }
}

} // namespace piejam::audio::engine