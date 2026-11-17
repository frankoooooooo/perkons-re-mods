# Microtiming

Hold a step and turn **TEMPO** to make that step play early or late, in sixths of a step.

> **Tested on hardware.** The core features passed on a Pērkons: switching it on, moving steps early and late, the LED view, the tempo staying put, and save/reload. Some worst cases (rotate and copy, play modes, external clock, other mods) aren't tested yet. See [TESTING.md](TESTING.md). If it misbehaves, flash the official firmware to go back.

Based on official Pērkons firmware **1.3.0**. Unofficial, and not affiliated with Erica Synths.

## Getting it

Build Pērkons firmware with **MICROTIMING** ticked in the **Mod Matrix**, from your own official 1.3.0 firmware. Follow [Get modded firmware](https://github.com/kiteless/perkons-re#get-modded-firmware) in perkons-re, and tick this repo on the mod-repo screen once it's listed (or point `PERKONS_MODS` at a clone of it). No firmware files are provided here.

## Turning it on

1. Hold **SHIFT** and press **MOD** to open the config menu.
2. Press **step 12 on the third row**. LED on means microtiming is on. LED off (the default) means TEMPO works as on the factory firmware.
3. Leave the config menu as usual. The setting is saved on the SD card.

## Using it

1. Program a step, then **hold** it. Empty steps don't take microtiming: with an empty step held, TEMPO is the tempo knob as usual.
2. Turn **TEMPO**. The held step's row changes to the microtiming view:
   * the 5 steps on each side of the held step light dim (unaccented), marking the range;
   * the step's current position lights bright;
   * the rest of that row goes dark. The other rows and playback carry on as normal.
3. Nothing happens for the first **20°**. After that, each **10°** moves the step one sixth of a step (4 of its 24 ticks): clockwise is later, counter-clockwise is earlier. The limit is 5 sixths either way. Turning back into the first 20° returns the step to where it started. Turning past the end of the range doesn't count: turn back and the step moves again at once. So if TEMPO is at the end of its travel when you hold a step, turn it the other way first, then back.
4. Let go of the step. To see or change its timing later, hold it again and move TEMPO slightly.

A step can't be moved onto or past the step played before or after it. If a neighbouring step is in the way, the range in the view is shorter on that side and the step stops one division short of it. This limit applies in the forward play mode only. In the other play modes, two steps moved onto each other play as one hit.

Turning a step off clears its microtiming. When you turn it on again it's back on the grid (or on the track's shuffle grid, if the track has shuffle).

An early first step plays at the end of the bar before it. So the first time round after you press **PLAY**, that step is silent: there is no bar before it yet.

While you hold a step, **the tempo doesn't change**. After you let go, TEMPO does nothing until you turn it back to near the current tempo (within 12 BPM). This is the same catch the firmware uses after the tempo page, so the tempo never jumps.

With two or more steps held, TEMPO works as on the factory firmware.

## How it's stored

The timing is kept in the step's own delay value, the one **SHUFFLE** sets. The firmware itself plays that value early (negative) or late (positive). So:

* microtiming is saved with the pattern;
* clearing a track clears it;
* it should move with the steps when you rotate or copy (not yet tested on a unit);
* the official firmware plays it back the same way.

Things to know:

* Setting a track's **SHUFFLE** again replaces that track's microtiming.
* A step that shuffle moved shows its timing rounded to the nearest sixth. It keeps shuffle's exact value until you turn TEMPO past the dead zone.
* **UNQUANTIZED** recording (seq-unquantized) writes the same value. Recording a hit sets that step's timing too.
* Like p-locks, the hold has to last longer than a tap (about a third of a second), otherwise releasing the step turns it off. Any real turn of TEMPO takes longer than that.
