// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <piejam/audio/engine/dag.h>

#include <piejam/audio/engine/dag_executor.h>
#include <piejam/audio/engine/event_buffer_memory.h>
#include <piejam/audio/engine/thread_context.h>
#include <piejam/range/indices.h>
#include <piejam/thread/job_deque.h>
#include <piejam/thread/worker.h>

#include <boost/assert.hpp>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>

namespace piejam::audio::engine
{

namespace
{

class dag_executor_base : public dag_executor
{
protected:
    dag_executor_base(dag::tasks_t const& tasks, dag::graph_t const& graph)
        : m_nodes(make_nodes(tasks, graph))
    {
    }

    struct node;
    using node_ref = std::reference_wrapper<node>;

    struct node
    {
        std::atomic_size_t parents_to_process{};
        std::size_t num_parents{};
        dag::task_t task;
        std::vector<node_ref> children;
    };

    using nodes_t = std::unordered_map<dag::task_id_t, node>;

    nodes_t m_nodes;

private:
    static auto make_nodes(dag::tasks_t const& tasks, dag::graph_t const& graph)
            -> nodes_t
    {
        nodes_t nodes;
        for (auto const& [id, task] : tasks)
            nodes[id].task = task;

        for (auto const& [parent_id, children] : graph)
        {
            BOOST_ASSERT(nodes.count(parent_id));
            std::ranges::transform(
                    children,
                    std::back_inserter(nodes[parent_id].children),
                    [&nodes](dag::task_id_t const child_id) {
                        BOOST_ASSERT(nodes.count(child_id));
                        return std::ref(nodes[child_id]);
                    });

            for (dag::task_id_t const child_id : children)
                ++nodes[child_id].num_parents;
        }

        return nodes;
    }
};

class dag_executor_st final : public dag_executor_base
{
public:
    dag_executor_st(
            dag::tasks_t const& tasks,
            dag::graph_t const& graph,
            std::size_t const event_memory_size)
        : dag_executor_base(tasks, graph)
        , m_event_memory(event_memory_size)
    {
        m_run_queue.reserve(m_nodes.size());
    }

    void operator()(std::size_t const buffer_size) override
    {
        for (auto& [id, nd] : m_nodes)
        {
            nd.parents_to_process = nd.num_parents;

            if (nd.num_parents == 0)
            {
                BOOST_ASSERT(m_run_queue.size() < m_run_queue.capacity());
                m_run_queue.emplace_back(std::addressof(nd));
            }
        }

        while (!m_run_queue.empty())
        {
            node* const nd = m_run_queue.back();
            m_run_queue.pop_back();

            BOOST_ASSERT(
                    nd->parents_to_process.load(std::memory_order_relaxed) ==
                    0);
            nd->task(m_thread_context, buffer_size);

            for (node& child : nd->children)
            {
                if (1 == child.parents_to_process.fetch_sub(
                                 1,
                                 std::memory_order_relaxed))
                {
                    BOOST_ASSERT(m_run_queue.size() < m_run_queue.capacity());
                    m_run_queue.push_back(std::addressof(child));
                }
            }
        }

        m_event_memory.release();
    }

private:
    audio::engine::event_buffer_memory m_event_memory;
    audio::engine::thread_context m_thread_context{
            &m_event_memory.memory_resource()};
    std::vector<node*> m_run_queue;
};

class dag_executor_mt final : public dag_executor_base
{
public:
    static constexpr std::size_t job_deque_capacity = 256;
    using job_deque_t = thread::job_deque<node, job_deque_capacity>;

    dag_executor_mt(
            dag::tasks_t const& tasks,
            dag::graph_t const& graph,
            std::size_t const event_memory_size,
            std::span<thread::worker> const& worker_threads)
        : dag_executor_base(tasks, graph)
        , m_run_queues(1 + worker_threads.size())
        , m_main_worker(
                  0,
                  event_memory_size,
                  m_nodes_to_process,
                  m_buffer_size,
                  m_run_queues)
        , m_worker_threads(worker_threads)
        , m_workers(make_workers(
                  worker_threads.size(),
                  event_memory_size,
                  m_nodes_to_process,
                  m_buffer_size,
                  m_run_queues))
        , m_worker_tasks(make_worker_tasks(m_workers))
    {
    }

    void operator()(std::size_t const buffer_size) override
    {
        BOOST_ASSERT(m_run_queues[0].size() == 0);

        m_nodes_to_process.store(m_nodes.size(), std::memory_order_release);
        m_buffer_size.store(buffer_size, std::memory_order_release);

        for (auto& rq : m_run_queues)
            rq.reset();

        distribute_initial_tasks();

        BOOST_ASSERT(m_worker_tasks.size() == m_worker_threads.size());
        for (std::size_t const w : range::indices(m_worker_threads))
        {
            m_worker_threads[w].wakeup(m_worker_tasks[w]);
        }

        m_main_worker();
    }

private:
    void distribute_initial_tasks()
    {
        std::size_t next_queue{1 % m_run_queues.size()};
        for (auto& id_node : m_nodes)
        {
            id_node.second.parents_to_process = id_node.second.num_parents;

            if (id_node.second.num_parents == 0)
            {
                BOOST_ASSERT(
                        m_run_queues[next_queue].size() <
                        m_run_queues[next_queue].capacity());
                m_run_queues[next_queue].push(std::addressof(id_node.second));
                next_queue = (next_queue + 1) % m_run_queues.size();
            }
        }
    }

    struct dag_worker
    {
        dag_worker(
                std::size_t const worker_index,
                std::size_t const event_memory_size,
                std::atomic_size_t& nodes_to_process,
                std::atomic_size_t& buffer_size,
                std::span<job_deque_t> const& run_queues)
            : m_worker_index(worker_index)
            , m_event_memory(event_memory_size)
            , m_nodes_to_process(nodes_to_process)
            , m_buffer_size(buffer_size)
            , m_run_queues(run_queues)
        {
        }

        void operator()()
        {
            std::size_t const buffer_size =
                    m_buffer_size.load(std::memory_order_consume);

            while (m_nodes_to_process.load(std::memory_order_relaxed))
            {
                if (node* n = m_run_queues[m_worker_index].pop())
                {
                    while (n)
                        n = process_node(*n, buffer_size);
                }
                else
                {
                    std::size_t const num_queues = m_run_queues.size();
                    for (std::size_t i = 1; i < num_queues; ++i)
                    {
                        std::size_t const steal_index =
                                (m_worker_index + i) % num_queues;
                        if (node* n = m_run_queues[steal_index].steal())
                        {
                            while (n)
                                n = process_node(*n, buffer_size);
                            break;
                        }
                    }
                }
            }

            m_event_memory.release();
        }

    private:
        auto process_node(node& n, std::size_t const buffer_size) -> node*
        {
            job_deque_t& run_queue = m_run_queues[m_worker_index];

            BOOST_ASSERT(n.parents_to_process == 0);

            n.task(m_thread_context, buffer_size);

            m_nodes_to_process.fetch_sub(1, std::memory_order_acq_rel);

            node* next{};
            for (node& child : n.children)
            {
                if (1 == child.parents_to_process.fetch_sub(
                                 1,
                                 std::memory_order_acq_rel))
                {
                    if (next)
                    {
                        BOOST_ASSERT(run_queue.size() < run_queue.capacity());
                        run_queue.push(std::addressof(child));
                    }
                    else
                    {
                        next = std::addressof(child);
                    }
                }
            }

            return next;
        }

        std::size_t m_worker_index;
        audio::engine::event_buffer_memory m_event_memory;
        audio::engine::thread_context m_thread_context{
                &m_event_memory.memory_resource()};
        std::atomic_size_t& m_nodes_to_process;
        std::atomic_size_t& m_buffer_size;
        std::span<job_deque_t> m_run_queues;
    };

    using workers_t = std::vector<dag_worker>;

    static auto make_workers(
            std::size_t const num_workers,
            std::size_t const event_memory_size,
            std::atomic_size_t& nodes_to_process,
            std::atomic_size_t& buffer_size,
            std::span<job_deque_t> const& run_queues) -> workers_t
    {
        workers_t workers;
        workers.reserve(num_workers);

        for (std::size_t i = 0; i < num_workers; ++i)
        {
            workers.emplace_back(
                    i + 1,
                    event_memory_size,
                    nodes_to_process,
                    buffer_size,
                    run_queues);
        }

        return workers;
    }

    static auto make_worker_tasks(std::span<dag_worker> const& workers)
            -> std::vector<thread::worker::task_t>
    {
        std::vector<thread::worker::task_t> tasks;
        tasks.reserve(workers.size());

        for (auto& w : workers)
            tasks.emplace_back([&w]() { w(); });

        return tasks;
    }

    std::atomic_size_t m_nodes_to_process{};
    std::vector<job_deque_t> m_run_queues;
    dag_worker m_main_worker;
    std::span<thread::worker> m_worker_threads;
    workers_t m_workers;
    std::vector<thread::worker::task_t> m_worker_tasks;
    std::atomic_size_t m_buffer_size{};
};

auto
is_descendent(
        dag::graph_t const& t,
        dag::task_id_t const parent,
        dag::task_id_t const descendent) -> bool
{
    if (parent == descendent)
        return true;

    return std::ranges::any_of(
            t.at(parent),
            [&t, descendent](dag::task_id_t const child) {
                return is_descendent(t, child, descendent);
            });
}

} // namespace

dag::dag() = default;

auto
dag::add_task(task_t t) -> task_id_t
{
    auto id = m_free_id++;
    m_tasks.emplace_back(id, std::move(t));
    m_graph[id];
    return id;
}

auto
dag::add_child_task(task_id_t const parent, task_t t) -> task_id_t
{
    auto it = m_graph.find(parent);

    BOOST_ASSERT_MSG(it != m_graph.end(), "parent node not found");

    auto id = add_task(std::move(t));
    it->second.push_back(id);
    return id;
}

void
dag::add_child(task_id_t const parent, task_id_t const child)
{
    auto it_parent = m_graph.find(parent);

    BOOST_ASSERT_MSG(it_parent != m_graph.end(), "parent node not found");

    auto it_child = m_graph.find(child);

    BOOST_ASSERT_MSG(it_child != m_graph.end(), "child node not found");

    BOOST_ASSERT_MSG(
            !is_descendent(m_graph, it_child->first, it_parent->first),
            "child is ancestor of the parent");

    it_parent->second.push_back(it_child->first);
}

auto
dag::make_runnable(
        std::span<thread::worker> const& worker_threads,
        std::size_t const event_memory_size) -> std::unique_ptr<dag_executor>
{
    if (worker_threads.empty())
        return std::make_unique<dag_executor_st>(
                m_tasks,
                m_graph,
                event_memory_size);

    return std::make_unique<dag_executor_mt>(
            m_tasks,
            m_graph,
            event_memory_size,
            worker_threads);
}

} // namespace piejam::audio::engine
