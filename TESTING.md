# Testing

## Offline (x86)
- `tests/run.sh fast build/demo.wav`: engine tests (every model x every VCA mode makes sound and goes quiet,
  3 oscillators + HP + drive + noise, polyphony and stealing, MONO note stack, LEGATO glide, sustain pedal, state
  round-trip, worst-case CPU timing) and a demo WAV. `tests/run.sh asan` runs the same under ASan/UBSan.
- `$MPC_VST/tools/test_port.sh vst/vst.json`: the port kit's wrapper-level host test. One known FAIL: the
  step_of "steps one option" check taps a button twice within a block, which the wrapper's tap debounce drops.

## Device
| # | Check | Result |
|---|---|---|
| 1 | Plugin appears in the plugin list and loads on a track | not yet run |
| 2 | Plays from pads / keys, polyphonic | not yet run |
| 3 | Skin pages draw, popups open and close, prev/next step one model per tap | not yet run |
| 4 | Q-Links move the right knobs per page | not yet run |
| 5 | Save / reload project restores every parameter | not yet run |
| 6 | CPU with 8 voices x 3 oscillators (tools/bench.sh) | not yet run |
