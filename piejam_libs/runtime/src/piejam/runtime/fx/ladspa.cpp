// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <piejam/runtime/fx/ladspa.h>

#include <piejam/audio/ladspa/port_descriptor.h>
#include <piejam/functional/overload.h>
#include <piejam/runtime/fx/module.h>
#include <piejam/runtime/fx/parameter.h>
#include <piejam/runtime/parameter/float_.h>
#include <piejam/runtime/parameter/float_normalize.h>
#include <piejam/runtime/parameter/generic_value.h>
#include <piejam/runtime/parameter/int_.h>
#include <piejam/runtime/parameter_maps_access.h>

#include <boost/container/flat_map.hpp>

namespace piejam::runtime::fx
{

static auto
make_module_parameters(
        std::span<audio::ladspa::port_descriptor const> control_inputs,
        parameters_t& fx_params,
        parameter_maps& params) -> fx::module_parameters
{
    fx::module_parameters module_params;

    for (auto const& port_desc : control_inputs)
    {
        if (auto const* const p = std::get_if<audio::ladspa::float_port>(
                    &port_desc.type_desc))
        {
            bool const logarithmic =
                    p->logarithmic && p->min > 0.f && p->max > 0.f;
            auto id = add_parameter(
                    params,
                    float_parameter{
                            .default_value = p->default_value,
                            .min = p->min,
                            .max = p->max,
                            .to_normalized =
                                    logarithmic ? &runtime::parameter::
                                                          to_normalized_log
                                                : &runtime::parameter::
                                                          to_normalized_linear,
                            .from_normalized =
                                    logarithmic
                                            ? &runtime::parameter::
                                                      from_normalized_log
                                            : &runtime::parameter::
                                                      from_normalized_linear});

            fx_params.emplace(
                    parameter_id(id),
                    parameter{
                            .name = port_desc.name,
                            .value_to_string = parameter_value_to_string()});

            module_params.emplace(port_desc.index, id);
        }
        else if (
                auto const* const p = std::get_if<audio::ladspa::int_port>(
                        &port_desc.type_desc))
        {
            BOOST_ASSERT(!p->logarithmic);
            auto id = add_parameter(
                    params,
                    int_parameter{
                            .default_value = p->default_value,
                            .min = p->min,
                            .max = p->max});

            fx_params.emplace(
                    id,
                    parameter{
                            .name = port_desc.name,
                            .value_to_string = parameter_value_to_string()});

            module_params.emplace(port_desc.index, id);
        }
        else if (
                auto const* const p = std::get_if<audio::ladspa::bool_port>(
                        &port_desc.type_desc))
        {
            auto id = add_parameter(
                    params,
                    bool_parameter{.default_value = p->default_value});

            fx_params.emplace(
                    id,
                    parameter{
                            .name = port_desc.name,
                            .value_to_string = parameter_value_to_string()});

            module_params.emplace(port_desc.index, id);
        }
    }

    return module_params;
}

auto
make_ladspa_module(
        ladspa_instance_id instance_id,
        std::string const& name,
        std::span<audio::ladspa::port_descriptor const> control_inputs,
        parameters_t& fx_params,
        parameter_maps& params) -> module
{
    return module{
            .fx_instance_id = instance_id,
            .name = name,
            .parameters =
                    make_module_parameters(control_inputs, fx_params, params)};
}

} // namespace piejam::runtime::fx
