<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](../README.md) · [Cheat sheet](../cheatsheet.md) · [Glossary](../glossary.md)_

# Serial Diagnostics

Open Serial Monitor @ 115200 and type commands:

| Command | Action |
|---------|--------|
| `recal` + Enter | Force full recalibration of all 4 MPR121 chips (stops, re-autoconfigs, restarts, prints baselines) |
| `diag` + Enter | Print current baseline + filtered data for all 48 electrodes |

Boot prints baseline values for all electrodes after the 500ms autoconfig delay. Electrodes with `0` or very low baselines failed calibration.
