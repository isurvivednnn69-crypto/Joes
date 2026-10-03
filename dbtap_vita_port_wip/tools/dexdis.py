#!/usr/bin/env python3
"""Tiny Dalvik disassembler. usage: dexdis.py classes.dex ClassSubstr [MethodSubstr]"""
import struct, sys
from dexlist import Dex, uleb

names = {}
def _n(start, lst):
    for i, n in enumerate(lst.split()): names[start+i] = n
_n(0x00,"nop move move/from16 move/16 move-wide move-wide/from16 move-wide/16 move-object move-object/from16 move-object/16 move-result move-result-wide move-result-object move-exception return-void return return-wide return-object const/4 const/16 const const/high16 const-wide/16 const-wide/32 const-wide const-wide/high16 const-string const-string/jumbo const-class monitor-enter monitor-exit check-cast instance-of array-length new-instance new-array filled-new-array filled-new-array/range fill-array-data throw goto goto/16 goto/32 packed-switch sparse-switch cmpl-float cmpg-float cmpl-double cmpg-double cmp-long if-eq if-ne if-lt if-ge if-gt if-le if-eqz if-nez if-ltz if-gez if-gtz if-lez")
_n(0x44,"aget aget-wide aget-object aget-boolean aget-byte aget-char aget-short aput aput-wide aput-object aput-boolean aput-byte aput-char aput-short iget iget-wide iget-object iget-boolean iget-byte iget-char iget-short iput iput-wide iput-object iput-boolean iput-byte iput-char iput-short sget sget-wide sget-object sget-boolean sget-byte sget-char sget-short sput sput-wide sput-object sput-boolean sput-byte sput-char sput-short invoke-virtual invoke-super invoke-direct invoke-static invoke-interface")
_n(0x74,"invoke-virtual/range invoke-super/range invoke-direct/range invoke-static/range invoke-interface/range")
_n(0x7b,"neg-int not-int neg-long not-long neg-float neg-double int-to-long int-to-float int-to-double long-to-int long-to-float long-to-double float-to-int float-to-long float-to-double double-to-int double-to-long double-to-float int-to-byte int-to-char int-to-short")
_ar = "add sub mul div rem and or xor shl shr ushr".split()
ops = []
for t in ("int","long"): ops += ["%s-%s"%(a,t) for a in _ar]
for t in ("float","double"): ops += ["%s-%s"%(a,t) for a in _ar[:5]]
for i,n in enumerate(ops): names[0x90+i] = n; names[0xb0+i] = n+"/2addr"
_n(0xd0,"add-int/lit16 rsub-int mul-int/lit16 div-int/lit16 rem-int/lit16 and-int/lit16 or-int/lit16 xor-int/lit16 add-int/lit8 rsub-int/lit8 mul-int/lit8 div-int/lit8 rem-int/lit8 and-int/lit8 or-int/lit8 xor-int/lit8 shl-int/lit8 shr-int/lit8 ushr-int/lit8")

def fmt_of(op):
    if op in (0x00,0x0e): return '10x'
    if op in (0x01,0x04,0x07,0x21) or 0x7b<=op<=0x8f or 0xb0<=op<=0xcf: return '12x'
    if op in (0x02,0x05,0x08): return '22x'
    if op in (0x03,0x06,0x09): return '32x'
    if op in (0x0a,0x0b,0x0c,0x0d,0x0f,0x10,0x11,0x1d,0x1e,0x27): return '11x'
    if op==0x12: return '11n'
    if op in (0x13,0x16): return '21s'
    if op in (0x14,0x17): return '31i'
    if op in (0x15,0x19): return '21h'
    if op==0x18: return '51l'
    if op in (0x1a,0x1c,0x1f,0x22,0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d): return '21c'
    if op==0x1b: return '31c'
    if op in (0x20,0x23) or 0x52<=op<=0x5f: return '22c'
    if op in (0x24,) or 0x6e<=op<=0x72: return '35c'
    if op==0x25 or 0x74<=op<=0x78: return '3rc'
    if op in (0x26,0x2b,0x2c): return '31t'
    if op==0x28: return '10t'
    if op==0x29: return '20t'
    if op==0x2a: return '30t'
    if 0x2d<=op<=0x31 or 0x44<=op<=0x51 or 0x90<=op<=0xaf: return '23x'
    if 0x32<=op<=0x37: return '22t'
    if 0x38<=op<=0x3d: return '21t'
    if 0xd0<=op<=0xd7: return '22s'
    if 0xd8<=op<=0xe2: return '22b'
    return None
SIZE = {'10x':1,'12x':1,'11n':1,'11x':1,'10t':1,'20t':2,'22x':2,'21t':2,'21s':2,'21h':2,'21c':2,'23x':2,'22b':2,'22t':2,'22s':2,'22c':2,'30t':3,'32x':3,'31i':3,'31t':3,'31c':3,'35c':3,'3rc':3,'51l':5}
def s16(x): return x-0x10000 if x&0x8000 else x
def s8(x): return x-0x100 if x&0x80 else x
def s32(x): return x-(1<<32) if x&(1<<31) else x

def disasm(dx, code_off):
    d = dx.d
    regs, ins, outs, tries, dbg, n = struct.unpack_from('<HHHHII', d, code_off)
    base = code_off + 16
    u = list(struct.unpack_from('<%dH' % n, d, base))
    out = ['  .registers %d  .ins %d  .outs %d' % (regs, ins, outs)]
    pc = 0
    def ref(op, idx):
        if op in (0x1a,0x1b): return repr(dx.strings[idx])
        if op in (0x1c,0x1f,0x20,0x22,0x23,0x24,0x25): return dx.type(idx)
        if 0x52<=op<=0x6d: c,nm,t = dx.field(idx); return '%s.%s:%s' % (c,nm,t)
        c,nm,p = dx.meth(idx); return '%s.%s%s' % (c,nm,p)
    while pc < n:
        w = u[pc]; op = w & 0xff
        if w in (0x0100,0x0200,0x0300) and op==0:
            if w==0x0100: sz = 4 + u[pc+1]*2
            elif w==0x0200: sz = 2 + u[pc+1]*4
            else: sz = 4 + (u[pc+1]*(u[pc+2]|u[pc+3]<<16)+1)//2
            out.append('%04x: .payload(%04x) size=%d' % (pc, w, sz)); pc += sz; continue
        f = fmt_of(op)
        if f is None: out.append('%04x: ?? %04x' % (pc, w)); pc += 1; continue
        nm = names.get(op, '?%02x' % op)
        a = w >> 8
        t = ''
        if f=='10x': t=''
        elif f=='12x': t='v%d, v%d' % (a&15, a>>4)
        elif f=='11n': t='v%d, #%d' % (a&15, (a>>4) - (16 if a>>4 > 7 else 0))
        elif f=='11x': t='v%d' % a
        elif f=='10t': t='%04x' % (pc+s8(a))
        elif f=='20t': t='%04x' % (pc+s16(u[pc+1]))
        elif f=='22x': t='v%d, v%d' % (a, u[pc+1])
        elif f=='21t': t='v%d, %04x' % (a, pc+s16(u[pc+1]))
        elif f=='21s': t='v%d, #%d' % (a, s16(u[pc+1]))
        elif f=='21h': t='v%d, #0x%x' % (a, u[pc+1]<<16 if op==0x15 else u[pc+1]<<48)
        elif f=='21c': t='v%d, %s' % (a, ref(op, u[pc+1]))
        elif f=='23x': t='v%d, v%d, v%d' % (a, u[pc+1]&0xff, u[pc+1]>>8)
        elif f=='22b': t='v%d, v%d, #%d' % (a, u[pc+1]&0xff, s8(u[pc+1]>>8))
        elif f=='22t': t='v%d, v%d, %04x' % (a&15, a>>4, pc+s16(u[pc+1]))
        elif f=='22s': t='v%d, v%d, #%d' % (a&15, a>>4, s16(u[pc+1]))
        elif f=='22c': t='v%d, v%d, %s' % (a&15, a>>4, ref(op, u[pc+1]))
        elif f=='30t': t='%04x' % (pc+s32(u[pc+1]|u[pc+2]<<16))
        elif f=='32x': t='v%d, v%d' % (u[pc+1], u[pc+2])
        elif f=='31i': t='v%d, #%d' % (a, s32(u[pc+1]|u[pc+2]<<16))
        elif f=='31t': t='v%d, %04x' % (a, pc+s32(u[pc+1]|u[pc+2]<<16))
        elif f=='31c': t='v%d, %s' % (a, ref(op, u[pc+1]|u[pc+2]<<16))
        elif f=='35c':
            cnt = a>>4; g = a&15; w2 = u[pc+2]
            rs = [w2&15,(w2>>4)&15,(w2>>8)&15,(w2>>12)&15,g][:cnt]
            t='{%s}, %s' % (', '.join('v%d'%r for r in rs), ref(op,u[pc+1]))
        elif f=='3rc':
            t='{v%d..v%d}, %s' % (u[pc+2], u[pc+2]+a-1, ref(op,u[pc+1]))
        elif f=='51l':
            v = u[pc+1]|u[pc+2]<<16|u[pc+3]<<32|u[pc+4]<<48
            t='v%d, #%d' % (a, v)
        out.append('%04x: %s %s' % (pc, nm, t))
        pc += SIZE[f]
    return out

if __name__=='__main__':
    dx = Dex(sys.argv[1]); cs = sys.argv[2]; ms = sys.argv[3] if len(sys.argv)>3 else None
    for name,sup,acc,fields,meths in dx.classes():
        if cs not in name: continue
        print('\n### class %s extends %s' % (name, sup))
        for (c,n,t),fl in fields: print('  field %s:%s flags=%x' % (n,t,fl))
        for (c,n,p),fl,co,isz in meths:
            if ms and ms not in n: continue
            print('\n  method %s%s flags=%x insns=%d' % (n,p,fl,isz))
            if co: print('\n'.join(disasm(dx,co)))
