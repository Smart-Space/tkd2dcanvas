import re
import pathlib

src = pathlib.Path('third_party/tcltk-src/tk/bitmaps')
names = ['error', 'gray12', 'gray25', 'gray50', 'gray75', 'hourglass',
         'info', 'questhead', 'question', 'warning']

def bits_of(name):
    text = (src / f'{name}.xbm').read_text()
    m = re.search(r'#define\s+\w+_width\s+(\d+).*?#define\s+\w+_height\s+(\d+).*?\{\s*(.*?)\}',
                  text, re.S)
    w, h = int(m.group(1)), int(m.group(2))
    body = m.group(3)
    bytes_ = [int(tok, 0) for tok in body.replace('\n', ' ').split(',') if tok.strip()]
    assert len(bytes_) == ((w + 7) // 8) * h, (name, len(bytes_))
    return w, h, bytes_

out = []
out.append('// TkBuiltinBitmaps.cpp — verbatim copies of the Tk 8.6 builtin XBM bitmaps')
out.append('// (third_party/tcltk-src/tk/bitmaps/*.xbm, Tk license — see COPYING).')
out.append('#include "TkBuiltinBitmaps.h"')
out.append('')
out.append('namespace tkd2dcanvas {')
out.append('')
out.append('namespace {')

for name in names:
    w, h, bytes_ = bits_of(name)
    cap = name.capitalize()
    out.append(f'constexpr std::uint8_t k{cap}Bits[] = {{')
    line = '    '
    for b in bytes_:
        tok = f'0x{b:02x}, '
        if len(line) + len(tok) > 78:
            out.append(line.rstrip())
            line = '    '
        line += tok
    if line.strip():
        out.append(line.rstrip())
    out.append('};')
    out.append('')

out.append('const BuiltinBitmap kBuiltinBitmaps[] = {')
for name in names:
    w, h, _ = bits_of(name)
    out.append(f'    {{"{name}", {w}, {h}, k{name.capitalize()}Bits}},')
out.append('};')
out.append('')
out.append('}  // namespace')
out.append('')
out.append('const BuiltinBitmap* FindBuiltinBitmap(const std::string& name) {')
out.append('    for (const auto& entry : kBuiltinBitmaps) {')
out.append('        if (name == entry.name) return &entry;')
out.append('    }')
out.append('    return nullptr;')
out.append('}')
out.append('')
out.append('}  // namespace tkd2dcanvas')

pathlib.Path('native/tk_glue/TkBuiltinBitmaps.cpp').write_text('\n'.join(out) + '\n')
print('written')
