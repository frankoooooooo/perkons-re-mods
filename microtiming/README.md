# Microtiming

Hold a step and turn **TEMPO** to make that step play early or late, in sixths of a step.

> **Untested beta.** This has only been run in the perkons-re emulator, not on a Pērkons. Expect rough edges. If it misbehaves, flash the official firmware to go back.

Based on official Pērkons firmware **1.3.0**. Unofficial, and not affiliated with Erica Synths.

## Getting it

Build Pērkons firmware with **MICROTIMING (BETA)** ticked in the **Mod Matrix**, from your own official 1.3.0 firmware. Follow [Get modded firmware](https://github.com/kiteless/perkons-re#get-modded-firmware) in perkons-re, and tick this repo on the mod-repo screen once it's listed (or point `PERKONS_MODS` at a clone of it). No firmware files are provided here.

## Turning it on

1. Hold **SHIFT** and press **MOD** to open the config menu.
2. Press **step 12 on the third row**. LED on means microtiming is on. LED off (the default) means TEMPO works as on the factory firmware.
3. Leave the config menu as usual. The setting is saved on the SD card.

## Using it

1. Program a step, then **hold** it.
2. Turn **TEMPO**. The held step's row changes to the microtiming view:
   * the 5 steps on each side of the held step light dim (unaccented), marking the range;
   * the step's current position lights bright;
   * the rest of that row goes dark. The other rows and playback carry on as normal.
3. Nothing happens for the first **20°**. After that, each **10°** moves the step one sixth of a step (4 of its 24 ticks): clockwise is later, counter-clockwise is earlier. The limit is 5 sixths either way. Turning back into the first 20° returns the step to where it started.
4. Let go of the step. To see or change its timing later, hold it again and move TEMPO slightly.

While you hold a step, **the tempo doesn't change**. After you let go, TEMPO does nothing until you turn it back to near the current tempo (within 12 BPM). This is the same catch the firmware uses after the tempo page, so the tempo never jumps.

With two or more steps held, TEMPO works as on the factory firmware.

## How it's stored

The timing is kept in the step's own delay value, the one **SHUFFLE** sets. The firmware itself plays that value early (negative) or late (positive). So:

* microtiming is saved with the pattern;
* it moves with the steps when you rotate or copy;
* clearing a track clears it;
* the official firmware plays it back the same way.

Things to know:

* Setting a track's **SHUFFLE** again replaces that track's microtiming.
* A step that shuffle moved shows its timing rounded to the nearest sixth. It keeps shuffle's exact value until you turn TEMPO past the dead zone.
* **UNQUANTIZED** recording (seq-unquantized) writes the same value. Recording a hit sets that step's timing too.
* Like p-locks, the hold has to last longer than a tap (about a third of a second), otherwise releasing the step turns it off. Any real turn of TEMPO takes longer than that.
