"""Compare portable fallback BGM decoding against the independent Windows ACM codec."""
import ctypes as c
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from local_config import ROOT, game_directory


def check(result):
    if result:
        raise RuntimeError('Windows ACM conversion failed: ' + str(result))


def reference_decode(format_bytes, encoded):
    # msacm.h uses byte packing and 15 reserved DWORDs on Win64, 10 on Win32.
    class Header(c.Structure):
        _pack_ = 1
        _fields_ = [('size', c.c_uint32), ('status', c.c_uint32), ('user', c.c_size_t),
                    ('source', c.c_void_p), ('source_size', c.c_uint32),
                    ('source_used', c.c_uint32), ('source_user', c.c_size_t),
                    ('destination', c.c_void_p), ('destination_size', c.c_uint32),
                    ('destination_used', c.c_uint32), ('destination_user', c.c_size_t),
                    ('reserved', c.c_uint32 * (15 if c.sizeof(c.c_void_p) == 8 else 10))]
    channels, rate = struct.unpack_from('<HI', format_bytes, 2)
    destination_format = c.create_string_buffer(struct.pack('<HHIIHHH', 1, channels, rate,
                                                rate * channels * 2, channels * 2, 16, 0))
    source_format = c.create_string_buffer(format_bytes)
    acm = c.WinDLL('msacm32')
    acm.acmStreamOpen.argtypes = [c.POINTER(c.c_void_p), c.c_void_p, c.c_void_p, c.c_void_p,
                                  c.c_void_p, c.c_size_t, c.c_size_t, c.c_uint32]
    acm.acmStreamSize.argtypes = [c.c_void_p, c.c_uint32, c.POINTER(c.c_uint32), c.c_uint32]
    for name in ('acmStreamPrepareHeader', 'acmStreamConvert', 'acmStreamUnprepareHeader'):
        getattr(acm, name).argtypes = [c.c_void_p, c.POINTER(Header), c.c_uint32]
    acm.acmStreamClose.argtypes = [c.c_void_p, c.c_uint32]
    handle = c.c_void_p()
    check(acm.acmStreamOpen(c.byref(handle), None, source_format, destination_format, None, 0, 0, 0))
    try:
        size = c.c_uint32()
        check(acm.acmStreamSize(handle, len(encoded), c.byref(size), 0))
        source = c.create_string_buffer(encoded)
        destination = c.create_string_buffer(size.value)
        header = Header()
        header.size = c.sizeof(header)
        header.source = c.addressof(source)
        header.source_size = len(encoded)
        header.destination = c.addressof(destination)
        header.destination_size = size.value
        check(acm.acmStreamPrepareHeader(handle, c.byref(header), 0))
        try:
            check(acm.acmStreamConvert(handle, c.byref(header), 0x10 | 0x20))
            if header.source_used != len(encoded):
                raise RuntimeError('ACM did not consume the whole original sample')
            return destination.raw[:header.destination_used]
        finally:
            check(acm.acmStreamUnprepareHeader(handle, c.byref(header), 0))
    finally:
        check(acm.acmStreamClose(handle, 0))


def main():
    original = (game_directory() / 'Sfx/RaceMusic.wav').read_bytes()
    chunks = {}
    offset = 12
    while offset + 8 <= len(original):
        kind, count = struct.unpack_from('<4sI', original, offset)
        chunks[kind] = original[offset + 8:offset + 8 + count]
        offset += 8 + count + (count & 1)
    output = ROOT / 'build/race_music.pcm'
    subprocess.run([str(ROOT / 'build/core/roadwars_race_music_test.exe'),
                    str(game_directory()), str(output)], check=True)
    portable = output.read_bytes()
    reference = reference_decode(chunks[b'fmt '], chunks[b'data'])
    frames = struct.unpack_from('<I', chunks[b'fact'])[0]
    channels = struct.unpack_from('<H', chunks[b'fmt '], 2)[0]
    reference = reference[:frames * channels * 2]
    if reference != portable:
        first = next((i for i, (a, b) in enumerate(zip(reference, portable)) if a != b), None)
        raise RuntimeError(f'Portable/ACM PCM mismatch at byte {first}; lengths {len(portable)}/{len(reference)}')
    report = dict(reference='Windows ACM Microsoft ADPCM decoder',
                  source_sha256=hashlib.sha256(original).hexdigest(),
                  pcm_sha256=hashlib.sha256(portable).hexdigest(),
                  decoded_frames=frames, compared_pcm_bytes=len(portable), exact_match=True)
    (ROOT / 'build/race_music_validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
