// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <piejam/audio/fwd.h>
#include <piejam/audio/types.h>
#include <piejam/system/fwd.h>

namespace piejam::audio::alsa
{

auto
get_hw_params(pcm_descriptor const&, samplerate_t const*, period_size_t const*)
        -> pcm_hw_params;

void set_hw_params(
        system::device&,
        pcm_device_config const&,
        pcm_process_config const&);

} // namespace piejam::audio::alsa
