// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2021  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <piejam/app/gui/model/MixerChannelPerform.h>

#include <piejam/app/gui/model/MidiAssignable.h>
#include <piejam/runtime/actions/fwd.h>
#include <piejam/runtime/actions/select_fx_chain_bus.h>
#include <piejam/runtime/actions/set_parameter_value.h>
#include <piejam/runtime/parameter/float_.h>
#include <piejam/runtime/parameter/generic_value.h>
#include <piejam/runtime/parameter/int_.h>
#include <piejam/runtime/selectors.h>

namespace piejam::app::gui::model
{

struct MixerChannelPerform::Impl
{
    runtime::mixer::bus_id busId;
    runtime::float_parameter_id volume;
    runtime::float_parameter_id panBalance;
    runtime::bool_parameter_id mute;
    runtime::stereo_level_parameter_id level;

    std::unique_ptr<MidiAssignable> volumeMidi;
    std::unique_ptr<MidiAssignable> panMidi;
    std::unique_ptr<MidiAssignable> muteMidi;
};

MixerChannelPerform::MixerChannelPerform(
        runtime::store_dispatch store_dispatch,
        runtime::subscriber& state_change_subscriber,
        runtime::mixer::bus_id const id,
        runtime::float_parameter_id const volume,
        runtime::float_parameter_id const panBalance,
        runtime::bool_parameter_id const mute,
        runtime::stereo_level_parameter_id const level)
    : Subscribable(store_dispatch, state_change_subscriber)
    , m_impl(std::make_unique<Impl>(
              id,
              volume,
              panBalance,
              mute,
              level,
              std::make_unique<MidiAssignable>(
                      store_dispatch,
                      state_change_subscriber,
                      volume),
              std::make_unique<MidiAssignable>(
                      store_dispatch,
                      state_change_subscriber,
                      panBalance),
              std::make_unique<MidiAssignable>(
                      store_dispatch,
                      state_change_subscriber,
                      mute)))
{
}

MixerChannelPerform::~MixerChannelPerform() = default;

void
MixerChannelPerform::onSubscribe()
{
    observe(runtime::selectors::make_mixer_bus_name_selector(m_impl->busId),
            [this](boxed_string const& name) {
                setName(QString::fromStdString(*name));
            });

    observe(runtime::selectors::make_mixer_bus_input_type_selector(
                    m_impl->busId),
            [this](audio::bus_type const bus_type) {
                setMono(bus_type == audio::bus_type::mono);
            });

    observe(runtime::selectors::make_float_parameter_value_selector(
                    m_impl->volume),
            [this](float x) { setVolume(x); });

    observe(runtime::selectors::make_float_parameter_value_selector(
                    m_impl->panBalance),
            [this](float x) { setPanBalance(x); });

    observe(runtime::selectors::make_bool_parameter_value_selector(
                    m_impl->mute),
            [this](bool x) { setMute(x); });

    //    observe(runtime::selectors::make_solo_selector(m_io_dir, m_bus_id),
    //            [this](bool x) { setSolo(x); });

    observe(runtime::selectors::make_level_parameter_value_selector(
                    m_impl->level),
            [this](runtime::stereo_level const& x) {
                setLevel(x.left, x.right);
            });
}

void
MixerChannelPerform::changeVolume(double value)
{
    dispatch(runtime::actions::set_float_parameter(
            m_impl->volume,
            static_cast<float>(value)));
}

void
MixerChannelPerform::changePanBalance(double value)
{
    dispatch(runtime::actions::set_float_parameter(
            m_impl->panBalance,
            static_cast<float>(value)));
}

void
MixerChannelPerform::changeMute(bool value)
{
    dispatch(runtime::actions::set_bool_parameter(m_impl->mute, value));
}

void
MixerChannelPerform::focusFxChain()
{
    runtime::actions::select_fx_chain_bus action;
    action.bus_id = m_impl->busId;
    dispatch(action);
}

auto
MixerChannelPerform::volumeMidi() const -> piejam::gui::model::MidiAssignable*
{
    return m_impl->volumeMidi.get();
}

auto
MixerChannelPerform::panMidi() const -> piejam::gui::model::MidiAssignable*
{
    return m_impl->panMidi.get();
}

auto
MixerChannelPerform::muteMidi() const -> piejam::gui::model::MidiAssignable*
{
    return m_impl->muteMidi.get();
}

} // namespace piejam::app::gui::model