// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <piejam/runtime/parameter/fwd.h>

namespace piejam::runtime
{

using float_parameter = parameter::float_;
using float_parameters = parameter::map<float_parameter>;
using float_parameter_id = parameter::id_t<float_parameter>;

using bool_parameter = parameter::bool_;
using bool_parameters = parameter::map<bool_parameter>;
using bool_parameter_id = parameter::id_t<bool_parameter>;

using int_parameter = parameter::int_;
using int_parameters = parameter::map<int_parameter>;
using int_parameter_id = parameter::id_t<int_parameter>;

using stereo_level_parameter = parameter::stereo_level;
using stereo_level_parameters = parameter::map<stereo_level_parameter>;
using stereo_level_parameter_id = parameter::id_t<stereo_level_parameter>;

} // namespace piejam::runtime
