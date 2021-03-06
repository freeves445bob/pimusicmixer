// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <piejam/runtime/actions/apply_app_config.h>

#include <piejam/runtime/state.h>

namespace piejam::runtime::actions
{

static void
update_channel(std::size_t& cur_ch, std::size_t const num_chs)
{
    if (cur_ch >= num_chs)
        cur_ch = npos;
}

template <io_direction D>
static auto
apply_bus_configs(
        state& st,
        std::vector<persistence::bus_config> const& configs,
        std::size_t const num_ch)
{
    BOOST_ASSERT_MSG(
            D == io_direction::input ? st.device_io_state.inputs->empty()
                                     : st.device_io_state.outputs->empty(),
            "configs should be cleared before applying");
    for (auto const& bus_conf : configs)
    {
        auto chs = bus_conf.channels;
        update_channel(chs.left, num_ch);
        update_channel(chs.right, num_ch);

        auto const type = D == io_direction::output ? audio::bus_type::stereo
                                                    : bus_conf.bus_type;

        add_device_bus(st, bus_conf.name, D, type, chs);
    }
}

auto
apply_app_config::reduce(state const& st) const -> state
{
    auto new_st = st;

    apply_bus_configs<io_direction::input>(
            new_st,
            conf.input_bus_config,
            new_st.input.hw_params->num_channels);

    apply_bus_configs<io_direction::output>(
            new_st,
            conf.output_bus_config,
            new_st.output.hw_params->num_channels);

    return new_st;
}

} // namespace piejam::runtime::actions
