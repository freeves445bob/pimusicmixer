// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <piejam/audio/engine/level_meter_processor.h>

#include <piejam/audio/engine/audio_slice.h>
#include <piejam/audio/engine/process_context.h>
#include <piejam/audio/engine/verify_process_context.h>
#include <piejam/functional/overload.h>

#include <algorithm>
#include <span>

namespace piejam::audio::engine
{

static constexpr auto
level_meter_decay_time(unsigned const samplerate) -> std::size_t
{
    return samplerate / 10;
}

level_meter_processor::level_meter_processor(
        unsigned const samplerate,
        std::string_view const& name)
    : engine::named_processor(name)
    , m_lm(level_meter_decay_time(samplerate))
{
}

void
level_meter_processor::process(engine::process_context const& ctx)
{
    verify_process_context(*this, ctx);

    std::visit(
            overload{
                    [this, bs = ctx.buffer_size](float const c) {
                        std::fill_n(std::back_inserter(m_lm), bs, c);
                    },
                    [this](std::span<float const> const& buffer) {
                        std::ranges::copy(buffer, std::back_inserter(m_lm));
                    }},
            ctx.inputs[0].get().as_variant());

    ctx.event_outputs.get<float>(0).insert(0, m_lm.get());
}

} // namespace piejam::audio::engine
