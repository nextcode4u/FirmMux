import sys
import os

def crc32_header(buf):
    crc = 0xFFFFFFFF
    for b in buf:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if (crc & 1) else 0)
    return crc & 0xFFFFFFFF

def main():
    if len(sys.argv) != 2:
        print('usage: python3 tools/nds_info.py <path-to-rom>')
        return 2
    path = sys.argv[1]
    if not os.path.isfile(path):
        print('file not found:', path)
        return 1
    with open(path, 'rb') as f:
        header = f.read(512)
    if len(header) != 512:
        print('header read failed')
        return 1
    gc = header[12:16]
    gc_str = gc.decode('ascii', errors='replace')
    gc_hex = ''.join(f'{b:02X}' for b in gc)
    crc = crc32_header(header)
    print('gamecode:', gc_str)
    print('gamecode_hex:', gc_hex)
    print('crc32_header:', f'{crc:08X}')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
