// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <piejam/runtime/state.h>

#include <piejam/algorithm/contains.h>
#include <piejam/audio/ladspa/port_descriptor.h>
#include <piejam/functional/overload.h>
#include <piejam/indexed_access.h>
#include <piejam/runtime/fx/gain.h>
#include <piejam/runtime/fx/ladspa.h>
#include <piejam/runtime/fx/parameter.h>
#include <piejam/runtime/parameter/float_normalize.h>
#include <piejam/runtime/parameter_maps_access.h>
#include <piejam/tuple_element_compare.h>

#include <boost/mp11/algorithm.hpp>
#include <boost/range/algorithm_ext/erase.hpp>

#include <algorithm>

namespace piejam::runtime
{

namespace
{

struct volume_low_db_interval
{
    static constexpr float min = -60.f;
    static constexpr float max = -12.f;
};

struct volume_high_db_interval
{
    static constexpr float min = -12.f;
    static constexpr float max = 12.f;
};

constexpr auto
to_normalized_volume(float_parameter const& p, float const value)
{
    if (value == 0.f)
        return 0.f;
    else if (0.f < value && value < 0.0625f)
        return parameter::to_normalized_db<volume_low_db_interval>(p, value) /
               4.f;
    else
        return (parameter::to_normalized_db<volume_high_db_interval>(p, value) *
                0.75f) +
               0.25f;
}

constexpr auto
from_normalized_volume(float_parameter const& p, float const norm_value)
        -> float
{
    if (norm_value == 0.f)
        return 0.f;
    else if (0.f < norm_value && norm_value < 0.25f)
        return parameter::from_normalized_db<volume_low_db_interval>(
                p,
                4.f * norm_value);
    else
        return parameter::from_normalized_db<volume_high_db_interval>(
                p,
                (norm_value - 0.25f) / 0.75f);
}

} // namespace

auto
make_initial_state() -> state
{
    state st;
    st.mixer_state.main = add_mixer_channel(st, "Main");
    // main doesn't belong into inputs
    remove_erase(st.mixer_state.inputs, st.mixer_state.main);
    // reset the output to default back again
    st.mixer_state.channels.update(
            st.mixer_state.main,
            [](mixer::channel& bus) { bus.out = nullptr; });
    return st;
}

template <class Vector>
static auto
set_intersection(Vector const& in, Vector const& out)
{
    Vector result;
    std::ranges::set_intersection(in, out, std::back_inserter(result));
    return result;
}

auto
samplerates(
        box<audio::pcm_hw_params> input_hw_params,
        box<audio::pcm_hw_params> output_hw_params) -> audio::samplerates_t
{
    return set_intersection(
            input_hw_params->samplerates,
            output_hw_params->samplerates);
}

auto
samplerates_from_state(state const& state) -> audio::samplerates_t
{
    return samplerates(state.input.hw_params, state.output.hw_params);
}

auto
period_sizes(
        box<audio::pcm_hw_params> input_hw_params,
        box<audio::pcm_hw_params> output_hw_params) -> audio::period_sizes_t
{
    return set_intersection(
            input_hw_params->period_sizes,
            output_hw_params->period_sizes);
}

auto
period_sizes_from_state(state const& state) -> audio::period_sizes_t
{
    return period_sizes(state.input.hw_params, state.output.hw_params);
}

auto
period_counts(
        box<audio::pcm_hw_params> input_hw_params,
        box<audio::pcm_hw_params> output_hw_params) -> audio::period_counts_t
{
    return set_intersection(
            input_hw_params->period_counts,
            output_hw_params->period_counts);
}

auto
period_counts_from_state(state const& state) -> audio::period_counts_t
{
    return period_counts(state.input.hw_params, state.output.hw_params);
}

static auto
make_fx_gain(
        fx::modules_t& fx_modules,
        fx::parameters_t& fx_params,
        parameter_maps& params)
{
    return fx_modules.add(fx::make_gain_module(fx_params, params));
}

static void
apply_parameter_values(
        std::vector<fx::parameter_value_assignment> const& values,
        fx::module const& fx_mod,
        parameter_maps& params)
{
    for (auto&& [key, value] : values)
    {
        if (auto it = fx_mod.parameters->find(key);
            it != fx_mod.parameters->end())
        {
            auto const& fx_param_id = it->second;
            std::visit(
                    overload{
                            [&params](float_parameter_id id, float v) {
                                set_parameter_value(params, id, v);
                            },
                            [&params](int_parameter_id id, int v) {
                                set_parameter_value(params, id, v);
                            },
                            [&params](bool_parameter_id id, bool v) {
                                set_parameter_value(params, id, v);
                            },
                            [](auto&&, auto&&) { BOOST_ASSERT(false); }},
                    fx_param_id,
                    value);
        }
    }
}

static void
update_midi_assignments(
        box<midi_assignments_map>& midi_assigns,
        midi_assignments_map const& new_assignments)
{
    midi_assigns.update([&](midi_assignments_map& midi_assigns) {
        for (auto&& [id, ass] : new_assignments)
        {
            std::erase_if(
                    midi_assigns,
                    tuple::element<1>.equal_to(std::cref(ass)));

            midi_assigns.insert_or_assign(id, ass);
        }
    });
}

static void
apply_fx_midi_assignments(
        std::vector<fx::parameter_midi_assignment> const& fx_midi_assigns,
        fx::module const& fx_mod,
        box<midi_assignments_map>& midi_assigns)
{
    midi_assignments_map new_assignments;
    for (auto&& [key, value] : fx_midi_assigns)
    {
        if (auto it = fx_mod.parameters->find(key);
            it != fx_mod.parameters->end())
        {
            new_assignments.emplace(it->second, value);
        }
    }

    update_midi_assignments(midi_assigns, new_assignments);
}

auto
insert_internal_fx_module(
        state& st,
        mixer::channel_id const bus_id,
        std::size_t const position,
        fx::internal const fx_type,
        std::vector<fx::parameter_value_assignment> const& initial_assignments,
        std::vector<fx::parameter_midi_assignment> const& midi_assigns)
        -> fx::module_id
{
    BOOST_ASSERT(bus_id != mixer::channel_id{});

    fx::chain_t fx_chain = st.mixer_state.channels[bus_id].fx_chain;
    auto const insert_pos = std::min(position, fx_chain.size());

    fx::parameters_t fx_params = st.fx_parameters;

    fx::module_id fx_mod_id;
    switch (fx_type)
    {
        case fx::internal::gain:
            fx_mod_id = make_fx_gain(st.fx_modules, fx_params, st.params);
            fx_chain.emplace(
                    std::next(fx_chain.begin(), insert_pos),
                    fx_mod_id);
            break;
    }

    st.fx_parameters = std::move(fx_params);

    auto const& fx_mod = st.fx_modules[fx_chain[insert_pos]];

    apply_parameter_values(initial_assignments, fx_mod, st.params);
    apply_fx_midi_assignments(midi_assigns, fx_mod, st.midi_assignments);

    st.mixer_state.channels.update(bus_id, [&](mixer::channel& bus) {
        bus.fx_chain = std::move(fx_chain);
    });

    return fx_mod_id;
}

void
insert_ladspa_fx_module(
        state& st,
        mixer::channel_id const bus_id,
        std::size_t const position,
        fx::ladspa_instance_id const instance_id,
        audio::ladspa::plugin_descriptor const& plugin_desc,
        std::span<audio::ladspa::port_descriptor const> const& control_inputs,
        std::vector<fx::parameter_value_assignment> const& initial_values,
        std::vector<fx::parameter_midi_assignment> const& midi_assigns)
{
    BOOST_ASSERT(bus_id != mixer::channel_id{});

    fx::chain_t fx_chain = st.mixer_state.channels[bus_id].fx_chain;
    auto const insert_pos = std::min(position, fx_chain.size());

    fx::parameters_t fx_params = st.fx_parameters;

    fx_chain.emplace(
            std::next(fx_chain.begin(), insert_pos),
            st.fx_modules.add(fx::make_ladspa_module(
                    instance_id,
                    plugin_desc.name,
                    control_inputs,
                    fx_params,
                    st.params)));

    st.fx_parameters = std::move(fx_params);

    auto const& fx_mod = st.fx_modules[fx_chain[insert_pos]];
    apply_parameter_values(initial_values, fx_mod, st.params);
    apply_fx_midi_assignments(midi_assigns, fx_mod, st.midi_assignments);

    st.mixer_state.channels.update(bus_id, [&](mixer::channel& bus) {
        bus.fx_chain = std::move(fx_chain);
    });

    st.fx_ladspa_instances.update(
            [&](fx::ladspa_instances& fx_ladspa_instances) {
                fx_ladspa_instances.emplace(instance_id, plugin_desc);
            });
}

void
insert_missing_ladspa_fx_module(
        state& st,
        mixer::channel_id const bus_id,
        std::size_t const position,
        fx::unavailable_ladspa const& unavail,
        std::string_view const& name)
{
    BOOST_ASSERT(bus_id != mixer::channel_id{});

    st.mixer_state.channels.update(bus_id, [&](mixer::channel& bus) {
        bus.fx_chain.update([&](fx::chain_t& fx_chain) {
            auto id = st.fx_unavailable_ladspa_plugins.add(unavail);
            auto const insert_pos = std::min(position, fx_chain.size());
            fx_chain.emplace(
                    std::next(fx_chain.begin(), insert_pos),
                    st.fx_modules.add(fx::module{
                            .fx_instance_id = id,
                            .name = name,
                            .parameters = {}}));
        });
    });
}

static auto
find_mixer_channel_containing_fx_module(
        mixer::channels_t const& channels,
        fx::module_id const fx_mod_id)
{
    return std::ranges::find_if(channels, [fx_mod_id](auto const& id_bus) {
        return algorithm::contains(*id_bus.second.fx_chain, fx_mod_id);
    });
}

template <class P>
static auto
remove_midi_assignement_for_parameter(state& st, parameter::id_t<P> id)
{
    st.midi_assignments.update(
            [id](midi_assignments_map& m) { m.erase(midi_assignment_id{id}); });
}

template <class P>
static auto
remove_parameter(state& st, parameter::id_t<P> id)
{
    remove_parameter(st.params, id);

    if constexpr (boost::mp11::mp_contains<
                          midi_assignment_id,
                          parameter::id_t<P>>::value)
    {
        remove_midi_assignement_for_parameter(st, id);
    }
}

void
remove_fx_module(state& st, fx::module_id id)
{
    fx::module const& fx_mod = st.fx_modules[id];

    auto it = find_mixer_channel_containing_fx_module(
            st.mixer_state.channels,
            id);
    BOOST_ASSERT(it != st.mixer_state.channels.end());

    st.mixer_state.channels.update(it->first, [id](mixer::channel& bus) {
        remove_erase(bus.fx_chain, id);
    });

    fx::parameters_t fx_params = st.fx_parameters;
    for (auto&& [key, fx_param_id] : *fx_mod.parameters)
    {
        std::visit([&st](auto&& id) { remove_parameter(st, id); }, fx_param_id);
        fx_params.erase(fx_param_id);
    }
    st.fx_parameters = std::move(fx_params);

    st.fx_modules.remove(id);

    if (auto id = std::get_if<fx::ladspa_instance_id>(&fx_mod.fx_instance_id))
    {
        st.fx_ladspa_instances.update([id](fx::ladspa_instances& instances) {
            instances.erase(*id);
        });
    }
    else if (
            auto id = std::get_if<fx::unavailable_ladspa_id>(
                    &fx_mod.fx_instance_id))
    {
        st.fx_unavailable_ladspa_plugins.remove(*id);
    }
}

auto
add_device_bus(
        state& st,
        std::string const& name,
        io_direction const io_dir,
        audio::bus_type const bus_type,
        channel_index_pair const& channels) -> device_io::bus_id
{
    auto id = st.device_io_state.buses.add(device_io::bus{
            .name = name,
            .bus_type = io_dir == io_direction::input ? bus_type
                                                      : audio::bus_type::stereo,
            .channels = channels});

    auto& bus_list = io_dir == io_direction::input ? st.device_io_state.inputs
                                                   : st.device_io_state.outputs;

    emplace_back(bus_list, id);

    st.mixer_state.channels.update(
            [id,
             io_dir,
             route_name = mixer::io_address_t(mixer::missing_device_address(
                     name))](mixer::channel_id, mixer::channel& mixer_channel) {
                if (io_dir == io_direction::input)
                {
                    if (mixer_channel.in == route_name)
                        mixer_channel.in = mixer::io_address_t(id);
                }
                else
                {
                    if (mixer_channel.out == route_name)
                        mixer_channel.out = mixer::io_address_t(id);
                }
            });

    return id;
}

auto
add_mixer_channel(state& st, std::string name) -> mixer::channel_id
{
    auto bus_id = st.mixer_state.channels.add(mixer::channel{
            .name = std::move(name),
            .in = {},
            .out = st.mixer_state.main,
            .volume = add_parameter(
                    st.params,
                    parameter::float_{
                            .default_value = 1.f,
                            .min = 0.f,
                            .max = 4.f,
                            .to_normalized = &to_normalized_volume,
                            .from_normalized = &from_normalized_volume}),
            .pan_balance = add_parameter(
                    st.params,
                    parameter::float_{
                            .default_value = 0.f,
                            .min = -1.f,
                            .max = 1.f,
                            .to_normalized = &parameter::to_normalized_linear,
                            .from_normalized =
                                    &parameter::from_normalized_linear}),
            .mute = add_parameter(
                    st.params,
                    parameter::bool_{.default_value = false}),
            .solo = add_parameter(
                    st.params,
                    parameter::bool_{.default_value = false}),
            .level = add_parameter(st.params, parameter::stereo_level{}),
            .fx_chain = {}});
    emplace_back(st.mixer_state.inputs, bus_id);
    return bus_id;
}

void
remove_mixer_channel(state& st, mixer::channel_id const channel_id)
{
    BOOST_ASSERT(channel_id != st.mixer_state.main);

    mixer::channel const& bus = st.mixer_state.channels[channel_id];

    remove_parameter(st, bus.volume);
    remove_parameter(st, bus.pan_balance);
    remove_parameter(st, bus.mute);
    remove_parameter(st, bus.solo);
    remove_parameter(st, bus.level);

    BOOST_ASSERT_MSG(
            bus.fx_chain->empty(),
            "fx_chain should be emptied before");

    if (st.fx_chain_channel == channel_id)
        st.fx_chain_channel = {};

    BOOST_ASSERT(algorithm::contains(*st.mixer_state.inputs, channel_id));
    remove_erase(st.mixer_state.inputs, channel_id);

    st.mixer_state.channels.remove(channel_id);

    st.mixer_state.channels.update(
            [channel_id](mixer::channel_id, mixer::channel& bus) {
                if (bus.in == mixer::io_address_t(channel_id))
                    bus.in = {};

                if (bus.out == mixer::io_address_t(channel_id))
                    bus.out = {};
            });
}

void
remove_device_bus(state& st, device_io::bus_id const device_bus_id)
{
    auto const name = st.device_io_state.buses[device_bus_id].name;

    st.mixer_state.channels.update(
            [device_bus_id,
             &name](mixer::channel_id, mixer::channel& mixer_channel) {
                if (mixer_channel.in == mixer::io_address_t(device_bus_id))
                    mixer_channel.in = mixer::missing_device_address(name);

                if (mixer_channel.out == mixer::io_address_t(device_bus_id))
                    mixer_channel.out = mixer::missing_device_address(name);
            });

    st.device_io_state.buses.remove(device_bus_id);

    if (algorithm::contains(*st.device_io_state.inputs, device_bus_id))
    {
        remove_erase(st.device_io_state.inputs, device_bus_id);
    }
    else
    {
        remove_erase(st.device_io_state.outputs, device_bus_id);
    }
}

void
update_midi_assignments(state& st, midi_assignments_map const& assignments)
{
    update_midi_assignments(st.midi_assignments, assignments);
}

} // namespace piejam::runtime
