/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

// gdsion-play — Phase 3 command-line tool: render MML offline to WAV, or play
// it back in realtime through PortAudio.

#include "portaudio.h"

#include "sequencer/base/mml_sequence_group.h"
#include "sequencer/base/mml_system_command.h"
#include "sion_core.h"
#include "sion_data.h"
#include "sion_driver.h"
#include "events/sion_track_event.h"
#include "wav_writer.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Shared gate between the audio callback thread and the main loop, mirroring
// the single-threaded model of the Godot version (the old AudioStreamPlayer
// pulled samples on the main thread too).
struct DriverGate {
	std::mutex mutex;
	SiONDriver *driver = nullptr;
	bool mono_to_stereo = false;
};

static std::atomic<bool> g_quit = false;

static void handle_signal(int) {
	g_quit = true;
}

static int pa_callback(const void *, void *output, unsigned long frame_count, const PaStreamCallbackTimeInfo *, PaStreamFlags, void *user_data) {
	DriverGate *gate = static_cast<DriverGate *>(user_data);
	std::lock_guard<std::mutex> lock(gate->mutex);

	gate->driver->render_chunk(static_cast<float *>(output), (int)frame_count);

	if (gate->mono_to_stereo) {
		float *buffer = static_cast<float *>(output);
		for (unsigned long i = 0; i < frame_count; i++) {
			buffer[i * 2 + 1] = buffer[i * 2];
		}
	}
	return paContinue;
}

struct Demo {
	const char *name;
	const char *description;
	const char *mml;
};

static const Demo DEMOS[] = {
	{ "scale", "Chromatic scale run", "t150 l8 o4 cdefgab>c<" },
	{ "arp", "Arpeggio pattern", "t160 l16 o4 cegc o5 <ceg<c" },
	{ "bass", "Low bass walk", "t120 l8 o3 c d e f g a b >c" },
};

static void print_usage(const char *p_argv0) {
	std::printf(
			"gdsion-play — play or render SiON MML music\n"
			"\n"
			"usage: %s [-f file.mml | -m \"MML\" | --demo name] [options]\n"
			"\n"
			"source options:\n"
			"  -f FILE          load MML from a file\n"
			"  -m MML           MML string\n"
			"  --demo NAME      play a built-in demo (--list-demos)\n"
			"\n"
			"output options:\n"
			"  -o FILE.wav      offline mode: render to a 16-bit PCM WAV instead of playing\n"
			"  -t SECONDS       rendering/playback time limit (default: realtime until the\n"
			"                   sequence ends, offline: %.0f s)\n"
			"  -r RATE          sample rate (SiON only supports 44100)\n"
			"  -c CHANNELS      1 (mono) or 2 (stereo, default)\n"
			"  -b SIZE          render block size: 2048 (default), 4096 or 8192\n"
			"  --repeat         loop playback until interrupted\n"
			"\n"
			"device options:\n"
			"  --device SPEC    output device index or name substring\n"
			"  --list-devices   list PortAudio output devices and exit\n"
			"\n"
			"misc:\n"
			"  --events         print MML system commands and live event notifications\n"
			"  -h, --help       this message\n",
			p_argv0, 30.0);
}

static void list_demos() {
	for (const Demo &demo : DEMOS) {
		std::printf("%-12s %s\n", demo.name, demo.description);
	}
}

static bool read_file(const std::string &p_path, std::string &r_out) {
	std::ifstream file(p_path, std::ios::binary);
	if (!file) {
		return false;
	}
	std::ostringstream contents;
	contents << file.rdbuf();
	r_out = contents.str();
	return true;
}

static void dump_system_commands(const Ref<SiONData> &p_data) {
	const List<Ref<MMLSystemCommand>> &commands = p_data->get_system_commands();
	if (commands.empty()) {
		std::printf("(no system commands)\n");
		return;
	}
	for (const Ref<MMLSystemCommand> &command : commands) {
		std::printf("%s %d : %s%s\n", command->command.c_str(), command->number, command->content.c_str(), command->postfix.c_str());
	}
}

static void print_event(const Ref<SiONEvent> &p_event) {
	SiONTrackEvent *track_event = dynamic_cast<SiONTrackEvent *>(p_event.get());
	if (track_event) {
		std::printf("[event] %-20s note=%d id=%d buffer=%d\n",
				p_event->get_event_type().c_str(), track_event->get_note(),
				track_event->get_event_trigger_id(), track_event->get_buffer_index());
	} else {
		std::printf("[event] %s\n", p_event->get_event_type().c_str());
	}
	fflush(stdout);
}

int main(int argc, char **argv) {
	setvbuf(stdout, nullptr, _IOLBF, 4096);

	std::string mml_file;
	std::string mml_string;
	std::string demo_name;
	std::string out_path;
	std::string device_spec;
	double time_limit = -1;
	int channels = 2;
	int sample_rate = 44100;
	int block_size = 2048;
	bool repeat = false;
	bool show_events = false;
	bool list_devices = false;

	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		auto next_value = [&](const char *p_option) -> const char * {
			if (i + 1 >= argc) {
				std::printf("error: %s requires a value\n", p_option);
				exit(1);
			}
			return argv[++i];
		};

		if (arg == "-f") {
			mml_file = next_value("-f");
		} else if (arg == "-m") {
			mml_string = next_value("-m");
		} else if (arg == "--demo") {
			demo_name = next_value("--demo");
		} else if (arg == "--list-demos") {
			list_demos();
			return 0;
		} else if (arg == "-o") {
			out_path = next_value("-o");
		} else if (arg == "-t") {
			time_limit = std::atof(next_value("-t"));
		} else if (arg == "-r") {
			sample_rate = std::atoi(next_value("-r"));
		} else if (arg == "-c") {
			channels = std::atoi(next_value("-c"));
		} else if (arg == "-b") {
			block_size = std::atoi(next_value("-b"));
		} else if (arg == "--repeat") {
			repeat = true;
		} else if (arg == "--events") {
			show_events = true;
		} else if (arg == "--device") {
			device_spec = next_value("--device");
		} else if (arg == "--list-devices") {
			list_devices = true;
		} else if (arg == "-h" || arg == "--help") {
			print_usage(argv[0]);
			return 0;
		} else {
			std::printf("error: unknown option '%s'\n\n", arg.c_str());
			print_usage(argv[0]);
			return 1;
		}
	}

	if (sample_rate != 44100) {
		std::printf("error: SiON only supports a sample rate of 44100 Hz.\n");
		return 1;
	}
	if (channels != 1 && channels != 2) {
		std::printf("error: channel count must be 1 or 2.\n");
		return 1;
	}
	if (block_size != 2048 && block_size != 4096 && block_size != 8192) {
		std::printf("error: block size must be 2048, 4096 or 8192.\n");
		return 1;
	}

	PaError pa_status = Pa_Initialize();
	if (pa_status != paNoError) {
		std::printf("error: PortAudio failed to initialize: %s\n", Pa_GetErrorText(pa_status));
		return 2;
	}

	int exit_code = 0;

	{
		sion::initialize();

		// NOTE: the driver (and every Ref'd object) must die before sion::finalize() —
		// ~MMLSequence frees events through the parser singleton which finalize kills.
		{
			SiONDriver driver(block_size, channels, sample_rate, 0);

		// --- Device listing needs no MML source.
		if (list_devices) {
			int count = Pa_GetDeviceCount();
			for (int i = 0; i < count; i++) {
				const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
				if (info->maxOutputChannels > 0) {
					std::printf("%2d  %s%s\n", i, info->name, i == Pa_GetDefaultOutputDevice() ? "  (default)" : "");
				}
			}
			Pa_Terminate();
			return 0;
		}

		// --- Resolve the MML source.
		std::string mml;
		if (!mml_file.empty()) {
			if (!read_file(mml_file, mml)) {
				std::printf("error: cannot read '%s'\n", mml_file.c_str());
				Pa_Terminate();
				return 1;
			}
		} else if (!mml_string.empty()) {
			mml = mml_string;
		} else if (!demo_name.empty()) {
			bool found = false;
			for (const Demo &demo : DEMOS) {
				if (demo_name == demo.name) {
					mml = demo.mml;
					found = true;
					break;
				}
			}
			if (!found) {
				std::printf("error: unknown demo '%s' (--list-demos)\n", demo_name.c_str());
				Pa_Terminate();
				return 1;
			}
		} else {
			print_usage(argv[0]);
			Pa_Terminate();
			return 1;
		}

		// --- Compile.
		Ref<SiONData> data = driver.compile(mml);
		if (data.is_null()) {
			std::printf("error: MML compilation failed.\n");
			Pa_Terminate();
			return 1;
		}

		int sequence_count = data->get_sequence_group()->get_sequence_count();
		std::printf("Compiled %d sequence(s) in %d ms\n", sequence_count, driver.get_compiling_time());

		if (show_events) {
			dump_system_commands(data);
			driver.on_event = print_event;
		}

		if (!out_path.empty()) {
			// --- Offline mode: render everything into memory and write a WAV.
			double seconds = (time_limit > 0 ? time_limit : 30.0);
			int total_samples = (int)(seconds * sample_rate) * channels;

			std::printf("Rendering %.1f s to '%s'...\n", seconds, out_path.c_str());
			PackedFloat64Array buffer = driver.render(data, total_samples, channels, true);

			double peak = 0;
			for (double value : buffer) {
				peak = std::max(peak, std::abs(value));
			}

			if (!WavWriter::write(out_path, buffer.data(), buffer.size(), channels, sample_rate)) {
				std::printf("error: cannot write '%s'\n", out_path.c_str());
				exit_code = 1;
			} else {
				std::printf("Wrote %zu samples (peak %.3f) in %d ms.\n", buffer.size(), peak, driver.get_rendering_time());
			}
		} else {
			// --- Realtime mode through PortAudio.
			int device = Pa_GetDefaultOutputDevice();
			if (!device_spec.empty()) {
				bool is_index = device_spec.find_first_not_of("0123456789") == std::string::npos;
				int count = Pa_GetDeviceCount();
				int matched = -1;
				for (int i = 0; i < count && matched < 0; i++) {
					if (is_index) {
						if (i == std::atoi(device_spec.c_str())) {
							matched = i;
						}
					} else if (Pa_GetDeviceInfo(i)->maxOutputChannels > 0) {
						std::string name = Pa_GetDeviceInfo(i)->name;
						for (auto &c : name) {
							c = (char)tolower((unsigned char)c);
						}
						std::string spec = device_spec;
						for (auto &c : spec) {
							c = (char)tolower((unsigned char)c);
						}
						if (name.find(spec) != std::string::npos) {
							matched = i;
						}
					}
				}
				if (matched < 0) {
					std::printf("error: no output device matches '%s' (--list-devices)\n", device_spec.c_str());
					Pa_Terminate();
					return 1;
				}
				device = matched;
			}

			DriverGate gate;
			gate.driver = &driver;
			gate.mono_to_stereo = (channels == 1);

			PaStream *stream = nullptr;
			PaStreamParameters output_params = {};
			output_params.device = device;
			output_params.channelCount = 2;
			output_params.sampleFormat = paFloat32;
			output_params.suggestedLatency = Pa_GetDeviceInfo(device)->defaultLowOutputLatency;
			output_params.hostApiSpecificStreamInfo = nullptr;

			pa_status = Pa_OpenStream(&stream, nullptr, &output_params, sample_rate,
					paFramesPerBufferUnspecified, 0, pa_callback, &gate);
			if (pa_status != paNoError) {
				std::printf("error: cannot open output stream: %s\n", Pa_GetErrorText(pa_status));
				Pa_Terminate();
				return 2;
			}

			driver.set_auto_stop(true);

			std::signal(SIGINT, handle_signal);
			std::signal(SIGTERM, handle_signal);

			const char *device_name = Pa_GetDeviceInfo(device)->name;
			std::printf("Playing via '%s' (Ctrl+C to stop)\n", device_name);

			int pass = 0;
			do {
				pass++;
				{
					std::lock_guard<std::mutex> lock(gate.mutex);
					driver.play(data);
				}

				pa_status = Pa_StartStream(stream);
				if (pa_status != paNoError) {
					std::printf("error: cannot start stream: %s\n", Pa_GetErrorText(pa_status));
					exit_code = 2;
					break;
				}

				auto loop_start = std::chrono::steady_clock::now();
				int last_printed = -1;
				bool streaming_seen = false;
				while (true) {
					double position_ms = 0;
					bool streaming = false;
					{
						std::lock_guard<std::mutex> lock(gate.mutex);
						driver.update();
						position_ms = driver.get_streaming_position();
						streaming = driver.is_streaming();
					}
					streaming_seen = streaming_seen || streaming;

					if (g_quit) {
						break;
					}
					if (time_limit > 0 && position_ms >= time_limit * 1000.0) {
						break;
					}
					if (streaming_seen && !streaming) {
						break; // Sequence finished (auto-stop).
					}

					int whole_seconds = (int)(position_ms / 1000.0);
					if (whole_seconds != last_printed) {
						last_printed = whole_seconds;
						if (time_limit > 0) {
							std::printf("\r  %d / %.1f s   ", whole_seconds, time_limit);
						} else {
							std::printf("\r  %d s   ", whole_seconds);
						}
						fflush(stdout);
					}

					Pa_Sleep(10);
				}
				std::printf("\n");

				{
					std::lock_guard<std::mutex> lock(gate.mutex);
					driver.stop();
				}
			} while (repeat && !g_quit);

			Pa_StopStream(stream);
			Pa_CloseStream(stream);
			std::printf("Done (%d pass(es)).\n", pass);
		}
		}

		sion::finalize();
	}

	Pa_Terminate();
	return exit_code;
}
