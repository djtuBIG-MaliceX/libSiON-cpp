# GDSiON standalone C++ libification — working notes

GOAL: refactor this Godot GDExtension (SiON synth emulator) into a standalone C++17
library buildable with CMake, with a command-line tool (`gdsion-play`) that plays
MML via PortAudio, and native tests (CTest). Branch: `cpp-libify`. HEAD = f384274
"WIP libify" (contains the OLD session's sed debris; main branch = clean GDExtension
reference for behavior parity checks — use `git show main:<path>` as ground truth).

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
  and continue. NO temp files outside workspace / `C:\Users\djtub\AppData\Local\Temp\opencode`.
- godot-cpp submodule = uninitialized stub; engine sources unavailable and must NOT be fetched.
  Verify Godot-API questions against github raw godot-cpp/godot master headers (network OK) —
  e.g. list.hpp fetched this session: List has get(i)/push_back/Element idiom, NO operator[] but
  GDSiON uses [i] → compat List HAS operator[] added (O(n), matches pinned-tag behavior).

## Build
- Generator "Visual Studio 18 2026" -A x64; cmake 4.2. Configure DONE and works:
  `cmake -S . -B build` (PCRE2 FetchContent builds fine; CMAKE_POLICY_VERSION_MINIMUM handled).
- Build: `cmake --build build --config Debug 2>&1 | Tee-Object build_log_NN.txt` then group errors.
- CLI later: `-DGDSION_BUILD_CLI=ON`; tests `-DGDSION_BUILD_TESTS=ON`; driver stays OUT
  (GDSION_DRIVER_EXPERIMENTAL=OFF; sion_driver.cpp excluded from glob; but sion_driver.h IS
  pulled in by events/*.cpp → must keep parseable via compat stubs: Variant placeholder,
  AudioStream*, PackedVector2Array etc.).
- src/ is include root; compat umbrella FORCE-INCLUDED everywhere (`/FI...sion_compat.h`);
  add same force-include to future cli/tests targets.

## Compat layer (src/compat — DONE, do not regress; it's excluded from all seds)
Umbrella sion_compat.h includes: containers(+templates/singly_linked_list.h), errors, math,
string, regex, audio, callable, random, time + likely/unlikely macros + `using ::sion::vformat`.
- sion_containers.h: `Ref<T>` : public shared_ptr — is_valid/is_null/instantiate(), implicit
  ctor+operator= from raw T* AND related U* (Godot parity), cross-Ref downcast ctor via
  dynamic_pointer_cast (enable_if !is_convertible). `HashMap<K,V>` with KeyValue{.key,.value},
  has/get/insert/erase/size/empty/is_empty. `List<T>` Godot-style: Element{next/prev/self/value/
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
  with rel_location trimming to src/...; all Godot-parity macros; vformat(%s,%d,%%) with typed
  overloads: arithmetic via snprintf (%.14g float, %lld int), bool→"-1"/"0" both specs (matches
  prior session decision), const char[N]→char*, std::string/String direct.
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
  repl, int64_t count=-1)` — IMPORTANT Godot parity: GDSiON calls sub(x,"",true) and old binding
  took count as int64 ⇒ true→1 = replace exactly ONE match (not all!). Implemented literal-replace
  loop w/ empty-match advance. subn not implemented (unused).
- sion_callable.h: `Callable` supporting only std::function<void(double)> — now ONLY used by
  driver remnants; fader uses real std::function already. Phase 2 deletes Callable entirely.
- sion_random.h: PCG32 (pcg-c XSH-RR 64/32 + bounded rejection) + RandomNumberGenerator facade
  randi/randi_range(incl, both directions)/randf/randd/set_seed/randomize. Default seed=time —
  noise-LFO table in SiOPMRefTable::initialize() varies per run (Godot was same); presets
  normally avoid it; goldens will tell.
- singly_linked_list.h restored (class SinglyLinkedList, public `value` field on Element,
  cursor get/set/advance/next/next_safe, static pool initialize_pool/finalize_pool — CALLERS OF
  POOL INIT/FINALIZE STILL UNCHECKED (likely driver; find when linking; library may need explicit init).

## Status: Phase 1 ~80% (libification compile sweep)
Error-line trend: 3000 → 733 → **363** (build_log_05.txt), then several more compat fixes made
WITHOUT rebuild yet. Next action: rebuild into build_log_06.txt and continue fix loop.

### DONE this session
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
1. Rebuild → build_log_06.txt; group errors by file+message (pattern from build_log_05 loop).
2. Dispatch DSP-dispatch agent (chip/ files, recipe): `SiOPMChannelBase::_process_function`
   Callable → PMF `void (SiOPMChannelBase::*_process_function)(int) = nullptr;` FM list
   `std::vector<std::vector<PMF>> _process_function_list` — entries become &SiOPMChannelFM::_x /
   &SiOPMChannelBase::_no_process (verify each process method signature in headers first!);
   `.is_valid()`→`!= nullptr`; `.call(x)`→`(this->*fn)(x)`; `Callable(this,"_no_process")` in
   base.cpp:463 / pcm.cpp:597 → `&SiOPMChannelPCM::_no_process` (each class's own shadow!).
   Same pattern for KS/Sampler if present. Also siopm_operator.cpp 4 errors (check what they are).
3. Sequencer-residue agent (or do myself): mml_sequencer.cpp:341 sort_custom now in compat List ✓;
   :372 prepare_compile header(`const MMLData&`) vs cpp(`const Ref<MMLData>&`) mismatch → unify
   to const Ref<MMLData>& across .h + all overrides/callers (check simml_sequencer.h); sion_regex.cpp
   9 errors unknown → inspect; mml_parser: `user_defs.sort()/.reverse()` → std::sort/std::reverse;
   :242 `std::vector<char32_t> note_letters`+`.find(ch)` → use std::find + distance (keep vector type);
   mml_event.cpp:83-100 `std::vector<int> *r_params; r_params[i]=...` → check `git show
   main:src/sequencer/base/mml_event.cpp` — fix LHS deref vs pointer semantics per main.
4. Vector<RegExMatch> agent wave: 12 sites declare `std::vector<RegExMatch> matches = ...->search_all(...)`
   → must be `std::vector<Ref<RegExMatch>>`; then check element access syntax at each site
   (files: si_effect_stream, translator_util×2, simml_sequencer×5, godot_util, mml_executor_connector,
   mml_parser).
5. ref_table/voice cluster agent: simml_ref_table.h/.cpp 74 errors — `_master_envelopes/_stencil_*`
   vector<SiMMLEnvelopeTable> vs copy_from(Ref<...>) mismatches — READ MAIN versions of
   simml_ref_table.{h,cpp}+simml_envelope_table+simml_voice to establish true member types before
   touching (types were mangled by first session between .h/.cpp). Also sion_voice.cpp 9,
   siopm_ref_table.cpp 7 (register_sampler_data Ref<SampleData> ✓ wired; rng instantiate ✓ works).
6. Effects agent: translator_util.cpp ~22 (String=double sites → wrap with rtos()/itos() — Godot was
   Variant stringification; NOT golden-compared, keep deterministic), siopm_channel_params 13,
   effector stream/composite ~40 (List[] now OK via compat — recheck), stereo effects packed-ops:
   change effect process buffers params/locals to `PackedFloat64Array` where *=/+= used; `fill(x)`
   → std::fill(v.begin(), v.end(), x) or member fill; vector<int> `.append_array(other)` →
   `v.insert(v.end(), o.begin(), o.end())`; std::clamp arg-type mismatches (casts).
7. Rebuild loop until 0 errors; grep for leftover `Object|ClassDB|GDCLASS|Variant|\bCallable\b`
   outside compat/driver before declaring Phase 1 compile-done.
8. Smoke test: tiny exe (tests/smoke or manual main, GDSION_BUILD_CLI not built yet) compiling an
   MML string via SiMMLSequencer/SiONData (compile-only, no driver): `t150 l8 o4 cdefgab>` + a bad
   MML capturing ERROR lines via sion::error_output() sink. Phase 1 milestone.
9. Then Phase 2 (headless driver: update()/render_chunk/std::function callbacks; delete Variant/
   Callable stubs; wire SinglyLinkedList pool init + SiONVoicePresetUtil), Phase 3 CLI, Phase 4 tests.

## sed debris / gotcha checklist (still relevant in residual passes)
- `.empty()()` gone ✓; `std::ector` fixed ✓ but watch for NEW mangled tokens — compiler error text
  is the detector. `#define OP_ERR_FAIL_RANGE(_V)` restored in mml_parser (uses ::sion vformat +
  ERR_FAIL_COND_MSG → golden message "MMLParser: Command '%s' has argument (%d) outside of valid
  range (%d : %d).").
- String+number / String=number sites = Godot Variant implicit conversions; fix by explicit
  itos()/rtos() at the site (never change numeric formatting semantics silently; not golden-compared
  unless in an error message — those keep their own %s/%d args verbatim).
- ERR macros re-enable audit: original upstream commented-out ones do NOT exist (verified 0 hits);
  any `//`ERR line seen now was sed-murdered and is correctly active now.
- HashMap key = sion::String needs std::hash — provided.
- Ref downcast sites (`Ref<AudioStreamWAV> wav = Ref<AudioStream>`) work via dynamic_pointer_cast;
  needs polymorphic base (AudioStream has virtual ~).

## Known Godot-parity facts (verified this session; keep!)
- godot-cpp List (master, fetched): Element::next()/get()/set(), push_back(Element*), find(T)→Element*,
  erase(bool), get(int) random access, sort_custom<C>, NO operator[] — compat adds [i].
- RegEx::sub(subject, replacement, count:int64=-1) since Godot 4.0: GDSiON's `true` == count 1!
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
- 2026-09-05 (session 1): compat v1, CMakeLists, plan (see history above).
- 2026-09-05 (session 2, this one): Configure green. Compat layer completed (string/random/audio/
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
