/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

// Main-thread player engine: hosts the Emscripten module and pulls audio with
// a ScriptProcessorNode sized to the driver's own block (2048 frames). See
// web/CMakeLists.txt for why WASM cannot live in an AudioWorklet.
//
// The driver renders at its fixed 44100 Hz; if the AudioContext runs at any
// other rate (Safari keeps the device rate) a linear-interpolating windowed
// resampler bridges the two.

// Channel-visualizer snapshot ABI, mirroring VIS_* in web/sion_web.cpp:
// per row (46 int32): channel_type, channel_number(-1=auto), module_type,
// program, note_count, reserved; then 8 notes of pitch(1/64 semitone live
// index), volume(permille), pan(0..127), flags, sweep-target(pitch index).
const SION_VIS = {
	MAX_ROWS: 26,
	NOTES_PER_ROW: 8,
	ROW_INTS: 46,
	KEY_ON: 1,
	SWEEP: 2, // portamento (`po`) / pitch-bend (`*`) glide in flight
	MUTE: 4,
	AUDIBLE: 8, // channel not idling (release tail included)
};

class SiONEngine {
	constructor() {
		this.mod = null;
		this.fns = null;
		this.bufBase = 0;
		this.chanBase = 0;
		this.ready = null; // promise, resolves once driver + ctx node are live
		this.onStreamingChanged = null; // callback(bool)

		this.driverRate = 44100;
		this.ratio = 1;
		this.resample = false;
		this.win = null;
	}

	start(ctx, driverRate) {
		if (this.ready === null) {
			this.driverRate = driverRate || this.driverRate;
			this.ready = this._start(ctx);
		}
		return this.ready;
	}

	async _start(ctx) {
		const mod = await createSiONModule();
		if (!mod.ccall('sion_web_init', 'number', ['number', 'number'], [this.driverRate, 2048])) {
			throw new Error('sion_web_init() failed');
		}
		this.mod = mod;
		this.bufBase = mod.cwrap('sion_web_buffer', 'number', [])() >> 2; // Float32 element index
		this.chanBase = mod.cwrap('sion_web_channel_data', 'number', [])() >> 2; // Int32 element index
		this.fns = {
			play: mod.cwrap('sion_web_play', 'number', ['string']),
			stop: mod.cwrap('sion_web_stop', null, []),
			update: mod.cwrap('sion_web_update', null, []),
			render: mod.cwrap('sion_web_render', 'number', ['number']),
			streaming: mod.cwrap('sion_web_is_streaming', 'number', []),
			volume: mod.cwrap('sion_web_set_volume', null, ['number']),
			lastError: mod.cwrap('sion_web_last_error', 'string', []),
			capture: mod.cwrap('sion_web_capture_channels', 'number', []),
		};

		this.ratio = this.driverRate / ctx.sampleRate;
		this.resample = Math.abs(this.ratio - 1) > 1e-9;
		if (this.resample) {
			this.WIN = 32768; // driver frames of window capacity
			this.win = new Float32Array(this.WIN * 2);
			this.winStart = 0; // absolute driver index held at win[0]
			this.winLen = 0;   // valid frames currently in win
			this.posAbs = 0;   // absolute fractional read cursor
		}

		const node = ctx.createScriptProcessor(2048, 0, 2);
		this.wasStreaming = false;
		node.onaudioprocess = (e) => this._process(e.outputBuffer);
		node.connect(ctx.destination);
		return node;
	}

	play(mml) {
		if (this.fns === null) throw new Error('engine not started');
		this.fns.play(mml);
		return this.fns.lastError();
	}

	stop() {
		if (this.fns !== null) this.fns.stop();
	}

	setVolume(v) {
		if (this.fns !== null) this.fns.volume(v);
	}

	// Refreshes the driver's channel snapshot and returns a decoded view:
	//   { count, rows: [ { type, chnum, module, program, notes: [note...] } ] }
	// with note = { pitch, volume, pan, flags, sweepTarget }. Read-only; safe to
	// call once per animation frame. Returns null before the engine has started.
	captureChannels() {
		if (this.fns === null) return null;
		const count = this.fns.capture();
		const heap = this.mod.HEAP32;
		const rows = [];
		for (let i = 0; i < count; i++) {
			const b = this.chanBase + i * SION_VIS.ROW_INTS;
			const row = {
				type: heap[b],
				chnum: heap[b + 1],
				module: heap[b + 2],
				program: heap[b + 3],
				notes: [],
			};
			const n = Math.min(heap[b + 4], SION_VIS.NOTES_PER_ROW);
			for (let k = 0; k < n; k++) {
				const p = b + 6 + k * 5;
				row.notes.push({
					pitch: heap[p],
					volume: heap[p + 1],
					pan: heap[p + 2],
					flags: heap[p + 3],
					sweepTarget: heap[p + 4],
				});
			}
			rows.push(row);
		}
		return { count, rows };
	}

	_peek(frames) {
		return this.mod.HEAPF32.subarray(this.bufBase, this.bufBase + frames * 2);
	}

	_process(outputBuffer) {
		if (this.fns === null) return;
		const outL = outputBuffer.getChannelData(0);
		const outR = outputBuffer.numberOfChannels > 1 ? outputBuffer.getChannelData(1) : null;
		const frames = outL.length;

		this.fns.update();
		if (this.resample) {
			this._renderResampled(outL, outR, frames);
		} else {
			this.fns.render(frames);
			const src = this._peek(frames);
			for (let i = 0; i < frames; i++) {
				const l = src[2 * i];
				if (outR) {
					outL[i] = l;
					outR[i] = src[2 * i + 1];
				} else {
					outL[i] = (l + src[2 * i + 1]) * 0.5;
				}
			}
		}

		const streaming = this.fns.streaming() !== 0;
		if (streaming !== this.wasStreaming && this.onStreamingChanged !== null) {
			this.wasStreaming = streaming;
			this.onStreamingChanged(streaming);
		}
	}

	_renderResampled(outL, outR, frames) {
		const ratio = this.ratio;
		const endAbs = this.posAbs + frames * ratio; // absolute driver position after this block
		const needUntil = Math.ceil(endAbs) + 1;     // exclusive absolute index coverage

		while (this.winStart + this.winLen < needUntil) {
			if (needUntil - this.winStart > this.WIN) {
				// Drop fully-consumed frames (reads never touch below floor(posAbs)).
				const drop = Math.floor(this.posAbs) - this.winStart;
				if (drop <= 0) break; // unreachable with per-block demand
				this.win.copyWithin(0, drop * 2, this.winLen * 2);
				this.winStart += drop;
				this.winLen -= drop;
			}
			const want = Math.min(needUntil - (this.winStart + this.winLen), this.WIN - this.winLen);
			const got = this.fns.render(want);
			this.win.set(this._peek(got), this.winLen * 2);
			this.winLen += got;
			if (got < want) break; // driver capped output; next block reloop-recovers
		}

		const win = this.win;
		for (let i = 0; i < frames; i++) {
			const local = this.posAbs - this.winStart + i * ratio;
			let i0 = local | 0;
			if (i0 < 0) i0 = 0;
			const f = local - i0;
			const a = i0 * 2;
			const b = (i0 + 1) * 2;
			const l = win[a] + (win[b] - win[a]) * f;
			const r = win[a + 1] + (win[b + 1] - win[a + 1]) * f;
			if (outR) {
				outL[i] = l;
				outR[i] = r;
			} else {
				outL[i] = (l + r) * 0.5;
			}
		}
		this.posAbs = endAbs;
	}
}
