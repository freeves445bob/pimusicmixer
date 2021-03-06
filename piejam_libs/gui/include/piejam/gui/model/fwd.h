// PieJam - An audio mixer for Raspberry Pi.
// SPDX-FileCopyrightText: 2020  Dimitrij Kotrev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace piejam::gui::model
{

template <class ListItem>
class GenericListModel;

class AudioDeviceSettings;
class AudioInputOutputSettings;

class FxChain;
class FxModule;
using FxModulesList = GenericListModel<FxModule>;

class Info;

class Mixer;
class MixerChannel;
using MixerChannelsList = GenericListModel<MixerChannel>;
class MixerChannelPerform;
class MixerChannelEdit;

class BusConfig;
using BusConfigsList = GenericListModel<BusConfig>;

class BusName;
using BusNamesList = GenericListModel<BusName>;

class FxParameter;
using FxParametersList = GenericListModel<FxParameter>;

class FxBrowser;
class FxBrowserEntry;
using FxBrowserList = GenericListModel<FxBrowserEntry>;

class MidiDeviceConfig;
using MidiDeviceList = GenericListModel<MidiDeviceConfig>;

class MidiInputSettings;

class MidiAssignable;

} // namespace piejam::gui::model
