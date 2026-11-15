#!/usr/bin/env python3
"""Generate the Density panels (res/*.svg), component SVGs (res/components/*.svg) and the
widget placement header (src/generated/Layout.hpp) from one table per module.

Every control is placed from the same coordinates in both outputs, every control gets
its printed label (or a column header, in the grids of TRACE and WEAVE), and the script
fails if anything overlaps or leaves the panel.

    python3 scripts/make_panels.py            (needs: pip install fonttools)

Text is converted to paths here, because Rack does not render SVG text.
Font: Inter SemiBold (SIL Open Font License).
"""
import math
import os
import sys

from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = "/usr/share/fonts/opentype/inter/Inter-SemiBold.otf"
if "--font" in sys.argv:
    FONT = sys.argv[sys.argv.index("--font") + 1]

# ---------------------------------------------------------------- look ------
GROUND = "#ecebe6"
INK = "#1d1d1d"
PLATE = "#2a2a2a"
PLATE_INK = "#f4f3ee"
FAMILY_COLOURS = {"HARMONIC": "#c98a1b", "ENTANGLE": "#1f7a78", "EQUATION": "#5b4a9e"}
H = 128.5
HP = 5.08
LABEL_CAP = 1.9
SMALL_CAP = 1.5
TITLE_CAP = 3.0
CONDENSE = 0.86

KNOBS = {  # size: (body radius, scale radius, check box half-size, C++ class)
    "L": (5.5, 6.6, 7.0, "DKnob"),
    "S": (4.0, 5.0, 5.4, "DKnobSmall"),
    "T": (3.0, 3.8, 4.1, "DTrim"),
}
JACK_R = 4.0
JACK_BOX = 4.2
LED_R = 1.2
BTN_R = 3.2
SW_W, SW_H = 5.0, 8.6
SL_W, SL_LEN, SL_TRAVEL, SL_HANDLE = 4.6, 27.0, 22.0, 5.0   # slider
PORT_CLASSES = {"audio": "PortAudio", "cv": "PortCv", "gate": "PortGate", "poly": "PortPoly"}

# ---------------------------------------------------------------- text ------
_font = TTFont(FONT)
_glyphs = _font.getGlyphSet()
_cmap = _font.getBestCmap()
_cap = getattr(_font["OS/2"], "sCapHeight", 0) or 0.729 * _font["head"].unitsPerEm
_hmtx = _font["hmtx"]


def text_width(s, cap):
    scale = cap / _cap
    w = 0
    for ch in s:
        g = _cmap.get(ord(ch))
        if g is None:
            raise SystemExit(f"font has no glyph for {ch!r}")
        w += _hmtx[g][0]
    return w * scale * CONDENSE


def text_path(s, x, y, cap, anchor="middle"):
    scale = cap / _cap
    w = text_width(s, cap)
    if anchor == "middle":
        x -= w / 2
    elif anchor == "end":
        x -= w
    d = []
    pen_x = x
    for ch in s:
        g = _cmap[ord(ch)]
        sp = SVGPathPen(_glyphs)
        tp = TransformPen(sp, (scale * CONDENSE, 0, 0, -scale, pen_x, y))
        _glyphs[g].draw(tp)
        d.append(sp.getCommands())
        pen_x += _hmtx[g][0] * scale * CONDENSE
    return " ".join(p for p in d if p)


# ---------------------------------------------------------------- model -----
class Box:
    def __init__(self, x0, y0, x1, y1, what, owner=None):
        self.x0, self.y0, self.x1, self.y1, self.what, self.owner = x0, y0, x1, y1, what, owner

    def overlaps(self, o, pad=0.2):
        return not (self.x1 + pad <= o.x0 or o.x1 + pad <= self.x0 or self.y1 + pad <= o.y0 or o.y1 + pad <= self.y0)


class Panel:
    def __init__(self, slug, title, short, family, hp):
        self.slug, self.title, self.short, self.family, self.hp = slug, title, short, family, hp
        self.w = hp * HP
        self.items = []      # (kind, id, label, x, y, extra)
        self.displays = []   # (id, x0, y0, x1, y1)
        self.sections = []   # (title, x0, y0, x1, y1)
        self.texts = []      # (text, x, y, cap, anchor, white)
        self.plates = []     # (x0, y0, x1, y1)

    def knob(self, pid, label, x, y, size="L", bipolar=False, positions=None, side=None, grid=False):
        assert pid.endswith("_PARAM"), pid
        self.items.append(("knob", pid, label, x, y, {"size": size, "bipolar": bipolar, "positions": positions, "side": side, "grid": grid}))

    def slider(self, pid, label, x, y, bipolar=False):
        assert pid.endswith("_PARAM"), pid
        self.items.append(("slider", pid, label, x, y, {"bipolar": bipolar}))

    def jack(self, pid, label, x, y, kind="audio", grid=False):
        assert pid.endswith("_INPUT") or pid.endswith("_OUTPUT"), pid
        self.items.append(("jack", pid, label, x, y, {"kind": kind, "out": pid.endswith("_OUTPUT"), "grid": grid}))

    def switch(self, pid, positions, x, y, label=None, grid=False, show_positions=True):
        self.items.append(("switch", pid, label, x, y, {"positions": positions, "grid": grid, "show": show_positions}))

    def button(self, pid, light, label, x, y, latch=False, rgb=False, grid=False, side=None):
        self.items.append(("button", pid, label, x, y, {"light": light, "latch": latch, "rgb": rgb, "grid": grid, "side": side}))

    def led(self, pid, label, x, y, grid=False):
        self.items.append(("led", pid, label, x, y, {"grid": grid}))

    def display(self, did, x0, y0, x1, y1):
        self.displays.append((did, x0, y0, x1, y1))

    def section(self, title, x0, y0, x1, y1):
        self.sections.append((title, x0, y0, x1, y1))

    def text(self, t, x, y, cap=LABEL_CAP, anchor="middle", white=False):
        self.texts.append((t, x, y, cap, anchor, white))

    def plate(self, x0, y0, x1, y1):
        self.plates.append((x0, y0, x1, y1))

    # labels -------------------------------------------------------------
    def label_lines(self, item):
        kind, pid, label, x, y, ex = item
        out = []
        if not label:
            return out
        lines = label.split("\n")
        side = ex.get("side")
        if side == "left":
            r = {"knob": KNOBS[ex.get("size", "L")][1] if kind == "knob" else 0, "button": BTN_R}.get(kind, 0)
            cap = SMALL_CAP if (kind == "knob" and ex["size"] == "T") else LABEL_CAP
            n = len(lines)
            for i, ln in enumerate(lines):
                out.append((ln, x - r - 0.9, y + cap / 2 + (i - (n - 1) / 2) * (cap + 0.6), cap, "end"))
            return out
        if kind == "knob":
            r = KNOBS[ex["size"]][1]
            cap = LABEL_CAP if ex["size"] != "T" else SMALL_CAP
            base = y - r - 1.1
            pos = ex.get("positions") or []
            n = len(pos)
            if any(abs(math.cos(math.radians(-135 + 270 * i / (n - 1) - 90))) < 0.5 and i not in (0, n - 1) for i in range(n)):
                base -= 2.2
            for i, ln in enumerate(reversed(lines)):
                out.append((ln, x, base - i * (cap + 0.6), cap, "middle"))
        elif kind == "slider":
            base = y - SL_LEN / 2 - 1.0
            out.append((lines[0], x, base, LABEL_CAP, "middle"))
        elif kind == "jack":
            base = y + JACK_R + 0.9 + LABEL_CAP
            for i, ln in enumerate(lines):
                out.append((ln, x, base + i * (LABEL_CAP + 0.6), LABEL_CAP, "middle"))
        elif kind == "led":
            out.append((lines[0], x, y + LED_R + 0.8 + SMALL_CAP, SMALL_CAP, "middle"))
        elif kind == "button":
            base = y - BTN_R - 1.1
            for i, ln in enumerate(reversed(lines)):
                out.append((ln, x, base - i * (LABEL_CAP + 0.6), LABEL_CAP, "middle"))
        elif kind == "switch":
            base = y - SW_H / 2 - 1.0
            for i, ln in enumerate(reversed(lines)):
                out.append((ln, x, base - i * (LABEL_CAP + 0.6), LABEL_CAP, "middle"))
        return out

    def extra_labels(self, item):
        kind, pid, label, x, y, ex = item
        out = []
        if kind == "switch" and ex["show"]:
            pos = ex["positions"]
            ys = [y - 2.9, y + 2.9 + SMALL_CAP] if len(pos) == 2 else [y - 2.6, y + SMALL_CAP / 2, y + 2.6 + SMALL_CAP]
            out += [(p_, x + SW_W / 2 + 0.7, yy, SMALL_CAP, "start") for p_, yy in zip(pos, ys)]
        if kind == "knob" and ex.get("positions"):
            pos = ex["positions"]
            r = KNOBS[ex["size"]][1] + 1.3
            n = len(pos)
            for i, t in enumerate(pos):
                a = math.radians(-135 + 270 * i / (n - 1) - 90)
                px, py = x + r * math.cos(a), y + r * math.sin(a)
                anchor = "end" if math.cos(a) < -0.3 else ("start" if math.cos(a) > 0.3 else "middle")
                out.append((t, px, py + SMALL_CAP / 2, SMALL_CAP, anchor))
        if kind == "slider" and ex.get("bipolar"):
            out.append(("+", x + SL_W / 2 + 0.5, y - SL_TRAVEL / 2 + SMALL_CAP / 2, SMALL_CAP, "start"))
            out.append(("−", x + SL_W / 2 + 0.5, y + SL_TRAVEL / 2 + SMALL_CAP / 2, SMALL_CAP, "start"))
        return out

    def all_labels(self):
        out = []
        for it in self.items:
            out += [(t, it) for t in self.label_lines(it)]
            out += [(t, it) for t in self.extra_labels(it)]
        return out

    def boxes(self):
        b = []
        for it in self.items:
            kind, pid, label, x, y, ex = it
            r = {"jack": JACK_BOX, "led": LED_R + 0.2, "button": BTN_R + 0.2}.get(kind)
            if kind == "knob":
                r = KNOBS[ex["size"]][2]
            if r:
                b.append(Box(x - r, y - r, x + r, y + r, pid, pid))
            elif kind == "slider":
                b.append(Box(x - SL_W / 2, y - SL_LEN / 2, x + SL_W / 2, y + SL_LEN / 2, pid, pid))
            else:
                b.append(Box(x - SW_W / 2, y - SW_H / 2, x + SW_W / 2, y + SW_H / 2, pid, pid))
        for (t, x, base, cap, anchor), it in self.all_labels():
            w = text_width(t, cap)
            x0 = x - w / 2 if anchor == "middle" else x if anchor == "start" else x - w
            own = it[1] if (it[0] in ("knob", "slider") and t in (it[5].get("positions") or []) + ["+", "−"]) else None
            if it[0] == "jack" and it[5]["out"]:
                own = it[1]
            b.append(Box(x0, base - cap, x0 + w, base, f"label '{t}' of {it[1]}", own))
        for (t, x, base, cap, anchor, white) in self.texts:
            w = text_width(t, cap)
            x0 = x - w / 2 if anchor == "middle" else x if anchor == "start" else x - w
            b.append(Box(x0, base - cap, x0 + w, base, f"text '{t}'"))
        for (did, x0, y0, x1, y1) in self.displays:
            b.append(Box(x0, y0, x1, y1, f"display {did}"))
        for it in self.items:
            kind, pid, label, x, y, ex = it
            if kind == "jack" and ex["out"] and label:
                x0, y0, x1, y1 = self.out_plate(it)
                b.append(Box(x0, y0, x1, y1, f"plate of {pid}", pid))
        return b

    def out_plate(self, it):
        kind, pid, label, x, y, ex = it
        nl = len(label.split("\n"))
        top = y - JACK_R - 0.9
        bot = y + JACK_R + 0.3 + nl * (LABEL_CAP + 0.6) + 0.9
        wl = max(2 * JACK_R + 1.6, max(text_width(l, LABEL_CAP) for l in label.split("\n")) + 1.4)
        return (x - wl / 2, top, x + wl / 2, bot)

    def check(self):
        errs = []
        bx = self.boxes()
        for i in range(len(bx)):
            a = bx[i]
            if a.x0 < 1.2 or a.x1 > self.w - 1.2 or a.y0 < 16.0 or a.y1 > (122.5 if a.what.startswith("plate") else 121.0):
                errs.append(f"{a.what} leaves the panel area ({a.x0:.1f},{a.y0:.1f})-({a.x1:.1f},{a.y1:.1f})")
            for j in range(i + 1, len(bx)):
                if a.owner and a.owner == bx[j].owner:
                    continue
                if a.overlaps(bx[j]):
                    errs.append(f"{a.what} overlaps {bx[j].what}")
        for it in self.items:
            if it[0] != "switch" and not it[2] and not it[5].get("grid"):
                errs.append(f"{it[1]} has no label")
        ids = [it[1] for it in self.items]
        if len(set(ids)) != len(ids):
            errs.append("duplicate ids")
        return errs

    # output ----------------------------------------------------------------
    def svg(self):
        w = self.w
        col = FAMILY_COLOURS[self.family]
        out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w:.2f}mm" height="{H}mm" viewBox="0 0 {w:.2f} {H}">']
        out.append(f'<rect x="0" y="0" width="{w:.2f}" height="{H}" fill="{GROUND}"/>')
        out.append(f'<rect x="1" y="1" width="{w - 2:.2f}" height="{H - 2}" fill="none" stroke="#2b2b2b" stroke-width="0.4"/>')
        ink, white, g = [], [], []
        # title
        cap = TITLE_CAP
        while text_width(self.title, cap) > w - 4:
            cap -= 0.1
        ink.append(text_path(self.title, w / 2, 6.6 + cap, cap))
        # family stripe
        g.append(f'<rect x="1.2" y="11.2" width="{w - 2.4:.2f}" height="3.0" fill="{col}"/>')
        gcap = 1.5
        txt = self.family if text_width(self.family, gcap) < w - 5 - text_width(self.short, gcap) else self.family[:3]
        white.append(text_path(txt, 2.4, 13.45, gcap, "start"))
        white.append(text_path(self.short, w - 2.4, 13.45, gcap, "end"))
        ink.append(text_path("DENSITY", w / 2, 125.4, SMALL_CAP))
        # sections
        for (title, x0, y0, x1, y1) in self.sections:
            g.append(f'<rect x="{x0:.2f}" y="{y0:.2f}" width="{x1 - x0:.2f}" height="{y1 - y0:.2f}" rx="0.8" fill="none" stroke="{INK}" stroke-opacity="0.7" stroke-width="0.35"/>')
            if title:
                tw = text_width(title, SMALL_CAP) + 1.4
                g.append(f'<rect x="{x0 + 1.6:.2f}" y="{y0 - 1.0:.2f}" width="{tw:.2f}" height="2.0" fill="{GROUND}"/>')
                ink.append(text_path(title, x0 + 2.3, y0 + SMALL_CAP / 2, SMALL_CAP, "start"))
        for (x0, y0, x1, y1) in self.plates:
            g.append(f'<rect x="{x0:.2f}" y="{y0:.2f}" width="{x1 - x0:.2f}" height="{y1 - y0:.2f}" rx="0.9" fill="{PLATE}"/>')
        # output plates behind labelled output jacks
        for it in self.items:
            kind, pid, label, x, y, ex = it
            if kind == "jack" and ex["out"] and label:
                x0, y0, x1, y1 = self.out_plate(it)
                g.append(f'<rect x="{x0:.2f}" y="{y0:.2f}" width="{x1 - x0:.2f}" height="{y1 - y0:.2f}" rx="0.9" fill="{PLATE}"/>')
        # knob scales
        for it in self.items:
            kind, pid, label, x, y, ex = it
            if kind == "knob":
                r = KNOBS[ex["size"]][1]
                a0, a1 = math.radians(-135 - 90), math.radians(135 - 90)
                sx, sy = x + r * math.cos(a0), y + r * math.sin(a0)
                ex_, ey = x + r * math.cos(a1), y + r * math.sin(a1)
                g.append(f'<path d="M{sx:.2f},{sy:.2f} A{r},{r} 0 1 1 {ex_:.2f},{ey:.2f}" fill="none" stroke="{INK}" stroke-width="0.35"/>')
                if ex.get("bipolar"):
                    g.append(f'<line x1="{x:.2f}" y1="{y - r - 0.8:.2f}" x2="{x:.2f}" y2="{y - r + 0.4:.2f}" stroke="{INK}" stroke-width="0.35"/>')
                    if ex["size"] != "T":
                        ink.append(text_path("−", sx - 0.5, sy + 2.2, SMALL_CAP, "end"))
                        ink.append(text_path("+", ex_ + 0.5, ey + 2.2, SMALL_CAP, "start"))
                if ex.get("positions"):
                    n = len(ex["positions"])
                    for i in range(n):
                        a = math.radians(-135 + 270 * i / (n - 1) - 90)
                        g.append(f'<circle cx="{x + r * math.cos(a):.2f}" cy="{y + r * math.sin(a):.2f}" r="0.35" fill="{INK}"/>')
            elif kind == "slider":
                y0, y1 = y - SL_TRAVEL / 2, y + SL_TRAVEL / 2
                for i in range(11):
                    yy = y0 + (y1 - y0) * i / 10
                    l = 1.3 if i in (0, 5, 10) else 0.7
                    g.append(f'<line x1="{x - SL_W / 2 - 0.4 - l:.2f}" y1="{yy:.2f}" x2="{x - SL_W / 2 - 0.4:.2f}" y2="{yy:.2f}" stroke="{INK}" stroke-width="0.25"/>')
        for (did, x0, y0, x1, y1) in self.displays:
            g.append(f'<rect x="{x0:.2f}" y="{y0:.2f}" width="{x1 - x0:.2f}" height="{y1 - y0:.2f}" rx="0.8" fill="#15171a" stroke="#2b2b2b" stroke-width="0.4"/>')
        for (t, x, base_, cap_, anchor), it in self.all_labels():
            is_out = it[0] == "jack" and it[5]["out"]
            (white if is_out else ink).append(text_path(t, x, base_, cap_, anchor))
        for (t, x, y, cap_, anchor, wh) in self.texts:
            (white if wh else ink).append(text_path(t, x, y, cap_, anchor))
        out.append("".join(g))
        out.append(f'<path fill="{INK}" d="{" ".join(ink)}"/>')
        out.append(f'<path fill="{PLATE_INK}" d="{" ".join(white)}"/>')
        out.append("</svg>")
        return "\n".join(out)

    def ids(self):
        par, inp, out, lig = [], [], [], []
        for kind, pid, label, x, y, ex in self.items:
            if kind in ("knob", "switch", "slider"):
                par.append(pid)
            elif kind == "button":
                par.append(pid)
                lig.append(ex["light"])
                if ex["rgb"]:
                    lig += [ex["light"] + "_G", ex["light"] + "_B"]
            elif kind == "jack":
                (out if ex["out"] else inp).append(pid)
            elif kind == "led":
                lig.append(pid)

        def en(name, xs, tail):
            return f"\tenum {name} {{ " + "".join(x + ", " for x in xs) + tail + " };"
        return "\n".join([f"struct {self.slug}Ids {{",
                          en("ParamId", par, "PARAMS_LEN"), en("InputId", inp, "INPUTS_LEN"),
                          en("OutputId", out, "OUTPUTS_LEN"), en("LightId", lig, "LIGHTS_LEN"), "};", ""])

    def cpp(self):
        lines = [f"template <class M, class W>\ninline void place{self.slug}(W* w, M* m) {{"]
        for kind, pid, label, x, y, ex in self.items:
            v = f"mm2px(Vec({x:.3f}f, {y:.3f}f))"
            if kind == "knob":
                lines.append(f"\tw->addParam(createParamCentered<{KNOBS[ex['size']][3]}>({v}, m, M::{pid}));")
            elif kind == "slider":
                lines.append(f"\tw->addParam(createParamCentered<DSlider>({v}, m, M::{pid}));")
            elif kind == "switch":
                cls = "DToggle2" if len(ex["positions"]) == 2 else "DToggle3"
                lines.append(f"\tw->addParam(createParamCentered<{cls}>({v}, m, M::{pid}));")
            elif kind == "button":
                light = "RedGreenBlueLight" if ex["rgb"] else "DLight"
                cls = f"VCVLightLatch<MediumSimpleLight<{light}>>" if ex["latch"] else f"VCVLightBezel<{light}>"
                lines.append(f"\tw->addParam(createLightParamCentered<{cls}>({v}, m, M::{pid}, M::{ex['light']}));")
            elif kind == "jack":
                cls = PORT_CLASSES[ex["kind"]]
                if ex["out"]:
                    lines.append(f"\tw->addOutput(createOutputCentered<{cls}>({v}, m, M::{pid}));")
                else:
                    lines.append(f"\tw->addInput(createInputCentered<{cls}>({v}, m, M::{pid}));")
            elif kind == "led":
                lines.append(f"\tw->addChild(createLightCentered<SmallLight<DLight>>({v}, m, M::{pid}));")
        lines.append("}")
        for (did, x0, y0, x1, y1) in self.displays:
            lines.append(f"static const float {self.slug.upper()}_{did}_MM[4] = {{{x0:.3f}f, {y0:.3f}f, {x1 - x0:.3f}f, {y1 - y0:.3f}f}};")
        lines.append("")
        return "\n".join(lines)


P = []

# ================================================================ panels ======
# ---- 1. HARMONIA (56 HP) -----------------------------------------------------
p = Panel("Harmonia", "HARMONIA", "HRM", "HARMONIC", 56)
SL_NAMES = [("ODD", False), ("EVEN", False), ("PART", False), ("TILT", True), ("TUNE", True), ("OCT", True)]
SL_LABELS = {"ODD": "ODD", "EVEN": "EVEN", "PART": "PARTIALS", "TILT": "TILT", "TUNE": "TUNE", "OCT": "OCTAVE"}
for v in range(6):
    row, colm = divmod(v, 3)
    x0 = 2.4 + 62.5 * colm
    y0 = 18.0 + 38.0 * row
    p.section(f"VOICE {v + 1}", x0, y0, x0 + 61.0, y0 + 36.0)
    for i, (k, bip) in enumerate(SL_NAMES):
        p.slider(f"{k}{v + 1}_PARAM", SL_LABELS[k], x0 + 6.0 + 9.0 * i, y0 + 20.0, bipolar=bip)
    p.button(f"MULT{v + 1}_PARAM", f"MULT{v + 1}_LIGHT", "MULT", x0 + 57.3, y0 + 20.0, latch=True)
p.display("DISPLAY", 190.0, 17.5, 282.0, 41.0)
p.section("ENVELOPE", 190.0, 44.5, 236.5, 94.0)
for i, (k, lab) in enumerate([("ATK", "ATTACK"), ("DEC", "DECAY"), ("SUS", "SUSTAIN"), ("REL", "RELEASE")]):
    p.slider(f"{k}_PARAM", lab, 197.0 + 10.5 * i, 63.5)
for i, (k, lab, bip) in enumerate([("RATE", "RATE", False), ("SPREAD", "SPREAD", False), ("FMFADE", "FM FADE", True), ("LEVEL", "LEVEL", False)]):
    p.knob(f"{k}_PARAM", lab, 197.0 + 11.0 * i, 87.0, size="S", bipolar=bip)
p.section("PLAY", 238.5, 44.5, 282.0, 94.0)
p.switch("ENVT_PARAM", ["DRONE", "ONE SHOT", "CYCLE"], 242.5, 55.0, "")
p.text("ENVELOPE", 249.0, 49.2, SMALL_CAP)
p.switch("ASSIGN_PARAM", ["STACK", "ROUND ROBIN", "CHANNEL"], 242.5, 78.5, "")
p.text("ASSIGN", 247.5, 72.7, SMALL_CAP)
p.button("LOCK_PARAM", "LOCK_LIGHT", "HARMONIC\nLOCK", 266.0, 59.0, latch=True)
p.button("MORPHON_PARAM", "MORPHON_LIGHT", "MORPH", 277.0, 59.0, latch=True)
p.button("STA_PARAM", "STA_LIGHT", "STORE\nA", 266.0, 75.5)
p.button("STB_PARAM", "STB_LIGHT", "STORE\nB", 277.0, 75.5)
p.button("DICE_PARAM", "DICE_LIGHT", "DICE", 271.5, 88.5)
# bottom band
for i, (k, lab) in enumerate([("VOCT", "V/OCT"), ("GATE", "GATE"), ("DICE", "DICE")]):
    p.jack(f"{k}_INPUT", lab, 7.5 + 11.0 * i, 110.0, "poly" if k in ("VOCT", "GATE") else "gate")
for i, (k, lab) in enumerate([("ODD", "ODD CV"), ("EVEN", "EVEN CV"), ("PART", "PART CV"), ("TILT", "TILT CV"),
                              ("MORPH", "MORPH CV"), ("LOCA", "LOC A CV"), ("LOCB", "LOC B CV"), ("LOCC", "LOC C CV")]):
    x = 40.0 + 12.0 * i
    p.knob(f"{k}_CV_PARAM", lab, x, 101.0, size="T", bipolar=True)
    p.jack(f"{k}_CV_INPUT", lab, x, 110.5, "poly" if i < 4 else "cv")
for i, (k, lab, bip) in enumerate([("ROOT", "ROOT", True), ("MULTD", "MULT DEPTH", False), ("MORPH", "MORPH", False),
                                   ("LOCA", "LOCATION A", True), ("LOCB", "LOCATION B", True), ("LOCC", "LOCATION C", True)]):
    p.knob(f"{k}_PARAM", lab, 136.2 + 16.2 * i, 108.0, bipolar=bip)
for i, (k, lab, kind) in enumerate([("OUTL", "LEFT", "audio"), ("OUTR", "RIGHT", "audio"), ("VOICES", "VOICES", "poly"), ("ENVS", "ENVELOPES", "poly")]):
    p.jack(f"{k}_OUTPUT", lab, [232.0, 244.0, 256.0, 270.0][i], 109.5, kind)
P.append(p)

# ---- 2. TRACE (26 HP) --------------------------------------------------------
p = Panel("Trace", "TRACE", "TRC", "HARMONIC", 26)
TX = {"SRC": 13.0, "REC": 24.5, "OFS": 34.0, "ATT": 43.5, "SPD": 53.0, "IN": 63.0, "OUT": 74.0}
for k, lab in [("SRC", "SOURCE"), ("REC", "REC"), ("OFS", "OFFSET"), ("ATT", "ATTEN"), ("SPD", "SPEED"), ("IN", "IN")]:
    p.text(lab, TX[k], 21.0, LABEL_CAP)
p.plate(TX["OUT"] - 5.2, 17.6, TX["OUT"] + 5.2, 112.4)
p.text("OUT", TX["OUT"], 21.0, LABEL_CAP, white=True)
for i in range(8):
    y = 27.5 + 11.2 * i
    n = i + 1
    p.text(str(n), 4.2, y + 0.95, LABEL_CAP)
    p.knob(f"SRC{n}_PARAM", "", TX["SRC"], y, size="S", bipolar=True, grid=True)
    p.button(f"REC{n}_PARAM", f"REC{n}_LIGHT", "", TX["REC"], y, rgb=True, grid=True)
    p.knob(f"OFS{n}_PARAM", "", TX["OFS"], y, size="T", bipolar=True, grid=True)
    p.knob(f"ATT{n}_PARAM", "", TX["ATT"], y, size="T", bipolar=True, grid=True)
    p.knob(f"SPD{n}_PARAM", "", TX["SPD"], y, size="T", grid=True)
    p.jack(f"CV{n}_INPUT", "", TX["IN"], y, "cv", grid=True)
    p.jack(f"OUT{n}_OUTPUT", "", TX["OUT"], y, "cv", grid=True)
p.section("ALL LANES", 82.5, 18.0, 130.0, 118.5)
p.knob("SLOW_PARAM", "SLOW", 96.0, 33.0, positions=["1×", "2×", "4×", "8×"])
p.button("SUB_PARAM", "SUB_LIGHT", "SUB\nLOOP", 118.0, 33.0, latch=True)
p.button("RPH_PARAM", "RPH_LIGHT", "RANDOM\nPHASE", 96.0, 56.0)
p.button("CLR_PARAM", "CLR_LIGHT", "CLEAR\nALL", 118.0, 56.0)
p.jack("RPH_INPUT", "RANDOM\nPHASE", 96.0, 71.0, "gate")
p.jack("CLR_INPUT", "CLEAR\nALL", 118.0, 71.0, "gate")
p.jack("RECG_INPUT", "REC\nGATES", 96.0, 92.0, "poly")
p.jack("ALL_OUTPUT", "ALL", 118.0, 92.0, "poly")
P.append(p)

# ---- 3. NONET (18 HP) --------------------------------------------------------
p = Panel("Nonet", "NONET", "NON", "HARMONIC", 18)
p.display("DISPLAY", 3.0, 17.5, 88.44, 36.0)
p.knob("ALG_PARAM", "ALGORITHM", 10.0, 47.5)
p.button("MODE_PARAM", "MODE_LIGHT", "MODE", 24.0, 47.5)
for i in range(3):
    p.led(f"MODE{i + 1}_LIGHT", str(i + 1), 30.5 + 4.0 * i, 47.5)
p.button("RST_PARAM", "RST_LIGHT", "RESTART", 50.5, 47.5)
p.knob("FILT_PARAM", "FILTER", 66.5, 47.5, bipolar=True)
p.knob("MIX_PARAM", "MIX", 82.0, 47.5)
PX = [9.5, 23.9, 38.3, 52.7, 67.1, 81.5]
for i, x in enumerate(PX):
    p.knob(f"P{i + 1}_PARAM", f"P{i + 1}", x, 64.8)
    p.knob(f"P{i + 1}_CV_PARAM", f"P{i + 1}", x, 79.0, size="T", bipolar=True)
    p.jack(f"P{i + 1}_INPUT", f"P{i + 1}", x, 88.5, "cv")
for i, (k, lab, kind) in enumerate([("INL", "IN L", "audio"), ("INR", "IN R", "audio"), ("ALG", "ALG", "cv"),
                                    ("MODE", "MODE", "gate"), ("CLK", "CLOCK", "gate"), ("RST", "RESTART", "gate")]):
    p.jack(f"{k}_INPUT", lab, PX[i], 100.0, kind)
p.knob("MIX_CV_PARAM", "MIX", 16.5, 114.0, size="T", bipolar=True, side="left")
p.jack("MIX_INPUT", "MIX", 27.0, 113.5, "cv")
p.jack("OUTL_OUTPUT", "OUT L", 67.1, 113.5)
p.jack("OUTR_OUTPUT", "OUT R", 81.5, 113.5)
P.append(p)

# ---- 4. TANGLE (22 HP) -------------------------------------------------------
p = Panel("Tangle", "TANGLE", "TGL", "ENTANGLE", 22)
p.display("DISPLAY", 3.0, 17.5, 108.76, 37.0)
p.knob("BANK_PARAM", "BANK", 12.5, 49.0, positions=["A", "B", "C", "D"])
p.knob("PROG_PARAM", "PROGRAM", 32.0, 49.0)
p.button("BTN_PARAM", "BTN_LIGHT", "BUTTON", 48.5, 49.0, latch=False)
p.knob("ENT_PARAM", "ENTANGLE", 65.0, 49.0, bipolar=True)
p.knob("INLV_PARAM", "INPUT", 85.0, 49.0, size="S")
p.knob("MIX_PARAM", "MIX", 100.0, 49.0, size="S")
GX = [9.0 + 15.6 * i for i in range(7)]
for i in range(6):
    p.knob(f"POT{i + 1}_PARAM", f"POT {i + 1}", GX[i], 69.0)
    p.knob(f"POT{i + 1}_CV_PARAM", f"POT {i + 1}", GX[i], 87.0, size="T", bipolar=True)
    p.jack(f"POT{i + 1}_INPUT", f"POT {i + 1}", GX[i], 96.5, "cv")
p.knob("ENT_CV_PARAM", "ENTANGLE", GX[6], 87.0, size="T", bipolar=True)
p.jack("ENT_CV_INPUT", "ENTANGLE", GX[6], 96.5, "cv")
BX = [7.0 + 12.0 * i for i in range(9)]
for i, (k, lab, kind) in enumerate([("INL", "IN L", "audio"), ("INR", "IN R", "audio"), ("PROG", "PROGRAM", "cv"),
                                    ("BANK", "BANK", "cv"), ("BTN", "BUTTON", "gate"), ("ENTIN", "ENTANGLE\nIN", "audio")]):
    p.jack(f"{k}_INPUT", lab, BX[i], 110.0, kind)
for i, (k, lab) in enumerate([("OUTL", "OUT L"), ("OUTR", "OUT R"), ("ENT", "ENTANGLE\nOUT")]):
    p.jack(f"{k}_OUTPUT", lab, BX[6 + i], 110.0)
P.append(p)

# ---- 5. WEAVE (32 HP) --------------------------------------------------------
p = Panel("Weave", "WEAVE", "WVE", "ENTANGLE", 32)
p.section("MODULATORS", 2.4, 18.0, 98.5, 100.2)
for i in range(12):
    row, colm = divmod(i, 4)
    cx = 14.0 + 24.0 * colm
    y = 27.6 + 27.0 * row
    p.knob(f"RATE{i + 1}_PARAM", "RATE", cx - 5.6, y, size="S")
    p.knob(f"SHAPE{i + 1}_PARAM", "SHAPE", cx + 5.6, y, size="S")
    p.jack(f"MOD{i + 1}_OUTPUT", str(i + 1), cx, y + 10.8, "cv")
p.knob("COUPLE_PARAM", "COUPLE", 10.0, 111.0)
p.knob("SPEED_PARAM", "SPEED", 27.0, 111.0)
p.switch("UNI_PARAM", ["BIPOLAR", "UNIPOLAR"], 40.0, 111.0, "")
p.jack("SPEED_INPUT", "SPEED", 62.0, 111.0, "cv")
p.jack("RESET_INPUT", "RESET", 74.0, 111.0, "gate")
p.jack("ALL_OUTPUT", "ALL", 89.0, 111.0, "poly")
WX = {"IN": 107.0, "CV": 118.0, "LEV": 129.0, "RESP": 138.5, "OUT": 154.5}
p.section("GATES", 100.5, 18.0, 160.2, 68.5)
for k, lab in [("IN", "IN"), ("CV", "CV"), ("LEV", "LEVEL")]:
    p.text(lab, WX[k], 23.0, LABEL_CAP)
p.text("RESPONSE", 141.5, 23.0, LABEL_CAP)
p.plate(WX["OUT"] - 4.9, 19.8, WX["OUT"] + 4.9, 66.5)
p.text("OUT", WX["OUT"], 23.0, LABEL_CAP, white=True)
for j in range(4):
    y = 29.5 + 11.0 * j
    p.jack(f"GIN{j + 1}_INPUT", "", WX["IN"], y, "audio", grid=True)
    p.jack(f"GCV{j + 1}_INPUT", "", WX["CV"], y, "cv", grid=True)
    p.knob(f"GLEV{j + 1}_PARAM", "", WX["LEV"], y, size="S", grid=True)
    p.switch(f"GRESP{j + 1}_PARAM", ["VCA", "LPG", "BOTH"], WX["RESP"], y, "", grid=True)
    p.jack(f"GOUT{j + 1}_OUTPUT", "", WX["OUT"], y, "audio", grid=True)
MX = {"IN": 107.0, "LEV": 118.5, "PAN": 130.0, "PCV": 141.5}
p.section("MIXER", 100.5, 71.0, 160.2, 120.0)
for k, lab in [("IN", "MIX IN"), ("LEV", "LEVEL"), ("PAN", "PAN"), ("PCV", "PAN CV")]:
    p.text(lab, MX[k], 75.2, LABEL_CAP)
for j in range(4):
    y = 81.5 + 11.2 * j
    p.jack(f"MIN{j + 1}_INPUT", "", MX["IN"], y, "audio", grid=True)
    p.knob(f"MLEV{j + 1}_PARAM", "", MX["LEV"], y, size="S", grid=True)
    p.knob(f"MPAN{j + 1}_PARAM", "", MX["PAN"], y, size="S", grid=True, bipolar=True)
    p.jack(f"MPCV{j + 1}_INPUT", "", MX["PCV"], y, "cv", grid=True)
p.jack("MOUTL_OUTPUT", "MIX L", 154.0, 84.0)
p.jack("MOUTR_OUTPUT", "MIX R", 154.0, 102.0)
P.append(p)

# ---- 6. AXIOM (28 HP) --------------------------------------------------------
p = Panel("Axiom", "AXIOM", "AXM", "EQUATION", 28)
p.display("DISPLAY", 3.0, 17.5, 65.0, 36.0)
for i, (k, lab, kind) in enumerate([("CLK", "CLOCK", "gate"), ("RST", "RESET", "gate"), ("EQ", "EQUATION", "poly")]):
    p.jack(f"{k}_INPUT", lab, 73.0 + 12.0 * i, 23.5, kind)
for i, (k, lab) in enumerate([("PGATE", "GATES"), ("PPITCH", "PITCHES"), ("PACC", "ACCENTS")]):
    p.jack(f"{k}_OUTPUT", lab, 109.0 + 12.0 * i, 23.5, "poly")
p.knob("TEMPO_PARAM", "TEMPO", 11.0, 48.5)
p.button("RUN_PARAM", "RUN_LIGHT", "RUN", 25.5, 48.5, latch=True)
p.knob("COMPLEX_PARAM", "COMPLEXITY", 42.0, 48.5)
p.knob("LOGIC_PARAM", "LOGIC", 64.0, 48.5, size="S", positions=["OFF", "XOR", "AND", "OR"])
p.knob("RANGE_PARAM", "RANGE", 87.0, 48.5)
p.knob("SCALE_PARAM", "SCALE", 106.0, 48.5)
p.button("SEED_PARAM", "SEED_LIGHT", "SEED", 124.0, 48.5)
CH = "WXYZ"
for c in range(4):
    cx = 18.7 + 35.1 * c
    n = CH[c]
    p.section(n, cx - 16.8, 56.5, cx + 16.8, 120.5)
    p.knob(f"LEN{n}_PARAM", "LENGTH", cx - 11.2, 66.5, size="S")
    p.knob(f"RATIO{n}_PARAM", "RATIO", cx, 66.5, size="S")
    p.knob(f"ROT{n}_PARAM", "ROTATE", cx + 11.2, 66.5, size="S")
    p.knob(f"A{n}_PARAM", "A", cx - 11.2, 80.5, size="S")
    p.knob(f"EQ{n}_PARAM", "EQUATION", cx, 80.5, size="S")
    p.knob(f"B{n}_PARAM", "B", cx + 11.2, 80.5, size="S")
    p.knob(f"A{n}_CV_PARAM", "A", cx - 11.2, 93.0, size="T", bipolar=True)
    p.led(f"GATE{n}_LIGHT", "GATE", cx, 92.0)
    p.knob(f"B{n}_CV_PARAM", "B", cx + 11.2, 93.0, size="T", bipolar=True)
    p.jack(f"A{n}_INPUT", "A", cx - 11.2, 102.0, "cv")
    p.jack(f"B{n}_INPUT", "B", cx + 11.2, 102.0, "cv")
    p.jack(f"GATE{n}_OUTPUT", "GATE", cx - 11.2, 114.0, "gate")
    p.jack(f"ACC{n}_OUTPUT", "ACC", cx, 114.0, "cv")
    p.jack(f"PITCH{n}_OUTPUT", "PITCH", cx + 11.2, 114.0, "cv")
P.append(p)

# ---- 7. TETRA (30 HP) --------------------------------------------------------
p = Panel("Tetra", "TETRA", "TET", "EQUATION", 30)
for c in range(4):
    cx = 16.5 + 29.5 * c
    n = CH[c]
    a, b = cx - 7.3, cx + 7.3
    p.section(n, cx - 14.3, 18.5, cx + 14.3, 120.5)
    p.knob(f"PITCH{n}_PARAM", "PITCH", a, 30.0, bipolar=True)
    p.knob(f"INDEX{n}_PARAM", "INDEX", b, 30.0)
    p.knob(f"RATIO{n}_PARAM", "RATIO", a, 47.0, size="S")
    p.knob(f"FB{n}_PARAM", "FEEDBACK", b, 47.0, size="S")
    p.knob(f"DECAY{n}_PARAM", "DECAY", a, 61.5, size="S")
    p.knob(f"PENV{n}_PARAM", "PITCH ENV", b, 61.5, size="S")
    p.knob(f"IENV{n}_PARAM", "INDEX ENV", a, 76.0, size="S", bipolar=True)
    p.knob(f"PAN{n}_PARAM", "PAN", b, 76.0, size="T", bipolar=True)
    p.jack(f"TRIG{n}_INPUT", "TRIG", a, 90.0, "gate" if c else "poly")
    p.jack(f"VOCT{n}_INPUT", "V/OCT", b, 90.0, "cv" if c else "poly")
    p.jack(f"ACC{n}_INPUT", "ACCENT", a, 106.0, "cv" if c else "poly")
    p.jack(f"OUT{n}_OUTPUT", "OUT", b, 106.0)
p.section("", 120.4, 18.5, 150.8, 120.5)
p.knob("ROUTE_PARAM", "MATRIX ROUTE", 135.6, 31.0, positions=[str(i) for i in range(1, 9)])
p.knob("MATRIX_PARAM", "MATRIX\nDEPTH", 128.0, 52.0)
p.knob("SPREAD_PARAM", "DETUNE", 143.5, 52.0, size="S")
p.knob("LEVEL_PARAM", "LEVEL", 128.0, 69.0)
p.switch("MODE_PARAM", ["VOICE", "DRUM"], 140.0, 69.0, "MODE")
p.knob("INDEX_CV_PARAM", "INDEX", 128.0, 84.0, size="T", bipolar=True)
p.knob("MATRIX_CV_PARAM", "MATRIX", 143.5, 84.0, size="T", bipolar=True)
p.jack("INDEX_INPUT", "INDEX", 128.0, 93.5, "poly")
p.jack("MATRIX_INPUT", "MATRIX", 143.5, 93.5, "cv")
p.jack("OUTL_OUTPUT", "LEFT", 128.0, 108.0)
p.jack("OUTR_OUTPUT", "RIGHT", 143.5, 108.0)
P.append(p)

# ---- 8. CHEB (10 HP) ---------------------------------------------------------
p = Panel("Cheb", "CHEB", "CHB", "EQUATION", 10)
p.display("DISPLAY", 3.0, 17.5, 47.8, 37.0)
p.knob("SCAN_PARAM", "SCAN", 11.5, 48.0)
p.knob("MIX_PARAM", "MIX", 25.4, 49.5, size="S")
p.knob("SPREAD_PARAM", "SPREAD", 39.3, 48.0, bipolar=True)
p.knob("DRIVE_PARAM", "DRIVE", 11.5, 65.0)
p.knob("BIAS_PARAM", "BIAS", 26.5, 65.5, size="S", bipolar=True)
p.knob("LEVEL_PARAM", "LEVEL", 40.0, 65.5, size="S")
CX = [8.5, 19.8, 31.1, 42.3]
for i, (k, lab) in enumerate([("SCAN", "SCAN"), ("SPREAD", "SPREAD"), ("DRIVE", "DRIVE"), ("BIAS", "BIAS")]):
    p.knob(f"{k}_CV_PARAM", lab, CX[i], 80.5, size="T", bipolar=True)
    p.jack(f"{k}_INPUT", lab, CX[i], 90.0, "cv")
p.jack("INL_INPUT", "IN L", CX[0], 105.0)
p.jack("INR_INPUT", "IN R", CX[1], 105.0)
p.jack("OUTL_OUTPUT", "OUT L", CX[2], 105.0)
p.jack("OUTR_OUTPUT", "OUT R", CX[3], 105.0)
P.append(p)


# ---------------------------------------------------------------- components
def components():
    c = {}
    rings = {"PortAudio": "#8a8f94", "PortCv": "#3a6ea5", "PortGate": "#b33a2e", "PortPoly": "#d1a823"}
    for name, col in rings.items():
        nut = []
        for i in range(6):
            a = math.pi / 3 * i + math.pi / 6
            nut.append(f"{4 + 3.55 * math.cos(a):.3f},{4 + 3.55 * math.sin(a):.3f}")
        c[name] = f"""<svg xmlns="http://www.w3.org/2000/svg" width="8mm" height="8mm" viewBox="0 0 8 8">
<circle cx="4" cy="4" r="3.95" fill="{col}"/>
<polygon points="{" ".join(nut)}" fill="#3a3a3a" stroke="#222" stroke-width="0.2"/>
<circle cx="4" cy="4" r="2.35" fill="#6d6d6d"/>
<circle cx="4" cy="4" r="1.7" fill="#0b0b0b"/>
</svg>"""

    def knob(name, r, line_w):
        d = 2 * r
        c[name] = f"""<svg xmlns="http://www.w3.org/2000/svg" width="{d}mm" height="{d}mm" viewBox="0 0 {d} {d}">
<circle cx="{r}" cy="{r}" r="{r}" fill="#2c2d2f"/>
<circle cx="{r}" cy="{r}" r="{r * 0.84:.3f}" fill="#38393c" stroke="#232425" stroke-width="0.25"/>
<line x1="{r}" y1="{r * 0.12:.3f}" x2="{r}" y2="{r * 0.78:.3f}" stroke="#f4f3ee" stroke-width="{line_w}" stroke-linecap="round"/>
</svg>"""
    knob("DKnob", 5.5, 0.7)
    knob("DKnobSmall", 4.0, 0.6)
    knob("DTrim", 3.0, 0.5)

    def toggle(d):
        cx, cy = SW_W / 2, SW_H / 2
        if d == 0:
            handle = f'<circle cx="{cx}" cy="{cy}" r="1.2" fill="#f0f0f0" stroke="#999" stroke-width="0.2"/>'
        else:
            ty = cy + d * 3.1
            handle = (f'<line x1="{cx}" y1="{cy}" x2="{cx}" y2="{ty}" stroke="#d8d8d8" stroke-width="1.2" stroke-linecap="round"/>'
                      f'<circle cx="{cx}" cy="{ty}" r="0.85" fill="#f0f0f0" stroke="#999" stroke-width="0.15"/>')
        return f"""<svg xmlns="http://www.w3.org/2000/svg" width="{SW_W}mm" height="{SW_H}mm" viewBox="0 0 {SW_W} {SW_H}">
<rect x="0.25" y="0.25" width="{SW_W - 0.5}" height="{SW_H - 0.5}" rx="0.6" fill="#2b2b2b"/>
<circle cx="{cx}" cy="{cy}" r="1.9" fill="#9a9a9a" stroke="#6a6a6a" stroke-width="0.25"/>
<circle cx="{cx}" cy="{cy}" r="1.3" fill="#bdbdbd"/>
{handle}
</svg>"""
    c["Toggle2_0"] = toggle(-1)
    c["Toggle2_1"] = toggle(1)
    c["Toggle3_0"] = toggle(-1)
    c["Toggle3_1"] = toggle(0)
    c["Toggle3_2"] = toggle(1)
    c["SliderBg"] = f"""<svg xmlns="http://www.w3.org/2000/svg" width="{SL_W}mm" height="{SL_LEN}mm" viewBox="0 0 {SL_W} {SL_LEN}">
<rect x="{SL_W / 2 - 0.7}" y="{SL_HANDLE / 2 - 0.3}" width="1.4" height="{SL_TRAVEL + 0.6}" rx="0.7" fill="#2a2a2a"/>
</svg>"""
    c["SliderHandle"] = f"""<svg xmlns="http://www.w3.org/2000/svg" width="{SL_W}mm" height="{SL_HANDLE}mm" viewBox="0 0 {SL_W} {SL_HANDLE}">
<rect x="0.1" y="0.1" width="{SL_W - 0.2}" height="{SL_HANDLE - 0.2}" rx="0.6" fill="#2c2d2f" stroke="#1a1a1a" stroke-width="0.2"/>
<line x1="0.5" y1="{SL_HANDLE / 2}" x2="{SL_W - 0.5}" y2="{SL_HANDLE / 2}" stroke="#f4f3ee" stroke-width="0.5"/>
</svg>"""
    return c


def main():
    errs = []
    for p in P:
        for e in p.check():
            errs.append(f"{p.slug}: {e}")
    if errs:
        print("\n".join(errs))
        raise SystemExit(f"{len(errs)} layout problems")
    os.makedirs(os.path.join(ROOT, "res", "components"), exist_ok=True)
    os.makedirs(os.path.join(ROOT, "src", "generated"), exist_ok=True)
    for p in P:
        with open(os.path.join(ROOT, "res", f"{p.slug}.svg"), "w") as f:
            f.write(p.svg())
    for name, svg in components().items():
        with open(os.path.join(ROOT, "res", "components", f"{name}.svg"), "w") as f:
            f.write(svg)
    with open(os.path.join(ROOT, "src", "generated", "Layout.hpp"), "w") as f:
        f.write("// Generated by scripts/make_panels.py. Do not edit.\n#pragma once\n#include \"../widgets/DensityComponents.hpp\"\n\n")
        f.write(f"static const float SLIDER_TRAVEL_MM = {SL_TRAVEL}f, SLIDER_HANDLE_MM = {SL_HANDLE}f;\n\n")
        for p in P:
            f.write(p.ids())
            f.write(p.cpp())
    print(f"wrote {len(P)} panels: " + ", ".join(f"{p.slug} {p.hp} HP" for p in P))


if __name__ == "__main__":
    main()
