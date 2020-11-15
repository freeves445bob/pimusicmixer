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

#include <piejam/audio/table.h>
#include <piejam/audio/types.h>
#include <piejam/runtime/audio_engine.h>
#include <piejam/runtime/mixer.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>

namespace piejam::runtime::test
{

// plot with gnuplot:
//     plot 'add_input_channel.txt' using 1 with lines

struct audio_engine_render_test : public ::testing::Test
{
    audio::samplerate_t const samplerate{48000};
    std::size_t const buffer_size{1024};
    audio::table audio_in{2, buffer_size};
    audio::table audio_out{2, buffer_size};
    audio_engine sut{{}, samplerate, 2, 2};

    std::size_t sine_wave_pos{};
    std::vector<audio::pair<float>> output;

    void fill_sine()
    {
        std::ranges::generate(audio_in.rows()[0], [this]() -> float {
            return std::sin(
                    (sine_wave_pos++ / static_cast<float>(samplerate)) *
                    M_PI_2);
        });
    }

    void render_buffer()
    {
        fill_sine();

        sut(as_const(audio_in.rows()), audio_out.rows());

        for (std::size_t o = 0; o < buffer_size; ++o)
            output.emplace_back(audio_out.rows()[0][o], audio_out.rows()[1][o]);
    }

    void render(std::size_t iterations)
    {
        while (iterations--)
            render_buffer();
    }

    void rebuild(mixer::state const& mixer_state)
    {
        auto engine_swap = std::async(std::launch::async, [&]() {
            sut.rebuild(mixer_state);
        });

        do
        {
            render_buffer();
        } while (engine_swap.wait_for(std::chrono::milliseconds{1}) !=
                 std::future_status::ready);
    }

    void dump_output(std::filesystem::path const& file)
    {
        std::ofstream os(file);
        for (auto const& p : output)
            os << p.left << '\t' << p.right << std::endl;
    }
};

TEST_F(audio_engine_render_test, add_input_channel)
{
    mixer::state mixer_state;

    mixer::channel in_ch1;
    in_ch1.type = mixer::channel_type::mono;
    in_ch1.device_channels = channel_index_pair(0);
    mixer::add_channel(
            mixer_state.channels,
            mixer_state.inputs,
            std::move(in_ch1));

    mixer::channel out_ch;
    out_ch.type = mixer::channel_type::stereo;
    out_ch.device_channels = channel_index_pair(0, 1);
    mixer::add_channel(
            mixer_state.channels,
            mixer_state.outputs,
            std::move(out_ch));

    rebuild(mixer_state);
    render(200);

    mixer::channel in_ch2;
    in_ch2.type = mixer::channel_type::mono;
    in_ch2.device_channels = channel_index_pair(1);
    mixer::add_channel(
            mixer_state.channels,
            mixer_state.inputs,
            std::move(in_ch2));

    rebuild(mixer_state);
    render(200);

    dump_output("add_input_channel.txt");
}

} // namespace piejam::runtime::test
