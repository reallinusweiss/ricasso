"""Regenerates ../rsrc_windows_amd64.syso (icon, manifest, version info) for Ricasso.exe.

Pure Python, no windres/binutils needed. Only needed after changing ricasso.ico,
Ricasso.exe.manifest or the version strings below. The output is a COFF object
with a single .rsrc section, the same layout windres produces; the Go linker
picks it up automatically.

    python tools/windows/launcher/winres/build_resources.py
"""
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

VERSION = (1, 3, 0, 0)
STRINGS = [
    ('CompanyName', 'Ricasso (unofficial)'),
    ('FileDescription', 'Ricasso – compatibility layer for Swordigo'),
    ('FileVersion', '1.3.0'),
    ('InternalName', 'Ricasso'),
    ('LegalCopyright', 'Starter: GPL-3.0-or-later. Swordigo © Touch Foo – game files not included.'),
    ('OriginalFilename', 'Ricasso.exe'),
    ('ProductName', 'Ricasso (unofficial)'),
    ('ProductVersion', '1.3.0'),
]
LANG_ID, CODEPAGE = 0x0409, 1200  # English (United States), Unicode
RESOURCE_LANG = 1033

RT_ICON, RT_GROUP_ICON, RT_VERSION, RT_MANIFEST = 3, 14, 16, 24


def pad4(b):
    return b + b'\0' * (-len(b) % 4)


def utf16z(s):
    return s.encode('utf-16-le') + b'\0\0'


def version_node(key, value=b'', value_len=0, binary=False, children=()):
    """One VS_VERSIONINFO-style block: wLength, wValueLength, wType, szKey, Value, Children."""
    body = pad4(struct.pack('<HHH', 0, value_len, 0 if binary else 1) + utf16z(key)) + value
    if children:
        body = pad4(body)
        for i, child in enumerate(children):
            body += child if i == len(children) - 1 else pad4(child)
    return struct.pack('<H', len(body)) + body[2:]


def version_info(version, strings, lang, codepage):
    ms = version[0] << 16 | version[1]
    ls = version[2] << 16 | version[3]
    fixed = struct.pack('<13I', 0xFEEF04BD, 0x00010000, ms, ls, ms, ls, 0, 0, 0x40004, 1, 0, 0, 0)
    table = [version_node(k, utf16z(v), len(v) + 1) for k, v in strings]
    string_info = version_node('StringFileInfo', children=[version_node(f'{lang:04X}{codepage:04X}', children=table)])
    var_info = version_node('VarFileInfo', children=[
        version_node('Translation', struct.pack('<HH', lang, codepage), 4, binary=True)])
    return version_node('VS_VERSION_INFO', fixed, len(fixed), binary=True, children=[string_info, var_info])


def icon_resources(ico):
    reserved, kind, count = struct.unpack_from('<HHH', ico, 0)
    if reserved != 0 or kind != 1 or count == 0:
        raise SystemExit('ricasso.ico is not a valid icon file')
    images, group = [], struct.pack('<HHH', 0, 1, count)
    for i in range(count):
        w, h, colors, res, planes, bits, size, offset = struct.unpack_from('<BBBBHHII', ico, 6 + 16 * i)
        images.append(ico[offset:offset + size])
        # PNG-compressed entries often store 0 planes; resource compilers write 1.
        group += struct.pack('<BBBBHHIH', w, h, colors, res, planes or 1, bits, size, i + 1)
    return images, group


def coff_resources(resources):
    """resources: {type_id: {name_id: bytes}} -> COFF object with one .rsrc section."""
    types = sorted(resources)
    # Directory tables first (root, then per type its name table followed by one
    # language table per name), then the data entries, then the 8-byte aligned data.
    dir_size = lambda n: 16 + 8 * n
    offset = dir_size(len(types))
    type_dirs, lang_dirs = {}, {}
    for t in types:
        type_dirs[t] = offset
        offset += dir_size(len(resources[t]))
        for n in sorted(resources[t]):
            lang_dirs[t, n] = offset
            offset += dir_size(1)
    entries = {}
    for t in types:
        for n in sorted(resources[t]):
            entries[t, n] = offset
            offset += 16
    data_offsets = {}
    for t in types:
        for n in sorted(resources[t]):
            offset += -offset % 8
            data_offsets[t, n] = offset
            offset += len(resources[t][n])

    section = bytearray(offset + (-offset % 4))
    directory = lambda count: struct.pack('<IIHHHH', 0, 0, 0, 0, 0, count)
    section[0:16] = directory(len(types))
    for i, t in enumerate(types):
        struct.pack_into('<II', section, 16 + 8 * i, t, 0x80000000 | type_dirs[t])
        names = sorted(resources[t])
        section[type_dirs[t]:type_dirs[t] + 16] = directory(len(names))
        for j, n in enumerate(names):
            struct.pack_into('<II', section, type_dirs[t] + 16 + 8 * j, n, 0x80000000 | lang_dirs[t, n])
            section[lang_dirs[t, n]:lang_dirs[t, n] + 16] = directory(1)
            struct.pack_into('<II', section, lang_dirs[t, n] + 16, RESOURCE_LANG, entries[t, n])
            data = resources[t][n]
            struct.pack_into('<IIII', section, entries[t, n], data_offsets[t, n], len(data), 0, 0)
            section[data_offsets[t, n]:data_offsets[t, n] + len(data)] = data

    # Each data entry's OffsetToData becomes an image-relative address (ADDR32NB)
    # relative to symbol 0, the .rsrc section itself.
    relocs = b''.join(struct.pack('<IIH', entries[k], 0, 3) for k in sorted(entries, key=entries.get))
    raw_ptr = 20 + 40
    reloc_ptr = raw_ptr + len(section)
    sym_ptr = reloc_ptr + len(relocs)
    header = struct.pack('<HHIIIHH', 0x8664, 1, 0, sym_ptr, 1, 0, 0x4)
    section_header = struct.pack('<8sIIIIIIHHI', b'.rsrc', 0, 0, len(section), raw_ptr, reloc_ptr, 0,
                                 len(entries), 0, 0x40300040)
    symbol = struct.pack('<8sIhHBB', b'.rsrc', 0, 1, 0, 3, 0)
    return header + section_header + bytes(section) + relocs + symbol + struct.pack('<I', 4)


def main():
    images, group = icon_resources((HERE / 'ricasso.ico').read_bytes())
    resources = {
        RT_ICON: {i + 1: img for i, img in enumerate(images)},
        RT_GROUP_ICON: {1: group},
        RT_VERSION: {1: version_info(VERSION, STRINGS, LANG_ID, CODEPAGE)},
        RT_MANIFEST: {1: (HERE / 'Ricasso.exe.manifest').read_bytes()},
    }
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE.parent / 'rsrc_windows_amd64.syso'
    out.write_bytes(coff_resources(resources))
    print('Resources:', out)


if __name__ == '__main__':
    main()
