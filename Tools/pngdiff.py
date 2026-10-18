# Jämför två mappar med PNG-skärmbilder pixel för pixel (utan PIL, bara zlib): python Tools/pngdiff.py <före> <efter>
# Skriver de bilder som skiljer, med antal pixlar och rutan där de skiljer, och hur många som är identiska.
import sys, os, zlib, struct

def decode(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    pos = 8
    idat = b''
    w = h = ct = depth = interlace = None
    while pos < len(data):
        n, = struct.unpack('>I', data[pos:pos+4])
        kind = data[pos+4:pos+8]
        body = data[pos+8:pos+8+n]
        pos += 12 + n
        if kind == b'IHDR':
            w, h, depth, ct, _, _, interlace = struct.unpack('>IIBBBBB', body)
        elif kind == b'IDAT':
            idat += body
        elif kind == b'IEND':
            break
    assert depth == 8 and interlace == 0 and ct in (2, 6), (depth, ct, interlace)
    bpp = 3 if ct == 2 else 4
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows = []
    prev = bytearray(stride)
    i = 0
    for y in range(h):
        f = raw[i]; i += 1
        line = bytearray(raw[i:i+stride]); i += stride
        if f == 1:
            for x in range(bpp, stride):
                line[x] = (line[x] + line[x-bpp]) & 255
        elif f == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 255
        elif f == 3:
            for x in range(stride):
                a = line[x-bpp] if x >= bpp else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 255
        elif f == 4:
            for x in range(stride):
                a = line[x-bpp] if x >= bpp else 0
                b = prev[x]
                c = prev[x-bpp] if x >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p-a), abs(p-b), abs(p-c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        rows.append(line)
        prev = line
    return w, h, bpp, rows

def diff(a, b):
    wa, ha, ba, ra = decode(a)
    wb, hb, bb, rb = decode(b)
    if (wa, ha) != (wb, hb):
        return 'size %dx%d vs %dx%d' % (wa, ha, wb, hb)
    x0, y0, x1, y1, count = wa, ha, -1, -1, 0
    for y in range(ha):
        if ra[y] != rb[y]:
            for x in range(wa):
                if ra[y][x*ba:x*ba+3] != rb[y][x*bb:x*bb+3]:
                    count += 1
                    x0 = min(x0, x); x1 = max(x1, x); y0 = min(y0, y); y1 = max(y1, y)
    return None if count == 0 else 'DIFF %d px, box x %d-%d y %d-%d' % (count, x0, x1, y0, y1)

if __name__ == '__main__':
    before, after = sys.argv[1], sys.argv[2]
    names = sorted(f for f in os.listdir(before) if f.endswith('.png'))
    same = 0
    for n in names:
        p = os.path.join(after, n)
        if not os.path.exists(p):
            print('SAKNAS', n); continue
        r = diff(os.path.join(before, n), p)
        if r is None:
            same += 1
        else:
            print(n, r)
    print('identiska: %d av %d' % (same, len(names)))
