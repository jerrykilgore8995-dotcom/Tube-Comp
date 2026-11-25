"""Draw the 176 editor's rack, scales and hardware as original vector artwork."""
import argparse
from math import cos, radians, sin
from pathlib import Path

WIDTH, HEIGHT = 1140, 218
KNOBS = ((237, 121, 49), (370, 67, 24), (370, 166, 24),
         (772, 67, 24), (772, 166, 24), (904, 121, 49), (1035, 67, 24))


def point(x, y, radius, degrees):
    return x + radius * sin(radians(degrees)), y - radius * cos(radians(degrees))


def label(x, y, name, size=9, color="#e3d4ad", spacing=1.25):
    return (f'<text x="{x:.2f}" y="{y:.2f}" fill="{color}" font-size="{size}" '
            f'letter-spacing="{spacing}">{name}</text>')


def stroke(x, y, a, b, angle, color="#c9bd9d", width=.9):
    p, q = point(x, y, a, angle), point(x, y, b, angle)
    return f'<path d="M{p[0]:.2f} {p[1]:.2f}L{q[0]:.2f} {q[1]:.2f}" stroke="{color}" stroke-width="{width}"/>'


def draw(preview):
    svg = ['''<svg xmlns="http://www.w3.org/2000/svg" width="1140" height="218" viewBox="0 0 1140 218">
<defs>
 <linearGradient id="metal" x2="0%" y2="100%"><stop stop-color="#524d4b"/><stop offset=".08" stop-color="#655c58"/><stop offset=".48" stop-color="#47413f"/><stop offset=".94" stop-color="#514945"/><stop offset="1" stop-color="#262421"/></linearGradient>
 <linearGradient id="rail" x2="0%" y2="100%"><stop stop-color="#7f7a73"/><stop offset=".12" stop-color="#33312f"/><stop offset=".85" stop-color="#252423"/><stop offset="1" stop-color="#726c63"/></linearGradient>
 <radialGradient id="glass" cx="0.5" cy="0.45" r="0.7"><stop stop-color="#f4d59a"/><stop offset=".72" stop-color="#ddb677"/><stop offset="1" stop-color="#795738"/></radialGradient>
 <radialGradient id="knob" cx="0.38" cy="0.25" r="0.77"><stop stop-color="#555356"/><stop offset=".2" stop-color="#2b292a"/><stop offset=".65" stop-color="#131214"/><stop offset=".92" stop-color="#090809"/><stop offset="1" stop-color="#262326"/></radialGradient>
 <linearGradient id="bezel" x2="0%" y2="100%"><stop stop-color="#4f4942"/><stop offset=".11" stop-color="#1a1816"/><stop offset=".77" stop-color="#080708"/><stop offset="1" stop-color="#393333"/></linearGradient>
 <linearGradient id="brass" x2="0%" y2="100%"><stop stop-color="#f2e3c0"/><stop offset=".36" stop-color="#9f835e"/><stop offset=".58" stop-color="#4b3828"/><stop offset=".84" stop-color="#cab18a"/></linearGradient>
 <radialGradient id="jewel" cx="0.3" cy="0.24" r="0.85"><stop stop-color="#f6a77b"/><stop offset=".38" stop-color="#b42c16"/><stop offset=".75" stop-color="#540e0b"/><stop offset="1" stop-color="#210a08"/></radialGradient>
</defs>
<rect width="1140" height="218" fill="#171616"/>
<rect x="2" y="3" width="1136" height="212" rx="3" fill="url(#rail)"/>
<rect x="21" y="8" width="1098" height="202" rx="3" fill="#0f0e0e"/>
<rect x="25" y="10" width="1090" height="198" rx="6" fill="url(#metal)" stroke="#4d453c" stroke-width="1.8"/>
<rect x="29" y="13" width="1082" height="192" rx="5" fill="none" stroke="#bda782" stroke-width="1.4"/>
<rect x="31" y="15" width="1078" height="188" rx="4" fill="none" stroke="#b59a72" opacity=".24"/>
<g font-family="DejaVu Sans, sans-serif" text-anchor="middle">
''']
    for y in range(19, 199, 3):
        opacity = .03 + ((y * 17) % 5) * .013
        svg.append(f'<path d="M32 {y}H1108" stroke="#bdac9e" opacity="{opacity:.3f}" stroke-width=".55"/>')
    for x in (44, 1097):
        svg.append(f'<circle cx="{x}" cy="32" r="12" fill="#201a16" stroke="#84715d"/>'
                   f'<circle cx="{x}" cy="31" r="9.5" fill="url(#brass)" stroke="#d0bc97"/>'
                   f'<path d="M{x-5} 34l10 -6" stroke="#5c4632" stroke-width="2"/>')
    for y, title in ((90, "INPUT"), (148, "OUTPUT")):
        svg.append(label(82, y-23, title, 8))
        for x in (62, 101):
            svg.append(f'<circle cx="{x}" cy="{y+1}" r="16" fill="#221915"/>'
                       f'<circle cx="{x}" cy="{y}" r="14.5" fill="url(#brass)" stroke="#b59b7c"/>'
                       f'<circle cx="{x}" cy="{y}" r="10" fill="#110f0f" stroke="#715d49"/>'
                       f'<circle cx="{x}" cy="{y-1}" r="6.4" fill="#040405"/>')
    for x, y, title in ((237, 36, "INPUT"), (904, 36, "OUTPUT"), (370, 26, "COMP. RATIO"),
                        (772, 26, "ATTACK"), (1035, 26, "RELEASE"),
                        (370, 123, "VERNIER"), (772, 123, "VERNIER"), (1070, 130, "ON")):
        svg.append(label(x, y, title))
    for x in (237, 904):
        for i in range(21):
            angle = -132 + i * 13.2
            svg.append(stroke(x, 121, 56, 61, angle, width=.8))
            if i % 2 == 0:
                px, py = point(x, 121, 68, angle)
                svg.append(label(px, py+2.6, str(40-i*2), 7, spacing=.1))
    for x in (772, 1035):
        for i in range(8):
            svg.append(stroke(x, 67, 28, 32, -124+i*35.4))
    for i, ratio in enumerate(("2:1", "4:1", "8:1", "12:1")):
        px, py = point(370, 67, 34, -68+i*44)
        svg.append(label(px, py+2, ratio, 6.8, spacing=.1))
    for x in (173, 308):
        svg.append(f'<circle cx="{x}" cy="75" r="6.7" fill="#151314" stroke="#706458"/>')
    for x,y,r in KNOBS:
        svg.append(f'<circle cx="{x+1}" cy="{y+3}" r="{r+2}" fill="#110f0f" opacity=".8"/>'
                   f'<circle cx="{x}" cy="{y}" r="{r}" fill="#141213" stroke="#665b56" stroke-width="1.2"/>')
        for i in range(32):
            svg.append(stroke(x,y,r-5,r-1,i*11.25,"#484145" if i%2 else "#252222",1.1))
        svg.append(f'<circle cx="{x}" cy="{y}" r="{r-5}" fill="url(#knob)" stroke="#706365" stroke-opacity=".48"/>'
                   f'<path d="M{x-r*.62:.1f} {y-r*.38:.1f} q {r*.28:.1f} {-r*.54:.1f} {r*.84:.1f} {-r*.43:.1f}" fill="none" stroke="#b2a6a5" stroke-opacity=".37" stroke-width="{max(1,r/20):.1f}"/>')
    svg.append('''<rect x="466" y="29" width="207" height="116" rx="2" fill="#141115" stroke="#81766b"/>
<rect x="470" y="33" width="199" height="108" rx="1" fill="url(#bezel)"/>
<rect x="481" y="42" width="177" height="87" rx="1.5" fill="#1a1612" stroke="#09090a" stroke-width="4"/>
<rect x="483" y="44" width="173" height="83" rx="1" fill="url(#glass)"/>
<path d="M484 45H655V50Q568 45 484 53Z" fill="#fff4d0" opacity=".16"/>
<circle cx="995" cy="168" r="12.2" fill="#262023" stroke="#86725d"/>
<circle cx="995" cy="168" r="8.3" fill="url(#jewel)" stroke="#200b0b"/>
<path d="M1055 157L1069 149L1083 157V174L1069 183L1055 174Z" fill="url(#brass)" stroke="#281e17"/>
<circle cx="1069" cy="166" r="9" fill="#4c4032" stroke="#dac5a3"/>
<path d="M1064 169l2-16q3-4 7 0l1 14q-5 7-10 2Z" fill="url(#brass)" stroke="#322417"/>
''')
    svg.append('''<path d="M498 97Q565 28 641 94" fill="none" stroke="#302219" stroke-width=".8"/>
<path d="M616 74Q631 82 641 94" fill="none" stroke="#a6402d" stroke-width="2"/>
<path d="M507 100Q566 43 633 97" fill="none" stroke="#33261a" stroke-width=".6"/>
''')
    for i, mark in enumerate(("-20","-10","-7","-5","-3","-1","0","+1","+2","+3")):
        x = 499 + i*15.7
        y = 94 - sin(i/9*3.14159265)*29
        svg.append(f'<path d="M{x:.1f} {y:.1f}l0 4" stroke="#3b2919" stroke-width=".8"/>')
        svg.append(label(x,y-3,mark,7,"#3b2919" if i<7 else "#9b3525",.05))
    svg.append(label(570,109,"VU",10,"#493021",1.75))
    svg.append(label(570,193,"LIMITING AMPLIFIER  176",9.6,spacing=2.7))
    if preview:
        for (x,y,r),a in zip(KNOBS,(0,-24,-132,0,-132,0,77)):
            svg.append(stroke(x,y,r*.66,r*.91,a,"#f3e1ba",1.8))
        svg.append('<path d="M565 126L613 78" stroke="#3c2720" stroke-width="1.4"/>')
    svg.append('</g></svg>')
    return "\n".join(svg)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--preview", action="store_true")
    args=parser.parse_args()
    args.output.write_text(draw(args.preview))
