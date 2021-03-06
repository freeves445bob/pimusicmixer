// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <piejam/runtime/parameters.h>

#include <boost/container/container_fwd.hpp>

#include <variant>
#include <vector>

namespace piejam::runtime
{

struct midi_assignment;

} // namespace piejam::runtime

namespace piejam::runtime::fx
{

enum class internal : unsigned;

struct ladspa_instance_id_tag;
using ladspa_instance_id = entity_id<ladspa_instance_id_tag>;

struct unavailable_ladspa;
using unavailable_ladspa_id = entity_id<unavailable_ladspa>;
using unavailable_ladspa_plugins = entity_map<unavailable_ladspa>;

using instance_id =
        std::variant<internal, ladspa_instance_id, unavailable_ladspa_id>;

class ladspa_control;
class ladspa_manager;

struct parameter;
struct module;
struct registry;

using parameter_id =
        std::variant<float_parameter_id, int_parameter_id, bool_parameter_id>;
using parameter_key = std::size_t;
using parameter_value = std::variant<float, int, bool>;

template <class Value>
struct parameter_assignment;

using parameter_value_assignment = parameter_assignment<parameter_value>;
using parameter_midi_assignment = parameter_assignment<midi_assignment>;

using module_parameters =
        boost::container::flat_map<parameter_key, parameter_id>;
using parameters_t = boost::container::flat_map<parameter_id, parameter>;

using module_id = entity_id<module>;
using modules_t = entity_map<module>;

using chain_t = std::vector<module_id>;

} // namespace piejam::runtime::fx
