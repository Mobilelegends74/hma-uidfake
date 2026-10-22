#!/usr/bin/env python3
"""Read ELF module metadata for CI auditing; no loader or mutation operations."""
import struct


def inspect(blob):
    if blob[:6] != b'\x7fELF\x02\x01' or len(blob) < 64:
        raise ValueError('Expected little-endian ELF64')
    if struct.unpack_from('<HH', blob, 16) != (1, 183):
        raise ValueError('Expected ARM64 relocatable module')
    off = struct.unpack_from('<Q', blob, 40)[0]
    size, count, names_index = struct.unpack_from('<HHH', blob, 58)
    if size != 64 or not 0 < count <= 65535 or names_index >= count or off + size * count > len(blob):
        raise ValueError('Invalid section table')
    headers = [struct.unpack_from('<IIQQQQIIQQ', blob, off + i * size) for i in range(count)]

    def data(header):
        if header[4] + header[5] > len(blob):
            raise ValueError('Truncated section')
        return blob[header[4]:header[4] + header[5]]

    def string(buf, index):
        end = buf.find(b'\0', index)
        if index >= len(buf) or end < 0:
            raise ValueError('Invalid ELF string')
        return buf[index:end].decode()

    names = data(headers[names_index])
    sections = {}
    imports = set()
    for header in headers:
        name = string(names, header[0])
        if name in {'.modinfo', '.note.gnu.build-id', '__versions', '__version_ext_names', '__version_ext_crcs'}:
            if name in sections:
                raise ValueError('Duplicate metadata section')
            sections[name] = data(header)
        if header[1] == 2:
            if header[6] >= count or header[9] != 24 or header[5] % 24:
                raise ValueError('Invalid symbol table')
            symbols, strings = data(header), data(headers[header[6]])
            for pos in range(24, len(symbols), 24):
                name_index, info, _, section, _, _ = struct.unpack_from('<IBBHQQ', symbols, pos)
                if section == 0 and info >> 4 == 1:
                    imports.add(string(strings, name_index))
    info = {}
    for item in sections.get('.modinfo', b'').split(b'\0'):
        if b'=' in item:
            key, value = item.decode().split('=', 1)
            if key in {'name', 'vermagic', 'uidfake_kmi', 'uidfake_variant'}:
                if key in info:
                    raise ValueError('Duplicate identity')
                info[key] = value
    versions = sections.get('__versions', b'')
    records = []
    if versions:
        if len(versions) % 64:
            raise ValueError('Invalid version table')
        records = [(string(versions, i + 8), struct.unpack_from('<Q', versions, i)[0]) for i in range(0, len(versions), 64)]
    else:
        crc_data = sections.get('__version_ext_crcs', b'')
        if len(crc_data) % 4:
            raise ValueError('Invalid extended versions')
        pos = 0
        for i in range(0, len(crc_data), 4):
            name = string(sections['__version_ext_names'], pos)
            records.append((name, struct.unpack_from('<I', crc_data, i)[0]))
            pos += len(name) + 1
    crcs = dict(records)
    if len(crcs) != len(records) or not crcs.get('module_layout') or not imports.issubset(crcs):
        raise ValueError('Incomplete or duplicate symbol versions')
    return {'info': info, 'crcs': crcs, 'imports': imports,
            'build_id': sections.get('.note.gnu.build-id', b'')}


def compare(candidate, expected):
    missing = sorted(candidate['crcs'].keys() - expected.keys())
    mismatches = sorted(name for name, crc in candidate['crcs'].items() if name in expected and crc != expected[name])
    if missing or mismatches:
        raise ValueError(f'ABI mismatch: missing={missing}; mismatched={mismatches}')
