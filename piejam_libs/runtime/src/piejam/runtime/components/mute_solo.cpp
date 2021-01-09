// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <piejam/runtime/components/mute_solo.h>

#include <piejam/audio/components/amplifier.h>
#include <piejam/audio/engine/component.h>
#include <piejam/audio/engine/graph.h>
#include <piejam/audio/engine/graph_endpoint.h>
#include <piejam/audio/engine/processor.h>
#include <piejam/entity_id.h>
#include <piejam/runtime/processors/mute_solo_processor.h>

#include <array>

namespace piejam::runtime::components
{

namespace
{

class mute_solo final : public audio::engine::component
{
public:
    mute_solo(mixer::bus_id solo_id, std::string_view const& name)
        : m_mute_solo_proc(processors::make_mute_solo_processor(solo_id, name))
        , m_mute_solo_amp(audio::components::make_stereo_amplifier("mute_amp"))
    {
    }

    auto inputs() const -> endpoints override { return m_inputs; }
    auto outputs() const -> endpoints override { return m_outputs; }

    auto event_inputs() const -> endpoints override { return m_event_inputs; }
    auto event_outputs() const -> endpoints override { return {}; }

    void connect(audio::engine::graph& g) const override
    {
        m_mute_solo_amp->connect(g);

        g.add_event_wire(
                {*m_mute_solo_proc, 0},
                m_mute_solo_amp->event_inputs()[0]);
    }

private:
    std::unique_ptr<audio::engine::processor> m_mute_solo_proc;
    std::unique_ptr<audio::engine::component> m_mute_solo_amp;

    std::array<audio::engine::graph_endpoint, 2> m_inputs{
            {m_mute_solo_amp->inputs()[0], m_mute_solo_amp->inputs()[1]}};
    std::array<audio::engine::graph_endpoint, 2> m_outputs{
            {m_mute_solo_amp->outputs()[0], m_mute_solo_amp->outputs()[1]}};
    std::array<audio::engine::graph_endpoint, 2> m_event_inputs{
            {{*m_mute_solo_proc, 0}, {*m_mute_solo_proc, 1}}};
};

} // namespace

auto
make_mute_solo(mixer::bus_id const solo_id, std::string_view const& name)
        -> std::unique_ptr<audio::engine::component>
{
    return std::make_unique<mute_solo>(solo_id, name);
}

} // namespace piejam::runtime::components
