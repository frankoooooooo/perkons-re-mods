/* microtiming: hold one step and turn TEMPO to play that step early or late.
 *
 * On (config row 3 step 12), in the main page, with exactly one step held:
 * - TEMPO no longer changes the tempo.
 * - Past a 20 degree dead zone, each further 10 degrees moves the step one division:
 *   1/6 of a step (4 of its 24 ticks), up to 5 divisions earlier or later.
 * - The held step's row shows the division LEDs: +-5 around the step dim, the step's
 *   current position bright, everything else in the row off.
 * On release, the tempo knob's catch is armed (as the tempo page does), so TEMPO takes
 * over again only when it gets back near the tempo. It never jumps.
 *
 * The offset is stored in the stock per-step delay (FW_STEP_DELAY), a signed % of a step.
 * The stock player fires a step with a negative delay early, during the step before it
 * (FUN_0810d248). So the timing is saved with the pattern and moves with rotate, copy and
 * clear, and the official firmware plays it the same way. SHUFFLE writes the same field:
 * setting a track's shuffle again replaces its microtiming. */
#include <stdint.h>
#include "pk_host.h"
#include "fw_m4_seq.h"

#define fw_held_first   FW_CALL(SYM_held_first, void (*)(uint32_t *it))
#define fw_held_next    FW_CALL(SYM_held_next, int (*)(uint32_t *it))   /* key, -1 at the end */

/* TEMPO's knob_value (0..1) per degree. shortcut: assumes a 300 degree pot, so measure the
 * real travel on hardware and change DEG if the divisions feel too wide or too narrow. */
#define DEG             (1.0f / 300.0f)
#define NOISE           (2 * DEG)       /* movement that opens the view */
#define DEAD            (20 * DEG)
#define DIV             (10 * DEG)
#define MAX_DIV         5
#define LED_RANGE       0x08            /* stock: an unaccented step */
#define LED_POS         0xff            /* stock: the playhead */

PK_AXI uint8_t pk_cfg_microtiming;      /* system/mods.json "microtiming" */
PK_AXI static int8_t held = -1;         /* the held step's key while TEMPO is ours, else -1 */
PK_AXI static uint8_t shown;            /* TEMPO moved this hold: the view is up */
PK_AXI static int8_t base;              /* the step's divisions when the hold began */
PK_AXI static float anchor;             /* TEMPO's position when the hold began */

/* divisions <-> the step's delay (% of a step). 24 ticks per step, so n divisions is 4n ticks:
 * the player's delay * 24 / 100 (truncated) lands on 4n for these percentages. */
static int div_to_delay(int n)
{
    int a = n < 0 ? -n : n;
    a = (50 * a + 2) / 3;                       /* 17, 34, 50, 67, 84 */
    return n < 0 ? -a : a;
}

static int delay_to_div(int d)
{
    int a = d < 0 ? -d : d;
    a = (6 * a + 50) / 100;                     /* nearest division (shuffle's values too) */
    return d < 0 ? -a : a;
}

static volatile int16_t *step_delay(int key)
{
    uint8_t *blk = (uint8_t *)(uintptr_t)FW_U32(fw_voice_state(key >> 4) + FW_VS_BLOCK);
    return (volatile int16_t *)(blk + (key & 15) * FW_STEP_SIZE + FW_STEP_DELAY);
}

/* the one step held in the main page, or -1 */
static int one_step_held(void)
{
    if (!pk_cfg_microtiming || fw_get_mode() != FW_MODE_MAIN || fw_steps_held() != 1)
        return -1;
    uint32_t it;
    fw_held_first(&it);
    int key = fw_held_next(&it);
    return key >= 0 && key < 0x40 ? key : -1;
}

/* ui_update: TEMPO's position and moved flag, into tempo_knob */
void PK_NEXT(pk_mt_tempo)(float pos, int moved);
void pk_mt_tempo(float pos, int moved)
{
    int key = one_step_held();
    if (key < 0) {
        if (held >= 0 && shown)                 /* catch: no jump to where TEMPO is now */
            fw_tempo_set(FW_F32(SYM_tempo_state + FW_TEMPO_BPM));
        held = -1;
        shown = 0;
        PK_NEXT(pk_mt_tempo)(pos, moved);
        return;
    }
    if (key != held) {
        held = (int8_t)key;
        shown = 0;
        anchor = pos;
        base = (int8_t)delay_to_div(*step_delay(key));
    }
    float d = pos - anchor;
    float a = d < 0 ? -d : d;
    if (a >= NOISE)
        shown = 1;
    if (shown) {
        int n = a < DEAD ? base : base + (d < 0 ? -1 : 1) * (1 + (int)((a - DEAD) / DIV));
        n = n > MAX_DIV ? MAX_DIV : n < -MAX_DIV ? -MAX_DIV : n;
        if (n != delay_to_div(*step_delay(key)))    /* untouched steps keep shuffle's value */
            *step_delay(key) = (int16_t)div_to_delay(n);
    }
    PK_NEXT(pk_mt_tempo)(pos, 0);               /* the tempo stays as it is */
}

/* main page LEDs: stock, then the held step's row as the division view */
void PK_NEXT(pk_mt_render)(void);
void pk_mt_render(void)
{
    PK_NEXT(pk_mt_render)();
    if (held < 0 || !shown || one_step_held() != held)
        return;
    int row = held & 0x30, col = held & 15;
    int n = delay_to_div(*step_delay(held));
    for (int i = 0; i < 16; i++)
        fw_set_led(row + i, 0);
    for (int i = -MAX_DIV; i <= MAX_DIV; i++)
        fw_set_led(row + ((col + i) & 15), LED_RANGE);    /* wraps round the row, as the loop does */
    fw_set_led(row + ((col + n) & 15), LED_POS);
}
