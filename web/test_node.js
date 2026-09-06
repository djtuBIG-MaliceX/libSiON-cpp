/***************************************************/
/* Part of GDSiON software synthesizer             */
/* Copyright (c) 2024 Yuri Sizov and contributors  */
/* Provided under MIT                              */
/***************************************************/

// Node smoke test for the WASM module (no browser needed):
//   node test_node.js          (run from the directory holding sion_wasm.js)
// Checks: init, clean compile + audible render, auto_stop at sequence end,
// and compiler error capture through the Godot-parity error sink.

const path = require('path');
const createSiONModule = require(path.join(__dirname, 'sion_wasm.js'));

function fail(msg) {
	console.error('FAIL: ' + msg);
	process.exit(1);
}

(async () => {
	const mod = await createSiONModule();

	if (!mod.ccall('sion_web_init', 'number', ['number', 'number'], [44100, 2048])) {
		fail('sion_web_init returned 0');
	}
	const play = mod.cwrap('sion_web_play', 'number', ['string']);
	const stop = mod.cwrap('sion_web_stop', null, []);
	const update = mod.cwrap('sion_web_update', null, []);
	const render = mod.cwrap('sion_web_render', 'number', ['number']);
	const streaming = mod.cwrap('sion_web_is_streaming', 'number', []);
	const lastError = mod.cwrap('sion_web_last_error', 'string', []);
	const bufBase = mod.cwrap('sion_web_buffer', 'number', [])() >> 2;

	// 1) Valid song renders non-silent audio and finishes via auto_stop.
	play('#TITLE{node test};\nt120;\ncdefgab>c;');
	const err = lastError();
	if (err !== '') fail('unexpected compile errors: ' + err);
	if (!streaming()) fail('driver not streaming after play');

	let peak = 0;
	const seconds = 8;
	for (let i = 0; i < seconds * 44100 / 2048; i++) {
		update();
		const n = render(2048);
		const src = mod.HEAPF32.subarray(bufBase, bufBase + n * 2);
		for (let k = 0; k < src.length; k++) {
			const a = Math.abs(src[k]);
			if (a > peak) peak = a;
		}
	}
	console.log('peak amplitude: ' + peak.toFixed(4));
	if (peak < 0.01) fail('render is silent');
	update();
	if (streaming() !== 0) fail('auto_stop did not stop streaming after sequence end');

	// 2) Channel visualizer snapshot tracks the song.
	play('#TITLE{viz test};\nt120;\nl8;\ncdefgab>c;');
	const capture = mod.cwrap('sion_web_capture_channels', 'number', []);
	const chanBase = mod.cwrap('sion_web_channel_data', 'number', [])() >> 2;
	const ROW_INTS = 46;

	let sawRows = 0, sawKeyOn = 0, sawMiddleC = false, maxNotesPerRow = 1;
	for (let i = 0; i < 300; i++) {
		update();
		render(2048);
		const count = capture();
		if (count > 26) fail('visualizer row count over cap: ' + count);
		sawRows = Math.max(sawRows, count);
		for (let r = 0; r < count; r++) {
			const b = chanBase + r * ROW_INTS;
			const heap = mod.HEAP32;
			if (heap[b] < 0 || heap[b] > 3) fail('bad channel type in row ' + r + ': ' + heap[b]);
			const n = heap[b + 4];
			if (n > 8) fail('note count over per-row cap: ' + n);
			maxNotesPerRow = Math.max(maxNotesPerRow, n);
			for (let k = 0; k < n; k++) {
				const p = b + 6 + k * 5;
				if ((heap[p + 3] & 1) !== 0) {
					sawKeyOn++;
					// First note of the scale is middle c (60 << 6) within a semitone.
					if (Math.abs(heap[p] - (60 << 6)) <= 64) sawMiddleC = true;
				}
			}
		}
	}
	console.log('visualizer: max rows ' + sawRows + ', key-on samples ' + sawKeyOn +
			', max notes/row ' + maxNotesPerRow);
	if (sawRows === 0) fail('visualizer never allocated a channel row');
	if (sawKeyOn === 0) fail('visualizer never reported a key-on note');
	if (!sawMiddleC) fail('visualizer did not report middle c for the first scale note');

	// 2b) Two concurrent MML tracks routed to the same %module,%number share one
	// row and sound polyphonically (note_count > 1 observed).
	play('#TITLE{poly test};\nt120;\nl8;\n%0,0 c d e f;\n%0,0 r c e g;');
	let sawPoly = 1, polyRows = 0;
	for (let i = 0; i < 300; i++) {
		update();
		render(2048);
		const count = capture();
		polyRows = Math.max(polyRows, count);
		for (let r = 0; r < count; r++) {
			const b = chanBase + r * ROW_INTS;
			if (mod.HEAP32[b] !== 0 || mod.HEAP32[b + 1] !== 0) fail('poly rows should merge on FM #0');
			sawPoly = Math.max(sawPoly, mod.HEAP32[b + 4]);
		}
	}
	console.log('polyphony: rows ' + polyRows + ', max simultaneous notes on one row ' + sawPoly);
	if (sawPoly < 2) fail('expected a polyphonic merged row (>= 2 notes), got ' + sawPoly);

	// 3) Pitch-bend sweep (`*` targeting a full-length note) flips the SWEEP flag.
	play('#TITLE{bend test};\nt120;\nc8 * < c8;');
	let sawSweep = false;
	for (let i = 0; i < 150 && !sawSweep; i++) {
		update();
		render(2048);
		const count = capture();
		for (let r = 0; r < count && !sawSweep; r++) {
			const b = chanBase + r * ROW_INTS;
			const n = mod.HEAP32[b + 4];
			for (let k = 0; k < n; k++) {
				if ((mod.HEAP32[b + 6 + k * 5 + 3] & 2) !== 0) sawSweep = true;
			}
		}
	}
	if (!sawSweep) fail('portamento sweep never observed');

	// 4) Error output is captured through the sink.
	play('o20 c;');
	const err2 = lastError();
	console.log('captured error: ' + err2.split('\n')[0]);
	if (err2.indexOf('outside of valid range') === -1) fail('expected range error not captured');

	// 5) After stop, capture reports no rows.
	stop();
	update();
	render(64);
	if (capture() !== 0) fail('visualizer still reports channels after stop');

	console.log('PASS');
	process.exit(0);
})().catch((e) => {
	console.error(e);
	process.exit(1);
});
