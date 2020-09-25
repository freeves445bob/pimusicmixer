// PieJam - An audio mixer for Raspberry Pi.
//
// Copyright (C) 2020  Dimitrij Kotrev
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <piejam/app/gui/model/Subscribable.h>
#include <piejam/app/store.h>
#include <piejam/app/subscriber.h>
#include <piejam/audio/types.h>
#include <piejam/container/boxed_string.h>
#include <piejam/gui/model/BusConfig.h>

namespace piejam::app::gui::model
{

struct BusConfigSelectors
{
    selector<container::boxed_string> name;
    selector<audio::bus_type> bus_type;
    selector<std::size_t> mono_channel;
    selector<std::size_t> stereo_left_channel;
    selector<std::size_t> stereo_right_channel;
};

class BusConfig final : public Subscribable<piejam::gui::model::BusConfig>
{
    using base_t = Subscribable<piejam::gui::model::BusConfig>;

public:
    BusConfig(subscriber&, BusConfigSelectors);

private:
    void subscribeStep(subscriber&, subscriptions_manager&, subscription_id)
            override;

    BusConfigSelectors m_selectors;
};

} // namespace piejam::app::gui::model
