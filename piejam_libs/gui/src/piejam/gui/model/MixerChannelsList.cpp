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

#include <piejam/gui/model/MixerChannelsList.h>

#include <piejam/gui/model/MixerChannel.h>

namespace piejam::gui::model
{

MixerChannelsList::MixerChannelsList(QObject* parent)
    : QAbstractListModel(parent)
{
}

MixerChannelsList::~MixerChannelsList() = default;

auto
MixerChannelsList::rowCount(const QModelIndex& /*parent*/) const -> int
{
    return static_cast<int>(m_list.size());
}

auto
MixerChannelsList::data(const QModelIndex& index, int role) const -> QVariant
{
    switch (role)
    {
        case ItemRole:
            return QVariant::fromValue(m_list[index.row()].get());
        default:
            return {};
    }
}

auto
MixerChannelsList::roleNames() const -> QHash<int, QByteArray>
{
    static QHash<int, QByteArray> s_roles = {{ItemRole, "item"}};
    return s_roles;
}

void
MixerChannelsList::addMixerChannel(std::unique_ptr<MixerChannel> busConfig)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    assert(busConfig);
    m_list.push_back(std::move(busConfig));
    m_list.back()->subscribe();
    endInsertRows();
}

void
MixerChannelsList::removeMixerChannel()
{
    beginRemoveRows(QModelIndex(), rowCount() - 1, rowCount() - 1);
    assert(!m_list.empty());
    m_list.erase(std::next(m_list.begin(), m_list.size() - 1));
    endRemoveRows();
}

} // namespace piejam::gui::model