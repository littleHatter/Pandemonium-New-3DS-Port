#!/usr/bin/env python3
"""Bounded SIS field parsing utilities, reused from the Undercover preparation tool.
GPL-2.0-or-later; see the project LICENSE. No game data is embedded here.
"""
import argparse
import hashlib
import json
import lzma
import os
from pathlib import Path, PurePosixPath
import re
import struct
import sys
import tempfile
import zlib

MAX_FILE = 32 * 1024 * 1024
MAX_PACKAGE = 128 * 1024 * 1024


class PreparationError(Exception):
    pass


def require(condition, message):
    if not condition:
        raise PreparationError(message)


def fields(data, typed=True, element_type=None):
    """Read bounded, four-byte-aligned SIS fields, including extended lengths."""
    offset = 0
    while offset < len(data):
        require(len(data) - offset >= (8 if typed else 4), 'Truncated SIS field header.')
        kind = element_type
        if typed:
            kind = struct.unpack_from('<I', data, offset)[0]
            offset += 4
        size = struct.unpack_from('<I', data, offset)[0]
        offset += 4
        if size & 0x80000000:
            require(len(data) - offset >= 4, 'Truncated extended SIS length.')
            size = (size & 0x7fffffff) | (struct.unpack_from('<I', data, offset)[0] << 31)
            offset += 4
        require(size <= len(data) - offset, 'SIS field exceeds its container.')
        yield kind, data[offset:offset + size]
        offset += (size + 3) & ~3
        require(offset <= len(data), 'Truncated SIS alignment padding.')


def array(data):
    require(len(data) >= 4, 'Truncated SIS array.')
    return list(fields(data[4:], False, struct.unpack_from('<I', data)[0]))


def first(data, kind):
    matches = [value for tag, value in fields(data) if tag == kind]
    require(len(matches) == 1, 'Missing or duplicate SIS field %d.' % kind)
    return matches[0]


def decompress(data):
    require(len(data) >= 12, 'Truncated SIS compression header.')
    algorithm, size = struct.unpack_from('<IQ', data)
    require(size <= MAX_FILE, 'SIS decoded file exceeds the supported size limit.')
    if algorithm == 0:
        result = data[12:]
    elif algorithm == 1:
        decoder = zlib.decompressobj()
        result = decoder.decompress(data[12:], size + 1)
        require(decoder.eof and not decoder.unconsumed_tail and not decoder.unused_data,
                'Incomplete or oversized SIS zlib stream.')
    else:
        raise PreparationError('Unsupported SIS compression algorithm %d.' % algorithm)
    require(len(result) == size, 'SIS decoded length mismatch.')
    return result


def descriptions(data, depth=0):
    require(depth <= 32, 'SIS controller nesting is too deep.')
    for kind, value in fields(data):
        if kind == 24:
            yield value
        elif kind in (13, 26, 27, 28):
            yield from descriptions(value, depth + 1)
        elif kind == 2:
            for element_kind, element in array(value):
                if element_kind == 24:
                    yield element
                elif element_kind in (26, 27, 28):
                    yield from descriptions(element, depth + 1)


