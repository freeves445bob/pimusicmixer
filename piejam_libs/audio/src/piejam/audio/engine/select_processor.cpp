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

#include <piejam/audio/engine/select_processor.h>

#include <piejam/audio/engine/audio_slice.h>
#include <piejam/audio/engine/named_processor.h>
#include <piejam/audio/engine/process_context.h>
#include <piejam/audio/engine/single_event_input_processor.h>
#include <piejam/audio/engine/verify_process_context.h>

#include <boost/assert.hpp>

#include <array>

namespace piejam::audio::engine
{

namespace
{

class select_processor final
    : public named_processor
    , public single_event_input_processor<select_processor, std::size_t>
{
public:
    select_processor(std::size_t const num_inputs, std::string_view const& name)
        : named_processor(name)
        , m_num_inputs(num_inputs)
    {
    }

    auto type_name() const -> std::string_view override { return "select"; }

    auto num_inputs() const -> std::size_t override { return m_num_inputs; }
    auto num_outputs() const -> std::size_t override { return 1; }

    auto event_inputs() const -> event_ports override
    {
        static std::array s_ports{event_port{typeid(std::size_t), "select"}};
        return s_ports;
    }

    auto event_outputs() const -> event_ports override { return {}; }

    void create_event_output_buffers(
            event_output_buffer_factory const&) const override
    {
    }

    void process(process_context const& ctx) override
    {
        verify_process_context(*this, ctx);

        process_sliced_on_events(ctx);
    }

    void process_without_events(process_context const& ctx)
    {
        ctx.results[0] = m_selected < m_num_inputs ? ctx.inputs[m_selected]
                                                   : audio_slice{};
    }

    void process_with_starting_event(
            process_context const& ctx,
            std::size_t const index)
    {
        m_selected = index;

        ctx.results[0] = m_selected < m_num_inputs ? ctx.inputs[m_selected]
                                                   : audio_slice{};
    }

    void process_event_slice(
            process_context const& ctx,
            std::size_t const from_offset,
            std::size_t const to_offset,
            std::size_t const next_index)
    {
        auto const subslice_size = to_offset - from_offset;
        copy(m_selected < m_num_inputs ? subslice(
                                                 ctx.inputs[m_selected].get(),
                                                 from_offset,
                                                 subslice_size)
                                       : audio_slice(),
             std::span<float>(
                     ctx.outputs[0].data() + from_offset,
                     subslice_size));
        m_selected = next_index;
    }

    void process_final_slice(
            process_context const& ctx,
            std::size_t const from_offset)
    {
        auto const subslice_size = ctx.buffer_size - from_offset;
        copy(m_selected < m_num_inputs ? subslice(
                                                 ctx.inputs[m_selected].get(),
                                                 from_offset,
                                                 subslice_size)
                                       : audio_slice(),
             std::span<float>(
                     ctx.outputs[0].data() + from_offset,
                     subslice_size));
    }

private:
    std::size_t m_selected{};
    std::size_t m_num_inputs{};
};

} // namespace

auto
make_select_processor(std::size_t num_inputs, std::string_view const& name)
        -> std::unique_ptr<processor>
{
    BOOST_ASSERT(num_inputs > 0);
    return std::make_unique<select_processor>(num_inputs, name);
}

} // namespace piejam::audio::engine