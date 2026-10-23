# Generates realistic Arduino board art (portrait, px units) whose female
# header holes sit exactly on SimulIDE package pin positions.
# Style (colours, header/USB/jack/chip shapes) follows wokwi-elements
# arduino-uno/mega/nano (MIT).
import re, sys

pkgdir, outdir = sys.argv[1], sys.argv[2]

PCB = "#0f7f8a"; PCB_EDGE = "#0a5c64"; SILK = "#ffffff"


def pins(pkg):
    s = open(pkgdir + "/" + pkg + ".package").read()
    res = []
    for m in re.finditer(r'<pin\s+side="(\w+)"\s+pos="(\d+)"[^>]*?id="([^"]*)"[^>]*?label="([^"]*)"', s):
        res.append((m.group(1), int(m.group(2)), m.group(3), m.group(4).strip()))
    return res


def groups(ps):
    ps = sorted(ps)
    g = [[ps[0]]]
    for p in ps[1:]:
        if p == g[-1][-1] + 1: g[-1].append(p)
        else: g.append([p])
    return g


class B:
    def __init__(s, W, H): s.W, s.H, s.o = W, H, []
    def a(s, x): s.o.append(x)
    def svg(s, title):
        return ('<?xml version="1.0" encoding="UTF-8"?>\n<!-- SPDX-License-Identifier: MIT | derived from wokwi-elements (c) Uri Shaked -->\n'
                '<svg xmlns="http://www.w3.org/2000/svg" version="1.2" baseProfile="tiny" width="%d" height="%d" viewBox="0 0 %d %d"><title>%s</title>\n' % (s.W, s.H, s.W, s.H, title)
                + "\n".join(s.o) + "\n</svg>\n")
    def pcb(s, path=None):
        if path: s.a('<path d="%s" fill="%s" stroke="%s" stroke-width="1.2"/>' % (path, PCB, PCB_EDGE))
        else: s.a('<rect x="0.6" y="0.6" width="%g" height="%g" rx="4" fill="%s" stroke="%s" stroke-width="1.2"/>' % (s.W - 1.2, s.H - 1.2, PCB, PCB_EDGE))
    def hole(s, x, y, r=4):
        s.a('<circle cx="%g" cy="%g" r="%g" fill="#d8d8d0" stroke="#9a9a90" stroke-width="0.6"/><circle cx="%g" cy="%g" r="%g" fill="#1a1a1a"/>' % (x, y, r, x, y, r * 0.6))
    def vheader(s, x, ys):  # vertical strip of female header holes
        y0, y1 = min(ys), max(ys)
        s.a('<rect x="%g" y="%g" width="8.4" height="%g" rx="0.6" fill="#1c1c1c" stroke="#000" stroke-width="0.4"/>' % (x - 4.2, y0 - 4.2, y1 - y0 + 8.4))
        for y in ys:
            s.a('<rect x="%g" y="%g" width="3.6" height="3.6" fill="#050505" stroke="#3a3a3a" stroke-width="0.5"/>' % (x - 1.8, y - 1.8))
    def hheader(s, y, xs, rows=1):
        x0, x1 = min(xs), max(xs)
        s.a('<rect x="%g" y="%g" width="%g" height="%g" rx="0.6" fill="#1c1c1c" stroke="#000" stroke-width="0.4"/>' % (x0 - 4.2, y - 4.2, x1 - x0 + 8.4, 8 * rows + 0.4))
        for r in range(rows):
            for x in xs:
                s.a('<rect x="%g" y="%g" width="3.6" height="3.6" fill="#050505" stroke="#3a3a3a" stroke-width="0.5"/>' % (x - 1.8, y + 8 * r - 1.8))
    def text(s, x, y, t, size=4.6, anchor="start", rot=0, weight="normal", fill=SILK):
        t = t.replace("&", "&amp;").replace("<", "&lt;")
        tr = ' transform="rotate(%d %g %g)"' % (rot, x, y) if rot else ""
        s.a('<text x="%g" y="%g" font-family="sans-serif" font-size="%g" font-weight="%s" fill="%s" text-anchor="%s"%s>%s</text>' % (x, y, size, weight, fill, anchor, tr, t))
    def usb_b(s, x, y, w=30, h=40):  # silver USB-B, opening at bottom edge
        s.a('<rect x="%g" y="%g" width="%g" height="%g" rx="1" fill="#b9b9b9" stroke="#7d7d7d" stroke-width="0.8"/>' % (x, y, w, h))
        s.a('<rect x="%g" y="%g" width="%g" height="%g" fill="#d6d6d6"/>' % (x + 3, y + 3, w - 6, h - 6))
        for i in range(4): s.a('<rect x="%g" y="%g" width="%g" height="1" fill="#a0a0a0"/>' % (x + 3, y + 8 + i * 7, w - 6))
    def usb_small(s, cx, y, w=18, h=14):
        s.a('<rect x="%g" y="%g" width="%g" height="%g" rx="1" fill="#c4c4c4" stroke="#7d7d7d" stroke-width="0.7"/>' % (cx - w / 2, y, w, h))
        s.a('<rect x="%g" y="%g" width="%g" height="3" rx="1" fill="#555"/>' % (cx - w / 2 + 3, y + h - 5, w - 6))
    def jack(s, x, y, w=24, h=44):
        s.a('<rect x="%g" y="%g" width="%g" height="%g" rx="1.5" fill="#1a1a1a" stroke="#000" stroke-width="0.6"/>' % (x, y, w, h))
        s.a('<rect x="%g" y="%g" width="%g" height="%g" rx="1" fill="#2c2c2c"/>' % (x + 3, y + 3, w - 6, h - 10))
        s.a('<circle cx="%g" cy="%g" r="%g" fill="#000" stroke="#444" stroke-width="0.6"/>' % (x + w / 2, y + h - 8, w * 0.28))
    def dip(s, x, y, n, label):  # vertical DIP, n pins total
        half = n // 2; h = half * 6.35 / 1.0
        w = 22
        for i in range(half):
            yy = y + 4 + i * 6.35
            s.a('<rect x="%g" y="%g" width="3" height="2.4" fill="#cfcfcf"/><rect x="%g" y="%g" width="3" height="2.4" fill="#cfcfcf"/>' % (x - 2.5, yy, x + w - 0.5, yy))
        s.a('<rect x="%g" y="%g" width="%g" height="%g" rx="1" fill="#252728" stroke="#111" stroke-width="0.5"/>' % (x, y, w, h + 4))
        s.a('<path d="M%g %g a3 3 0 0 0 6 0" fill="#111"/>' % (x + w / 2 - 3, y))
        s.text(x + w / 2 + 2, y + h / 2 + 2, label, 4.2, "middle", 90, fill="#bbbbbb")
    def qfp(s, cx, cy, size, label, rot=45):
        s.a('<g transform="rotate(%d %g %g)">' % (rot, cx, cy))
        n = 8; st = size / (n + 1)
        for i in range(n):
            o = -size / 2 + st * (i + 1)
            s.a('<rect x="%g" y="%g" width="1" height="%g" fill="#d0d0d0"/>' % (cx + o - 0.5, cy - size / 2 - 2.5, size + 5))
            s.a('<rect x="%g" y="%g" width="%g" height="1" fill="#d0d0d0"/>' % (cx - size / 2 - 2.5, cy + o - 0.5, size + 5))
        s.a('<rect x="%g" y="%g" width="%g" height="%g" fill="#252728"/>' % (cx - size / 2, cy - size / 2, size, size))
        s.a('<circle cx="%g" cy="%g" r="1" fill="#3c3c3c"/>' % (cx - size / 2 + 2.5, cy - size / 2 + 2.5))
        s.a('</g>')
        s.text(cx, cy + 1.5, label, 3.6, "middle", fill="#bbbbbb")
    def button(s, cx, cy, sz=11):
        s.a('<rect x="%g" y="%g" width="%g" height="%g" rx="1" fill="#d0d0d0" stroke="#888" stroke-width="0.5"/>' % (cx - sz / 2, cy - sz / 2, sz, sz))
        s.a('<circle cx="%g" cy="%g" r="%g" fill="#c9302c" stroke="#7a1a18" stroke-width="0.5"/>' % (cx, cy, sz * 0.3))
    def crystal(s, x, y, w=7, h=16):
        s.a('<rect x="%g" y="%g" width="%g" height="%g" rx="3.5" fill="#d8d8d8" stroke="#999" stroke-width="0.5"/>' % (x, y, w, h))
    def smd(s, x, y, w=3, h=1.6, c="#6b5a3a"):
        s.a('<rect x="%g" y="%g" width="%g" height="%g" fill="%s"/><rect x="%g" y="%g" width="0.7" height="%g" fill="#ddd"/><rect x="%g" y="%g" width="0.7" height="%g" fill="#ddd"/>' % (x, y, w, h, c, x, y, h, x + w - 0.7, y, h))
    def led(s, x, y, lab):
        s.a('<rect x="%g" y="%g" width="4" height="3" fill="#f2e9c9" stroke="#bbb" stroke-width="0.3"/>' % (x, y))
        s.text(x + 6, y + 3, lab, 3.8)
    def logo(s, cx, cy, name, rot=-90, size=9):
        # infinity logo + name, rotated to read along the board
        s.a('<g transform="rotate(%d %g %g)">' % (rot, cx, cy))
        s.a('<path d="M%g %g c-3 -4 -8 -4 -8 0 s5 4 8 0 c3 -4 8 -4 8 0 s-5 4 -8 0z" fill="none" stroke="%s" stroke-width="1.4"/>' % (cx, cy - size * 1.1, SILK))
        s.a('<text x="%g" y="%g" font-family="sans-serif" font-size="%g" font-weight="bold" fill="%s" text-anchor="middle">%s</text>' % (cx, cy + size * 0.35, size, SILK, name))
        s.a('</g>')


def side_headers(b, ps, W):
    left = [p for p in ps if p[0] == "left"]; right = [p for p in ps if p[0] == "right"]
    for g in groups([p[1] for p in left]): b.vheader(8, [8 * q for q in g])
    for g in groups([p[1] for p in right]): b.vheader(W - 8, [8 * q for q in g])
    # pin labels are drawn by SimulIDE's pins themselves


def uno(name, pkg, fname, title, leo=False):
    W, H = 160, 224
    b = B(W, H)
    b.pcb("M4 0.6 H156 a3.4 3.4 0 0 1 3.4 3.4 V206 l-6 6 V223.4 H4 a3.4 3.4 0 0 1 -3.4 -3.4 V4 a3.4 3.4 0 0 1 3.4 -3.4z")
    for x, y in ((136, 9), (28, 210), (136, 186), (110, 10)): pass
    b.hole(140, 10); b.hole(22, 148); b.hole(140, 205)
    ps = pins(pkg)
    side_headers(b, ps, W)
    if leo:
        b.usb_small(44, 210, 20, 14)
        b.qfp(100, 160, 22, "32U4")
    else:
        b.usb_b(28, 178, 32, 46)
        b.dip(90, 118, 28, "ATMEGA328P")
        b.crystal(76, 128, 6, 14)
        b.qfp(72, 160, 10, "16U2", 0)
    b.jack(122, 176, 24, 48)
    b.button(22, 168)
    b.text(22, 160, "RESET", 3.6, "middle")
    for r in range(3):
        for c in range(2): b.a('<rect x="%g" y="%g" width="4.4" height="4.4" fill="#1c1c1c"/><circle cx="%g" cy="%g" r="1" fill="#c8a640"/>' % (96 + c * 6 - 2.2, 14 + r * 6 - 2.2, 96 + c * 6, 14 + r * 6))
    b.text(99, 9, "ICSP", 3.6, "middle")
    b.text(42, 129, "L", 4.2)
    b.led(104, 216, "ON")
    b.logo(78, 195, name, -90, 9 if len(name) < 5 else 6)
    open(outdir + "/" + fname, "w").write(b.svg(title))


def mega():
    W, H = 160, 319
    b = B(W, H)
    b.pcb("M4 0.6 H156 a3.4 3.4 0 0 1 3.4 3.4 V300 l-6 6 V318.4 H4 a3.4 3.4 0 0 1 -3.4 -3.4 V4 a3.4 3.4 0 0 1 3.4 -3.4z")
    b.hole(140, 40); b.hole(22, 40); b.hole(22, 266); b.hole(140, 290)
    ps = pins("mega")
    side_headers(b, ps, W)
    b.hheader(16, [8 * q for q in range(1, 19)], 2)
    b.usb_b(28, 270, 32, 49)
    b.jack(122, 268, 24, 51)
    b.qfp(86, 150, 40, "ATMEGA2560")
    b.crystal(60, 196)
    b.qfp(62, 240, 12, "16U2", 0)
    b.button(76, 266)
    b.text(76, 258, "RESET", 3.6, "middle")
    for i in range(8): b.smd(110 + (i % 2) * 6, 210 + (i // 2) * 6)
    b.led(52, 222, "L" if False else ""); b.text(58, 234, "L", 4.2)
    b.led(110, 254, "ON")
    b.logo(86, 92, "MEGA", -90, 13)
    open(outdir + "/arduino_mega.svg", "w").write(b.svg("Arduino Mega 2560"))


def nano():
    W, H = 64, 152
    b = B(W, H)
    b.pcb()
    b.hole(6, 5, 2.4); b.hole(58, 5, 2.4); b.hole(6, 147, 2.4); b.hole(58, 147, 2.4)
    ps = pins("arduino_nano")
    left = [p for p in ps if p[0] == "left"]; right = [p for p in ps if p[0] == "right"]
    for g in groups([p[1] for p in left]): b.vheader(8, [8 * q for q in g])
    for g in groups([p[1] for p in right]): b.vheader(W - 8, [8 * q for q in g])

    b.qfp(32, 76, 14, "328P", 45)
    b.button(32, 108, 8)
    b.crystal(29, 40, 6, 12)
    b.usb_small(32, 138, 16, 14)
    open(outdir + "/arduino_nano.svg", "w").write(b.svg("Arduino Nano"))


uno("UNO", "arduino_uno", "arduino_uno.svg", "Arduino Uno")
uno("DUEMILANOVE", "arduino_uno", "arduino_duemilanove.svg", "Arduino Duemilanove")
uno("LEONARDO", "leonardo", "arduino_leonardo.svg", "Arduino Leonardo", True)
mega(); nano()
