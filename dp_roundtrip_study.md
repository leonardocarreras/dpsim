# DP Round-Trip Study

## Goal

Understand what the VILLAS `dp` hook is actually producing in the EMT -> DP path, and whether the returned waveform from `inverse = true` is the quantity we should expect to compare directly against the original EMT waveform.

The motivating question was:

- does the EMT -> DP transform produce the expected DP state?
- if we reconstruct back to a waveform, should that waveform look like the original EMT carrier?

## Main Files

Configs:

- `configs/villas-conf2-threephase-dp.conf`
- `configs/villas-conf2-threephase-roundtrip.conf`

Raw / generated CSVs:

- `villas-conf2-threephase-raw.csv`
- `villas-conf2-threephase-dp.csv`
- `villas-conf2-threephase-roundtrip-raw.csv`
- `villas-conf2-threephase-roundtrip-reconstructed.csv`

Study scripts:

- `compare_threephase_dp_reconstruction.py`
- `plot_threephase_dp_comparison.py`
- `plot_threephase_roundtrip.py`
- `candidate_reconstruct_first_harmonic.py`
- `study_villas_dp_behavior.py`
- `extract_roundtrip_lowfreq.py`

## Step 1: Isolate EMT -> DP generation

We created a synthetic three-phase source so the transform itself could be studied without the full co-simulation path.

Important choices:

- `rate = 20000.0`
- `dt = 0.00005`
- `realtime = false`
- `harmonics = [ 1 ]`

Reasoning:

- `rate = 1 / dt` keeps the discrete-time setup consistent.
- `realtime = false` removes scheduler jitter from the analysis.
- `harmonics = [ 1 ]` matches the DPSim interface expectation of one complex fundamental phasor per quantity.

## Step 2: Offline reconstruction from DP log

We first compared:

- original EMT waveform from `villas-conf2-threephase-raw.csv`
- reconstructed waveform from `villas-conf2-threephase-dp.csv`

using:

- `compare_threephase_dp_reconstruction.py`
- `plot_threephase_dp_comparison.py`

Generated plot:

![Three-phase DP comparison](threephase_dp_comparison.png)

Interpretation:

- this path uses an offline reconstruction model
- it is **not** the same as the actual VILLAS `inverse = true` round-trip path
- this comparison can look "reasonably okay" even if the true round-trip waveform does not

## Step 3: Check the actual VILLAS round-trip

To answer "what waveform would come back?", we created:

- `configs/villas-conf2-threephase-roundtrip.conf`

This does:

1. synthetic EMT waveform
2. `dp` forward transform
3. `dp` inverse transform
4. write both original and returned waveform

Generated plot:

![Three-phase round-trip waveform](villas-conf2-threephase-roundtrip-comparison.png)

Interpretation:

- the actual VILLAS round-trip waveform does **not** look like a clean reconstruction of the original EMT carrier
- therefore the issue is not only in our plotting or CSV mapping

## Step 4: Inspect VILLAS `dp.cpp`

We then inspected:

- `/home/leo/git/github/my/villas-node/lib/hooks/dp.cpp`

Key observations:

- the forward transform computes a one-cycle sliding-window DFT coefficient
- it applies a correction term `corr`
- the inverse path is not obviously symmetric to the forward path

This means the exported DP coefficient is not trivially "the instantaneous waveform encoded as one complex number".

## Step 5: Test alternative remodulation candidates

We tested several reconstruction candidates from the exported first harmonic:

- reuse the formulas from `reconstruct_emt_from_dp.py`
- a simple first-harmonic remodulation candidate
- exact VILLAS forward + exact VILLAS inverse
- SFA-style remodulation with different time anchors
- explicit `corr` compensation

Generated plot:

![VILLAS behavior study](study_villas_dp_behavior.png)

What this showed:

- harmonic mapping was correct
- SFA remodulation matters
- the `corr` term matters
- but there was no single remodulation convention that cleanly recovered all three phases as a carrier waveform

So the mismatch is not just:

- wrong harmonic label
- wrong sign
- missing `f0`
- wrong sequence alignment

## Step 6: Extract the slow component from the round-trip waveform

At this point the returned waveform looked like it might be dominated by a low-frequency envelope-like component.

We tested that with:

- `extract_roundtrip_lowfreq.py`

Generated plot:

![Low-frequency extraction](villas-conf2-threephase-roundtrip-lowfreq.png)

And the RMS split for the returned waveform was:

- `VA_emt`: lowfreq `35.42`, highfreq `1.77`
- `VB_emt`: lowfreq `35.81`, highfreq `1.14`
- `VC_emt`: lowfreq `34.84`, highfreq `2.08`

Interpretation:

- the returned round-trip signal is dominated by the low-frequency component
- the high-frequency part is small
- this supports the view that the visible returned signal is closer to the DP envelope / baseband state than to a remodulated EMT carrier

## Current Interpretation

### What seems to be working

- the EMT -> DP extraction likely produces a meaningful slow DP state
- the low-frequency/baseband-like signal looks physically plausible
- the phase/harmonic mapping is now understood and consistent

### What seems questionable

- the VILLAS waveform inverse path is not behaving like a clean EMT carrier reconstruction
- comparing the round-trip returned waveform directly to the original EMT waveform is likely the wrong expectation for the current inverse behavior

## Practical Conclusion

There are really two different validation questions:

1. **DP state validation**

Is the extracted DP/envelope quantity sensible?

Current answer:

- likely yes

2. **Waveform round-trip validation**

Does `dp` forward + `inverse = true` return a clean EMT waveform carrier?

Current answer:

- not convincingly

## Suggested Next Steps

If the real goal is the co-simulation interface:

1. treat the low-frequency DP-like quantity as the object of interest for DP coupling validation
2. separate that from any expectation of direct EMT waveform recovery
3. only if a literal waveform must be returned, investigate or patch the inverse path in `dp.cpp`

If the immediate goal is to proceed with the paper / study:

1. keep the DP extraction study as evidence that the envelope/state looks reasonable
2. state clearly that the VILLAS inverse waveform path appears not to be a faithful EMT waveform reconstruction in this setup

## Commands Used Most Often

Offline DP comparison:

```bash
python3 compare_threephase_dp_reconstruction.py --time-base sequence --harmonics 1 --mapping positional --dt 0.00005
MPLCONFIGDIR=/tmp/matplotlib-codex venv/bin/python plot_threephase_dp_comparison.py --x-axis sequence --harmonics 1
```

Round-trip waveform plot:

```bash
MPLCONFIGDIR=/tmp/matplotlib-codex venv/bin/python plot_threephase_roundtrip.py --x-axis sequence
```

Low-frequency extraction:

```bash
MPLCONFIGDIR=/tmp/matplotlib-codex venv/bin/python extract_roundtrip_lowfreq.py --cutoff 10 --x-axis sequence
```

Full behavior study:

```bash
MPLCONFIGDIR=/tmp/matplotlib-codex venv/bin/python study_villas_dp_behavior.py --raw villas-conf2-threephase-roundtrip-raw.csv --f0 60 --dt 0.00005 --harmonics 1 --x-axis sequence
```
