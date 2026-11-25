/*
    This file is part of Helio music sequencer.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "Common.h"
#include "BuiltInSynthsPluginFormat.h"
#include "DefaultSynthAudioPlugin.h"
#include "MetronomeSynthAudioPlugin.h"
#include "SoundFontSynthAudioPlugin.h"
#include "BuiltInEffectsAudioPlugin.h"
#include "BuiltInEffectModuleAudioPlugin.h"

const String BuiltInSynthsPluginFormat::formatName = "BuiltIn";
const String BuiltInSynthsPluginFormat::formatIdentifier = "BuiltIn";

BuiltInSynthsPluginFormat::BuiltInSynthsPluginFormat()
{
    DefaultSynthAudioPlugin defaultAudioPlugin;
    defaultAudioPlugin.fillInPluginDescription(this->defaultInstrument);

    MetronomeSynthAudioPlugin metronomeAudioPlugin;
    metronomeAudioPlugin.fillInPluginDescription(this->metronomeInstrument);

    SoundFontSynthAudioPlugin soundFontSynthAudioPlugin;
    soundFontSynthAudioPlugin.fillInPluginDescription(this->soundFontPlayerInstrument);

    BuiltInEffectsAudioPlugin effectsPlugin;
    effectsPlugin.fillInPluginDescription(this->effectsRack);
    for (int i = 0; i < BuiltInEffectModuleAudioPlugin::moduleCount; ++i)
    {
        BuiltInEffectModuleAudioPlugin module(BuiltInEffectModuleAudioPlugin::getTypeForIndex(i));
        module.fillInPluginDescription(this->effectModules[static_cast<size_t>(i)]);
    }
}

String BuiltInSynthsPluginFormat::getName() const
{
    return BuiltInSynthsPluginFormat::formatName;
}

void BuiltInSynthsPluginFormat::findAllTypesForFile(OwnedArray<PluginDescription> &description, const String &id)
{
    if (id == DefaultSynthAudioPlugin::instrumentId)
    {
        description.add(new PluginDescription(this->defaultInstrument));
    }
    else if (id == MetronomeSynthAudioPlugin::instrumentId)
    {
        description.add(new PluginDescription(this->metronomeInstrument));
    }
    else if (id == SoundFontSynthAudioPlugin::instrumentId)
    {
        description.add(new PluginDescription(this->soundFontPlayerInstrument));
    }
    else if (id == BuiltInEffectsAudioPlugin::pluginId)
    {
        // Keep the combined rack identifier resolvable for older projects.
        description.add(new PluginDescription(this->effectsRack));
    }
    else
    {
        for (int i = 0; i < BuiltInEffectModuleAudioPlugin::moduleCount; ++i)
        {
            if (id == BuiltInEffectModuleAudioPlugin::getModuleIdentifier(i))
            {
                description.add(new PluginDescription(this->effectModules[static_cast<size_t>(i)]));
                break;
            }
        }
    }
}

bool BuiltInSynthsPluginFormat::fileMightContainThisPluginType(const String &fileOrIdentifier)
{
    if (fileOrIdentifier.isEmpty() ||
        fileOrIdentifier == BuiltInSynthsPluginFormat::formatIdentifier ||
        fileOrIdentifier == BuiltInEffectsAudioPlugin::pluginId)
        return true;

    for (int i = 0; i < BuiltInEffectModuleAudioPlugin::moduleCount; ++i)
        if (fileOrIdentifier == BuiltInEffectModuleAudioPlugin::getModuleIdentifier(i))
            return true;

    return false;
}

void BuiltInSynthsPluginFormat::createPluginInstance(const PluginDescription &desc,
    double initialSampleRate, int initialBufferSize, PluginCreationCallback callback)
{
    if (desc.name == this->defaultInstrument.name)
    {
        callback(make<DefaultSynthAudioPlugin>(), {});
        return;
    }

    if (desc.name == this->metronomeInstrument.name)
    {
        callback(make<MetronomeSynthAudioPlugin>(), {});
        return;
    }

    if (desc.name == this->soundFontPlayerInstrument.name)
    {
        callback(make<SoundFontSynthAudioPlugin>(), {});
        return;
    }

    if (desc.name == this->effectsRack.name)
    {
        callback(make<BuiltInEffectsAudioPlugin>(), {});
        return;
    }

    for (int i = 0; i < BuiltInEffectModuleAudioPlugin::moduleCount; ++i)
    {
        if (desc.name == this->effectModules[static_cast<size_t>(i)].name)
        {
            callback(make<BuiltInEffectModuleAudioPlugin>(
                BuiltInEffectModuleAudioPlugin::getTypeForIndex(i)), {});
            return;
        }
    }

    callback(nullptr, {});
}
