"""Check actual YR 1.001 constructor bytes; do not execute the game."""
from pathlib import Path
import struct
import sys

image=Path(sys.argv[1]).read_bytes()
pe=struct.unpack_from('<I',image,0x3c)[0]
assert image[pe:pe+4]==b'PE\0\0'
count=struct.unpack_from('<H',image,pe+6)[0]
size=struct.unpack_from('<H',image,pe+20)[0]
assert struct.unpack_from('<H',image,pe+24)[0]==0x10b, 'expected x86 PE'
base=struct.unpack_from('<I',image,pe+52)[0]
sections=[struct.unpack_from('<8sIIII',image,pe+24+size+40*i) for i in range(count)]
def read(va,n):
    rva=va-base
    for _,vsize,address,rawsize,raw in sections:
        if address<=rva and rva+n<=address+rawsize:
            return image[raw+rva-address:raw+rva-address+n]
    raise ValueError('VA outside file sections')

cases=[
    (0x457aa0,bytes.fromhex('83 EC 0C 53 55 56'),'common freeze entry: sub esp,0Ch; push ebx/ebp/esi'),
    (0x440bfb,bytes.fromhex('6A 00 6A 00 6A 00 8B CE'),'Building AI freeze uses Visible=false'),
    (0x4d0efe,bytes.fromhex('8B 5C 24 54'),'original building argument loaded into EBX'),
    (0x4d10d3,bytes.fromhex('8B CB C6 83 E7 06 00 00 01'),'new nine-byte hook: EBX building, before animation loop'),
    (0x4d119a,bytes.fromhex('89 54 24 54'),'animation type overwrites original argument slot'),
    (0x4d1310,bytes.fromhex('6A 00 8D 77 40'),'old hook point occurs after argument reuse'),
]
for va,expected,description in cases:
    actual=read(va,len(expected))
    assert actual==expected, f'{va:08X}: unsupported native bytes {actual.hex()}'
    print(f'PASS {va:08X}: {description}')
