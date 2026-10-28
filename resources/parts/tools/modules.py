#!/usr/bin/env python3
# Generates the original (MIT) art of the modules / discrete parts in
# resources/parts/modules.   Usage: modules.py <resources/parts dir>
# Each file is a stand-alone SVG; swap any of them (keep the size/aspect and
# the pin hole positions) or re-point the part in src/gui/circuitwidget/
# partart.cpp (PartArt::init, "modules" block) to use other art.
# SVG units = scene px; SVG (x, y) = component (x - w/2, y - h/2).
import os
import sys

OUT = os.path.join(sys.argv[1] if len(sys.argv) > 1 else ".", "modules")
os.makedirs(OUT, exist_ok=True)

GRADS = '''<defs>
<linearGradient id="metal" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f4f5f7"/><stop offset="0.5" stop-color="#b9bdc2"/><stop offset="1" stop-color="#7d8186"/></linearGradient>
<linearGradient id="metalh" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#8d9196"/><stop offset="0.45" stop-color="#eceff1"/><stop offset="1" stop-color="#7a7e83"/></linearGradient>
<linearGradient id="blk" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4a4a50"/><stop offset="1" stop-color="#1b1b1e"/></linearGradient>
<linearGradient id="blkh" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#2a2a2d"/><stop offset="0.4" stop-color="#4b4b51"/><stop offset="1" stop-color="#1a1a1c"/></linearGradient>
<radialGradient id="dome" cx="0.4" cy="0.35" r="0.75"><stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#cfd6d8"/></radialGradient>
<radialGradient id="knob" cx="0.4" cy="0.35" r="0.8"><stop offset="0" stop-color="#6a6a70"/><stop offset="1" stop-color="#18181a"/></radialGradient>
<linearGradient id="screw" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#f2f2f2"/><stop offset="1" stop-color="#8e9296"/></linearGradient>
</defs>'''


class Svg:
    def __init__(self, name, w, h, title, license_note="Original art"):
        self.name, self.w, self.h = name, w, h
        self.o = []
        self.title = title

    def a(self, s):
        self.o.append(s)

    def rect(self, x, y, w, h, fill, stroke="none", sw=0.5, rx=0):
        self.a('<rect x="%g" y="%g" width="%g" height="%g" rx="%g" fill="%s" stroke="%s" stroke-width="%g"/>' % (x, y, w, h, rx, fill, stroke, sw))

    def circle(self, x, y, r, fill, stroke="none", sw=0.5):
        self.a('<circle cx="%g" cy="%g" r="%g" fill="%s" stroke="%s" stroke-width="%g"/>' % (x, y, r, fill, stroke, sw))

    def ell(self, x, y, rx, ry, fill, stroke="none", sw=0.5):
        self.a('<ellipse cx="%g" cy="%g" rx="%g" ry="%g" fill="%s" stroke="%s" stroke-width="%g"/>' % (x, y, rx, ry, fill, stroke, sw))

    def line(self, x1, y1, x2, y2, stroke, sw=1, cap="butt"):
        self.a('<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s" stroke-width="%g" stroke-linecap="%s"/>' % (x1, y1, x2, y2, stroke, sw, cap))

    def path(self, d, fill="none", stroke="none", sw=0.5):
        self.a('<path d="%s" fill="%s" stroke="%s" stroke-width="%g"/>' % (d, fill, stroke, sw))

    def text(self, x, y, s, size=5, fill="#ffffff", anchor="middle", weight="bold", rot=0):
        tr = ' transform="rotate(%g %g %g)"' % (rot, x, y) if rot else ""
        self.a('<text x="%g" y="%g" font-family="sans-serif" font-size="%g" font-weight="%s" fill="%s" text-anchor="%s"%s>%s</text>' % (x, y, size, weight, fill, anchor, tr, s))

    # pads / holes at pin positions (comp coords relative to the art centre)
    def hole(self, x, y, r=2.2):
        self.circle(x + self.w / 2, y + self.h / 2, r, "#e8c25a", "#9c7a20", 0.4)
        self.circle(x + self.w / 2, y + self.h / 2, r * 0.45, "#1c1c1c")

    def screw(self, x, y, r=4):
        cx, cy = x + self.w / 2, y + self.h / 2
        self.circle(cx, cy, r, "url(#screw)", "#555a5e", 0.5)
        self.line(cx - r * 0.6, cy, cx + r * 0.6, cy, "#444", 1)

    def led(self, x, y, col, r=2):
        self.circle(x, y, r, col, "#222", 0.3)
        self.circle(x - r * 0.3, y - r * 0.3, r * 0.35, "#ffffff")

    def save(self):
        s = '<?xml version="1.0" encoding="UTF-8"?>\n<!-- %s. Original art, MIT License (SimulIDE-AI-Sloped contributors) -->\n' % self.title
        s += '<svg xmlns="http://www.w3.org/2000/svg" version="1.2" baseProfile="tiny" width="%g" height="%g" viewBox="0 0 %g %g"><title>%s</title>\n' % (self.w, self.h, self.w, self.h, self.title)
        s += GRADS + "\n" + "\n".join(self.o) + "\n</svg>\n"
        open(os.path.join(OUT, self.name + ".svg"), "w").write(s)


def pcb(s, color="#1d6b3a", edge="#0e3d1f", rx=3, pad=0.5):
    s.rect(pad, pad, s.w - 2 * pad, s.h - 2 * pad, color, edge, 1, rx)
    for (x, y) in ((4, 4), (s.w - 4, 4), (4, s.h - 4), (s.w - 4, s.h - 4)):
        s.circle(x, y, 1.6, "#e8e8e8", "#9a9a9a", 0.4)


def silk(s, items):  # [(label, comp x, comp y, rot)]
    for (t, x, y, rot) in items:
        s.text(x + s.w / 2, y + s.h / 2, t, 3.6, "#f4f4f4", "middle", "bold", rot)


# --------------------------------------------------------------- modules ---

def tb6612():
    s = Svg("tb6612", 56, 72, "TB6612FNG dual motor driver breakout")
    pcb(s, "#c8232c", "#8e141b")
    left = ["VM", "VCC", "GND", "AO1", "AO2", "BO2", "BO1", "GND"]
    right = ["PWMA", "AIN2", "AIN1", "STBY", "BIN1", "BIN2", "PWMB", "GND"]
    for i in range(8):
        y = -28 + 8 * i
        s.hole(-24, y)
        s.hole(24, y)
    # chip
    s.rect(18, 24, 20, 24, "#202022", "#000", 0.5, 1)
    s.text(28, 36.5, "TB6612", 4.2, "#d8d8d8")
    s.text(28, 42, "FNG", 4.2, "#d8d8d8")
    s.circle(22, 28, 1.2, "#101010")
    for i in range(8):
        s.line(17, 26 + i * 3, 18, 26 + i * 3, "#c0c4c8", 1)
        s.line(38, 26 + i * 3, 39, 26 + i * 3, "#c0c4c8", 1)
    s.rect(21, 52, 14, 8, "#1f3f8f", "#0f2257", 0.5, 1)  # decoupling cap
    s.save()


def uln2003():
    s = Svg("uln2003", 40, 72, "ULN2003A Darlington array DIP-16")
    s.a('<g fill="#c9ccd0" stroke="#7d8186" stroke-width="0.4">')
    for i in range(8):
        y = 6.6 + 8 * i
        s.a('<rect x="0" y="%g" width="5" height="2.8"/><rect x="35" y="%g" width="5" height="2.8"/>' % (y, y))
    s.a('</g>')
    s.rect(4, 1, 32, 70, "url(#blkh)", "#111", 0.6, 1.5)
    s.path("M16,1 A4,4 0 0 0 24,1 Z", "#141416")
    s.circle(9, 6, 1.6, "#18181a", "#000", 0.3)
    s.text(20, 38, "ULN2003A", 4.6, "#d8d8d8", rot=-90)
    s.save()


def stepper28byj():
    # 28BYJ-48 geared stepper, wires toward the pins on the left
    s = Svg("stepper28byj", 128, 96, "28BYJ-48 stepper motor")
    ys = {"#2f6fd6": -32, "#f06fa8": -16, "#d8262c": 0, "#f2d21f": 16, "#f08a1e": 32}
    for col, y in ys.items():
        yy = y + 48
        s.path("M0,%g L16,%g L40,%g" % (yy, yy, 48 + y * 0.35), "none", col, 1.4)
    cx, cy = 64 + 0, 48
    s.rect(28, 38, 20, 20, "url(#metal)", "#555", 0.5, 2)           # ears bracket
    s.rect(44, 4, 64, 88, "none")
    s.rect(60, 2, 50, 8, "url(#metal)", "#555", 0.6, 1)             # mounting ears
    s.rect(60, 86, 50, 8, "url(#metal)", "#555", 0.6, 1)
    s.circle(66, 6, 1.7, "#222")
    s.circle(66, 90, 1.7, "#222")
    s.circle(104, 6, 1.7, "#222")
    s.circle(104, 90, 1.7, "#222")
    s.circle(85, 48, 40, "url(#metalh)", "#4a4f55", 1)               # steel can
    s.circle(85, 48, 33, "#c9ced3", "#8a9096", 0.6)
    s.circle(85, 48, 22, "#2b6bd0", "#173f85", 0.8)
    s.circle(85, 48, 6, "#f0b21e", "#a87808", 0.6)
    s.rect(34, 40, 18, 16, "#f2f2f2", "#999", 0.4, 1)                # plastic wire block
    s.text(85, 74, "28BYJ-48", 6, "#26303a")
    s.save()


def buzzer(active):
    s = Svg("buzzer_active" if active else "buzzer_passive", 24, 40, ("Active" if active else "Passive") + " buzzer, top view")
    c = (12, 20)
    s.circle(c[0], c[1], 12, "url(#blk)", "#000", 0.8)
    s.circle(c[0], c[1], 9.5, "#222225", "#3c3c40", 0.5)
    if active:
        s.circle(c[0], c[1], 2.6, "#0e0e10", "#444", 0.4)
        s.rect(6, 13, 5, 5, "#e9d9a0", "#a99a60", 0.3, 1)               # sticker
        s.text(8.5, 17.2, "+", 4.5, "#8a1c1c")
    else:
        s.circle(c[0], c[1], 3.2, "#000", "#555", 0.5)                   # sound hole
    s.text(6.5, 33.5, "+", 4.2, "#e0c050")
    s.save()


# ------------------------------------------------------ discrete parts ---

def ldr():
    s = Svg("ldr", 32, 20, "LDR photoresistor (GL5528)")
    s.line(0, 10, 11, 10, "#9aa0a6", 1.6)
    s.line(21, 10, 32, 10, "#9aa0a6", 1.6)
    s.circle(16, 10, 9.5, "#c9a063", "#7a5a2a", 0.8)
    s.circle(16, 10, 8, "#e8d6a8")
    s.path("M9,6 L23,6 L23,8 L9,8 L9,10 L23,10 L23,12 L9,12 L9,14 L23,14", "none", "#8d4b24", 1.1)
    s.rect(14.7, 3, 2.6, 14, "none")
    s.save()


def ntc():
    s = Svg("ntc", 32, 16, "NTC thermistor, epoxy bead")
    s.line(0, 8, 11, 8, "#9aa0a6", 1.6)
    s.line(21, 8, 32, 8, "#9aa0a6", 1.6)
    s.ell(16, 8, 7, 6.5, "#2b5aa8", "#16356a", 0.7)
    s.ell(14.5, 5.5, 3.5, 2.2, "#6f97d8")
    s.text(16, 10.5, "10K", 3.6, "#e8eefc")
    s.save()


def slidepot():
    s = Svg("slidepot", 24, 16, "Slide potentiometer")
    s.rect(0.5, 0.5, 23, 11, "url(#metal)", "#555a5e", 0.6, 1)
    s.rect(3, 4, 18, 4, "#16161a", "#000", 0.3, 2)
    for i in range(7):
        s.line(3.5 + i * 2.5, 1.5, 3.5 + i * 2.5, 3, "#555", 0.4)
    s.line(12, 11, 12, 16, "#9aa0a6", 1.6)
    s.save()


def to92(name, label, col="url(#blk)"):
    # Same geometry as the generic BJT art (leg tips B 1.8 / E 6.15 / C 10.45)
    s = Svg(name, 12.3, 24, label + " transistor, TO-92")
    for x, y2 in ((1.8, 23.2), (6.15, 23.7), (10.45, 23.2)):
        s.line(x, 14, x, y2, "#b4b8bc", 1.1)
    s.path("M0.6,14 L0.6,7 A5.55,5.55 0 0 1 11.7,7 L11.7,14 Z", col, "#000", 0.5)
    s.rect(1.2, 12, 9.9, 1.4, "none")
    s.text(6.15, 9.2, label, 2.6, "#e6e6e6")
    s.save()


def to220(name, label, w=20, h=26, bjt=False):
    s = Svg(name, w, h, label + " TO-220")
    cx = w / 2
    s.rect(cx - 8, 0, 16, 7, "url(#metal)", "#666", 0.5, 0.8)
    s.circle(cx, 3.2, 1.7, "#1c1c1c")
    s.rect(cx - 8, 7, 16, 12, "url(#blk)", "#000", 0.5, 0.8)
    s.text(cx, 14.5, label, 3.8, "#e6e6e6")
    for dx in (-5, 0, 5):
        s.line(cx + dx, 19, cx + dx, 25.5, "#b4b8bc", 1.2)
    s.save()


def axial(name, body, band, label, glass=False):
    s = Svg(name, 32, 10, name)
    s.line(0, 5, 32, 5, "#9aa0a6", 1.6)
    if glass:
        s.rect(8, 1.2, 16, 7.6, body, "#7a5a2a", 0.5, 3)
        s.rect(9, 2, 14, 2, "#ffffff", "none", 0, 1)
        s.a('<rect x="9" y="2" width="14" height="2" rx="1" fill="#ffffff" fill-opacity="0.35"/>')
    else:
        s.rect(7, 1, 18, 8, body, "#000", 0.5, 1.2)
    s.rect(20.5, 1.2, 3, 7.6, band, "none", 0, 0.5)
    s.text(14, 6.6, label, 3, "#e6e6e6" if not glass else "#222")
    s.save()


def fuse():
    s = Svg("fuse", 32, 10, "5x20 mm glass fuse")
    s.line(0, 5, 32, 5, "#9aa0a6", 1.6)
    s.rect(6, 1.2, 20, 7.6, "#dfeaee", "#8aa0a8", 0.5, 1.5)
    s.a('<rect x="6" y="1.2" width="20" height="7.6" rx="1.5" fill="#ffffff" fill-opacity="0.35"/>')
    s.path("M9,5 L12,3.4 L15,6.6 L18,3.4 L21,5", "none", "#555", 0.6)
    s.rect(4, 0.8, 5, 8.4, "url(#metalh)", "#666", 0.5, 1)
    s.rect(23, 0.8, 5, 8.4, "url(#metalh)", "#666", 0.5, 1)
    s.save()


def reed():
    s = Svg("reed", 32, 10, "Reed switch")
    s.line(0, 5, 32, 5, "#9aa0a6", 1.6)
    s.rect(5, 2, 22, 6, "#cfe6ee", "#7aa0ad", 0.5, 3)
    s.a('<rect x="5" y="2" width="22" height="6" rx="3" fill="#ffffff" fill-opacity="0.4"/>')
    s.line(9, 5, 17, 5, "#5a5a5a", 0.8)
    s.line(17, 5, 25, 4.2, "#5a5a5a", 0.8)
    s.save()


# ----------------------------------------------------------- sensors etc ---

def pir():
    s = Svg("pir", 48, 56, "HC-SR501 PIR motion sensor")
    pcb(s, "#2a7a45", "#14452a")
    s.circle(24, 22, 16, "#bfc8ca", "#8d9a9c", 0.8)
    s.circle(24, 22, 14.5, "url(#dome)", "#aab3b5", 0.6)
    for a in range(6):
        s.line(24, 22, 24 + 14 * (1 if a % 2 else -1) * (0.3 + 0.1 * a), 8 + a * 5, "#b8c0c2", 0.5)
    s.rect(4, 42, 14, 6, "#1b1b1f", "none", 0, 0.5)
    s.circle(41, 6, 2.6, "#d5a021", "#7a5a10", 0.5)                   # trim pots
    s.circle(41, 50, 2.6, "#d5a021", "#7a5a10", 0.5)
    for x, t in ((16, "VCC"), (24, "OUT"), (32, "GND")):
        s.hole(x - 24, 24)
    s.save()


def irsensor():
    s = Svg("irsensor", 40, 48, "IR obstacle avoidance sensor (FC-51)")
    pcb(s, "#1d4ea8", "#0e2a63")
    s.circle(10, 9, 4.2, "#e8e8f0", "#99a", 0.5)                      # IR emitter
    s.circle(10, 9, 2, "#c6c6d4")
    s.circle(30, 9, 4.2, "#2a1010", "#000", 0.5)                      # receiver
    s.circle(30, 9, 2, "#3a1a1a")
    s.rect(15, 20, 10, 10, "#2a6bd6", "#12357a", 0.5, 1)              # trim pot
    s.circle(20, 25, 3, "#d5a021", "#7a5a10", 0.5)
    s.line(18.5, 25, 21.5, 25, "#222", 0.8)
    s.led(7, 33, "#d02020", 1.8)
    s.led(33, 33, "#20c040", 1.8)
    s.rect(14, 33, 12, 5, "#1b1b1f")
    for x in (-8, 0, 8):
        s.hole(x, 20)
    s.text(20, 17, "FC-51", 3.8, "#dfe8ff")
    s.save()


def soil():
    s = Svg("soil", 48, 72, "Soil moisture sensor with comparator board")
    # probe
    s.path("M12,0.5 L36,0.5 L36,22 L24,34 L12,22 Z", "#1c1c20", "#000", 0.8)
    s.path("M16,3 L32,3 L32,21 L24,29 L16,21 Z", "#c9a24a", "#7a5a10", 0.5)
    s.line(24, 5, 24, 26, "#1c1c20", 0.8)
    s.rect(14, 33, 20, 6, "none")
    # board
    s.rect(0.5, 36, 47, 35.5, "#1d6b3a", "#0e3d1f", 1, 3)
    for (x, y) in ((4, 40), (44, 40), (4, 67), (44, 67)):
        s.circle(x, y, 1.6, "#e8e8e8", "#9a9a9a", 0.4)
    s.rect(14, 41, 20, 8, "#1b1b1f")
    s.text(24, 47, "LM393", 3.6, "#cfd")
    s.rect(33, 52, 10, 7, "#2a6bd6", "#12357a", 0.5, 1)
    s.circle(38, 55.5, 2.2, "#d5a021", "#7a5a10", 0.4)
    s.led(7, 52, "#d02020", 1.6)
    s.led(7, 58, "#20c040", 1.6)
    for x in (-12, -4, 4, 12):
        s.hole(x, 30)
    s.save()


def sound():
    s = Svg("sound", 48, 56, "Sound sensor module (KY-038)")
    pcb(s, "#1d4ea8", "#0e2a63")
    s.circle(15, 17, 9, "url(#metalh)", "#555a5e", 0.8)               # mic can
    s.circle(15, 17, 5, "#1a1a1d", "#000", 0.5)
    s.circle(15, 17, 1.8, "#444")
    s.rect(30, 9, 13, 13, "#2a6bd6", "#12357a", 0.6, 1)               # trim pot
    s.circle(36.5, 15.5, 4, "#d5a021", "#7a5a10", 0.5)
    s.line(34, 15.5, 39, 15.5, "#222", 1)
    s.rect(18, 30, 12, 6, "#1b1b1f")
    s.led(6, 33, "#d02020", 1.7)
    s.led(42, 33, "#20c040", 1.7)
    for x in (-12, -4, 4, 12):
        s.hole(x, 24)
    s.text(24, 31, "SOUND", 3.8, "#dfe8ff")
    s.save()


def joystick():
    s = Svg("joystick", 56, 56, "Analog joystick module (KY-023)")
    pcb(s, "#1c1c22", "#000")
    s.rect(6, 6, 44, 38, "url(#metal)", "#555", 0.8, 3)               # gimbal housing
    s.circle(28, 24, 15, "#2a2a2e", "#000", 0.8)
    s.circle(28, 24, 11, "url(#knob)", "#000", 0.8)
    s.circle(28, 24, 6, "#18181a", "#444", 0.5)
    for x in (-16, -8, 0, 8, 16):
        s.hole(x, 24)
    s.save()


def encoder():
    s = Svg("encoder", 56, 48, "Rotary encoder module (KY-040)")
    pcb(s, "#1d6b3a", "#0e3d1f")
    s.rect(15, 6, 26, 24, "url(#metal)", "#555", 0.8, 2)
    s.circle(28, 18, 11, "url(#knob)", "#000", 0.8)
    s.circle(28, 18, 8, "url(#metalh)", "#444", 0.5)
    s.line(28, 11, 28, 18, "#222", 1.4)
    for x in (-16, -8, 0, 8, 16):
        s.hole(x, 20)
    s.save()


def battery9v():
    s = Svg("battery9v", 28, 48, "9V battery (PP3)")
    s.rect(1, 4, 26, 43.5, "#1c1c20", "#000", 1, 2.5)
    s.rect(1, 4, 26, 15, "#c9ccd0", "#7d8186", 0.6, 2)
    s.rect(1, 18, 26, 29.5, "url(#blk)", "#000", 0.6, 2)
    s.rect(1, 20, 26, 8, "#d9a621", "none", 0, 0)
    s.text(14, 26.5, "9V", 6, "#222")
    s.text(14, 38, "Alkaline", 3.8, "#e0e0e0", weight="normal")
    s.rect(2.2, 0.5, 7.6, 5, "url(#metalh)", "#666", 0.5, 1)           # (+) female snap
    s.circle(6, 3, 1.8, "#222")
    s.rect(17.5, 0.5, 8.5, 5, "url(#metalh)", "#666", 0.5, 1)
    s.circle(22, 3, 2.1, "#d0d0d0", "#666", 0.4)
    s.text(6, 15, "+", 5, "#c01010")
    s.text(21, 15, "-", 5, "#222")
    s.save()


def aa_holder(name, rows, label):
    h = 24 if rows == 1 else 40
    s = Svg(name, 72, h, label)
    cy = h / 2
    s.rect(0.5, 0.5, 55, h - 1, "#222226", "#000", 1, 3)
    for r in range(rows):
        y = 3 + r * 18
        s.rect(4, y, 48, 14, "url(#blk)", "#000", 0.5, 2)
        s.rect(4, y, 38, 14, "#c9a826", "none", 0, 2)
        s.rect(42, y, 10, 14, "#c9ccd0", "#555", 0.4, 2)
        s.text(23, y + 9.3, "AA 1.5V", 5, "#2a2a2a")
        s.text(8, y + 9, "-", 6, "#222")
        s.text(48, y + 9.5, "+", 6, "#a01010")
    ys = (cy - 4, cy + 4)
    s.line(56, ys[0], 72, ys[0], "#d8262c", 1.8, "round")
    s.line(56, ys[1], 72, ys[1], "#111", 1.8, "round")
    s.save()


def cr2032():
    s = Svg("cr2032", 32, 32, "CR2032 coin cell in holder")
    s.rect(1, 6, 8, 20, "none")
    s.rect(0.5, 12, 6, 8, "url(#metalh)", "#666", 0.5, 1)             # - tab (left)
    s.rect(25.5, 12, 6, 8, "url(#metalh)", "#666", 0.5, 1)            # + tab (right)
    s.circle(16, 16, 14, "url(#metal)", "#666", 0.8)
    s.circle(16, 16, 11.5, "#d9dce0", "#8d9196", 0.5)
    s.text(16, 14.5, "CR2032", 4.4, "#444")
    s.text(16, 20.5, "3V", 4.4, "#444")
    s.text(24, 11, "+", 5, "#222")
    s.save()


def tactile():
    s = Svg("tactile", 24, 24, "Tactile push button, 4 pin")
    s.rect(2, 2, 20, 20, "url(#blk)", "#000", 0.6, 2)
    s.circle(12, 12, 7, "#303034", "#000", 0.6)
    s.circle(12, 12, 5.5, "#4a4a50", "#222", 0.5)
    for (x, y) in ((4.5, 4.5), (19.5, 4.5), (4.5, 19.5), (19.5, 19.5)):
        s.circle(x, y, 1, "#b4b8bc")
    for y in (4, 20):
        s.line(0, y, 3, y, "#b4b8bc", 1.4)
        s.line(21, y, 24, y, "#b4b8bc", 1.4)
    s.save()


def slideswitch():
    s = Svg("slideswitch", 24, 12, "SPDT slide switch")
    s.rect(0.5, 0.5, 23, 11, "url(#metal)", "#555a5e", 0.6, 1)
    s.rect(4, 3, 16, 5, "#16161a", "#000", 0.4, 2)
    s.circle(3, 6, 1, "#555")
    s.circle(21, 6, 1, "#555")
    s.save()


def toggle():
    s = Svg("toggle", 24, 28, "SPDT toggle switch (lever drawn by the part)")
    s.rect(2, 12, 20, 14, "url(#metal)", "#555a5e", 0.6, 2)
    s.circle(12, 14, 7, "url(#metalh)", "#555", 0.7)                 # hex nut
    s.circle(12, 14, 4, "#9aa0a6", "#555", 0.5)
    s.rect(6, 20, 12, 6, "#222", "none", 0, 1)
    s.save()


def irf520_module():
    s = Svg("irf520", 64, 56, "IRF520 MOSFET driver module")
    pcb(s, "#c8232c", "#8e141b")
    s.rect(8, 4, 15, 30, "#1c64c8", "#0e3d80", 0.6, 1)                # left terminal
    s.rect(41, 4, 15, 30, "#1c64c8", "#0e3d80", 0.6, 1)               # right terminal
    for (x, y) in ((-24, -8), (-24, 8), (24, -8), (24, 8)):
        s.screw(x, y, 4.2)
    s.rect(22, 6, 20, 22, "url(#blk)", "#000", 0.5, 1)                # TO-220
    s.rect(22, 2, 20, 6, "url(#metal)", "#666", 0.5, 0.5)
    s.text(32, 20, "IRF520", 4.5, "#e6e6e6")
    s.led(14, 40, "#d02020", 2)
    s.rect(24, 34, 14, 4, "#1b1b1f")
    for x in (-8, 0, 8):
        s.hole(x, 24)
    s.save()


def relaymod():
    s = Svg("relaymod", 64, 48, "Relay module (SRD-05VDC, 1 channel)")
    pcb(s, "#1d4ea8", "#0e2a63")
    s.rect(20, 5, 28, 28, "#1c64c8", "#0e2a63", 0.8, 2)               # relay can
    s.rect(22, 7, 24, 24, "#2a78e0", "none", 0, 1)
    s.text(34, 16, "SRD-05VDC", 3.6, "#eef")
    s.text(34, 21, "-SL-C", 3.6, "#eef")
    s.rect(54 - 6, 4, 14, 32, "none")
    s.rect(52, 5, 11, 34, "#1c3b7a", "#0b1d44", 0.6, 1)               # screw terminals
    for y in (-8, 0, 8):
        s.screw(28, y, 3.4)
    s.led(10, 38, "#20c040", 1.9)
    s.led(10, 31, "#d02020", 1.9)
    s.rect(18, 36, 12, 6, "#1b1b1f")
    s.rect(34, 36, 8, 6, "#1b1b1f")
    for y in (-8, 0, 8):
        s.hole(-28, y)
    s.save()


def pc817():
    s = Svg("pc817", 24, 16, "PC817 optocoupler DIP-4")
    for y in (4, 12):
        s.rect(0, y - 1, 4.5, 2, "#c9ccd0", "#7d8186", 0.3)
        s.rect(19.5, y - 1, 4.5, 2, "#c9ccd0", "#7d8186", 0.3)
    s.rect(4, 0.5, 16, 15, "url(#blkh)", "#111", 0.5, 1)
    s.circle(7, 3.2, 1, "#111", "#000", 0.3)
    s.text(12, 9.6, "PC817", 3.8, "#d8d8d8")
    s.save()


def main():
    tb6612(); uln2003(); stepper28byj(); buzzer(True); buzzer(False)
    ldr(); ntc(); slidepot()
    to92("t2n2222", "2N2222"); to92("bc547", "BC547")
    to220("tip120", "TIP120", 16, 26)
    to220("reg7805", "7805")
    to220("lm317", "LM317")
    axial("d1n4007", "url(#blk)", "#c9ccd0", "1N4007")
    axial("d1n4148", "#e8b44a", "#222", "4148", glass=True)
    axial("zener", "#9fd0e8", "#222", "5V1", glass=True)
    fuse(); reed()
    pir(); irsensor(); soil(); sound(); joystick(); encoder()
    battery9v(); aa_holder("aa2", 1, "2xAA battery holder"); aa_holder("aa4", 2, "4xAA battery holder")
    cr2032(); tactile(); slideswitch(); toggle()
    irf520_module(); relaymod(); pc817()


main()
