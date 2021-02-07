// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2021  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <piejam/runtime/midi_control_middleware.h>

#include <piejam/algorithm/contains.h>
#include <piejam/algorithm/for_each_visit.h>
#include <piejam/midi/device_update.h>
#include <piejam/runtime/actions/activate_midi_device.h>
#include <piejam/runtime/actions/refresh_midi_devices.h>
#include <piejam/runtime/state.h>

#include <boost/assert.hpp>
#include <boost/range/algorithm_ext/erase.hpp>

namespace piejam::runtime
{

namespace
{

struct update_midi_devices final
    : ui::cloneable_action<update_midi_devices, action>
{
    std::vector<midi::device_update> updates;

    struct midi_device_update_handler
    {
        midi_devices_t& midi_devices;
        std::vector<midi::device_id_t>& midi_inputs;

        void operator()(midi::device_added const& op) const
        {
            midi_devices.emplace(
                    op.device_id,
                    midi_device_config{.name = op.name, .enabled = false});
            midi_inputs.emplace_back(op.device_id);
        }

        void operator()(midi::device_removed const& op) const
        {
            midi_devices.erase(op.device_id);
            boost::remove_erase(midi_inputs, op.device_id);
        }
    };

    auto reduce(state const& st) const -> state override
    {
        auto new_st = st;

        auto midi_devices = *new_st.midi_devices;
        auto midi_inputs = *new_st.midi_inputs;

        for (auto const& update : updates)
        {
            std::visit(
                    midi_device_update_handler{
                            .midi_devices = midi_devices,
                            .midi_inputs = midi_inputs},
                    update);
        }

        new_st.midi_devices = std::move(midi_devices);
        new_st.midi_inputs = std::move(midi_inputs);

        return new_st;
    }
};

auto
no_device_updates() -> midi_control_middleware::device_updates_f
{
    return []() { return std::vector<midi::device_update>(); };
}

} // namespace

midi_control_middleware::midi_control_middleware(
        middleware_functors mw_fs,
        device_updates_f device_updates)
    : middleware_functors(std::move(mw_fs))
    , m_device_updates(
              device_updates ? std::move(device_updates) : no_device_updates())
{
}

void
midi_control_middleware::operator()(action const& action)
{
    if (auto const* const a =
                dynamic_cast<actions::midi_control_action const*>(&action))
    {
        auto v = ui::make_action_visitor<actions::midi_control_action_visitor>(
                [this](auto const& a) { process_midi_control_action(a); });

        a->visit(v);
    }
    else
    {
        next(action);
    }
}

void
midi_control_middleware::process_device_update(midi::device_added const& up)
{
    if (auto it = std::ranges::find(m_enabled_devices, up.name);
        it != m_enabled_devices.end())
    {
        actions::activate_midi_device action;
        action.device_id = up.device_id;
        dispatch(action);

        m_enabled_devices.erase(it);
        BOOST_ASSERT(!algorithm::contains(m_enabled_devices, up.name));
    }
}

void
midi_control_middleware::process_device_update(midi::device_removed const& up)
{
    auto const& st = get_state();
    if (auto it = st.midi_devices->find(up.device_id);
        it != st.midi_devices->end() && it->second.enabled)
    {
        BOOST_ASSERT(!algorithm::contains(m_enabled_devices, *it->second.name));
        m_enabled_devices.emplace_back(it->second.name);
    }
}

template <>
void
midi_control_middleware::process_midi_control_action(
        actions::refresh_midi_devices const&)
{
    update_midi_devices next_action;
    next_action.updates = m_device_updates();

    if (!next_action.updates.empty())
    {
        algorithm::for_each_visit(next_action.updates, [this](auto const& up) {
            process_device_update(up);
        });

        next(next_action);
    }
}

} // namespace piejam::runtime