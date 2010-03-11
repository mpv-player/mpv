#!/usr/bin/python
"""
Generate C definitions for parsing Matroska files.
Can also be used to directly parse Matroska files and display their contents.
"""

#
# This file is part of MPlayer.
#
# MPlayer is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# MPlayer is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with MPlayer; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#


elements_ebml = (
    'EBML, 1a45dfa3, sub', (
        'EBMLVersion, 4286, uint',
        'EBMLReadVersion, 42f7, uint',
        'EBMLMaxIDLength, 42f2, uint',
        'EBMLMaxSizeLength, 42f3, uint',
        'DocType, 4282, str',
        'DocTypeVersion, 4287, uint',
        'DocTypeReadVersion, 4285, uint',
    ),

    'CRC32, bf, binary',
    'Void, ec, binary',
)

elements_matroska = (
    'Segment, 18538067, sub', (

        'SeekHead*, 114d9b74, sub', (
            'Seek*, 4dbb, sub', (
                'SeekID, 53ab, ebml_id',
                'SeekPosition, 53ac, uint',
            ),
        ),

        'Info*, 1549a966, sub', (
            'SegmentUID, 73a4, binary',
            'PrevUID, 3cb923, binary',
            'NextUID, 3eb923, binary',
            'TimecodeScale, 2ad7b1, uint',
            'DateUTC, 4461, sint',
            'Title, 7ba9, str',
            'MuxingApp, 4d80, str',
            'WritingApp, 5741, str',
            'Duration, 4489, float',
        ),

        'Cluster*, 1f43b675, sub', (
            'Timecode, e7, uint',
            'BlockGroup*, a0, sub', (
                'Block, a1, binary',
                'BlockDuration, 9b, uint',
                'ReferenceBlock*, fb, sint',
            ),
            'SimpleBlock*, a3, binary',
        ),

        'Tracks*, 1654ae6b, sub', (
            'TrackEntry*, ae, sub', (
                'TrackNumber, d7, uint',
                'TrackUID, 73c5, uint',
                'TrackType, 83, uint',
                'FlagEnabled, b9, uint',
                'FlagDefault, 88, uint',
                'FlagForced, 55aa, uint',
                'FlagLacing, 9c, uint',
                'MinCache, 6de7, uint',
                'DefaultDuration, 23e383, uint',
                'TrackTimecodeScale, 23314f, float',
                'MaxBlockAdditionID, 55ee, uint',
                'Name, 536e, str',
                'Language, 22b59c, str',
                'CodecID, 86, str',
                'CodecPrivate, 63a2, binary',
                'CodecDecodeAll, aa, uint',
                'Video, e0, sub', (
                    'FlagInterlaced, 9a, uint',
                    'PixelWidth, b0, uint',
                    'PixelHeight, ba, uint',
                    'DisplayWidth, 54b0, uint',
                    'DisplayHeight, 54ba, uint',
                    'FrameRate, 2383e3, float',
                ),
                'Audio, e1, sub', (
                    'SamplingFrequency, b5, float',
                    'Channels, 9f, uint',
                    'BitDepth, 6264, uint',
                ),
                'ContentEncodings, 6d80, sub', (
                    'ContentEncoding*, 6240, sub', (
                        'ContentEncodingOrder, 5031, uint',
                        'ContentEncodingScope, 5032, uint',
                        'ContentEncodingType, 5033, uint',
                        'ContentCompression, 5034, sub', (
                            'ContentCompAlgo, 4254, uint',
                            'ContentCompSettings, 4255, binary',
                        ),
                    ),
                ),
            ),
        ),

        'Cues, 1c53bb6b, sub', (
            'CuePoint*, bb, sub', (
                'CueTime, b3, uint',
                'CueTrackPositions*, b7, sub', (
                    'CueTrack, f7, uint',
                    'CueClusterPosition, f1, uint',
                ),
            ),
        ),

        'Attachments, 1941a469, sub', (
            'AttachedFile*, 61a7, sub', (
                'FileName, 466e, str',
                'FileMimeType, 4660, str',
                'FileData, 465c, binary',
                'FileUID, 46ae, uint',
            ),
        ),

        'Chapters, 1043a770, sub', (
            'EditionEntry*, 45b9, sub', (
                'EditionUID, 45bc, uint',
                'EditionFlagHidden, 45bd, uint',
                'EditionFlagDefault, 45db, uint',
                'EditionFlagOrdered, 45dd, uint',
                'ChapterAtom*, b6, sub', (
                    'ChapterUID, 73c4, uint',
                    'ChapterTimeStart, 91, uint',
                    'ChapterTimeEnd, 92, uint',
                    'ChapterFlagHidden, 98, uint',
                    'ChapterFlagEnabled, 4598, uint',
                    'ChapterSegmentUID, 6e67, binary',
                    'ChapterSegmentEditionUID, 6ebc, uint',
                    'ChapterDisplay*, 80, sub', (
                        'ChapString, 85, str',
                        'ChapLanguage*, 437c, str',
                    ),
                ),
            ),
        ),
        'Tags*, 1254c367, sub', (
            'Tag*, 7373, sub', (
                'Targets, 63c0, sub', (
                    'TargetTypeValue, 68ca, uint',
                 ),
            ),
        ),
    ),
)


import sys
from math import ldexp

def byte2num(s):
    return int(s.encode('hex'), 16)

def camelcase_to_words(name):
    parts = []
    start = 0
    for i in range(1, len(name)):
        if name[i].isupper() and (name[i-1].islower() or
                                  name[i+1:i+2].islower()):
            parts.append(name[start:i])
            start = i
    parts.append(name[start:])
    return '_'.join(parts).lower()

class MatroskaElement(object):

    def __init__(self, name, elid, valtype, namespace):
        self.name = name
        self.definename = '%s_ID_%s' % (namespace, name.upper())
        self.fieldname = camelcase_to_words(name)
        self.structname = 'ebml_' + self.fieldname
        self.elid = elid
        self.valtype = valtype
        if valtype == 'sub':
            self.ebmltype = 'EBML_TYPE_SUBELEMENTS'
            self.valname = 'struct %s' % self.structname
        else:
            self.ebmltype = 'EBML_TYPE_' + valtype.upper()
            try:
                self.valname = {'uint': 'uint64_t', 'str': 'struct bstr',
                                'binary': 'struct bstr', 'ebml_id': 'uint32_t',
                                'float': 'double', 'sint': 'int64_t',
                                }[valtype]
            except KeyError:
                raise SyntaxError('Unrecognized value type ' + valtype)
        self.subelements = ()

    def add_subelements(self, subelements):
        self.subelements = subelements
        self.subids = set(x[0].elid for x in subelements)

elementd = {}
elementlist = []
def parse_elems(l, namespace):
    subelements = []
    for el in l:
        if isinstance(el, str):
            name, hexid, eltype = [x.strip() for x in el.split(',')]
            multiple = name.endswith('*')
            name = name.strip('*')
            new = MatroskaElement(name, hexid, eltype, namespace)
            elementd[hexid] = new
            elementlist.append(new)
            subelements.append((new, multiple))
        else:
            new.add_subelements(parse_elems(el, namespace))
    return subelements

parse_elems(elements_ebml, 'EBML')
parse_elems(elements_matroska, 'MATROSKA')

def generate_C_header():
    print('// Generated by TOOLS/matroska.py, do not edit manually')
    print

    for el in elementlist:
        print('#define %-40s 0x%s' % (el.definename, el.elid))

    print

    for el in reversed(elementlist):
        if not el.subelements:
            continue
        print
        print('struct %s {' % el.structname)
        l = max(len(subel.valname) for subel, multiple in el.subelements)+1
        for subel, multiple in el.subelements:
            print('    %-*s %s%s;' % (l, subel.valname, (' ', '*')[multiple],
                                    subel.fieldname))
        print
        for subel, multiple in el.subelements:
            print('    int  n_%s;' % (subel.fieldname))
        print('};')

    for el in elementlist:
        if not el.subelements:
            continue
        print('extern const struct ebml_elem_desc %s_desc;' % el.structname)

    print
    print('#define MAX_EBML_SUBELEMENTS %d' % max(len(el.subelements)
                                                  for el in elementlist))



def generate_C_definitions():
    print('// Generated by TOOLS/matroska.py, do not edit manually')
    print
    for el in reversed(elementlist):
        print
        if el.subelements:
            print('#define N %s' % el.fieldname)
            print('E_S("%s", %d)' % (el.name, len(el.subelements)))
            for subel, multiple in el.subelements:
                print('F(%s, %s, %d)' % (subel.definename, subel.fieldname,
                                         multiple))
            print('}};')
            print('#undef N')
        else:
            print('E("%s", %s, %s)' % (el.name, el.fieldname, el.ebmltype))

def read(s, length):
    t = s.read(length)
    if len(t) != length:
        raise IOError
    return t

def read_id(s):
    t = read(s, 1)
    i = 0
    mask = 128
    if ord(t) == 0:
        raise SyntaxError
    while not ord(t) & mask:
        i += 1
        mask >>= 1
    t += read(s, i)
    return t

def read_vint(s):
    t = read(s, 1)
    i = 0
    mask = 128
    if ord(t) == 0:
        raise SyntaxError
    while not ord(t) & mask:
        i += 1
        mask >>= 1
    t = chr(ord(t) & (mask - 1))
    t += read(s, i)
    return i+1, byte2num(t)

def read_str(s, length):
    return read(s, length)

def read_uint(s, length):
    t = read(s, length)
    return byte2num(t)

def read_sint(s, length):
    i = read_uint(s, length)
    mask = 1 << (length * 8 - 1)
    if i & mask:
        i -= 2 * mask
    return i

def read_float(s, length):
    t = read(s, length)
    i = byte2num(t)
    if length == 4:
        f = ldexp((i & 0x7fffff) + (1 << 23), (i >> 23 & 0xff) - 150)
        if i & (1 << 31):
            f = -f
    elif length == 8:
        f = ldexp((i & ((1 << 52) - 1)) + (1 << 52), (i >> 52 & 0x7ff) - 1075)
        if i & (1 << 63):
            f = -f
    else:
        raise SyntaxError
    return f

def parse_one(s, depth, parent, maxlen):
    elid = read_id(s).encode('hex')
    elem = elementd.get(elid)
    if parent is not None and elid not in parent.subids and elid not in ('ec', 'bf'):
        print('Unexpected:', elid)
        if 1:
            raise NotImplementedError
    size, length = read_vint(s)
    this_length = len(elid) / 2 + size + length
    if elem is not None:
        if elem.valtype != 'skip':
            print depth, elid, elem.name, 'size:', length, 'value:',
        if elem.valtype == 'sub':
            print('subelements:')
            while length > 0:
                length -= parse_one(s, depth + 1, elem, length)
            if length < 0:
                raise SyntaxError
        elif elem.valtype == 'str':
            print 'string', repr(read_str(s, length))
        elif elem.valtype in ('binary', 'ebml_id'):
            t = read_str(s, length)
            dec = ''
            if elem.valtype == 'ebml_id':
                idelem = elementd.get(t.encode('hex'))
                if idelem is None:
                    dec = '(UNKNOWN)'
                else:
                    dec = '(%s)' % idelem.name
            if len(t) < 20:
                t = t.encode('hex')
            else:
                t = '<skipped %d bytes>' % len(t)
            print 'binary', t, dec
        elif elem.valtype == 'uint':
            print 'uint', read_uint(s, length)
        elif elem.valtype == 'sint':
            print 'sint', read_sint(s, length)
        elif elem.valtype == 'float':
            print 'float', read_float(s, length)
        elif elem.valtype == 'skip':
            read(s, length)
        else:
            raise NotImplementedError
    else:
        print(depth, 'Unknown element:', elid, 'size:', length)
        read(s, length)
    return this_length

def parse_toplevel(s):
    parse_one(s, 0, None, 1 << 63)

if sys.argv[1] == '--generate-header':
    generate_C_header()
elif sys.argv[1] == '--generate-definitions':
    generate_C_definitions()
else:
    s = open(sys.argv[1])
    while 1:
        parse_toplevel(s)
