import sys
d = sys.argv[1]
# 4x3 membrane keypad (same style as mit/keypad.svg)
labels = "123456789*0#"
o = []
W = 3 * 11.7 + 3.6
o.append('<rect x="0.5" y="0.5" width="%g" height="57" rx="2" fill="#1d1d1d" stroke="#000" stroke-width="0.3"/>' % W)
o.append('<rect x="%g" y="57" width="18" height="5" fill="#8a7a50" stroke="#5a4f30" stroke-width="0.2"/>' % ((W + 1) / 2 - 9))
for r in range(4):
    for c in range(3):
        x = 3 + c * 11.7; y = 3 + r * 13.4
        o.append('<rect x="%g" y="%g" width="9.6" height="11.4" rx="1" fill="#f2f2f2" stroke="#666" stroke-width="0.2"/>' % (x, y))
        o.append('<rect x="%g" y="%g" width="9.6" height="11.4" rx="1" fill="url(#g)"/>' % (x, y))
        o.append('<text x="%g" y="%g" font-family="sans-serif" font-size="5" font-weight="bold" fill="#222" text-anchor="middle">%s</text>' % (x + 4.8, y + 7.3, labels[r * 3 + c]))
open(d + "/mit/keypad_4x3.svg", "w").write(
    '<?xml version="1.0" encoding="UTF-8"?>\n<!-- SPDX-License-Identifier: MIT | (c) SimulIDE-AI-Sloped contributors -->\n'
    '<svg xmlns="http://www.w3.org/2000/svg" version="1.2" baseProfile="tiny" width="%gmm" height="62.5mm" viewBox="0 0 %g 62.5"><title>4x3 membrane keypad</title><defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity="0.25"/><stop offset="1" stop-color="#000" stop-opacity="0.15"/></linearGradient></defs>' % (W + 1, W + 1)
    + "".join(o) + "</svg>\n")

# Bench power-supply module (icon for Volt. Source)
psu = '''<?xml version="1.0" encoding="UTF-8"?>
<!-- SPDX-License-Identifier: MIT | (c) SimulIDE-AI-Sloped contributors -->
<svg xmlns="http://www.w3.org/2000/svg" version="1.2" baseProfile="tiny" width="56" height="80" viewBox="0 0 56 80"><title>DC power supply</title>
<defs><linearGradient id="c" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4a5058"/><stop offset="1" stop-color="#262a2f"/></linearGradient>
<radialGradient id="k" cx="0.4" cy="0.35" r="0.7"><stop offset="0" stop-color="#6a6a6a"/><stop offset="1" stop-color="#1b1b1b"/></radialGradient></defs>
<rect x="1" y="1" width="54" height="78" rx="4" fill="url(#c)" stroke="#15181b" stroke-width="1.2"/>
<rect x="6" y="6" width="44" height="16" rx="2" fill="#0b1a0b" stroke="#000" stroke-width="0.8"/>
<text x="28" y="18.5" font-family="monospace" font-size="11" font-weight="bold" fill="#5cff6a" text-anchor="middle">5.00V</text>
<circle cx="28" cy="46" r="15" fill="url(#k)" stroke="#000" stroke-width="1"/>
<circle cx="28" cy="46" r="10" fill="#2b2b2b" stroke="#555" stroke-width="0.6"/>
<rect x="27" y="37" width="2" height="8" rx="1" fill="#e0e0e0"/>
<circle cx="14" cy="71" r="4" fill="#d32f2f" stroke="#600" stroke-width="0.8"/>
<circle cx="42" cy="71" r="4" fill="#222" stroke="#000" stroke-width="0.8"/>
</svg>
'''
open(d + "/mit/psu.svg", "w").write(psu)
