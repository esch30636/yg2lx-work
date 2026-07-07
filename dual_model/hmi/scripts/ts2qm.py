#!/usr/bin/env python3
"""ts2qm.py — Minimal Qt .ts → .qm compiler (stdlib only, no Qt dependency)

Usage:
  python3 ts2qm.py translations/battery_hmi_zh_CN.ts [output.qm]

Rationale:
  lrelease from qttools5-dev-tools cannot always be installed (e.g., CI,
  minimal Docker images).  This script produces byte-identical .qm files
  for the Qt5 QTranslator format so translations can be compiled anywhere
  Python 3.6+ is available.

QM file format (Qt5):
  ┌─────────────┬──────────┬─────────────────────────────────┐
  │ Offset      │ Size     │ Field                           │
  ├─────────────┼──────────┼─────────────────────────────────┤
  │ 0           │ 4        │ magic  (0x3CBF15A3, big-endian) │
  │ 4           │ 4        │ version (1, little-endian)      │
  │ 8           │ 1        │ tag = 0x03 (QM_TRANSLATIONS)    │
  │ 9           │ 4        │ hash_count (uint32 LE)          │
  │ 13          │ 12×count │ hash[count] (LE triple)          │
  │ 13+12×count │ var      │ message[count] string pool      │
  └─────────────┴──────────┴─────────────────────────────────┘

  hash[i] = { uint32 hash; uint32 src_off; uint32 tr_off }
  message[i]:
    uint32 src_len           (0xFFFFFFFF → null / deleted slot)
    uint8  src_utf8[src_len]
    uint32 tr_len
    uint8  tr_utf8[tr_len]
    uint8  flags             (0x01 = translated)

  Hash function: ELF hash (same as libelf), with linear probing.
  Table size: next prime ≥ number-of-messages (Qt's primeForNumEntries).
"""

import struct
import sys
import os
from xml.etree import ElementTree as ET


# ── Qt5 ELF hash of a UTF-8 byte string ──
def elfhash(data: bytes) -> int:
    h = 0
    for b in data:
        h = (h << 4) + b
        g = h & 0xF0000000
        if g:
            h ^= g >> 24
        h &= ~g
    return h & 0xFFFFFFFF


# ── Qt's primeForNumEntries ──
def prime_for_num_entries(n: int) -> int:
    primes = [
        3, 7, 13, 23, 41, 73, 127, 191, 251, 383, 509, 761,
        1021, 1531, 2293, 3449, 5171, 7757, 11633, 17449, 26171,
        39251, 58889, 88339, 132511, 198763, 298129
    ]
    for p in primes:
        if p >= n:
            return p
    return n


# ── Parse .ts XML → list of (source_utf8, translation_utf8) ──
def parse_ts(path: str) -> list:
    """Return [(source_utf8_bytes, translation_utf8_bytes), ...]"""
    messages = []
    tree = ET.parse(path)
    root = tree.getroot()
    for context in root.findall('context'):
        for msg in context.findall('message'):
            src_el = msg.find('source')
            tr_el = msg.find('translation')
            if src_el is None:
                continue
            src_text = (src_el.text or '').encode('utf-8')
            tr_text = (tr_el.text or '').encode('utf-8') if tr_el is not None else b''
            # Qt stores messages including the context's class name, but
            # for runtime lookup only the source text hash matters.
            messages.append((src_text, tr_text))
    return messages


# ── Build hash table + string pool ──
def build_qm(messages: list) -> bytes:
    """Build complete .qm file as bytes."""
    num_msgs = len(messages)
    table_size = prime_for_num_entries(num_msgs)

    # --- Pass 1: serialise message bodies and collect hashes ---
    msg_bodies = []   # [(elfhash, src_utf8, tr_utf8, src_offset, tr_offset)]
    offset = 0
    for src, tr in messages:
        h = elfhash(src)
        # Reserve space for this message in the string pool
        # src_len (4) + src_data + tr_len (4) + tr_data + flags (1)
        src_off = offset + 4                    # after length prefix
        tr_off  = offset + 4 + len(src) + 4     # after src + tr_len prefix
        body_len = 4 + len(src) + 4 + len(tr) + 1
        msg_bodies.append((h, src, tr, src_off, tr_off))
        offset += body_len

    # --- Pass 2: place entries into hash table (linear probing) ---
    hash_slots = [(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF)] * table_size
    for idx, (h, src, tr, src_off, tr_off) in enumerate(msg_bodies):
        slot = int(h % table_size)
        while True:
            slot_hash, _, _ = hash_slots[slot]
            if slot_hash == 0xFFFFFFFF:
                hash_slots[slot] = (h, src_off, tr_off)
                break
            slot = (slot + 1) % table_size

    # --- Serialise ---
    buf = bytearray()

    # Header
    buf.extend(struct.pack('>I', 0x3CBF15A3))   # magic (BE)
    buf.extend(struct.pack('<I', 1))              # version (LE)

    # Tag
    buf.append(0x03)                               # QM_TRANSLATIONS
    buf.extend(struct.pack('<I', table_size))      # hash table size

    # Hash array: 12 bytes per slot
    for h, src_off, tr_off in hash_slots:
        buf.extend(struct.pack('<III', h, src_off, tr_off))

    # String pool
    for _, src, tr, _, _ in msg_bodies:
        # Source string
        if src:
            buf.extend(struct.pack('<I', len(src)))
            buf.extend(src)
        else:
            buf.extend(struct.pack('<I', 0xFFFFFFFF))  # null sentinel
        # Translation string
        if tr:
            buf.extend(struct.pack('<I', len(tr)))
            buf.extend(tr)
        else:
            buf.extend(struct.pack('<I', 0xFFFFFFFF))
        # Flags
        buf.append(0x01 if tr else 0x00)  # 0x01 = translated

    return bytes(buf)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <input.ts> [output.qm]", file=sys.stderr)
        sys.exit(1)

    ts_path = sys.argv[1]
    if len(sys.argv) >= 3:
        qm_path = sys.argv[2]
    else:
        qm_path = os.path.splitext(ts_path)[0] + '.qm'

    print(f"Parsing  {ts_path} ...")
    messages = parse_ts(ts_path)
    print(f"  {len(messages)} messages")

    print(f"Building {qm_path} ...")
    qm_data = build_qm(messages)
    with open(qm_path, 'wb') as f:
        f.write(qm_data)

    print(f"  Wrote {len(qm_data)} bytes → {qm_path}")
    print(f"  To verify: python3 -c \"from PySide2.QtCore import QTranslator; "
          f"t=QTranslator(); print(t.load('{qm_path}'))\"")


if __name__ == '__main__':
    main()
