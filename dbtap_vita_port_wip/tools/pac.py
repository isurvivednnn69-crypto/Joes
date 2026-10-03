#!/usr/bin/env python3
"""Namco Bandai 'Dragon Ball Tap Battle' .pac container reader/extractor.

Layout (little endian), reverse-engineered from the APK assets:
  u16  count
  count * { u32 offset; u32 size; char type[8] (NUL padded) }   # 16 bytes each
  data blob; offsets are relative to (2 + 16*count)
"""
import struct, sys, os

def read_pac(path_or_bytes):
    d = open(path_or_bytes,'rb').read() if isinstance(path_or_bytes,str) else path_or_bytes
    n, = struct.unpack_from('<H', d, 0)
    base = 2 + 16*n
    ents = []
    for i in range(n):
        off, size, t = struct.unpack_from('<II8s', d, 2+16*i)
        t = t.split(b'\0')[0].decode('ascii','replace')
        assert base+off+size <= len(d), (path_or_bytes, i, off, size, len(d))
        ents.append((t, d[base+off: base+off+size]))
    return ents

def extract(path, outdir):
    os.makedirs(outdir, exist_ok=True)
    for i,(t,b) in enumerate(read_pac(path)):
        open(os.path.join(outdir, '%03d.%s'%(i,t)),'wb').write(b)

if __name__=='__main__':
    src, dst = sys.argv[1], sys.argv[2]
    if os.path.isdir(src):
        for f in sorted(os.listdir(src)):
            if f.endswith('.pac'):
                extract(os.path.join(src,f), os.path.join(dst, f[:-4]))
    else:
        extract(src, dst)
