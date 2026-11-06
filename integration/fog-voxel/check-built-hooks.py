"""Verify the shipped DLL contains the corrected Syringe hook declaration."""
from pathlib import Path
import hashlib
import struct
import sys

data=Path(sys.argv[1]).read_bytes()
pe=struct.unpack_from('<I',data,0x3c)[0]
assert data[pe:pe+4]==b'PE\0\0'
count=struct.unpack_from('<H',data,pe+6)[0]
size=struct.unpack_from('<H',data,pe+20)[0]
assert struct.unpack_from('<H',data,pe+24)[0]==0x10b
base=struct.unpack_from('<I',data,pe+52)[0]
sections=[struct.unpack_from('<8sIIII',data,pe+24+size+i*40) for i in range(count)]
def offset(va):
    rva=va-base
    for _,vsize,address,rawsize,raw in sections:
        if address<=rva<address+rawsize:
            return raw+rva-address
    raise ValueError('invalid VA')
hook_section=next(s for s in sections if s[0].rstrip(b'\0')==b'.syhks00')
hooks=[]
for i in range(hook_section[4],hook_section[4]+hook_section[1],16):
    address,length,name=struct.unpack_from('<III',data,i)
    if not address:
        continue
    pos=offset(name)
    label=data[pos:data.index(0,pos)].decode('ascii')
    hooks.append((label,address,length))
expected={
    'BuildingClass_FreezeInFog_PrepareVoxel':(0x457aa0,6),
    'TechnoClass_BlitVoxel_RememberFogPalette':(0x70750e,5),
    'TechnoClass_BlitVoxel_RememberFogImage':(0x707736,5),
    'TechnoClass_RenderVoxel_RememberFogImage':(0x707235,5),
    'FoggedObjectClass_CTOR_FreezeVoxel':(0x4d10d3,9),
    'FoggedObjectClass_DrawFrozenVoxel':(0x4d2203,5),
}
for label,(address,length) in expected.items():
    matches=[h for h in hooks if h[0]==label]
    assert matches==[(label,address,length)], f'wrong or duplicate hook: {label}: {matches}'
    print(f'PASS {address:08X} size={length}: {label}')
assert not any(address==0x4d1310 for _,address,_ in hooks), 'obsolete constructor hook remains'
assert b'[FogVoxel v3]' in data, 'missing bounded v3 diagnostic marker'
print('PASS: obsolete constructor hook removed; v3 diagnostics present')
print('SHA256:',hashlib.sha256(data).hexdigest().upper())
