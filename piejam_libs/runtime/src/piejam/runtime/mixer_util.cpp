// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2021  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <piejam/runtime/mixer_util.h>

#include <piejam/entity_id.h>
#include <piejam/functional/overload.h>
#include <piejam/runtime/device_io.h>
#include <piejam/runtime/mixer.h>

namespace piejam::runtime
{

auto
mixer_channel_input_type(
        mixer::channels_t const& channels,
        mixer::channel_id const channel_id,
        device_io::buses_t const& device_buses) -> audio::bus_type
{
    mixer::channel const* const bus = channels.find(channel_id);
    if (!bus)
        return audio::bus_type::stereo;

    return std::visit(
            overload{
                    [&device_buses](device_io::bus_id const device_bus_id) {
                        return device_buses[device_bus_id].bus_type;
                    },
                    [](auto&&) { return audio::bus_type::stereo; }},
            bus->in);
}

} // namespace piejam::runtime
