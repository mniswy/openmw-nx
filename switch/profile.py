#!/usr/bin/env python3
"""
Reads a profile written by OpenMW on Nintendo Switch (OPENMW_SWITCH_PROFILE, see README.md) and prints
where each sampled thread spent its time.

    profile.py profile.bin openmw.elf [--thread draw] [--from 60 --to 120] [--top 40]
                                      [--contains REGEX] [--folded out.txt]

The openmw.elf has to be the one the openmw.nro that produced the profile was made from.
"""

import argparse
import bisect
import collections
import os
import re
import shutil
import struct
import subprocess
import sys

UNKNOWN = 0xFFFFFFFF


def find_nm():
    name = 'aarch64-none-elf-nm'
    found = shutil.which(name)
    if found:
        return found
    for root in (os.environ.get('DEVKITPRO'), 'C:/devkitPro', '/opt/devkitpro'):
        if root:
            for suffix in ('', '.exe'):
                path = os.path.join(root, 'devkitA64', 'bin', name + suffix)
                if os.path.exists(path):
                    return path
    sys.exit(f'{name} not found, pass --nm')


def load_symbols(elf, nm):
    """Returns the sorted start addresses of all functions and their names."""
    output = subprocess.run([nm, '-C', '-n', '--defined-only', elf], check=True, capture_output=True,
                            text=True, errors='replace').stdout
    addresses = []
    names = []
    for line in output.splitlines():
        parts = line.split(' ', 2)
        if len(parts) != 3 or parts[1] not in 'tTwW':
            continue
        address = int(parts[0], 16)
        if addresses and addresses[-1] == address:
            continue
        addresses.append(address)
        names.append(parts[2])
    return addresses, names


def load_profile(path):
    with open(path, 'rb') as stream:
        data = stream.read()
    if data[:8] != b'OMWPROF1':
        sys.exit(f'{path} is not a profile')
    priority, count = struct.unpack_from('<II', data, 8)
    offset = 16
    threads = []
    for _ in range(count):
        threads.append(data[offset:offset + 16].split(b'\0', 1)[0].decode(errors='replace'))
        offset += 16
    (used,) = struct.unpack_from('<Q', data, offset)
    offset += 8
    words = struct.unpack_from(f'<{used}I', data, offset)
    samples = []
    i = 0
    while i < used:
        thread, time, depth = words[i:i + 3]
        samples.append((thread, time, words[i + 3:i + 3 + depth]))
        i += 3 + depth
    return priority, threads, samples


class Symbols:
    def __init__(self, addresses, names):
        self.addresses = addresses
        self.names = names
        self.end = addresses[-1] + 0x10000 if addresses else 0

    def index(self, address):
        if address == UNKNOWN or address >= self.end:
            return None
        i = bisect.bisect_right(self.addresses, address) - 1
        return i if i >= 0 else None

    def stack(self, frames):
        """Innermost function first. See the comment on the link register in switchprofiler.cpp."""
        if not frames:
            return []
        result = [self.index(frames[0])]
        chain = [self.index(address - 4) if address != UNKNOWN else None for address in frames[2:]]
        if len(frames) > 1 and frames[1] != UNKNOWN:
            caller = self.index(frames[1] - 4)
            # A function with a frame of its own has its return address in the chain already, and
            # whatever is in the link register is left over from a call it made itself.
            if caller is not None and caller != result[0] and not (frames[2:] and frames[1] == frames[2]):
                result.append(caller)
        result.extend(chain)
        return [self.names[i] for i in result if i is not None]


def print_table(title, counter, total, top):
    print(f'\n{title}')
    for name, count in counter.most_common(top):
        print(f'  {100 * count / total:6.2f}%  {count:7d}  {name[:150]}')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('profile')
    parser.add_argument('elf')
    parser.add_argument('--nm', help='path of aarch64-none-elf-nm')
    parser.add_argument('--thread', action='append', help='name or number of a thread, all by default')
    parser.add_argument('--from', dest='begin', type=float, default=0, help='seconds since the start')
    parser.add_argument('--to', dest='end', type=float, default=float('inf'), help='seconds since the start')
    parser.add_argument('--top', type=int, default=30)
    parser.add_argument('--contains', help='only samples with a function matching this expression in the stack')
    parser.add_argument('--folded', help='write stacks in the format flame graph tools read')
    args = parser.parse_args()

    priority, threads, samples = load_profile(args.profile)
    symbols = Symbols(*load_symbols(args.elf, args.nm or find_nm()))
    contains = re.compile(args.contains) if args.contains else None

    print(f'sampler priority {priority}, {len(samples)} samples')
    folded = collections.Counter()
    for number, name in enumerate(threads):
        if args.thread and name not in args.thread and str(number) not in args.thread:
            continue
        own = [s for s in samples if s[0] == number]
        if not own:
            continue
        print(f'\n=== thread {number} "{name}": {len(own)} samples from {own[0][1] / 1000:.1f} s '
              f'to {own[-1][1] / 1000:.1f} s')
        self_time = collections.Counter()
        inclusive = collections.Counter()
        total = 0
        for _, time, frames in own:
            if not args.begin <= time / 1000 < args.end:
                continue
            stack = symbols.stack(frames)
            if not stack or (contains and not any(contains.search(function) for function in stack)):
                continue
            total += 1
            self_time[stack[0]] += 1
            inclusive.update(set(stack))
            folded[f'{name}{number};' + ';'.join(reversed(stack))] += 1
        if total == 0:
            print('  no samples selected')
            continue
        print(f'  {total} samples selected')
        print_table('  time in the function itself', self_time, total, args.top)
        print_table('  time in the function and everything it calls', inclusive, total, args.top)

    if args.folded:
        with open(args.folded, 'w', encoding='utf-8') as stream:
            for stack, count in sorted(folded.items()):
                stream.write(f'{stack} {count}\n')


if __name__ == '__main__':
    main()
