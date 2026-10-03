#!/usr/bin/env python3
"""Minimal DEX reader: lists classes, fields, methods (with code size)."""
import struct, sys

def uleb(d,o):
    r=0;s=0
    while True:
        b=d[o];o+=1
        r|=(b&0x7f)<<s;s+=7
        if not b&0x80: return r,o

class Dex:
    def __init__(s,path):
        d=s.d=open(path,'rb').read()
        (s.str_n,s.str_off,s.type_n,s.type_off,s.proto_n,s.proto_off,
         s.field_n,s.field_off,s.meth_n,s.meth_off,s.cls_n,s.cls_off)=struct.unpack_from('<12I',d,0x38)
        s.strings=[s.rs(i) for i in range(s.str_n)]
    def rs(s,i):
        off,=struct.unpack_from('<I',s.d,s.str_off+4*i)
        n,o=uleb(s.d,off)
        return s.d[o:o+n*3].split(b'\0')[0].decode('utf-8','replace')
    def type(s,i):
        x,=struct.unpack_from('<I',s.d,s.type_off+4*i); return s.strings[x]
    def proto(s,i):
        sh,ret,par=struct.unpack_from('<III',s.d,s.proto_off+12*i)
        ps=[]
        if par:
            n,=struct.unpack_from('<I',s.d,par)
            ps=[s.type(struct.unpack_from('<H',s.d,par+4+2*k)[0]) for k in range(n)]
        return '(%s)%s'%(''.join(ps),s.type(ret))
    def field(s,i):
        c,t,n=struct.unpack_from('<HHI',s.d,s.field_off+8*i)
        return s.type(c),s.strings[n],s.type(t)
    def meth(s,i):
        c,p,n=struct.unpack_from('<HHI',s.d,s.meth_off+8*i)
        return s.type(c),s.strings[n],s.proto(p)
    def classes(s):
        for i in range(s.cls_n):
            cidx,acc,sup,ifs,src,ann,cdata,sval=struct.unpack_from('<8I',s.d,s.cls_off+32*i)
            name=s.type(cidx); supn=s.type(sup) if sup!=0xffffffff else None
            fields=[];meths=[]
            if cdata:
                o=cdata
                sf,o=uleb(s.d,o);inf,o=uleb(s.d,o);dm,o=uleb(s.d,o);vm,o=uleb(s.d,o)
                for cnt,lst in ((sf,fields),(inf,fields)):
                    idx=0
                    for _ in range(cnt):
                        dlt,o=uleb(s.d,o);fl,o=uleb(s.d,o);idx+=dlt
                        lst.append((s.field(idx),fl))
                for cnt,virt in ((dm,False),(vm,True)):
                    idx=0
                    for _ in range(cnt):
                        dlt,o=uleb(s.d,o);fl,o=uleb(s.d,o);co,o=uleb(s.d,o);idx+=dlt
                        isz=struct.unpack_from('<I',s.d,co+12)[0] if co else 0
                        meths.append((s.meth(idx),fl,co,isz))
            yield name,supn,acc,fields,meths

if __name__=='__main__':
    dx=Dex(sys.argv[1])
    for name,sup,acc,fields,meths in dx.classes():
        tot=sum(m[3] for m in meths)
        print('%s extends %s  fields=%d methods=%d insns=%d'%(name,sup,len(fields),len(meths),tot))
