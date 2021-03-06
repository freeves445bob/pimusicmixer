// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <piejam/audio/engine/graph.h>

#include <piejam/audio/engine/event_port.h>
#include <piejam/audio/engine/processor.h>
#include <piejam/functional/compare.h>

#include <boost/assert.hpp>

#include <algorithm>

namespace piejam::audio::engine
{

void
graph::add_wire(graph_endpoint const& src, graph_endpoint const& dst)
{
    BOOST_ASSERT(src.port < src.proc.get().num_outputs());
    BOOST_ASSERT(dst.port < dst.proc.get().num_inputs());

    BOOST_ASSERT_MSG(
            std::ranges::none_of(
                    m_wires,
                    [&dst](auto const& wire) { return wire.second == dst; }),
            "destination endpoint already added, missing a mixer?");

    m_wires.emplace(src, dst);
}

void
graph::remove_wire(graph_endpoint const& src, graph_endpoint const& dst)
{
    auto from_source = m_wires.equal_range(src);
    auto it = std::ranges::find_if(
            from_source.first,
            from_source.second,
            equal_to(dst),
            &wires_t::value_type::second);
    BOOST_ASSERT(it != m_wires.end());
    m_wires.erase(it);
}

void
graph::add_event_wire(const graph_endpoint& src, const graph_endpoint& dst)
{
    BOOST_ASSERT(src.port < src.proc.get().event_outputs().size());
    BOOST_ASSERT(dst.port < dst.proc.get().event_inputs().size());
    BOOST_ASSERT(
            src.proc.get().event_outputs()[src.port].type() ==
            dst.proc.get().event_inputs()[dst.port].type());

    BOOST_ASSERT_MSG(
            std::ranges::none_of(
                    m_event_wires,
                    [&dst](auto const& wire) { return wire.second == dst; }),
            "destination endpoint already added, missing an event mixer?");

    m_event_wires.emplace(src, dst);
}

void
graph::remove_event_wire(wires_t::const_iterator const& it)
{
    m_event_wires.erase(it);
}

} // namespace piejam::audio::engine
