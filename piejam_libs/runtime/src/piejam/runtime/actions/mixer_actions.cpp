// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2021  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <piejam/runtime/actions/mixer_actions.h>

#include <piejam/runtime/state.h>

namespace piejam::runtime::actions
{

auto
add_mixer_channel::reduce(state const& st) const -> state
{
    auto new_st = st;

    runtime::add_mixer_channel(new_st, name);

    return new_st;
}

auto
delete_mixer_channel::reduce(state const& st) const -> state
{
    auto new_st = st;

    runtime::remove_mixer_channel(new_st, channel_id);

    return new_st;
}

auto
set_mixer_channel_name::reduce(state const& st) const -> state
{
    auto new_st = st;

    new_st.mixer_state.channels.update(channel_id, [this](mixer::channel& bus) {
        bus.name = name;
    });

    return new_st;
}

template <io_direction D>
auto
set_mixer_channel_route<D>::reduce(state const& st) const -> state
{
    auto new_st = st;

    new_st.mixer_state.channels.update(channel_id, [this](mixer::channel& bus) {
        (D == io_direction::input ? bus.in : bus.out) = route;
    });

    return new_st;
}

template auto
set_mixer_channel_route<io_direction::input>::reduce(state const&) const
        -> state;
template auto
set_mixer_channel_route<io_direction::output>::reduce(state const&) const
        -> state;

} // namespace piejam::runtime::actions
