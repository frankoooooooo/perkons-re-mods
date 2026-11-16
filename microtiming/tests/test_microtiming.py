"""microtiming: hold one step + TEMPO sets that step's timing; the tempo never changes."""
import os
import struct
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
PERKONS = os.environ.get('PERKONS_RE', os.path.join(HERE, '..', '..', '..', 'perkons-re'))
sys.path[:0] = [os.path.join(PERKONS, 'tools'), os.path.join(PERKONS, 'tests')]
os.environ.setdefault('PERKONS_MODS', os.path.join(HERE, '..', '..'))

from fixtures import build_patched, require_image  # noqa: E402
from perkons.emu import Symbols  # noqa: E402
from perkons.fwimage import Image  # noqa: E402
from perkons.rig import M4Rig  # noqa: E402

SYM = Symbols(os.path.join(PERKONS, 'symbols', 'v1.3.0-m4.json'))
TEMPO = 0x20
DEG = 1 / 300
TEMPO_BPM = SYM.g['tempo_state']
TEMPO_CATCH = SYM.g['tempo_state'] + 0xd
_image = []


def image():
    if not _image:
        img, p = build_patched(Image.load(require_image('v1.3.0')), ['microtiming'], tempfile.mkdtemp())
        _image.append((img, p.cave_symbols['m4host']))
    return _image[0]


class Rig(M4Rig):
    def __init__(self, on=True):
        super().__init__(*image())
        self.e.w8(self.cave['pk_cfg_microtiming'], 1 if on else 0)
        f = self.sym.fn
        # the stock list counts held steps (the base rig's stub says 0 or 1); a hold that
        # turns a knob lasts longer than a tap (350 ms), so a step key up doesn't toggle
        self.e.stub(f['steps_held'], lambda emu, *_: sum(k < 0x40 for k in self.held))
        self.long = set()           # keys held long (the rig drops a key before its key-up)
        self.e.stub(f['key_held_ms'], lambda emu, k, *_: 1000 if k in self.long else 0)
        self.knob(TEMPO, 0.5)
        self.ui()
        self.sub = 0
        self.fired = []             # (step, tick into it) of each trigger track 1 sent

    def ui(self):
        self.e.call(self.sym.fn['ui_update'], max_insns=20_000_000)

    def tempo_to(self, x):
        self.turn(TEMPO, x, self.ui)

    def bpm(self):
        return struct.unpack('<f', bytes(self.e.uc.mem_read(TEMPO_BPM, 4)))[0]

    def delay(self, step, track=0):
        return struct.unpack('<h', bytes(self.e.uc.mem_read(self.block(track) + 8 * step + 2, 2)))[0]

    def row(self, track=0):
        return [self.leds.get(16 * track + c) for c in range(16)]

    # the sequencer, one sub-tick at a time (as seq-unquantized's test)
    def tick(self):
        c = self.clock
        n = self.r16(c + 0x1c) * self.r16(c + 0x1e)
        if self.sub % n == 0:
            self.w16(c + 0x22, 0)
            self.w16(c + 0x20, self.r16(c + 0x20) + 1)
            self.e.call(self.e.r32(c + 0x3c) & ~1, 1, max_insns=5_000_000)
        else:
            self.subtick()
        self.sub += 1
        vs = self.voice_state(0)
        if any(p[0] for typ, p in self.e.sent(2) if typ == 0):
            self.fired.append((self.r16(vs + 0x1c), self.r16(vs + 0x24)))


class TestMicrotiming(unittest.TestCase):
    def test_tempo_knob_alone_still_sets_the_tempo(self):
        r = Rig()
        before = r.bpm()
        r.tempo_to(0.8)
        self.assertNotAlmostEqual(r.bpm(), before, places=1)

    def test_dead_zone_then_a_division_every_10_degrees(self):
        r = Rig()
        r.set_step(0, 4)
        bpm = r.bpm()
        r.held.add(4)
        for deg, div in ((15, 0), (25, 1), (35, 2), (45, 3), (-15, 0), (-25, -1), (-100, -5), (100, 5)):
            with self.subTest(deg=deg):
                r.tempo_to(0.5 + deg * DEG)
                self.assertEqual(r.delay(4), [0, 17, 34, 50, 67, 84][abs(div)] * (1 if div >= 0 else -1))
        self.assertEqual(r.bpm(), bpm)

    def test_leds_show_the_divisions_round_the_held_step(self):
        r = Rig()
        r.set_step(0, 4)
        r.set_step(0, 12)
        r.held.add(4)
        r.tempo_to(0.5 - 70 * DEG)                  # -5: wraps to step 16
        row = r.row()
        self.assertEqual(row[15], 0xff)
        self.assertEqual([row[c] for c in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9)], [0x08] * 10)
        self.assertEqual([row[c] for c in (10, 11, 12, 13, 14)], [0] * 5)   # step 13's trig hidden

    def test_release_keeps_the_tempo_and_arms_the_catch(self):
        r = Rig()
        r.set_step(0, 4)
        bpm = r.bpm()
        r.held.add(4)
        r.tempo_to(0.9)
        r.held.discard(4)
        r.ui()
        self.assertEqual((r.bpm(), r.e.r8(TEMPO_CATCH)), (bpm, 1))
        r.tempo_to(0.92)                            # far from the tempo: caught, no jump
        self.assertEqual(r.bpm(), bpm)

    def test_holding_again_shows_the_stored_offset(self):
        r = Rig()
        r.set_step(1, 7)
        r.held.add(0x17)
        r.tempo_to(0.5 + 35 * DEG)                  # +2
        r.held.discard(0x17)
        r.ui()
        r.held.add(0x17)
        r.ui()
        self.assertEqual(r.row(1)[9], 0)            # view not up before TEMPO moves: stock row
        r.tempo_to(r.knob_at(TEMPO) + 3 * DEG)      # past the noise, inside the dead zone
        self.assertEqual((r.delay(7, 1), r.row(1)[9], r.row(1)[7]), (34, 0xff, 0x08))

    def test_off_in_the_config_menu_is_stock(self):
        r = Rig(on=False)
        r.set_step(0, 4)
        bpm = r.bpm()
        r.held.add(4)
        r.tempo_to(0.9)
        self.assertEqual(r.delay(4), 0)
        self.assertNotEqual(r.bpm(), bpm)

    def test_two_steps_held_is_stock(self):
        r = Rig()
        bpm = r.bpm()
        r.held.update({4, 5})
        r.tempo_to(0.9)
        self.assertEqual((r.delay(4), r.delay(5)), (0, 0))
        self.assertNotEqual(r.bpm(), bpm)

    def test_the_step_stays_on_after_the_hold(self):
        """Step key down, TEMPO, step key up through the image's key handler: the step is
        still on (the stock handler doesn't take the hold for a tap)."""
        r = Rig()
        r.set_step(0, 4)
        r.long.add(4)
        r.event(4, True)
        r.tempo_to(0.5 + 25 * DEG)
        r.event(4, False)
        r.ui()
        self.assertEqual((r.e.r8(r.block(0) + 8 * 4), r.delay(4)), (1, 17))

    def test_shifted_steps_play_early_and_late(self):
        r = Rig()
        for s in (4, 8):
            r.set_step(0, s)
        for s, deg in ((4, 35), (8, -25)):          # 4: +2 (8 ticks late); 8: -1 (4 ticks early)
            r.held.add(s)
            r.tempo_to(0.5 + deg * DEG)
            r.held.discard(s)
            r.ui()
            r.knob(TEMPO, 0.5)
        r.play()
        for _ in range(24 * 12 * 16 * 2 + 1):     # two passes: 24 ticks x 12 sub-ticks a step
            r.tick()
        self.assertEqual(r.fired[-2:], [(4, 8), (8, 20)])     # 8's trigger lands 20 ticks into step 7
        self.assertEqual(r.fired.count((8, 20)), 2)           # once a pass


if __name__ == '__main__':
    unittest.main()
