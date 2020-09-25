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

#include <piejam/app/gui/model/BusConfig.h>

namespace piejam::app::gui::model
{

BusConfig::BusConfig(
        subscriber& state_change_subscriber,
        BusConfigSelectors selectors)
    : base_t(state_change_subscriber)
    , m_selectors(std::move(selectors))
{
}

void
BusConfig::subscribeStep(
        subscriber& state_change_subscriber,
        subscriptions_manager& subs,
        subscription_id subs_id)
{
    subs.observe(
            subs_id,
            state_change_subscriber,
            m_selectors.name,
            [this](container::boxed_string name) {
                setName(QString::fromStdString(*name));
            });

    subs.observe(
            subs_id,
            state_change_subscriber,
            m_selectors.bus_type,
            [this](audio::bus_type const t) {
                setMono(t == audio::bus_type::mono);
            });

    subs.observe(
            subs_id,
            state_change_subscriber,
            m_selectors.mono_channel,
            [this](std::size_t const ch) { setMonoChannel(ch + 1); });

    subs.observe(
            subs_id,
            state_change_subscriber,
            m_selectors.stereo_left_channel,
            [this](std::size_t const ch) { setStereoLeftChannel(ch + 1); });

    subs.observe(
            subs_id,
            state_change_subscriber,
            m_selectors.stereo_right_channel,
            [this](std::size_t const ch) { setStereoRightChannel(ch + 1); });
}

} // namespace piejam::app::gui::model