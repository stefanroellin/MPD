/*
 * Copyright 2003-2017 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_AUDIO_OUTPUT_INTERFACE_HXX
#define MPD_AUDIO_OUTPUT_INTERFACE_HXX

struct AudioOutputPlugin;

class AudioOutput {
	/**
	 * The plugin which implements this output device.
	 */
	const AudioOutputPlugin &plugin;

	bool need_fully_defined_audio_format = false;

public:
	AudioOutput(const AudioOutputPlugin &_plugin)
		:plugin(_plugin) {}

	const AudioOutputPlugin &GetPlugin() const {
		return plugin;
	}

	bool GetNeedFullyDefinedAudioFormat() const {
		return need_fully_defined_audio_format;
	}

	/**
	 * Plugins shall call this method if they require an
	 * "audio_format" setting which evaluates
	 * AudioFormat::IsFullyDefined().
	 */
	void NeedFullyDefinedAudioFormat() {
		need_fully_defined_audio_format = true;
	}
};

#endif