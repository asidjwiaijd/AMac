#!/usr/bin/env python3
"""Build native 68k Finder launchers and their original monochrome icons.

No cross compiler is required: the application consists of a normal CODE 0
jump table, CODE 1 with ExitToShell, and a strictly checked birthday marker.
The emulator's existing resource-load hook opens the shared birthday UI.
"""
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
APPS = [("For You", "bGft", 1), ("Birthday Settings", "bSet", 2),
        ("Pocket Terminal", "bTrm", 3), ("About This Gift", "bAbt", 4)]

def crc16(data):
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xffff
    return crc

def macbinary(name, kind, creator, data=b"", resource=b"", x=80, y=90):
    header = bytearray(128)
    encoded = name.encode("mac_roman")
    header[1] = len(encoded)
    header[2:2+len(encoded)] = encoded
    header[65:69], header[69:73] = kind, creator
    flags = 0x2500 if kind == b"APPL" else 0x0100 # bundle + custom icon + positioned
    header[73], header[101] = flags >> 8, flags & 255
    struct.pack_into(">hh", header, 75, y, x)
    struct.pack_into(">II", header, 83, len(data), len(resource))
    header[122] = header[123] = 129
    struct.pack_into(">H", header, 124, crc16(header[:124]))
    pad = lambda value: value + bytes(-len(value) % 128)
    return bytes(header) + pad(data) + pad(resource)

def resources(items):
    data = bytearray()
    grouped = {}
    for kind, rid, payload in items:
        offset = len(data)
        data += struct.pack(">I", len(payload)) + payload
        grouped.setdefault(kind, []).append((rid, offset))
    types = bytearray(struct.pack(">H", len(grouped)-1))
    refs = bytearray()
    for kind, values in grouped.items():
        types += kind + struct.pack(">HH", len(values)-1, 2+len(grouped)*8+len(refs))
        for rid, offset in values:
            refs += struct.pack(">hH", rid, 0xffff) + bytes([0]) + offset.to_bytes(3, "big") + bytes(4)
    typelist = bytes(types + refs)
    map_size = 28 + len(typelist)
    header = struct.pack(">IIII", 256, 256+len(data), len(data), map_size)
    resource_map = header + bytes(8) + struct.pack(">HH", 28, map_size) + typelist
    assert len(resource_map) == map_size
    return header + bytes(240) + data + resource_map

def icon(entry):
    pixels = [[False]*32 for _ in range(32)]
    mask = [[False]*32 for _ in range(32)]
    def box(x, y, w, h, fill=False):
        for yy in range(y,y+h):
            for xx in range(x,x+w):
                mask[yy][xx] = True
                pixels[yy][xx] = fill or yy in (y,y+h-1) or xx in (x,x+w-1)
    if entry == 1:
        box(4,12,24,18); box(3,10,26,5); box(14,10,4,20,True)
        box(7,4,8,7); box(17,4,8,7); box(13,7,6,4,True)
    elif entry == 2:
        box(3,4,26,25)
        for y,x in ((10,20),(17,10),(24,18)):
            box(6,y,20,1,True); box(x,y-2,4,5,True)
    elif entry == 3:
        box(2,5,28,23); box(3,6,26,4,True)
        for x,y in ((7,14),(8,15),(9,16),(8,17),(7,18)):
            box(x,y,2,1,True)
        box(14,20,8,2,True)
    else:
        box(6,3,20,24); box(9,6,14,13)
        box(12,11,1,2,True); box(20,11,1,2,True); box(13,16,7,1,True)
        box(4,27,24,3,True)
    def packed(grid):
        return b"".join(sum((1 << (7-bit)) for bit in range(8) if grid[y][x+bit]).to_bytes(1,"big")
                        for y in range(32) for x in range(0,32,8))
    return packed(pixels) + packed(mask)

def application(name, creator, entry):
    creator = creator.encode("ascii")
    # Near-model jump offsets start AFTER the four-byte segment header.
    code0 = struct.pack(">IIIIHHHH",40,256,8,32,0,0x3f3c,1,0xa9f0)
    # Explicit GetResource('bDAY',128) invokes vCheckLoad even when the Segment
    # Manager takes a CODE-loading fast path. The Pascal result slot is 4 bytes.
    code1 = struct.pack(">11H",0,1,0x598f,0x2f3c,0x6244,0x4159,0x3f3c,128,0xa9a0,0x205f,0xa9f4)
    bundle = creator + struct.pack(">HH",0,1)
    bundle += b"ICN#" + struct.pack(">HHH",0,0,128)
    bundle += b"FREF" + struct.pack(">HHH",0,0,128)
    items = [(b"CODE",0,code0), (b"CODE",1,code1), (b"bDAY",128,b"aMacBday\x01"+bytes([entry])),
             (b"SIZE",-1,struct.pack(">HII",0x5880,131072,65536)),
             (b"ICN#",128,icon(entry)), (b"BNDL",128,bundle),
             (b"ICN#",-16455,icon(entry)),
             (b"FREF",128,b"APPL\x00\x00\x00"),
             (creator,0,b"\x10Birthday Edition")]
    return macbinary(name,b"APPL",creator,resource=resources(items))

def main():
    out = ROOT / "guest/birthday/dist"
    out.mkdir(parents=True,exist_ok=True)
    manifest = []
    for name, creator, entry in APPS:
        target = out / f"{name}.bin"
        target.write_bytes(application(name,creator,entry))
        manifest.append({"name":name,"file":target.name,"entry":entry})
    text = ("A LITTLE SOMETHING FOR YOU\r\r"
            "Double-click For You on the desktop to begin.\r"
            "After making a wish, follow the clue on the card.\r\r"
            "A few places are a little different in this computer:\r"
            "  Apple menu > Control Panels > Birthday Settings\r"
            "  Apple menu > Pocket Terminal (try help, then ls)\r"
            "  Apple menu > About This Gift\r\r"
            "There are five surprises. Some take a little curiosity.\r"
            "Look for the five small squares in each gift window.\r"
            "Escape or the window close box returns to the Mac.\r\r"
            "Made with care, just for you.\r")
    (out / "Start Here.bin").write_bytes(macbinary("Start Here",b"TEXT",b"ttxt",text.encode("mac_roman"),x=80,y=170))
    (out / "manifest.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    print(f"Generated {len(APPS)} native Finder applications and a Start Here note")

def read_macbinary(path):
    raw=Path(path).read_bytes()
    data_len,resource_len=struct.unpack_from(">II",raw,83)
    start=128+(data_len+127)//128*128
    return raw[65:73],raw[128:128+data_len],raw[start:start+resource_len]

def compare_for_update(old, new):
    before,after=read_macbinary(old),read_macbinary(new)
    if before==after: return 0
    if before[0]==after[0] and before[0][:4]==b"APPL" and b"aMacBday\x01" in before[2]: return 1
    raise ValueError(f"Preserving unrelated or edited guest file: {old}")

if __name__ == "__main__":
    if len(sys.argv)==4 and sys.argv[1]=="--compare":
        raise SystemExit(compare_for_update(sys.argv[2],sys.argv[3]))
    main()
