// AudiOGC v1.0.0
// By HTV04

/*
MIT License

Copyright (c) 2022 HTV04

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define DR_FLAC_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define DR_WAV_IMPLEMENTATION

// Libraries
#include <dr_flac.h>
#include <dr_mp3.h>
#include <stb_vorbis.c>
#include <dr_wav.h>
#include <aesndlib.h>
#include <ogc/message.h>
#include <ogc/lwp.h>
#include <map>
#include <string>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <cstring>

// Header
#include <audiogc.hpp>

#define MAX_FREQUENCY 144000 // AESND's max frequency

// Local variables
namespace {
#if defined(AUDIOGC_LIBOGC_2)
	void aesnd_callback(AESNDPB *pb, unsigned int state) {
		void *cb_arg = AESND_GetVoiceUserData(pb);
#else
	void aesnd_callback(AESNDPB *pb, unsigned int state, void *cb_arg) {
#endif
		if (state == VOICE_STATE_STREAM) {
			audiogc::data_t &data = static_cast<audiogc::player *>(cb_arg)->data;

			void *buffer;

			if (MQ_Receive(data.message_queue, &buffer, MQ_MSG_NOBLOCK) == true)
				AESND_SetVoiceBuffer(pb, buffer, data.buffer_size);
		}
	}

	namespace thread {
		namespace function {
			void stop(audiogc::data_t &data) { // We can't truly "stop" via a thread, but we can set some flags
				if (data.looping == false && data.true_stopped == false) {
					AESNDPB *aesnd_pb = static_cast<AESNDPB *>(data.aesnd_pb);

					AESND_SetVoiceStop(aesnd_pb, true);

					data.playing = false;
				}
			}
			void send(audiogc::data_t &data) {
				MQ_Send(data.message_queue, data.buffers[data.current_buffer], MQ_MSG_BLOCK);

				data.current_buffer = 1 - data.current_buffer;
			}
		}

		void *flac(void *arg) {
			audiogc::data_t &data = static_cast<audiogc::player *>(arg)->data;
			drflac *flac = static_cast<drflac *>(data.audio_handle);

			while (data.playing == true) {
				if (drflac_read_pcm_frames_s16(flac, data.buffer_rate, static_cast<short *>(data.buffers[data.current_buffer])) == 0) {
					drflac_seek_to_pcm_frame(flac, 0);
					data.position = 0;

					function::stop(data);
				} else {
					data.position = flac->currentPCMFrame;

					function::send(data);
				}
			}

			return nullptr;
		}
		void *mp3(void *arg) {
			audiogc::data_t &data = static_cast<audiogc::player *>(arg)->data;
			drmp3 *mp3 = static_cast<drmp3 *>(data.audio_handle);

			while (data.playing == true) {
				if (drmp3_read_pcm_frames_s16(mp3, data.buffer_rate, static_cast<short *>(data.buffers[data.current_buffer])) == 0) {
					drmp3_seek_to_pcm_frame(mp3, 0);
					data.position = 0;

					function::stop(data);
				} else {
					data.position = mp3->currentPCMFrame;

					function::send(data);
				}
			}

			return nullptr;
		}
		void *vorbis(void *arg) {
			audiogc::data_t &data = static_cast<audiogc::player *>(arg)->data;
			stb_vorbis *vorbis = static_cast<stb_vorbis *>(data.audio_handle);

			while (data.playing == true) {
				if (stb_vorbis_get_samples_short_interleaved(vorbis, data.channels, static_cast<short *>(data.buffers[data.current_buffer]), data.buffer_rate) == 0) {
					stb_vorbis_seek_start(vorbis);
					data.position = 0;

					function::stop(data);
				} else {
					data.position = vorbis->current_loc;

					function::send(data);
				}
			}

			return nullptr;
		}
		void *wav(void *arg) {
			audiogc::data_t &data = static_cast<audiogc::player *>(arg)->data;
			drwav *wav = static_cast<drwav *>(data.audio_handle);

			while (data.playing == true) {
				if (drwav_read_pcm_frames_s16(wav, data.buffer_rate, static_cast<short *>(data.buffers[data.current_buffer])) == 0) {
					drwav_seek_to_pcm_frame(wav, 0);
					data.position = 0;

					function::stop(data);
				} else {
					data.position = wav->readCursorInPCMFrames;

					function::send(data);
				}
			}

			return nullptr;
		}
	}

	std::map<audiogc::type, void *(*)(void *)> thread_map = {
		{ audiogc::type::flac, thread::flac },
		{ audiogc::type::mp3, thread::mp3 },
		{ audiogc::type::vorbis, thread::vorbis },
		{ audiogc::type::wav, thread::wav }
	};

	std::map<unsigned char, int> voice_format_map = {
		{ 1, VOICE_MONO16 },
		{ 2, VOICE_STEREO16 }
	};
}

namespace audiogc {

// Internal functions
void player::get_file_data() {
	std::ifstream file(object_data->file_name, std::ios::binary | std::ios::ate);

	object_data->audio_size = file.tellg();
	object_data->audio_data = std::malloc(object_data->audio_size);
	file.seekg(0, std::ios::beg);
	file.read(static_cast<char *>(object_data->audio_data), object_data->audio_size);
}
void player::init() {
	drflac *flac;
	drmp3 *mp3;
	stb_vorbis *vorbis;
	drwav *wav;

	// If type is "detect," check file magic for type
	if (object_data->audio_type == type::detect) {
		constexpr unsigned int magic_size = 4;

		char magic[magic_size];

		if (object_data->in_memory == true) {
			if (object_data->audio_size < magic_size) {
				throw std::runtime_error("Data is too small to detect audio type");
			}

			memcpy(magic, object_data->audio_data, magic_size);
		} else {
			std::ifstream(object_data->file_name, std::ios::binary).read(magic, magic_size);
		}

		if (std::memcmp(magic, "fLaC", 4) == 0) {
			object_data->audio_type = type::flac;
		} else if (std::memcmp(magic, "ID3", 3) == 0) {
			object_data->audio_type = type::mp3;
		} else if (std::memcmp(magic, "OggS", 4) == 0) { // Ogg files can be different formats, but Vorbis is the most common
			object_data->audio_type = type::vorbis;
		} else if (std::memcmp(magic, "RIFF", 4) == 0) {
			object_data->audio_type = type::wav;
		} else {
			throw std::runtime_error("Unable to detect audio type");
		}
	}

	switch (object_data->audio_type) {
		case type::flac:
			if (object_data->in_memory == true) {
				data.audio_handle = drflac_open_memory(object_data->audio_data, object_data->audio_size, nullptr);
			} else {
				if (object_data->file_mode == mode::stream) {
					data.audio_handle = drflac_open_file(object_data->file_name, nullptr);
				} else {
					get_file_data();

					data.audio_handle = drflac_open_memory(object_data->audio_data, object_data->audio_size, nullptr);
				}
			}
			if (data.audio_handle == nullptr) {
				throw std::runtime_error("Failed to create FLAC handle");
			}
			flac = static_cast<drflac *>(data.audio_handle);

			data.max_pitch = MAX_FREQUENCY / flac->sampleRate;

			switch(flac->channels) {
				case 1:
					data.channels = 1;

					break;

				case 2:
					data.channels = 2;

					break;

				default:
					throw std::runtime_error("Unsupported number of channels");
			}

			break;

		case type::mp3:
			data.audio_handle = new drmp3;
			mp3 = static_cast<drmp3 *>(data.audio_handle);

			if (object_data->in_memory == true) {
				drmp3_init_memory(mp3, object_data->audio_data, object_data->audio_size, nullptr);
			} else {
				if (object_data->file_mode == mode::stream) {
					drmp3_init_file(mp3, object_data->file_name, nullptr);
				} else {
					get_file_data();

					drmp3_init_memory(mp3, object_data->audio_data, object_data->audio_size, nullptr);
				}
			}
			if (data.audio_handle == nullptr) {
				throw std::runtime_error("Failed to create MP3 handle");
			}

			data.max_pitch = MAX_FREQUENCY / mp3->sampleRate;

			switch(mp3->channels) {
				case 1:
					data.channels = 1;

					break;

				case 2:
					data.channels = 2;

					break;

				default:
					throw std::runtime_error("Unsupported number of channels");
			}

			break;

		case type::vorbis:
			if (object_data->in_memory == true) {
				data.audio_handle = stb_vorbis_open_memory(static_cast<unsigned char *>(object_data->audio_data), object_data->audio_size, nullptr, nullptr);
			} else {
				if (object_data->file_mode == mode::stream) {
					data.audio_handle = stb_vorbis_open_filename(object_data->file_name, nullptr, nullptr);
				} else {
					get_file_data();

					data.audio_handle = stb_vorbis_open_memory(static_cast<unsigned char *>(object_data->audio_data), object_data->audio_size, nullptr, nullptr);
				}
			}
			if (data.audio_handle == nullptr) {
				throw std::runtime_error("Failed to create Vorbis handle");
			}
			vorbis = static_cast<stb_vorbis *>(data.audio_handle);

			data.max_pitch = MAX_FREQUENCY / vorbis->sample_rate;

			switch(vorbis->channels) {
				case 1:
					data.channels = 1;

					break;

				case 2:
					data.channels = 2;

					break;

				default:
					throw std::runtime_error("Unsupported number of channels");
			}

			break;

		case type::wav:
			data.audio_handle = new drwav;
			wav = static_cast<drwav *>(data.audio_handle);

			if (object_data->in_memory == true) {
				drwav_init_memory(wav, object_data->audio_data, object_data->audio_size, nullptr);
			} else {
				if (object_data->file_mode == mode::stream) {
					drwav_init_file(wav, object_data->file_name, nullptr);
				} else {
					get_file_data();

					drwav_init_memory(wav, object_data->audio_data, object_data->audio_size, nullptr);
				}
			}
			if (data.audio_handle == nullptr) {
				throw std::runtime_error("Failed to create WAV handle");
			}

			data.max_pitch = MAX_FREQUENCY / wav->sampleRate;

			switch(wav->channels) {
				case 1:
					data.channels = 1;

					break;

				default:
					data.channels = 2;
			}

			break;

		default:
			throw std::runtime_error("Invalid type");
	}

	change_settings(); // Set default player settings

	data.buffers[0] = std::malloc(data.buffer_size);
	data.buffers[1] = std::malloc(data.buffer_size);

	data.thread_stackbase = std::malloc(data.thread_stack_size);
}

// Constructors
player::player(type audio_type, const std::string &file_name, mode file_mode) {
	unsigned int file_name_size = file_name.size() + 1;

	object_data = new object_data_t;

	object_data->instances = 1;
	object_data->audio_type = audio_type;
	object_data->in_memory = false;
	object_data->file_mode = file_mode;

	object_data->file_name = new char[file_name_size];
	std::memcpy(object_data->file_name, file_name.c_str(), file_name_size);

	init();
}
player::player(type audio_type, const void *audio_data, unsigned int audio_size) {
	object_data = new object_data_t;

	object_data->instances = 1;
	object_data->audio_type = audio_type;
	object_data->in_memory = true;

	object_data->audio_data = const_cast<void *>(audio_data);
	object_data->audio_size = audio_size;

	init();
}
player::player(const player &other) {
	object_data = other.object_data;

	object_data->instances++;

	init();

	set_looping(other.data.looping);
	set_pitch(other.data.pitch);
	set_volume(other.data.volume);

	change_settings(other.data.buffer_size, data.thread_stack_size, other.data.thread_priority);
}

// Playback functions
void player::pause() {
	AESNDPB *aesnd_pb = static_cast<AESNDPB *>(data.aesnd_pb);

	if (data.true_stopped == true)
		return;

	// Set parameters to stop the thread
	data.playing = false;
	data.true_stopped = true;

	// Stop AESND voice callbacks
	AESND_SetVoiceStop(aesnd_pb, true);

	// Stop the thread
	MQ_Close(data.message_queue);
	LWP_JoinThread(data.thread, nullptr);

	// Free AESND voice
	AESND_FreeVoice(aesnd_pb);
}
bool player::play() {
	AESNDPB *aesnd_pb;

	if (data.playing == false) {
		if (data.true_stopped == false)
			pause();

		data.current_buffer = 0;

		data.playing = true;
		data.true_stopped = false;

		// Initialize AESND voice
#if defined(AUDIOGC_LIBOGC_OG)
		data.aesnd_pb = AESND_AllocateVoiceWithArg(aesnd_callback, this);
#elif defined(AUDIOGC_LIBOGC_2)
		data.aesnd_pb = AESND_AllocateVoice(aesnd_callback);
		AESND_SetVoiceUserData(data.aesnd_pb, this);
#else
		data.aesnd_pb = AESND_AllocateVoice(aesnd_callback, this);
		if (data.aesnd_pb == nullptr) {
			data.playing = false;
			data.true_stopped = true;

			return false;
		}
#endif
		aesnd_pb = static_cast<AESNDPB *>(data.aesnd_pb);

		// Update AESND voice settings
		AESND_SetVoiceStream(aesnd_pb, true);
		AESND_SetVoiceFormat(aesnd_pb, voice_format_map[data.channels]);
		set_volume(data.volume);
		set_pitch(data.pitch);

		// Start the thread
		if (MQ_Init(&data.message_queue, 2) < 0) {
			data.playing = false;
			data.true_stopped = true;

			AESND_FreeVoice(aesnd_pb);

			return false;
		}
		if (LWP_CreateThread(&data.thread, thread_map[object_data->audio_type], this, data.thread_stackbase, data.thread_stack_size, data.thread_priority) < 0) {
			data.playing = false;
			data.true_stopped = true;

			AESND_FreeVoice(aesnd_pb);

			MQ_Close(data.message_queue);

			return false;
		}

		AESND_SetVoiceStop(aesnd_pb, false);
	}

	return true;
}
void player::stop() {
	pause();

	data.position = 0;
}

// Playback state functions
unsigned char player::get_channel_count() { return data.channels; }
double player::get_pitch() { return data.pitch; }
unsigned char player::get_volume() { return data.volume; }
bool player::is_looping() { return data.looping; }
bool player::is_playing() { return data.playing; }
void player::seek(int offset) {
	int was_playing = data.playing;

	drflac *flac = static_cast<drflac *>(data.audio_handle);
	drmp3 *mp3 = static_cast<drmp3 *>(data.audio_handle);
	stb_vorbis *vorbis = static_cast<stb_vorbis *>(data.audio_handle);
	drwav *wav = static_cast<drwav *>(data.audio_handle);

	if (data.true_stopped == false)
		pause();

	switch (object_data->audio_type) {
		case type::flac:
			data.position = offset * flac->sampleRate;
			drflac_seek_to_pcm_frame(flac, data.position);

			break;

		case type::mp3:
			data.position = offset * mp3->sampleRate;
			drmp3_seek_to_pcm_frame(mp3, data.position);

			break;

		case type::vorbis:
			data.position = offset * vorbis->sample_rate;
			stb_vorbis_seek(vorbis, data.position);

			break;

		case type::wav:
			data.position = offset * wav->sampleRate;
			drwav_seek_to_pcm_frame(wav, data.position);

			break;

		default:
			return;
	}

	if (was_playing == true)
		play();
}
void player::set_looping(bool looping) { data.looping = looping; }
double player::set_pitch(double pitch) {
	drflac *flac = static_cast<drflac *>(data.audio_handle);
	drmp3 *mp3 = static_cast<drmp3 *>(data.audio_handle);
	stb_vorbis *vorbis = static_cast<stb_vorbis *>(data.audio_handle);
	drwav *wav = static_cast<drwav *>(data.audio_handle);

	float frequency;

	if (pitch <= 0.0)
		throw std::out_of_range("Pitch must be greater than 0.0");

	pitch = std::min(pitch, data.max_pitch);
	switch (object_data->audio_type) {
		case type::flac:
			frequency = flac->sampleRate;

			break;

		case type::mp3:
			frequency = mp3->sampleRate;

			break;

		case type::vorbis:
			frequency = vorbis->sample_rate;

			break;

		case type::wav:
			frequency = wav->sampleRate;

			break;

		default:
			return 0.0;
	}
	frequency *= pitch;

	if (data.playing == true)
		AESND_SetVoiceFrequency(static_cast<AESNDPB *>(data.aesnd_pb), frequency);

	data.pitch = pitch;

	return pitch;
}
void player::set_volume(unsigned char volume) {
	// Despite being a short, the safe volume range is 0-255
	if (data.playing == true)
		AESND_SetVoiceVolume(static_cast<AESNDPB *>(data.aesnd_pb), volume, volume);

	data.volume = volume;
}
double player::tell() {
	unsigned int sample_rate;

	switch (object_data->audio_type) {
		case type::flac:
			sample_rate = static_cast<drflac *>(data.audio_handle)->sampleRate;

			break;

		case type::mp3:
			sample_rate = static_cast<drmp3 *>(data.audio_handle)->sampleRate;

			break;

		case type::vorbis:
			sample_rate = static_cast<stb_vorbis *>(data.audio_handle)->sample_rate;

			break;

		case type::wav:
			sample_rate = static_cast<drwav *>(data.audio_handle)->sampleRate;

			break;

		default:
			return 0.0;
	}

	return static_cast<double>(data.position) / sample_rate;
}

// Special functions
void player::change_settings(unsigned int buffer_size, unsigned int thread_stack_size, unsigned char thread_priority) {
	data.buffer_size = buffer_size;
	switch (object_data->audio_type) {
		case type::flac:
			data.buffer_rate = data.buffer_size / (static_cast<drflac *>(data.audio_handle)->channels * sizeof(short));

			break;

		case type::mp3:
			data.buffer_rate = data.buffer_size / (static_cast<drmp3 *>(data.audio_handle)->channels * sizeof(short));

			break;

		case type::vorbis:
			data.buffer_rate = data.buffer_size / sizeof(short);

			break;

		case type::wav:
			data.buffer_rate = data.buffer_size / (static_cast<drwav *>(data.audio_handle)->channels * sizeof(short));

			break;

		default:
			break;
	}

	data.thread_stack_size = thread_stack_size;
	data.thread_priority = thread_priority;
}

// Destructor
player::~player() {
	drflac *flac = static_cast<drflac *>(data.audio_handle);
	drmp3 *mp3 = static_cast<drmp3 *>(data.audio_handle);
	stb_vorbis *vorbis = static_cast<stb_vorbis *>(data.audio_handle);
	drwav *wav = static_cast<drwav *>(data.audio_handle);

	stop();

	std::free(data.thread_stackbase);

	std::free(data.buffers[0]);
	std::free(data.buffers[1]);

	switch (object_data->audio_type) {
		case type::mp3:
			drmp3_uninit(mp3);
			delete mp3;

			break;

		case type::wav:
			drwav_uninit(wav);
			delete wav;

			break;

		case type::vorbis:
			stb_vorbis_close(vorbis);

			break;

		case type::flac:
			drflac_close(flac);

			break;

		default:
			return;
	}

	if (--object_data->instances == 0) {
		if ((object_data->in_memory == false) && (object_data->file_mode == mode::store)) {
			std::free(object_data->audio_data);

			delete[] object_data->file_name;
		}

		delete object_data;
	}
}

} // audiogc
