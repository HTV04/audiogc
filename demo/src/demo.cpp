// AudiOGC Audio Demo v1.0.0
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

#include <gccore.h>
#include <aesndlib.h>
#include <fat.h>
#if !defined(HW_DOL)
#include <wiiuse/wpad.h>
#endif
#include <audiogc.hpp>
#if defined(HW_DOL)
#include <ogc/pad.h>
#endif
#include <string>
#include <tuple>
#include <vector>
#include <iostream>

#if defined(HW_DOL)
#include <mp3_file_mp3.h>
#include <vorbis_file_ogg.h>
#include <wav_file_wav.h>
#endif

int main(int argc, char **argv) {
	void *xfb;
	GXRModeObj *rmode;

#if defined(HW_DOL)
	std::vector<std::tuple<audiogc::type, const void *, unsigned int, const char *, const char *>> types = {
		{ audiogc::type::mp3, mp3_file_mp3, mp3_file_mp3_size, "MP3", "Bleeping Demo" },
		{ audiogc::type::vorbis, vorbis_file_ogg, vorbis_file_ogg_size, "Ogg Vorbis", "Cipher" },
		{ audiogc::type::wav, wav_file_wav, wav_file_wav_size, "WAV (11,025 Hz)", "Local Forecast" },
	};
#else
	std::string executable_path;

	std::vector<std::tuple<audiogc::type, const char *, const char *>> types = {
		{ audiogc::type::flac, "flac_file.flac", "FLAC" },
		{ audiogc::type::mp3, "mp3_file.mp3", "MP3" },
		{ audiogc::type::vorbis, "vorbis_file.ogg", "Ogg Vorbis" },
		{ audiogc::type::wav, "wav_file.wav", "WAV" }
	};
#endif
	unsigned int cur_type = 0;
	auto type = types[cur_type];

	audiogc::player *player;

	bool no_error = true;
	double pitch = 1.0;

	VIDEO_Init();

	// Necessary for AudiOGC!
	AESND_Init();
#if !defined(HW_DOL)
	WPAD_Init();
	fatInitDefault();
#endif

#if defined(HW_DOL)
	PAD_Init();
#endif

	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	CON_Init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

#if defined(HW_DOL)
	player = new audiogc::player(std::get<0>(type), std::get<1>(type), std::get<2>(type));
#else
	if (argc > 0) {
		executable_path = argv[0];
		executable_path = executable_path.substr(0, executable_path.find_last_of("/") + 1);
	} else {
		executable_path = "sd:/audiogc-demo/";
	}
	player = new audiogc::player(std::get<0>(type), executable_path + std::get<1>(type));
#endif
	player->set_looping(true);
	no_error = player->play();

	while(1) {
#if defined(HW_DOL)
		PAD_ScanPads();

		u32 pressed = PAD_ButtonsDown(0);

		if ((pressed & PAD_BUTTON_LEFT) || (pressed & PAD_BUTTON_RIGHT)) {
			if (pressed & PAD_BUTTON_LEFT) {
#else
		WPAD_ScanPads();

		u32 pressed = WPAD_ButtonsDown(0);

		if ((pressed & WPAD_BUTTON_LEFT) || (pressed & WPAD_BUTTON_RIGHT)) {
			if (pressed & WPAD_BUTTON_LEFT) {
#endif
				if (cur_type == 0)
					cur_type = types.size();

				cur_type--;
			} else {
				cur_type++;

				if (cur_type == types.size())
					cur_type = 0;
			}

			type = types[cur_type];

			delete player;
#if defined(HW_DOL)
			player = new audiogc::player(std::get<0>(type), std::get<1>(type), std::get<2>(type));
#else
			player = new audiogc::player(std::get<0>(type), executable_path + std::get<1>(type));
#endif
			player->set_looping(true);
			no_error = player->play();

			pitch = 1.0;
		}

#if defined(HW_DOL)
		if ((pressed & PAD_BUTTON_UP) || (pressed & PAD_BUTTON_DOWN)) {
			if (pressed & PAD_BUTTON_UP) {
#else
		if ((pressed & WPAD_BUTTON_UP) || (pressed & WPAD_BUTTON_DOWN)) {
			if (pressed & WPAD_BUTTON_UP) {
#endif
				pitch += 0.1;
			} else {
				pitch -= 0.1;

				if (pitch < 0.1)
					pitch = 0.1;
			}

			pitch = player->set_pitch(pitch);
		}

#if defined(HW_DOL)
		if (pressed & PAD_BUTTON_START)
#else
		if (pressed & WPAD_BUTTON_HOME)
#endif
			std::exit(0);

		std::cout << "\x1b[2J\x1b[3;0H";

		std::cout << "\t\tAudiOGC v" << AUDIOGC_VERSION_STRING << "\n";
		std::cout << "\t\tAudio Demo\n";
		std::cout << "\t\thttps://github.com/HTV04/audiogc\n\n";

#if defined(HW_DOL)
		std::cout << "\t\tType: " << std::get<3>(type) << "\n";
#else
		std::cout << "\t\tType: " << std::get<2>(type) << "\n";
#endif
		std::cout << "\t\tPosition: " << player->tell() << "\n";
		std::cout << "\t\tPitch: " << pitch << "\n\n";

		std::cout << "\t\tPress left/right to change audio\n";
		std::cout << "\t\tPress up/down to change pitch\n";
#if defined(HW_DOL)
		std::cout << "\t\tPress START to exit\n\n";

		if (no_error == true) {
			std::cout << "\t\t\"" << std::get<4>(type) << "\" Kevin MacLeod (incompetech.com)\n";
			std::cout << "\t\tLicensed under Creative Commons: By Attribution 4.0 License\n";
			std::cout << "\t\thttp://creativecommons.org/licenses/by/4.0/\n";
#else
		std::cout << "\t\tPress the HOME button to exit\n\n";

		if (no_error == true) {
			std::cout << "\t\tTip: Replace the audio files with your own!\n\n";
#endif
		} else {
			std::cout << "\t\tOops, looks like there was an issue starting the audio :(\n\n";

			std::cout << "\t\tPlease report any issues here:\n";
			std::cout << "\t\t\thttps://github.com/HTV04/audiogc/issues\n\n";
		}

		VIDEO_WaitVSync();
	}

	return 0;
}
