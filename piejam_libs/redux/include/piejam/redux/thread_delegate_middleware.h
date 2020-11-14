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

#include <piejam/redux/cloneable.h>
#include <piejam/redux/functors.h>

#include <memory>
#include <thread>

namespace piejam::redux
{

template <class DelegateMethod, class Action>
requires cloneable<Action> class thread_delegate_middleware
{
public:
    thread_delegate_middleware(
            std::thread::id delegate_to_id,
            DelegateMethod delegate,
            dispatch_f<Action> dispatch,
            next_f<Action> next)
        : m_delegate_to_id(delegate_to_id)
        , m_delegate(std::move(delegate))
        , m_dispatch(std::move(dispatch))
        , m_next(std::move(next))
    {
    }

    void operator()(Action const& a)
    {
        if (std::this_thread::get_id() == m_delegate_to_id)
        {
            m_next(a);
        }
        else
        {
            m_delegate([dispatch = m_dispatch, clone = a.clone()]() {
                dispatch(*clone);
            });
        }
    }

private:
    std::thread::id m_delegate_to_id;
    DelegateMethod m_delegate;
    dispatch_f<Action> m_dispatch;
    next_f<Action> m_next;
};

template <class DelegateMethod, class Action>
thread_delegate_middleware(
        std::thread::id,
        DelegateMethod,
        dispatch_f<Action>,
        next_f<Action>) -> thread_delegate_middleware<DelegateMethod, Action>;

template <class DelegateMethod>
struct make_thread_delegate_middleware
{
    make_thread_delegate_middleware(
            std::thread::id delegate_to_id,
            DelegateMethod delegate)
        : m_delegate_to_id(delegate_to_id)
        , m_delegate(std::move(delegate))
    {
    }

    template <class GetState, class Dispatch, class Action>
    auto operator()(GetState&&, Dispatch&& dispatch, next_f<Action> next) const
    {
        auto m = std::make_shared<
                thread_delegate_middleware<DelegateMethod, Action>>(
                m_delegate_to_id,
                m_delegate,
                std::move(dispatch),
                std::move(next));
        return [m](auto const& a) { (*m)(a); };
    }

private:
    std::thread::id m_delegate_to_id;
    DelegateMethod m_delegate;
};

template <class DelegateMethod>
make_thread_delegate_middleware(std::thread::id, DelegateMethod)
        -> make_thread_delegate_middleware<DelegateMethod>;

} // namespace piejam::redux
