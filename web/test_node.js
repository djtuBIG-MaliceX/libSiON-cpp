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

	// 2) Error output is captured through the sink.
	play('o20 c;');
	const err2 = lastError();
	console.log('captured error: ' + err2.split('\n')[0]);
	if (err2.indexOf('outside of valid range') === -1) fail('expected range error not captured');

	console.log('PASS');
	process.exit(0);
})().catch((e) => {
	console.error(e);
	process.exit(1);
});
