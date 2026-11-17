/* off-grid recording: the trigger keys and MIDI notes record where they were played, on the
 * nearest step, early or late. Based on seq-unquantized (kiteless/perkons-re-mods, MIT).
 *
 * On (config row 3 step 13), while playing with REC on: stock records a hit on the nearest
 * step (this one in the step's first half, the next one after that; it marks the next one
 * not to play again this pass, as the hit has sounded). Here that step gets the delay (the
 * signed % of a step it plays off the grid, the value shuffle sets) that plays it at the tick
 * the hit came: late in this step, or early before the next. The delay is in the track's own
 * clock ticks (24 per step at every rate), so the timing holds at other tempos and rates.
 * Off: stock (the nearest step, its delay left as it was). */
#include <stdint.h>
#include "pk_host.h"
#include "fw_m4_seq.h"

#define seq_play_state      FW_CALL(SYM_seq_play_state, int (*)(void))     /* 1: playing */

PK_AXI uint8_t pk_cfg_unquantized;          /* system/mods.json "unquantized" (seq-unquantized's) */
PK_AXI static uint8_t played;               /* the trigger key being handled: row + 1, 0 none */

static int recording(void)
{
    return pk_cfg_unquantized && FW_U8(SYM_rec_flags + FW_REC_ON);
}

/* Stock is about to record a hit on track `vs`: set the delay of the step it will write, so
 * that step plays at the tick the hit came. The delay comes first: a clock tick in between
 * finds the step's time already passed. */
static void set_delay(uint8_t *vs)
{
    uint32_t per = FW_U16(FW_U32(vs + FW_VS_CLOCK) + FW_CLK_STEP_TICKS);
    uint32_t tick = FW_U16(vs + FW_VS_TICK);
    int step = (int16_t)FW_U16(vs + FW_VS_STEP);
    int next = (int16_t)FW_U16(vs + FW_VS_NEXT);
    if (per == 0 || step < 0 || step >= FW_STEPS)
        return;
    if (tick >= per)
        tick = per - 1;
    int late = FW_U16(vs + FW_VS_SUBTICK) < per / 2;          /* stock's choice: this step */
    if (!late && (next < 0 || next >= FW_STEPS))
        return;
    /* the player: delay * per / 100 ticks, truncated towards 0, so round away from it */
    int ticks = late ? (int)tick : (int)(per - tick);
    int delay = (100 * ticks + (int)per - 1) / (int)per;
    if (delay > 99)
        delay = 99;
    uint8_t *rec = (uint8_t *)(uintptr_t)FW_U32(vs + FW_VS_BLOCK) + (late ? step : next) * FW_STEP_SIZE;
    *(volatile int16_t *)(rec + FW_STEP_DELAY) = (int16_t)(late ? delay : -delay);
}

/* the global key handler (chained): which trigger key key_event is handling */
int PK_NEXT(pk_og_rec_key_event)(const uint8_t *evt);
int pk_og_rec_key_event(const uint8_t *evt)
{
    int trig = evt[0] - FW_KEY_TRIG(0);
    played = evt[1] == FW_EV_DOWN && trig >= 0 && trig < 4 ? (uint8_t)(trig + 1) : 0;
    int r = PK_NEXT(pk_og_rec_key_event)(evt);
    played = 0;
    return r;
}

/* key_event, right after: stock records the nearest step when this returns 1 (playing) and
 * REC is on */
int PK_NEXT(pk_og_playing)(void);
int pk_og_playing(void)
{
    int state = PK_NEXT(pk_og_playing)();
    if (state == 1 && recording() && played)
        set_delay(fw_voice_state(played - 1));
    return state;
}

/* MIDI note-on (chained; ch: the voice in multi channel mode, 4 and up in single channel
 * mode, where the note picks the voice). Stock records the nearest step, then plays it. */
void PK_NEXT(pk_og_midi_note)(uint32_t ch, uint32_t note, uint32_t vel);
void pk_og_midi_note(uint32_t ch, uint32_t note, uint32_t vel)
{
    int track = ch < 4 ? (int)ch : -1;
    for (int t = 0; track < 0 && t < 4; t++)
        if (FW_U8(SYM_device_config + FW_DC_TRIG_NOTE + t) == note)
            track = t;
    if (track >= 0 && recording() && seq_play_state() == 1)
        set_delay(fw_voice_state(track));
    PK_NEXT(pk_og_midi_note)(ch, note, vel);
}
