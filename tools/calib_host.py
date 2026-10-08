#!/usr/bin/env python3
"""Host-side RS485 driver for the aio calibration protocol.

Frame: 0xA5 | len | payload[len] | crc32(payload)  (LE)
Opcodes: 0x7F PING -> 0xFF PONG | 0x01 SET_DA{ch,u16 ideal} -> 0x81 | 0x02 READ_AD -> 0x82 f32[4] | 0x00 NACK
"""

import argparse
import struct
import sys
import time
import zlib

FRAME_HEAD = 0xA5
OP_PING = 0x7F
OP_PONG = 0xFF
OP_SET_DA = 0x01
OP_ACK_SET_DA = 0x81
OP_READ_AD = 0x02
OP_ACK_READ_AD = 0x82
OP_NACK = 0x00
MAX_PAYLOAD = 26

GRID = [0] + list(range(1000, 66000, 1000)) + [65535]


def build(payload):
    return bytes([FRAME_HEAD, len(payload)]) + payload + struct.pack('<I', zlib.crc32(payload) & 0xFFFFFFFF)


def parse(buf, pos=0):
    """Scan buf from pos for the first valid frame. Returns (payload, next_pos) or (None, pos)."""
    n = len(buf)
    while pos + 2 <= n:
        if buf[pos] != FRAME_HEAD:
            pos += 1
            continue
        plen = buf[pos + 1]
        if plen > MAX_PAYLOAD:
            pos += 1
            continue
        if pos + 2 + plen + 4 > n:
            return None, pos
        payload = buf[pos + 2:pos + 2 + plen]
        crc = struct.unpack_from('<I', buf, pos + 2 + plen)[0]
        if crc != (zlib.crc32(payload) & 0xFFFFFFFF):
            pos += 1
            continue
        return payload, pos + 2 + plen + 4
    return None, pos


class Link:
    def __init__(self, port, baud):
        import serial
        self.ser = serial.Serial(port, baud, timeout=0.05)
        self.rx = b''

    def _pump(self, deadline):
        while time.time() < deadline:
            chunk = self.ser.read(256)
            if chunk:
                self.rx += chunk
                return True
        return False

    def request(self, payload, expect, timeout=0.3):
        self.rx = b''
        self.ser.reset_input_buffer()
        self.ser.write(build(payload))
        self.ser.flush()
        deadline = time.time() + timeout
        pos = 0
        while time.time() < deadline:
            pl, pos = parse(self.rx, pos)
            if pl is None:
                self.rx = self.rx[pos:]
                pos = 0
                if not self._pump(deadline):
                    continue
                continue
            if pl[0] == OP_NACK:
                raise RuntimeError('NACK from target')
            if pl[0] == expect:
                # ACK_SET_DA 无载荷；带内容说明解析错位，帧已不可信，吞掉重来
                if not pl[1:] or expect != OP_ACK_SET_DA:
                    return pl[1:]
                self.rx = self.rx[pos:]
                pos = 0
                continue
            # unexpected frame, keep scanning
        raise TimeoutError('no reply for opcode 0x%02X' % payload[0])


def cmd_ping(link, _args):
    link.request(bytes([OP_PING]), OP_PONG)
    print('PONG')


def cmd_set_da(link, args):
    pl = bytes([OP_SET_DA, args.ch]) + struct.pack('<H', args.ideal)
    link.request(pl, OP_ACK_SET_DA)
    print('ACK_SET_DA ch=%d ideal=%d' % (args.ch, args.ideal))


def cmd_read_ad(link, _args):
    data = link.request(bytes([OP_READ_AD]), OP_ACK_READ_AD)
    vals = struct.unpack_from('<4f', data, 0)
    print(' '.join('%9.3f' % v for v in vals))
    return vals


def cmd_sweep(link, args):
    worst = [0.0] * 4
    worst_pt = [0] * 4
    fps = [-1.0] * 4
    for g in GRID:
        link.request(bytes([OP_SET_DA, args.ch]) + struct.pack('<H', g), OP_ACK_SET_DA)
        time.sleep(args.settle)
        vals = cmd_read_ad(link, args)
        for ch in range(4):
            err = abs(vals[ch] - g)
            if err > worst[ch]:
                worst[ch], worst_pt[ch] = err, g
            if fps[ch] < 0:
                fps[ch] = vals[ch] - g
    print('--- max |measured - set| per channel (gate %g) ---' % args.gate)
    ok = True
    for ch in range(4):
        bad = worst[ch] > args.gate
        ok = ok and not bad
        print('ch%d: max=%.3f @ set=%d   offset_at_0=%+.3f  %s'
              % (ch, worst[ch], worst_pt[ch], fps[ch], 'FAIL' if bad else 'ok'))
    print('RESULT: %s' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('-p', '--port', required=True, help='serial device, e.g. /dev/ttyUSB0')
    p.add_argument('-b', '--baud', type=int, default=6000000, help='default 6000000; use 1000000 if the adapter cannot keep up')
    sub = p.add_subparsers(dest='cmd', required=True)

    sub.add_parser('ping').set_defaults(func=cmd_ping)

    s = sub.add_parser('set-da')
    s.add_argument('ch', type=int)
    s.add_argument('ideal', type=int)
    s.set_defaults(func=cmd_set_da)

    sub.add_parser('read-ad').set_defaults(func=cmd_read_ad)

    s = sub.add_parser('sweep', help='closed-loop check: drive REF DA ch, read REF AD, compare')
    s.add_argument('--ch', type=int, default=1)
    s.add_argument('--settle', type=float, default=0.1)
    s.add_argument('--gate', type=float, default=16.0)
    s.set_defaults(func=cmd_sweep)

    args = p.parse_args()
    link = Link(args.port, args.baud)
    rc = args.func(link, args)
    sys.exit(0 if rc is None or rc == 0 else 1)


if __name__ == '__main__':
    main()
