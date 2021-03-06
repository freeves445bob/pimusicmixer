// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <piejam/gui/model/GenericListModel.h>
#include <piejam/gui/model/SubscribableModel.h>
#include <piejam/gui/model/fwd.h>

namespace piejam::gui::model
{

class FxModule : public SubscribableModel
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name NOTIFY nameChanged FINAL)
    Q_PROPERTY(bool bypassed READ bypassed NOTIFY bypassedChanged FINAL)
    Q_PROPERTY(QAbstractListModel* parameters READ parameters CONSTANT)
    Q_PROPERTY(
            bool canMoveLeft READ canMoveLeft NOTIFY canMoveLeftChanged FINAL)
    Q_PROPERTY(bool canMoveRight READ canMoveRight NOTIFY canMoveRightChanged
                       FINAL)

public:
    auto name() const noexcept -> QString const& { return m_name; }
    void setName(QString const& x)
    {
        if (m_name != x)
        {
            m_name = x;
            emit nameChanged();
        }
    }

    bool bypassed() const noexcept { return m_bypassed; }
    void setBypassed(bool const x)
    {
        if (m_bypassed != x)
        {
            m_bypassed = x;
            emit bypassedChanged();
        }
    }

    auto canMoveLeft() const noexcept -> bool { return m_canMoveLeft; }
    void setCanMoveLeft(bool x)
    {
        if (m_canMoveLeft != x)
        {
            m_canMoveLeft = x;
            emit canMoveLeftChanged();
        }
    }

    auto canMoveRight() const noexcept -> bool { return m_canMoveRight; }
    void setCanMoveRight(bool x)
    {
        if (m_canMoveRight != x)
        {
            m_canMoveRight = x;
            emit canMoveRightChanged();
        }
    }

    auto parameters() noexcept -> FxParametersList* { return &m_parameters; }

    Q_INVOKABLE virtual void toggleBypass() = 0;
    Q_INVOKABLE virtual void deleteModule() = 0;
    Q_INVOKABLE virtual void moveLeft() = 0;
    Q_INVOKABLE virtual void moveRight() = 0;

signals:
    void nameChanged();
    void bypassedChanged();
    void canMoveLeftChanged();
    void canMoveRightChanged();

private:
    QString m_name;
    bool m_bypassed{};
    FxParametersList m_parameters;
    bool m_canMoveLeft{};
    bool m_canMoveRight{};
};

} // namespace piejam::gui::model
