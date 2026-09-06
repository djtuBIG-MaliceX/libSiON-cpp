# libSiON-cpp

**libSiON-cpp** is a standalone C++17 software synthesizer library, forked from [GDSiON](https://github.com/YuriSizov/gdsion) — the Godot GDExtension port of [SiON](https://github.com/keim/SiON), keim_at_Si's software synthesizer originally written in ActionScript 3 for Flash and Adobe AIR.

The Godot glue has been removed entirely: what remains is a self-contained, dependency-light native library that compiles MML (a SiON-specific flavor of Music Macro Language) and renders it to audio — with byte-identical output to the original GDSiON renderer. Use it as a static library in your own application, or render and play music right away with the bundled `sion-cpp-play` command-line tool.

The name of the synthesizer should be pronounced like the word "_scion_".

Try it out online [here](https://codingchords.com/junk/sion-mml-web) - visualizer available :)

## Features

- SiON MML compiler and sequencer (macros `#A{...}`, system commands like `#OPM@`, per-channel patching with `#`, loops, user events, ...)
- The full SiON sound chip emulation: OPM / OPL / OPN / MA3 algorithm voices as implemented in GDSiON.
- Effectors (delay, chorus, fader, filters, ...) wired through `SiONDriver`
- Offline rendering straight to a buffer, or realtime streaming through PortAudio
- Deterministic DSP — the render output is verified against captured goldens from the original renderer

## Building

Requirements: CMake 3.20+ and a C++17 compiler (MSVC, GCC, Clang). PCRE2 and (for the CLI) PortAudio are pulled in automatically via `FetchContent`.

```shell
cmake -S . -B build -DLIBSION_BUILD_CLI=ON -DLIBSION_BUILD_TESTS=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Debug
```

| Option | Default | Meaning |
| --- | --- | --- |
| `LIBSION_BUILD_CLI` | `OFF` | Build the `sion-cpp-play` command-line player |
| `LIBSION_BUILD_TESTS` | `OFF` | Build the native CTest suites (MML goldens, audio determinism) |
| `LIBSION_DRIVER_EXPERIMENTAL` | `ON` | Include `SiONDriver` in the library |
| `LIBSION_BUILD_WEB` | auto (`ON` under Emscripten) | Build the browser player module from `web/` |

The static library is produced as `libSiONcpp.a` / `libSiONcpp.so` on Linux, `libSiONcpp.a` on macOS and `SiONcpp.lib` on Windows.

## Command line

`sion-cpp-play` renders MML to a WAV file or plays it back in realtime:

```shell
# Play a built-in demo out loud (PortAudio).
sion-cpp-play --demo scale

# Render a tune to a 16-bit PCM WAV (no audio device required).
sion-cpp-play -m "t100 l8 [ ccggaag4 ffeeddc4 | [ggffeed4]2 ]2" -o tune.wav

# Play an .sionmml / .mml file for at most 60 seconds, looping.
sion-cpp-play -f song.sionmml -t 60 --repeat

# List output devices and pick one.
sion-cpp-play --list-devices
sion-cpp-play -f song.mml --device "line out"
```

Run `sion-cpp-play --help` for the full option list. The SiON MML syntax reference lives [here](https://keim.github.io/SiON/mmlref/sion_mml_reference_e.html).

## Web (Emscripten)

The library cross-compiles to WebAssembly and ships with a single-page editor/player
(`web/index.html`) that plays MML directly in the browser — no server-side component,
no Godot. The page keeps the classic `?songdata=<base64>` URL convention of the old
GDSiON web demo, so existing song links keep working.

Build with [emsdk](https://emscripten.org/docs/getting_started/downloads.html) (Ninja required on Windows):

```shell
emcmake cmake -S . -B build_web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_web
```

This produces `build_web/web/` containing the module (`sion_wasm.js` + `sion_wasm.wasm`),
the page, and its engine glue. Serve that directory over HTTP and open it (AudioContext
needs a real origin, `file://` won't do):

```shell
cd build_web/web && python -m http.server 8000   # then browse http://localhost:8000
node test_node.js                                # headless smoke test of the module
```

Audio is pulled with a main-thread `ScriptProcessorNode`: Chrome's `AudioWorkletGlobalScope`
exposes no `fetch`/`importScripts` and forbids `import()`, so hosting the WASM inside an
AudioWorklet would additionally require SharedArrayBuffer plus COOP/COEP cross-origin-
isolation headers — a deployment burden not worth it for this use case. The context rate is
requested at 44100 Hz; when the browser forces another device rate (Safari) the engine
resamples on the fly.

## Using the library

```cpp
#include "sion_core.h"
#include "sion_driver.h"

int main() {
    sion::initialize();
    {
        // One driver at a time: some internal global state is shared.
        SiONDriver driver(2048, 2, 44100, 0);

        // Offline render into memory (caller gets an interleaved float buffer).
        Ref<SiONData> data = driver.compile("t100 l8 cdefgab>c<");
        PackedFloat64Array samples = driver.render(data, 44100 * 2, 2, true);

        // Or realtime: feed your audio callback from render_chunk() and
        // pump update() from the main loop. on_event receives SiONEvent
        // notifications (stream started/stopped, note events, ...).
    }
    sion::finalize();
}
```

Link against `SiONcpp` (`target_link_libraries(app PRIVATE SiONcpp::SiONcpp)` when using this repo via CMake). Note that every object referencing library internals must be released before `sion::finalize()`.

## Testing

The native test suite doubles as the behavioral contract:

- `mml_compilation` — compiles 212 songs from the original SiONMML corpus and diffs all compiler error output against goldens captured from GDSiON.
- `voices_sound_consistency` — renders every one of the 654 preset voices and compares raw int32 captures against the GDSiON renderer, byte for byte.
- `smoke_mml_compile` / `smoke_driver_render` — compile/parse sanity and driver lifecycle checks (including portamento glide).

## Provenance

- [SiON](https://github.com/keim/SiON) by @keim_at_Si — the original ActionScript 3 synthesizer.
- [GDSiON](https://github.com/YuriSizov/gdsion) by @YuriSizov and contributors — the Godot port this fork is built from, including the libification groundwork.

## AI DISCLOSURE

While the fork was initially attempted by hand, much of the completion effort was eventually co-assisted by [Qwen3.8-flash-next UD_Q4_K_XL](https://huggingface.co/unsloth/Qwen3.8-Flash-Next-GGUF/tree/main/UD-Q4_K_XL) operating on a Mac Studio M3 Ultra 256GB local AI/ML setup via opencode v1.

Either way, all authorship and resulting outputs are humanly preserved with intention.

## License

This project is provided under an [MIT license](LICENSE). Original SiON software synthesizer library is provided under an [MIT license](https://github.com/keim/SiON/blob/1e6d6cd20bbc0379f5a81f607ac87a105163648f/LICENSE.md).
