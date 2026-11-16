"""microtiming: hold a step and turn TEMPO to play it up to 5/6 of a step early or late,
in sixths of a step (config row 3 step 12). Built by the framework's M4 host; hook sites
are in ../symbols/<version>-m4.json (merged into the framework's symbols)."""
import os

from perkons import m4host

TARGET = 'v1.3.0'
SUMMARY = 'Microtiming: hold a step + turn TEMPO to shift it early/late (config row 3 step 12)'

MOD = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(MOD, 'src', 'm4')
KEY = 43                    # config row 3 step 12


def apply(p, arg=None):
    h = m4host.host(p)
    h.add([os.path.join(SRC, 'microtiming.c')], [SRC])
    h.toggle(KEY, 'microtiming', 'pk_cfg_microtiming')
    h.redirect_call('mt.tempo.08103bd6', 'pk_mt_tempo', 'TEMPO with one step held: that step\'s timing')
    h.replace_word('mt.page0_render.0812bf68', 'pk_mt_render', 'main page LEDs: the division view')
