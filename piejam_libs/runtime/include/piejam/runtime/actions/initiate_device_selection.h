// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <piejam/runtime/actions/device_action.h>
#include <piejam/runtime/fwd.h>
#include <piejam/runtime/ui/cloneable_action.h>

namespace piejam::runtime::actions
{

struct initiate_device_selection final
    : ui::cloneable_action<initiate_device_selection, action>
    , visitable_device_action<initiate_device_selection>
{
    bool input{};
    std::size_t index{};

    auto reduce(state const&) const -> state override;
};

} // namespace piejam::runtime::actions
