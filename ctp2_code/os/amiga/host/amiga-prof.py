#!/usr/bin/env python3
"""Resolve an ENV:CTP2_PROF dump (RAM:ctp2prof.bin, see os/amiga/amiga_prof.c).

usage: amiga-prof.py ctp2prof.bin ctp2.debug [--top N] [--event NAME] [--callers FUNC]

Prints the game events the samples fell in, the functions the sampled PCs
fall in (self), and the functions found anywhere in a sample (the PC or its
callers, counted once per sample: "inclusive"). With ENV:CTP2_PROF=2 and a
build with -fno-omit-frame-pointer the callers are the frame chain; else they
are code addresses found on the stack (stale ones included). A PC outside the
game's code (Kickstart or a library) shows as [ROM/library].

--event NAME   only the samples taken while that game event was processed
--callers FUNC who called FUNC (substring of the function's name), by count
"""
import argparse
import bisect
import collections
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EVENTS_H = os.path.join(HERE, '..', '..', '..', 'gs', 'events', 'GameEventDescription.h')


def symbols(debug):
    out = subprocess.run(['/opt/amiga/bin/m68k-amigaos-nm', '-C', '-n', debug],
                         capture_output=True, text=True, check=True).stdout
    addrs, names, log = [], [], None
    for line in out.splitlines():
        parts = line.split(' ', 2)
        if len(parts) < 3:
            continue
        if parts[2] in ('amiga_log', '_amiga_log'):
            log = int(parts[0], 16)
        if parts[1] not in 'tT':
            continue
        addrs.append(int(parts[0], 16))
        names.append(parts[2])
    if log is None:
        sys.exit('amiga_log not found in ' + debug)
    return addrs, names, log


def event_names():
    try:
        text = open(EVENTS_H, encoding='latin-1').read()
    except OSError:
        return []
    return re.findall(r'^\s*EVENT\(\s*(\w+)', text, re.M)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dump')
    ap.add_argument('debug')
    ap.add_argument('--top', type=int, default=40)
    ap.add_argument('--event')
    ap.add_argument('--callers')
    a = ap.parse_args()

    data = open(a.dump, 'rb').read()
    magic, version, entry, count, waiting = struct.unpack('>5I', data[:20])
    if magic != 0x43545050 or version not in (1, 2):
        sys.exit('not a ctp2prof.bin')
    tagged = version >= 2
    addrs, names, log = symbols(a.debug)
    events = event_names()

    def name(off):
        if off == 1:
            return '[ROM/library]'
        if off >= 0x80000000:
            off -= 0x100000000
        i = bisect.bisect_right(addrs, log + off) - 1
        return names[i] if i >= 0 else '?'

    def event(tag):
        if tag == 0xfffffffe:
            return "(between events, another player's turn)"
        if tag >= 0x80000000:
            return '(none)'
        return events[tag] if tag < len(events) else 'event %d' % tag

    words = struct.unpack('>%dI' % (count * entry), data[20:20 + count * entry * 4])
    per_event = collections.Counter()
    self_ = collections.Counter()
    incl = collections.Counter()
    callers = collections.Counter()
    used = 0
    for s in range(count):
        e = words[s * entry:(s + 1) * entry]
        tag = e[-1] if tagged else 0xffffffff
        addrs_ = e[:-1] if tagged else e
        ev = event(tag)
        per_event[ev] += 1
        if a.event and ev.lower() != a.event.lower():
            continue
        used += 1
        chain = [name(o) for o in addrs_ if o]
        if not chain:
            continue
        self_[chain[0]] += 1
        for n in set(chain):
            incl[n] += 1
        if a.callers:
            for k, n in enumerate(chain[:-1]):
                if a.callers in n:
                    callers[chain[k + 1]] += 1
                    break

    print('%d samples running, %d waiting (1 ms each)' % (count, waiting))
    if tagged:
        print('\nby game event:')
        for n, c in per_event.most_common(15):
            print('%6d %5.1f%%  %s' % (c, 100.0 * c / max(count, 1), n))
    print('\n%s%d samples' % ('event %s: ' % a.event if a.event else '', used))
    print('\nself:')
    for n, c in self_.most_common(a.top):
        print('%6d %5.1f%%  %s' % (c, 100.0 * c / max(used, 1), n))
    print('\nin the sample (PC or a caller):')
    for n, c in incl.most_common(a.top):
        print('%6d %5.1f%%  %s' % (c, 100.0 * c / max(used, 1), n))
    if a.callers:
        print('\ncallers of %s:' % a.callers)
        for n, c in callers.most_common(a.top):
            print('%6d  %s' % (c, n))


if __name__ == '__main__':
    main()
