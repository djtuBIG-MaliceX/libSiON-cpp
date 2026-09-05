# GDSiON standalone C++ libification — working notes

GOAL: refactor this Godot GDExtension (SiON synth emulator) into a standalone C++17
library buildable with CMake, with a command-line tool (`gdsion-play`) that plays
MML via PortAudio, and native tests (CTest). Branch: `cpp-libify`. HEAD = f209d12
"WIP libify" — ALL session-3 work is UNCOMMITTED in the working tree (checkpoint
commit not requested yet; tree is dirty, do not revert). main branch = clean GDExtension
reference for behavior parity checks — use `git show main:<path>` as ground truth.

## Locked decisions (agreed with user)
- C++17, MSVC/GCC/Clang. Library target `gdsion` (static), `GDSION_BUILD_CLI`, `GDSION_BUILD_TESTS`.
- Regex: PCRE2 via FetchContent (Godot parity). Shim in src/compat/sion_regex.{h,cpp}. VERIFIED WORKING.
- Audio backend: PortAudio via FetchContent (+ find_package fallback). WAV export headless.
- Godot glue: REMOVE FULLY at FINAL CLEANUP once C++ tests pass (register_types, SConstruct,
  .gitmodules, godot-cpp/, bin/, doc_classes/, example/, tests/*.gd).
- Errors: ERR_FAIL_*/ERR_PRINT/WARN_PRINT emit SAME message text Godot printed (mml-compilation
  goldens depend on it), via settable sink `sion::error_output()` (default stderr). VERIFIED wording.
- Test corpus: vendor SiONMML tunes tests/data/mml (index-named) in Phase 4; rename goldens.
- voices-sound-consistency goldens (.dat, raw int32 LE) = THE audio-determinism gate.
  DO NOT alter DSP math ever. Float ops byte-identical.
- User workflow: subagents for bulk fixes; do NOT pause for review; dump progress to AGENTS.md
  and continue. MAX 2 CONCURRENT SUBAGENTS (user constraint — run them in waves, never more).
  NO temp files outside workspace / `C:\Users\djtub\AppData\Local\Temp\opencode`.
- godot-cpp submodule = uninitialized stub; engine sources unavailable and must NOT be fetched.
  Verify Godot-API questions against github raw godot-cpp/godot master headers (network OK) —
  e.g. list.hpp fetched this session: List has get(i)/push_back/Element idiom, NO operator[] but
  GDSiON uses [i] → compat List HAS operator[] added (O(n), matches pinned-tag behavior).

## Phases

Phase 1 — Libify the core (chip/effector/sequencer/events/utils):
- Fix corrupted sed artifacts; convert List/HashMap/Callable/Variant usage to compat/std; delete _bind_methods/ClassDB blocks; Callable dispatch → member fn pointers or std::function (channels and sequencer hot paths use raw member fn pointers `void (SiOPMChannelBase::*)(int)`); Object→plain classes; rename godot_util → string_util and fix Packedstd::stringArray.
- Goal: `gdsion_core` compiles; compile-only smoke test (MML parse/compile) works.

Phase 2 — Libify the driver (headless-first):
- SiONDriver: plain class; delete Node lifecycle (call _notification handler explicitly), delete AudioStreamPlayer/Generator; keep the internal ring `_streaming()` render but expose it as `render_chunk()`; replace `render()` PackedFloat64Array → std::vector<double>/caller buffer; signals (streaming, finished, timer_interval...) → std::function callbacks set by the user; background sample/AudioStream input → plain sample buffer struct (float stereo) or feature-cut initially; job queue maintained with explicit `update()` pump.
- Public API for the CLI: SiONDriver::play/compile/render + update pump; note_on/off etc. maintained for tests.

Phase 3 — CLI tool (`gdsion-play`):
- Args: `-f file.mml` / `-m "MML"` / demo list; `-o out.wav` (WAV writer, 16-bit); `-r sample rate` default 44100, `-c channels` 2, `-t time`, `--repeat`, `--events` print MML events/system commands, `--list-voices [filter]`, portaudio device selection `--device`.
- Playback loop: PortAudio callback blocking write; render chunks with the driver; print progress/finish event; graceful Ctrl+C handling.
- Offline mode: render all → WAV (works headless, no audio device required).

Phase 4 — Test porting + CI:
- `gdsion-test` runner + CTest:
  - Port driver-lifecycle (create/driver play/stop state).
  - Port mml-compilation golden comparison: fetch mmltalks_mml.json (nlohmann/json), sort by title/author as the gd tests do; inputs vendored under tests/data/mml (renamed by index/slug to avoid porting the Godot String.hash); golden error text comparison — requires compat error format match (Godot error string parity).
  - voices-sound-consistency: render int32 per preset voice, compare against the existing `data/*.dat` goldens → determinism check against the original GDSiON renderer.
- Optional GitHub workflow for CMake matrix.

Milestones/verification: cmake build; run gdsion-play demo → audio audible; voices test passes byte-identical; mml test error output matches goldens.

## Build
- Generator "Visual Studio 18 2026" -A x64; cmake 4.2. Configure DONE and works:
  `cmake -S . -B build` (PCRE2 FetchContent builds fine; CMAKE_POLICY_VERSION_MINIMUM handled).
- Build: `cmake --build build --config Debug 2>&1 | Tee-Object build_log_NN.txt` then group errors.
- CLI later: `-DGDSION_BUILD_CLI=ON`; tests `-DGDSION_BUILD_TESTS=ON` (already ON in current build
  dir); driver stays OUT (GDSION_DRIVER_EXPERIMENTAL=OFF; sion_driver.cpp excluded from glob; but
  sion_driver.h IS pulled in by events/*.cpp → must keep parseable via compat stubs: Variant
  placeholder, AudioStream*, PackedVector2Array etc.).
- src/ is include root; compat umbrella FORCE-INCLUDED everywhere (`/FI...sion_compat.h`);
  add same force-include to future cli/tests targets.

## Compat layer (src/compat — DONE, do not regress; it's excluded from all seds)
Umbrella sion_compat.h includes: containers(+templates/singly_linked_list.h), errors, math,
string, regex, audio, callable, random, time + likely/unlikely macros + `using ::sion::vformat`.
- sion_containers.h: `Ref<T>` : public shared_ptr — is_valid/is_null/instantiate(), implicit
  ctor+operator= from raw T* AND related U* (Godot parity), cross-Ref downcast ctor via
  dynamic_pointer_cast (enable_if !is_convertible). NEW: static `Ref<T>::borrow(T*)` =
  non-owning alias (no-op deleter) — use wherever code passes raw `this` into a `const Ref&`
  param (Godot INCREF'd; a plain shared_ptr wrap would double-free — see sion_voice.cpp
  clone()/get_as_mml() sites). `HashMap<K,V>` with KeyValue{.key,.value}, has/get/insert/
  erase/size/empty/is_empty, operator[] const+non-const; begin()/end() const return the
  MUTABLE-style iterator (const_cast) so Godot-era loops binding `const KeyValue<K,V> &`
  over const maps keep compiling. `List<T>` Godot-style: Element{next/prev/self/value/
  get/set/operator->}, push_back/push_front(Element*), insert_before/after, erase(E)/erase(T),
  find, at_index, operator[]/get(int) (O(n)), sort()/sort_custom<C>() (STABLE, relink helper),
  reverse(), empty(), range-for.
- sion_string.h: `sion::String : public std::string` + ctors from std::string&/&&, itos/rtos
  globals, std::hash spec. Godot methods implemented: to_int(0x-aware)/to_float/hex_to_int/
  is_valid_int/is_valid_float/length->int/unicode_at/begins_with/ends_with/contains/
  find(String|char)->int64(-1!)/substr(clamped)/left/right/insert(non-mutating)/erase(pos,len)
  mutating/replace/repeat/to_lower/to_upper/pad_zeros/split/get_slice/join(vector)/strip_edges.
  rtos = "%.5f".
- sion_errors.h: sink `sion::error_output()`; format `ERROR: <msg>\n<err>\n  at: f (file:line)\n`
  with rel_location trimming to src/...; all Godot-parity macros incl. ERR_CONTINUE_MSG
  ("Condition \"X\" is true. Continued." — golden ERROR: line carries the custom message);
  vformat(%s,%d,%%) with typed overloads: arithmetic via snprintf (%.14g float, %lld int),
  bool→"-1"/"0" both specs (matches prior session decision), const char[N]→char*,
  std::string/String direct, ENUMS→numeric. IMPORTANT C++17 gotcha: overloads constrain via
  enable_if on the RETURN TYPE — two templates differing only in a default template argument
  = MSVC C2995 redefinition.
- sion_audio.h: `PackedInt32Array`/`PackedFloat32Array` aliases; `PackedFloat64Array : vector<double>`
  CLASS with fill(), append_array(), += -= vector, *= /= scalar (effects rely on these);
  `PackedByteArray : vector<uint8_t>` with decode_s8/decode_s16 (LE); `Vector2{x,y}` +
  `PackedVector2Array`; AudioStream(+WAV w/ Format enum,get_format/get_data/is_stereo/setters,
  Generator,GeneratorPlayback,Player stubs); `SampleData` = Variant replacement for wave input:
  {type: NIL/INT32_ARRAY/FLOAT32_ARRAY/WAVE, int32_samples, float_samples, Ref<AudioStreamWAV>
  wave} + from_floats/from_int32s/from_wave factories; legacy placeholder `Variant` class (enum
  Type incl NIL/STRING/OBJECT/PACKED_*; get_type()) ONLY so sion_driver.h parses — kill in Phase 2.
- sion_regex.h/.cpp: PCRE2. RegExMatch::get_string->sion::String, get_index/get_start/get_end/
  get_group/is_empty; RegEx::create_from_string/search/search_all return Ref<>; `sub(subject,
   repl, int64_t count=-1)` + BOOL overload — USER OVERRIDE 2026-09-06: sub(x,"",true) replaces ALL
   matches (Godot parity NOT required; upstream true→count=1 was a parser bug that left every
   `//` comment after the first one in the MML stream). Implemented literal-replace
   loop w/ empty-match advance. subn not implemented (unused). PCRE2-10.44 API gotchas:
  pcre2_get_error_message_8(errcode, buffer, size) — width arg REMOVED vs older docs; unmatched
  ovector sentinel is PCRE2_UNSET (PCRE2_NOMATCH does not exist); RegExMatch fields are public
  (a free helper in the cpp fills them).
- sion_callable.h: `Callable` supporting only std::function<void(double)> — now ONLY used by
  driver remnants; fader uses real std::function already. Phase 2 deletes Callable entirely.
- sion_random.h: PCG32 (pcg-c XSH-RR 64/32 + bounded rejection) + RandomNumberGenerator facade
  randi/randi_range(incl, both directions)/randf/randd/set_seed/randomize. `using ::sion::
  RandomNumberGenerator;` at end (code uses it unqualified in Ref<> — session-3 fix). Default
  seed=time — noise-LFO table in SiOPMRefTable::initialize() varies per run (Godot was same);
  presets normally avoid it; goldens will tell.
- singly_linked_list.h restored (class SinglyLinkedList, public `value` field on Element,
  cursor get/set/advance/next/next_safe, static pool initialize_pool/finalize_pool — CALLERS OF
  POOL INIT/FINALIZE: only register_types.cpp (excluded target). Library NEEDS an explicit init
  story → Phase 2 add sion::initialize()/finalize() entry points wrapping
  SinglyLinkedList<int/double>::initialize_pool/finalize_pool (+ SiOPMChannelFM::finalize_pool).
  Smoke test currently calls initialize_pool by hand.)

## Status: Phase 1 + Phase 2 COMPLETE; Phase 3 CLI working (offline WAV + realtime PortAudio). Phase 4 test porting NEXT.
Build: 0 errors/warnings. `ctest --test-dir build -C Debug` → 2/2 Passed.
Full dev configure: `cmake -S . -B build -DGDSION_BUILD_TESTS=ON -DGDSION_BUILD_CLI=ON`.

### DONE — session 5 (Phase 2 driver libification + Phase 3 CLI core)
- HEAD = a88e050 "WIP libify" (session-4 work committed). Session-5 changes UNCOMMITTED:
  sion_driver.{h,cpp} rewritten, sion_event.h (virtual ~SiONEvent — dynamic_cast for CLI),
  compat/sion_audio.h (Vector2 ctor; DELETED Variant placeholder + AudioStreamPlayer/Generator/
  Playback stubs), compat/sion_callable.h DELETED (+ umbrella include removed), new
  tests/smoke_driver_render.cpp, new cli/ (CMakeLists.txt, main.cpp, wav_writer.h).
- SiONDriver Phase 2 design (locked): plain class; NO Node/AudioStreamPlayer. Public
  std::function members replace signals: `on_event(Ref<SiONEvent>)` (all SiONEvent dispatch),
  `on_compilation_finished(Ref<SiONData>)`, `on_render_finished(PackedFloat64Array)`,
  `on_timer_interval()`. Play/render/queue APIs are overload-based on `Ref<SiONData>` +
  convenience `play_mml()/render_mml()/queue_render(String)` (Variant gone everywhere).
  Background sample = `Ref<SampleData>`. note_off/sequence_on/off return std::vector<SiMMLTrack*>.
- Realtime model: `render_chunk(float*, frames)` = old `_streaming()` — audio-callback entry
  (PortAudio); internal `_chunk_buffer/_chunk_position` ring so arbitrary callback sizes work;
  master*fader linear gain applied HERE only (replaces AudioStreamPlayer volume; render()
  offline path untouched → goldens safe); pause/suspend/not-streaming = zero-fill.
  `update()` pumps queue jobs + track-event queue (was NOTIFICATION_PROCESS minus streaming);
  STREAM_STARTED dispatch rides first update() after play(). Upstream _find_or_create_track
  arg-swap quirk PRESERVED (main has it too — parity).
- `_update_volume`/Math::linear2db removed from driver (volume via chunk gain).
- CMake: GDSION_DRIVER_EXPERIMENTAL default NOW ON; verified both ON and OFF configure+build clean.
- CLI `gdsion-play` (cli/, opt-in GDSION_BUILD_CLI): -f/-m/--demo [scale|arp|bass] --list-demos,
  -o wav (16-bit PCM writer, header-only cli/wav_writer.h), -t secs (offline default cap 30s;
  realtime: until sequence end w/ auto_stop unless -t), -r (44100 only), -c 1|2, -b block,
  --repeat, --events (system-command dump after compile + live event print), --device idx|substr,
  --list-devices. PortAudio v19.7.0 FetchContent tarball: link target `portaudio_static`
  (set PA_BUILD_SHARED OFF — the SHARED default left a missing-DLL 0xC0000135 at launch).
- Realtime threading: std::mutex gate held by BOTH pa_callback (render_chunk) and main loop
  (update/stop/get_*); event callbacks must stay cheap, never call driver reentrantly (would
  deadlock — non-recursive mutex). Mono driver dup L→R in callback (chip output buffer is stereo).
- VERIFIED: offline WAV peaks non-silent (--demo scale -t 2 → peak 0.176 == smoke test); realtime
  playback through 'Line (Steinberg UR824)' completed w/ progress print; device listing works.

## IMMEDIATE NEXT (Phase 4 kickoff)
1. voices-sound-consistency native test: render int32 per preset voice, compare data/*.dat goldens
   byte-identical — THE determinism gate (study tests/*.gd on main for exact render params!).
2. mml-compilation golden port: mmltalks_mml.json via nlohmann/json; index-named vendored inputs.
3. CLI polish (optional): --list-voices not implemented yet; driver --events thread caveat.
4. Godot glue final cleanup (register_types/SConstruct/.gitmodules/godot-cpp/bin/doc_classes/
   example/tests/*.gd) once Phase 4 goldens pass.

### Session-5 gotchas learned (keep!)
- LIFETIME: driver MUST be destroyed BEFORE sion::finalize() (~MMLSequence→free_all_events uses the
  parser singleton; AV at MMLParser::free_all_events if not — bit us in cli/main.cpp; driver+data
  now live in an inner scope closed before finalize). Early `return` paths SKIP finalize entirely —
  that is safe (leak-at-exit only), don't "fix" by adding finalize-after-return.
- MSVC `setvbuf(stdout, NULL, _IOLBF, 0)` = Debug assert (buffer_size must be ≥2). Use a real size.
- cdb via pwsh: nested quoting of -c breaks; use a command file `-cf cmds.txt` instead.
- ctest on win multi-config needs `-C Debug`; stale exe shows as "Unable to find executable" — build first.

### DONE — session 4
- HEAD moved: c417362 "WIP libify" (earlier work committed). Working tree now has session-4 changes
  (sion_core.{h,cpp} new, singly_linked_list.h, simml_track.cpp, tests/smoke_mml_compile.cpp).
- NEW `src/sion_core.{h,cpp}`: `sion::initialize()/finalize()` — standalone replacements for
  register_types module init (pool init + MMLParser/MMLSequencer/SiOPMRefTable/SiMMLRefTable/
  SiMMLTrack statics; finalize mirrors main's exact order incl. SiOPMChannelFM::finalize_pool).
  ALL consumers must call sion::initialize() first; release all Ref'd objects BEFORE sion::finalize()
  (parser singleton dies there; ~MMLSequence calls free_all_events through it → AV if data outlives it —
  bit us once; tests scope their objects in a block).
- LATENT UPSTREAM UAF FIXED: `SinglyLinkedList<T>::finalize_pool()` left `_element_pool` DANGLING
  (main relies on Godot pooled allocator silently tolerating writes to freed pool from late list
  destructions e.g. SiMMLTrack zero-table in SiMMLTrack::finalize; MSVC debug heap = feeefeee AV).
  Our copy nulls the static after delete → late releases just leak (process exiting). DSP untouched.
  SiMMLTrack::finalize now also nulls `_envelope_zero_table` (double-finalize safety).
- Smoke test CRASH chain was: MMLParser singleton never initialized (get_instance()→null in
  MMLData::clear→initialize→alloc_event); then second data freed after finalize. Test rewritten:
  compile_mml() uses stack `SiMMLSequencer(&chip)` — ctor REQUIRES SiOPMSoundChip* (null → AV in
  _reset_initial_operator_params; driver always passes one, tests must too). Checks: valid MML compiles
  clean + ≥1 sequence; error-sink capture = substring of global sink since last call.
- GOLDEN TRIGGER DECISION: `$unknown_event` CANNOT error (regex group [6] only matches REGISTERED user
  event names; '$' alone is a default token). Use `o20` → golden "ERROR: MMLParser: Command 'o' has
  argument (20) outside of valid range (0 : 9)." VERIFIED matching via compat error format. Range
  triggers available: length/q/@q/o/v/@v/[/] per OP_ERR_FAIL_RANGE sites mml_parser.cpp:365-756.
- Debug tooling: cdb at "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe" works great:
  `& cdb -G -c "sxe -c \`"k 25; q\`" av; g" exe`. NOTE stdout is fully buffered under pipes —
  test sets setvbuf(_IONBF) to keep checkpoint prints visible.

## ~~IMMEDIATE NEXT (Phase 2 kickoff)~~ — DONE session 5; see "IMMEDIATE NEXT (Phase 4 kickoff)" above.

### DONE this session (session 3 — compile-green push)
- 4 agent waves, max 2 concurrent (ref_table+sequencer / chip+utils / effector; orchestrator fixed
  the rest between rebuilds). Agent reports condensed below.
- DSP DISPATCH FINAL DESIGN (do not "fix" back to raw PMFs): derived-only methods' PMFs are NOT
  convertible to a base-class member's PMF type, so `SiOPMChannelBase::_process_function` =
  `std::function<void(int)>`; FM list stays `std::vector<std::vector<void (SiOPMChannelFM::*)(int)>>`
  and `_update_process_function()` wraps via lambda capture `[this, fn=list[..]](int n){(this->*fn)(n);}`;
  base/PCM ctors assign `[this](int n){ _no_process(n); };` (resolves each class's OWN shadow —
  parity with Godot name-dispatch). KS inherits FM's ctor/list like main. GOTCHA: std::bind(PMF,this)
  failed its callability SFINAE vs std::function in this codebase on MSVC (`_Enable_if_callable_t`
  error) — use plain lambdas, NOT std::bind, anywhere in this project.
- prepare_compile/prepare_process unified to `const Ref<MMLData>&` across base .h/.cpp + overrides;
  driver callers pass Refs already (verified). mml_parser: std::sort/reverse, std::find+distance for
  note_letters, RegExMatch→Ref vectors everywhere (12 sites incl. sequencer/effector/utils).
- Effector mass-fix: `v->write[i]` sed-damage = `ptr[i]` → `(*ptr)[i]` across ~15 files + translator
  parse_wav/parse_wavb; buffer types DELIBERATELY stay `std::vector<double>` (base virtuals +
  SiOPMStream::get_buffer_ptr take `std::vector<double>*` — PackedFloat64Array* would NOT convert;
  main's effector code has NO whole-vector ops). std::clamp/min/max casts to double bounds where
  main used Variant coercion. si_effector slot API restored to vector<Ref<SiEffectBase>> per main;
  virtual ~SiEffectBase added (Ref delete safety).
- ref_table cluster: members were already vector<Ref<...>> (agent verified vs main); _setup_ym2413_
  default_voices→vector<Ref<SiMMLVoice>>, _dump_ym2413_register const Ref&; resize+fill(nullptr) →
  assign(n, Ref<T>()) (= main's resize_zeroed+fill); envelope_table restored copy_from(Ref) API +
  FIXED TWO LATENT BUGS vs main (DSP-affecting): discarded `std::clamp` result → `value = std::clamp(
  ...)` and `r_destination[i]` → `(*r_destination)[i]`; _data is SinglyLinkedList<int>*. External
  callers verified clean (simml_data, si_controllable_filter_base, siopm_ref_table:148).
- mml_event get_parameters: `(*r_params)[i] = ...` (main's write[i]); simml_track table->get/set(i,x)
  → (*table)[i]. beats_per_minute clamp double. mml_sequencer.cpp prepare_* bodies reattached.
- Compat additions this session: vformat ENUM overload (+return-type SFINAE fix for ALL three
  template overloads), ERR_CONTINUE_MSG macro, Ref::borrow(), HashMap const operator[] + permissive
  const begin/end, `using ::sion::RandomNumberGenerator;`, regex pcre2-10.44 fixes (see compat section).
- GOLDEN-CRITICAL: restored THREE ERR_CONTINUE_MSG sites in mml_parser.cpp (~261 key-signature,
  ~435 unknown user-event — also prevents UB map-insert-of-null, ~541 unknown standard event) that
  a previous "audit" wrongly believed already-active. Re-audited `//\s*ERR_` repo-wide = clean now
  (siopm_ref_table.cpp:41 stays commented — commented in main too).
- sion_voice.cpp double-free hazards fixed with Ref::borrow: clone() copy_from(this) at ~351 and
  get_as_mml() `Ref<SiONVoice> this_voice = const_cast...` at ~138. NOTE: compat Ref's implicit
  raw-T* ctor OWNS — any future `fn(rawptr)`→const Ref& site is a double-free candidate; prefer borrow().
- Created tests/CMakeLists.txt (gdsion_add_test() helper w/ same /FI force-include) +
  tests/smoke_mml_compile.cpp (Phase-1 milestone test: valid MML clean-compile via error_output()
  sink capture + broken-MML golden error check; currently CRASHING, see next tasks).

### DONE — session 2 (condensed history)
- Compat layer finalized (above). CMake Configure verified green incl. PCRE2.
- Mechanical mega-pass over all src/ except compat (idempotent script:
  `C:\Users\djtub\AppData\Local\Temp\opencode\megapass.ps1` — string/comment-aware):
  `.empty()()`→`.empty()`; Packedstd::stringArray→vector<std::string>; Packedstd::vector2Array→
  PackedVector2Array; **std::ector<→std::vector< (HEAD corruption!)**; forward_list→SinglyLinkedList;
  memnew(/memdelete( → new/delete with BALANCED paren matching; std::shared_ptr<→Ref<;
  \bstd::string\b→sion::String; strip `: public Object/RefCounted` bases; uncomment sed-hidden
  ERR macros (audited vs main — no false positives, original lines were active in main);
  OP_//ERR_FAIL_RANGE repaired; `.append(`→`.push_back(`; `.resize_zeroed(n)`→`.assign(n,0)`;
  dead `#include <forward_list>` removed; `VARIANT_ENUM_CAST`/ENABLE_ENUM_BITWISE lines deleted.
- `_bind_methods()` cpp blocks (42) + .h decls removed by brace-matching script.
- Variant removed from library: SiOPMWavePCMData/SiOPMWaveSamplerData ctors+_prepare_* now take
  `const Ref<SampleData>&` (switch rewritten on SampleData::Type; wave path uses p_data->wave);
  sion_voice/sion_data set_pcm_wave/set_sampler_wave etc → Ref<SampleData>; SiOPMRefTable
  sound_reference → HashMap<sion::String, Ref<SampleData>>. Driver keeps Variant placeholder shim.
- AGENT A1 DONE (sequencer/track callbacks, Callable→typed std::function):
  * MMLSequencer::_event_handlers = vector<std::function<MMLEvent*(MMLEvent*)>>;
    _set/_create_mml_event_listener take it; new protected `_set_default_listener(int, PMF, bool)`
    + `friend class SiMMLSequencer;` for private-base defaults (verified non-virtual/unshadowed);
    Object::cast_to<MMLEvent> dropped at cb call sites.
  * MMLSequence internal-call callbacks: List<std::function<MMLEvent*(int)>> (+getter/add sigs).
  * SiMMLTrack: note_on/off + event_trigger cbs = std::function<void(SiMMLTrack*)>;
    _callback_update_register = std::function<void(int,int)>; setters take typed fn = nullptr.
  * SiMMLSequencer: _callback_timer void(); _callback_beat void(int,int);
    _callback_tempo_changed void(int, bool!!) (_dummy_process is bool);
    _callback_parse_system_command function<bool(const Ref<SiMMLData>&, const Ref<MMLSystemCommand>&)>;
    fwd-decl `class SiMMLData;` added. Added null-guard at trigger_note_on_event (was unguarded).
  * fader_util: std::function<void(double)>, ctor default nullptr.
- Compat List: operator[]/get(int)/reverse()/sort_custom<C>()(stable)/relink; Element::set().

### IMMEDIATE NEXT TASKS (in order)
1. Phase 2 driver libification — see "IMMEDIATE NEXT (Phase 2 kickoff)" above. sion_driver.{h,cpp}
   were never sed-passed cleanly: expect Object/Node/Callable/Variant debris; the compat Variant
   placeholder + sion_callable.h exist ONLY to keep sion_driver.h parsing today — kill them in
   Phase 2. Flip GDSION_DRIVER_EXPERIMENTAL ON when it builds.
2. Phase 3 CLI (`cli/` dir does not exist yet), Phase 4 tests + CI per plan above.

## sed debris / gotcha checklist (still relevant in residual passes)
- `.empty()()` gone ✓; `std::ector` fixed ✓ but watch for NEW mangled tokens — compiler error text
  is the detector. `#define OP_ERR_FAIL_RANGE(_V)` restored in mml_parser (uses ::sion vformat +
  ERR_FAIL_COND_MSG → golden message "MMLParser: Command '%s' has argument (%d) outside of valid
  range (%d : %d).").
- String+number / String=number sites = Godot Variant implicit conversions; fix by explicit
  itos()/rtos() at the site (never change numeric formatting semantics silently; not golden-compared
  unless in an error message — those keep their own %s/%d args verbatim).
- ERR macros re-enable audit: session-2's "clean" claim was WRONG — three ERR_CONTINUE_MSG sites
  in mml_parser were still commented out; restored session 3 via new compat macro. Current re-audit
  of `//\s*(ERR_|WARN_)` = clean. siopm_ref_table.cpp:41 ERR_FAIL_COND_V stays commented — it is
  commented in main too (intentional upstream).
- POINTER-INDEX SED DAMAGE PATTERN: `vecPtr->write[i]`/`vecPtr[i]`-style became bare `ptr[i]`
  (indexes the POINTER, compiles for scalar RHS but is UB / fails for vector RHS). Symptoms:
  "binary '=' no operator takes double" or silently-wrong code. Fix = `(*ptr)[i]`. Swept in
  effector+utils session 3; re-grep `\w+_ptr\[|\b(r_|p_)\w+\[i\]` when touching pointer-param funcs.
- HashMap key = sion::String needs std::hash — provided.
- Ref downcast sites (`Ref<AudioStreamWAV> wav = Ref<AudioStream>`) work via dynamic_pointer_cast;
  needs polymorphic base (AudioStream has virtual ~).

## Known Godot-parity facts (verified this session; keep!)
- godot-cpp List (master, fetched): Element::next()/get()/set(), push_back(Element*), find(T)→Element*,
  erase(bool), get(int) random access, sort_custom<C>, NO operator[] — compat adds [i].
- RegEx::sub: SUPERSEDED 2026-09-06 by user decision — bool arg means REPLACE ALL (upstream Godot
  binding ate `true` as count=1, a bug that broke multi-comment MML files; do not re-port it).
  subn handles $0; sub does literal replacement. Empty-match iteration advances 1 char (both search_all
  and sub implemented that way).
- godot-cpp Ref: implicit ctor/assign from T* raw pointer (shim mirrors); Ref<Derived>↔Ref<Base> via
  cast on assignment — shim uses dynamic_pointer_cast for the non-convertible direction.
- PackedFloat64Array in Godot 4 has elementwise +,-,scalar *,/ — effects use them; class provides them.
- Vector<char32_t>::find() existed in GDSiON's pinned bindings — compat std::vector lacks it (fix at site).
- String::hash/rtos details TBD only if Phase 4 needs golden renaming by hash (murmur3_32 over UTF-32
  — port then; vendored-index rename may avoid entirely).
- ERR macro wording + `ERROR: ` line = golden content (see prior sessions; unchanged).

## Progress log
- 2026-09-06 (session 6): USER OVERRIDE — Godot compat NOT required for RegEx::sub bool arg;
  `true` = replace ALL (SiON semantics: strip every `//...` comment before macro expansion). Added
  bool overload in sion_regex.h mapping true→-1. Verified with legacy SiON example
  "D:\MY SHIT\djtbmx_td1.txt" (#OPN@/#OPL@ patches, A-Z macros, `$` segno loops): was 0 sequences +
  flood of "Unknown standard event '/'" errors → now 11 sequences clean, WAV renders w/ musical
  content (RMS rises w/ arrangement; dense sections hit full-scale). ctest 2/2. Note: SiON MML ref =
  https://keim.github.io/SiON/mmlref/sion_mml_reference_e.html (macros #A=.. referenced BARE as A,
  `^` extension, `|` repeat-break inside [...], `%n,m` module select).
- 2026-09-05 (session 1): compat v1, CMakeLists, plan (see history above).
- 2026-09-05 (session 3): Phase 1 COMPILE GREEN — errors 342→0 across build_log_06..10, zero
  warnings; `cmake -S . -B build -DGDSION_BUILD_TESTS=ON` configured. Subagent waves (≤2 concurrent
  per user request) + orchestrator fixes: see "DONE this session" above for the full list — key
  items: DSP dispatch via std::function+lambdas, `(*ptr)[i]` deref sweep, ref_table API restore,
  prepare_compile/prepare_process unified to Ref, ERR_CONTINUE_MSG macro + 3 golden-restored sites,
  compat upgrades (borrow/HashMap/vformat-enum/regex-10.44/random using-decl). New files:
  tests/CMakeLists.txt + tests/smoke_mml_compile.cpp (CRASHES — data->clear() AV, WIP; has debug
  [step] prints to remove). Uncommitted changes on top of f209d12. build_log_06..11.txt in repo root.
  Phase 2 not started; sion_driver.cpp untouched (new debris noted there, e.g. std::stringName).
- 2026-09-05 (session 2): Configure green. Compat layer completed (string/random/audio/
  callable/umbrella + List/Ref/vformat upgrades). Mega-pass executed clean (revert-restart once:
  first memnew sed left unbalanced parens — transformer now balanced-paren aware; megapass.ps1 is
  idempotent, lives in opencode temp). bind_methods removal (42 blocks) ×2 (once lost to revert).
  Variant→SampleData across wave ingestion. Macro-uncomment audit vs main = clean. Callable→std::function
  for sequencer/track/fader via subagent A1 (see summary above; friend+PMF helper added).
  Errors 3000→733→363→pending rebuild. NOT done: DSP PMF conversion (chip files), Vector<RegExMatch>
  sites, ref_table type mismatches, effects packed-ops, mml_parser fixes, prepare_compile mismatch,
  sion_regex.cpp own errors, smoke test, Phase 2+.
  Files: build_log_0[1-5].txt in repo root (delete later); temp scripts megapass.ps1 + singly backup
  in %LOCALAPPDATA%\Temp\opencode.
