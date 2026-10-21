"""Supplement a Ghidra MCP inventory with PE32 IAT imports and verified jump thunks.

This reads the declared original binary; it never executes or modifies it.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

def supplement(data, payload):
    if hashlib.sha256(payload).hexdigest() != data['binary_sha256']:
        raise ValueError('Executable SHA256 differs from Ghidra inventory identity')
    u16 = lambda offset: struct.unpack_from('<H', payload, offset)[0]
    u32 = lambda offset: struct.unpack_from('<I', payload, offset)[0]
    pe = u32(0x3c)
    if payload[:2] != b'MZ' or payload[pe:pe+4] != b'PE\0\0':
        raise ValueError('Not a PE executable')
    optional = pe + 24
    if u16(optional) != 0x10b:
        raise ValueError('Expected PE32')
    image_base = u32(optional + 28)
    sections = optional + u16(pe + 20)
    regions = []
    for index in range(u16(pe + 6)):
        entry = sections + index * 40
        regions.append((u32(entry + 12), u32(entry + 16), u32(entry + 20)))
    def file_offset(rva):
        for start, size, raw in regions:
            if start <= rva < start + size:
                return raw + rva - start
        raise ValueError(f'Unmapped RVA {rva:x}')
    def string(rva):
        offset = file_offset(rva)
        return payload[offset:payload.index(b'\0', offset)].decode('ascii')
    functions = [row for row in data['functions'] if not row.get('is_external')]
    for row in functions:
        start = int(row['entry_point'], 16)
        end = int(row['end_address'], 16)
        if end - start == 4 and row['name'].startswith('thunk_'):
            offset = file_offset(start - image_base)
            if payload[offset] == 0xe9:
                row['is_thunk'] = True
                row['thunk_target'] = f'{start + 5 + struct.unpack_from("<i", payload, offset + 1)[0]:08x}'
    descriptor = file_offset(u32(optional + 104))
    imports = []
    while any(payload[descriptor:descriptor + 20]):
        lookup, _, _, library_rva, iat = struct.unpack_from('<IIIII', payload, descriptor)
        library = string(library_rva)
        lookup = file_offset(lookup or iat)
        index = 0
        while value := u32(lookup + index * 4):
            name = f'ordinal_{value & 0xffff}' if value & 0x80000000 else string(value + 2)
            address = f'{image_base + iat + index * 4:08x}'
            imports.append(dict(entry_point=address, end_address=address, name=name, namespace='Global', is_external=True, library=library, address_kind='PE IAT slot'))
            index += 1
        descriptor += 20
    data['functions'] = functions + imports
    data['internal_function_count'] = len(functions)
    data['external_function_count'] = len(imports)
    data['function_count'] = len(data['functions'])
    data['source'] += '; PE import table read from hash-matched executable; external IDs use IAT virtual addresses; thunk flags require a named five-byte E9 jump'
    return data

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inventory', type=Path)
    parser.add_argument('executable', type=Path)
    args = parser.parse_args()
    data = supplement(json.loads(args.inventory.read_text()), args.executable.read_bytes())
    args.inventory.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    print({key: data[key] for key in ('internal_function_count', 'external_function_count', 'function_count', 'class_count')})
