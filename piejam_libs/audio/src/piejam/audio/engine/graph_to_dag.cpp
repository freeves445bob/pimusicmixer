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

#include <piejam/audio/engine/graph_to_dag.h>

#include <piejam/audio/engine/dag.h>
#include <piejam/audio/engine/event_input_buffers.h>
#include <piejam/audio/engine/event_output_buffers.h>
#include <piejam/audio/engine/graph.h>
#include <piejam/audio/engine/processor.h>
#include <piejam/audio/engine/processor_job.h>
#include <piejam/audio/period_sizes.h>
#include <piejam/functional/address_compare.h>

#include <boost/assert.hpp>

#include <array>
#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <vector>

namespace piejam::audio::engine
{

static auto
jobs_without_children(dag const& dag_) -> std::vector<dag::task_id_t>
{
    std::vector<dag::task_id_t> result;
    for (auto const& [id, node] : dag_.nodes())
    {
        if (node.children.empty())
            result.push_back(id);
    }
    return result;
}

auto
graph_to_dag(
        graph const& g,
        std::size_t const run_queue_size,
        std::size_t const& buffer_size_ref) -> dag
{
    dag result(run_queue_size);

    std::map<
            std::reference_wrapper<processor>,
            std::pair<dag::task_id_t, processor_job*>,
            address_less<processor>>
            processor_job_mapping;

    std::vector<processor_job*> clear_event_buffer_jobs;

    auto add_job = [&](graph::endpoint const& e) {
        auto job = std::make_shared<processor_job>(e.proc, buffer_size_ref);
        auto job_ptr = job.get();
        auto id = result.add_task(
                [j = std::move(job)](thread_context const& ctx) { (*j)(ctx); });
        processor_job_mapping.emplace(e.proc, std::pair(id, job_ptr));
        if (e.proc.get().num_event_outputs())
            clear_event_buffer_jobs.push_back(job_ptr);
    };

    // create a job for each processor
    for (auto const& [src, dst] : g.wires())
    {
        if (!processor_job_mapping.count(src.proc))
            add_job(src);

        if (!processor_job_mapping.count(dst.proc))
            add_job(dst);
    }

    for (auto const& [src, dst] : g.event_wires())
    {
        if (!processor_job_mapping.count(src.proc))
            add_job(src);

        if (!processor_job_mapping.count(dst.proc))
            add_job(dst);
    }

    // connect jobs according to audio wires
    std::set<std::pair<dag::task_id_t, dag::task_id_t>> added_deps;
    for (auto const& [src, dst] : g.wires())
    {
        auto const& [src_id, src_job] = processor_job_mapping[src.proc];
        auto const& [dst_id, dst_job] = processor_job_mapping[dst.proc];

        if (!added_deps.count({src_id, dst_id}))
        {
            result.add_child(src_id, dst_id);
            added_deps.emplace(src_id, dst_id);
        }

        dst_job->connect_result(dst.port, src_job->result_ref(src.port));
    }

    // connect jobs according to event wires
    for (auto const& [src, dst] : g.event_wires())
    {
        auto const& [src_id, src_job] = processor_job_mapping[src.proc];
        auto const& [dst_id, dst_job] = processor_job_mapping[dst.proc];

        if (!added_deps.count({src_id, dst_id}))
        {
            result.add_child(src_id, dst_id);
            added_deps.emplace(src_id, dst_id);
        }

        dst_job->connect_event_result(
                dst.port,
                src_job->event_result_ref(src.port));
    }

    // if we have processors with event outputs, we need to clear their
    // buffers as last step
    if (!clear_event_buffer_jobs.empty())
    {
        // get final jobs first, before inserting the clear job
        auto const final_jobs = jobs_without_children(result);

        auto clear_job_id =
                result.add_task([jobs = std::move(clear_event_buffer_jobs)](
                                        thread_context const&) {
                    for (auto job : jobs)
                        job->clear_event_output_buffers();
                });

        for (dag::task_id_t const final_job_id : final_jobs)
            result.add_child(final_job_id, clear_job_id);
    }

    return result;
}

} // namespace piejam::audio::engine