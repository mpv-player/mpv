#!/usr/bin/env python3
"""
Generate C definitions for parsing Matroska files.
Can also be used to directly parse Matroska files and display their contents.
"""

#
# This file is part of mpv.
#
# mpv is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# mpv is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
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
                'DiscardPadding,  75A2, sint',
                'BlockAdditions, 75A1, sub', (
                    'BlockMore*, A6, sub', (
                        'BlockAddID, EE, uint',
                        'BlockAdditional, A5, binary',
                    ),
                ),
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
                'MaxCache, 6df8, uint',
                'DefaultDuration, 23e383, uint',
                'TrackTimecodeScale, 23314f, float',
                'MaxBlockAdditionID, 55ee, uint',
                'Name, 536e, str',
                'Language, 22b59c, str',
                'CodecID, 86, str',
                'CodecPrivate, 63a2, binary',
                'CodecName, 258688, str',
                'CodecDecodeAll, aa, uint',
                'CodecDelay, 56aa, uint',
                'SeekPreRoll, 56bb, uint',
                'Video, e0, sub', (
                    'FlagInterlaced, 9a, uint',
                    'PixelWidth, b0, uint',
                    'PixelHeight, ba, uint',
                    'DisplayWidth, 54b0, uint',
                    'DisplayHeight, 54ba, uint',
                    'DisplayUnit, 54b2, uint',
                    'FrameRate, 2383e3, float',
                    'ColourSpace, 2eb524, binary',
                    'StereoMode, 53b8, uint',
                    'Colour, 55b0, sub', (
                        'MatrixCoefficients,      55B1, uint',
                        'BitsPerChannel,          55B2, uint',
                        'ChromaSubsamplingHorz,   55B3, uint',
                        'ChromaSubsamplingVert,   55B4, uint',
                        'CbSubsamplingHorz,       55B5, uint',
                        'CbSubsamplingVert,       55B6, uint',
                        'ChromaSitingHorz,        55B7, uint',
                        'ChromaSitingVert,        55B8, uint',
                        'Range,                   55B9, uint',
                        'TransferCharacteristics, 55BA, uint',
                        'Primaries,               55BB, uint',
                        'MaxCLL,                  55BC, uint',
                        'MaxFALL,                 55BD, uint',
                        'MasteringMetadata,       55D0, sub', (
                            'PrimaryRChromaticityX,   55D1, float',
                            'PrimaryRChromaticityY,   55D2, float',
                            'PrimaryGChromaticityX,   55D3, float',
                            'PrimaryGChromaticityY,   55D4, float',
                            'PrimaryBChromaticityX,   55D5, float',
                            'PrimaryBChromaticityY,   55D6, float',
                            'WhitePointChromaticityX, 55D7, float',
                            'WhitePointChromaticityY, 55D8, float',
                            'LuminanceMax,            55D9, float',
                            'LuminanceMin,            55DA, float',
                        ),
                    ),
                    'Projection, 7670, sub', (
                        'ProjectionType, 7671, uint',
                        'ProjectionPrivate, 7672, binary',
                        'ProjectionPoseYaw, 7673, float',
                        'ProjectionPosePitch, 7674, float',
                        'ProjectionPoseRoll, 7675, float',
                    ),
                ),
                'Audio, e1, sub', (
                    'SamplingFrequency, b5, float',
                    'OutputSamplingFrequency, 78b5, float',
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
                    'CueRelativePosition, f0, uint',
                    'CueDuration, b2, uint',
                ),
            ),
        ),

        'Attachments, 1941a469, sub', (
            'AttachedFile*, 61a7, sub', (
                'FileDescription, 467e, str',
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
                        'ChapCountry*, 437e, str',
                    ),
                ),
            ),
        ),
        'Tags*, 1254c367, sub', (
            'Tag*, 7373, sub', (
                'Targets, 63c0, sub', (
                    'TargetTypeValue, 68ca, uint',
                    'TargetType, 63ca, str',
                    'TargetTrackUID, 63c5, uint',
                    'TargetEditionUID, 63c9, uint',
                    'TargetChapterUID, 63c4, uint',
                    'TargetAttachmentUID, 63c6, uint',
                 ),
                'SimpleTag*, 67c8, sub', (
                    'TagName, 45a3, str',
                    'TagLanguage, 447a, str',
                    'TagString, 4487, str',
                    'TagDefault, 4484, uint',
                ),
            ),
        ),
    ),
)


import sys
from math import ldexp
from binascii import hexlify

def byte2num(s):
    return int(hexlify(s), 16)

class EOF(Exception): pass

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
        self.definename = '{0}_ID_{1}'.format(namespace, name.upper())
        self.fieldname = camelcase_to_words(name)
        self.structname = 'ebml_' + self.fieldname
        self.elid = elid
        self.valtype = valtype
        if valtype == 'sub':
            self.ebmltype = 'EBML_TYPE_SUBELEMENTS'
            self.valname = 'struct ' + self.structname
        else:
            self.ebmltype = 'EBML_TYPE_' + valtype.upper()
            try:
                self.valname = {'uint': 'uint64_t', 'str': 'char *',
                                'binary': 'bstr', 'ebml_id': 'uint32_t',
                                'float': 'double', 'sint': 'int64_t',
                                }[valtype]
            except KeyError:
                raise SyntaxError('Unrecognized value type ' + valtype)
        self.subelements = ()

    def add_subelements(self, subelements):
        self.subelements = subelements
        self.subids = {x[0].elid for x in subelements}

elementd = {}
elementlist = []
def parse_elems(l, namespace):
    subelements = []
    for el in l:
        if isinstance(el, str):
            name, hexid, eltype = [x.strip() for x in el.split(',')]
            hexid = hexid.lower()
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

def printf(out, *args):
    out.write(' '.join(str(x) for x in args))
    out.write('\n')

def generate_C_header(out):
    printf(out, '// Generated by TOOLS/matroska.py, do not edit manually')
    printf(out)

    for el in elementlist:
        printf(out, '#define {0.definename:40} 0x{0.elid}'.format(el))

    printf(out)

    for el in reversed(elementlist):
        if not el.subelements:
            continue
        printf(out)
        printf(out, 'struct {0.structname} {{'.format(el))
        l = max(len(subel.valname) for subel, multiple in el.subelements)+1
        for subel, multiple in el.subelements:
            printf(out, '    {e.valname:{l}} {star}{e.fieldname};'.format(
                        e=subel, l=l, star=' *'[multiple]))
        printf(out)
        for subel, multiple in el.subelements:
            printf(out, '    int  n_{0.fieldname};'.format(subel))
        printf(out, '};')

    for el in elementlist:
        if not el.subelements:
            continue
        printf(out, 'extern const struct ebml_elem_desc {0.structname}_desc;'.format(el))

    printf(out)
    printf(out, '#define MAX_EBML_SUBELEMENTS', max(len(el.subelements)
                                                    for el in elementlist))


def generate_C_definitions(out):
    printf(out, '// Generated by TOOLS/matroska.py, do not edit manually')
    printf(out)
    for el in reversed(elementlist):
        printf(out)
        if el.subelements:
            printf(out, '#define N', el.fieldname)
            printf(out, 'E_S("{0}", {1})'.format(el.name, len(el.subelements)))
            for subel, multiple in el.subelements:
                printf(out, 'F({0.definename}, {0.fieldname}, {1})'.format(
                            subel, int(multiple)))
            printf(out, '}};')
            printf(out, '#undef N')
        else:
            printf(out, 'E("{0.name}", {0.fieldname}, {0.ebmltype})'.format(el))

def read(s, length):
    t = s.read(length)
    if len(t) != length:
        raise EOF
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
    t = bytes((ord(t) & (mask - 1),))
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
    elid = hexlify(read_id(s)).decode('ascii')
    elem = elementd.get(elid)
    size, length = read_vint(s)
    this_length = len(elid) / 2 + size + length
    if elem is not None:
        if elem.valtype != 'skip':
            print("    " * depth, '[' + elid + ']', elem.name, 'size:', length, 'value:', end=' ')
        if elem.valtype == 'sub':
            print('subelements:')
            while length > 0:
                length -= parse_one(s, depth + 1, elem, length)
            if length < 0:
                raise SyntaxError
        elif elem.valtype == 'str':
            print('string', repr(read_str(s, length).decode('utf8', 'replace')))
        elif elem.valtype in ('binary', 'ebml_id'):
            t = read_str(s, length)
            dec = ''
            if elem.valtype == 'ebml_id':
                idelem = elementd.get(hexlify(t).decode('ascii'))
                if idelem is None:
                    dec = '(UNKNOWN)'
                else:
                    dec = '({0.name})'.format(idelem)
            if len(t) < 20:
                t = hexlify(t).decode('ascii')
            else:
                t = '<{0} bytes>'.format(len(t))
            print('binary', t, dec)
        elif elem.valtype == 'uint':
            print('uint', read_uint(s, length))
        elif elem.valtype == 'sint':
            print('sint', read_sint(s, length))
        elif elem.valtype == 'float':
            print('float', read_float(s, length))
        elif elem.valtype == 'skip':
            read(s, length)
        else:
            raise NotImplementedError
    else:
        print("    " * depth, '[' + elid + '] Unknown element! size:', length)
        read(s, length)
    return this_length

if __name__ == "__main__":
    def parse_toplevel(s):
        parse_one(s, 0, None, 1 << 63)

    if sys.argv[1] == '--generate-header':
        generate_C_header(sys.stdout)
    elif sys.argv[1] == '--generate-definitions':
        generate_C_definitions(sys.stdout)
    else:
        s = open(sys.argv[1], "rb")
        while 1:
            start = s.tell()
            try:
                parse_toplevel(s)
            except EOF:
                if s.tell() != start:
                    raise Exception("Unexpected end of file")
                break
