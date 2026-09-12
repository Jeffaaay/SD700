"""Strict ELF32/ARM executable + Intel HEX load-image reader for this STM32 candidate.

File-backed PT_LOAD bytes use physical/load addresses (including RAM .data's
flash initializer). NOBITS/zero-fill RAM is not fabricated into the flash image.
No ARM executable or third-party Python package is required.
"""
import re
import struct

FLASH_START, FLASH_END = 0x08000000, 0x08040000
RAM_START, RAM_END = 0x20000000, 0x20020000


def require(condition, message):
    if not condition:
        raise ValueError(message)


def bounded(data, offset, size, label):
    require(0 <= offset <= len(data) and 0 <= size <= len(data)-offset,
            f'ELF {label} outside file bounds')
    return data[offset:offset+size]


def memory_range(address, size):
    return ((FLASH_START <= address <= FLASH_END and size <= FLASH_END-address) or
            (RAM_START <= address <= RAM_END and size <= RAM_END-address))


def cstring(table, offset):
    require(0 <= offset < len(table), 'ELF string offset outside table')
    end = table.find(b'\0', offset)
    require(end >= 0, 'ELF unterminated string')
    return table[offset:end].decode('ascii')


class ElfImage:
    def __init__(self, data):
        require(len(data) >= 52, 'ELF truncated header')
        (ident, kind, machine, version, self.entry, phoff, shoff, flags,
         ehsize, phsize, phcount, shsize, shcount, shstr) = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
        require(ident == b'\x7fELF\x01\x01\x01'+bytes(9), 'ELF must be ELF32 little-endian System V version1')
        require(kind == 2 and machine == 40 and version == 1, 'ELF must be an ARM executable')
        require(flags == 0x05000400, 'ELF must use the expected ARM EABI5 hard-float ABI')
        require(ehsize == 52 and phsize == 32 and shsize == 40, 'ELF header/table entry size mismatch')
        require(0 < phcount < 128 and 0 < shstr < shcount < 4096, 'ELF invalid table counts')
        bounded(data, phoff, phsize*phcount, 'program table')
        bounded(data, shoff, shsize*shcount, 'section table')
        require(phoff >= ehsize and shoff >= phoff+phsize*phcount, 'ELF overlapping header tables')
        self.sections = [struct.unpack_from('<10I', data, shoff+i*shsize) for i in range(shcount)]
        require(self.sections[0] == (0,)*10, 'ELF invalid null section')
        self.loads = []
        self.load_bytes = {}
        for i in range(phcount):
            p = struct.unpack_from('<8I', data, phoff+i*phsize)
            kind, off, virtual, physical, filesz, memsz, perms, align = p
            require(kind == 1, 'ELF unexpected program segment type')
            require(filesz <= memsz and memory_range(virtual, memsz), 'ELF invalid segment memory range/size')
            require(align in (0, 1) or (align & (align-1) == 0 and (virtual-off) % align == 0),
                    'ELF invalid segment alignment')
            require(perms & ~7 == 0, 'ELF invalid segment flags')
            segment = bounded(data, off, filesz, 'load segment')
            require(FLASH_START <= physical <= FLASH_END and filesz <= FLASH_END-physical,
                    'ELF physical load bytes outside flash')
            for prior in self.loads:
                pv, pm = prior[2], prior[5]
                require(not (memsz and pm and virtual < pv+pm and pv < virtual+memsz),
                        'ELF overlapping virtual segments')
            for j, byte in enumerate(segment):
                address = physical+j
                require(address not in self.load_bytes, 'ELF overlapping physical load bytes')
                self.load_bytes[address] = byte
            self.loads.append(p)
        require(self.load_bytes and self.entry & 1 and
                any(p[6] & 1 and p[2] <= (self.entry & ~1) < p[2]+p[4] for p in self.loads),
                'ELF entry is not Thumb code in a file-backed executable segment')
        names = self.sections[shstr]
        require(names[1] == 3, 'ELF section-name table is not STRTAB')
        names_data = bounded(data, names[4], names[5], 'section-name table')
        covered = set()
        self.section_names = []
        for i, section in enumerate(self.sections):
            n, kind, attrs, addr, off, size, link, info, align, entsize = section
            self.section_names.append(cstring(names_data, n))
            require(link < shcount, 'ELF invalid section link')
            require(align in (0, 1) or align & (align-1) == 0, 'ELF invalid section alignment')
            if kind != 8:
                bounded(data, off, size, 'section')
            if not attrs & 2 or not size:
                continue
            require(memory_range(addr, size), 'ELF allocated section outside device memory')
            candidates = [p for p in self.loads if p[2] <= addr and addr+size <= p[2]+p[5]]
            require(len(candidates) == 1, 'ELF allocated section lacks a unique load segment')
            p = candidates[0]
            if kind == 8:
                require(addr >= p[2]+p[4], 'ELF NOBITS overlaps initialized bytes')
                continue
            require(off == p[1]+addr-p[2] and addr+size <= p[2]+p[4],
                    'ELF section/load file mapping disagrees')
            start = p[3]+addr-p[2]
            for address in range(start, start+size):
                require(address not in covered, 'ELF overlapping allocated sections')
                covered.add(address)
        require(covered == self.load_bytes.keys(), 'ELF load bytes not exactly covered by allocated file sections')
        self.symbols, self.symbol_offsets = {}, {}
        for section in self.sections:
            if section[1] != 2:
                continue
            require(section[9] == 16 and section[5] % 16 == 0, 'ELF malformed symbol table')
            strings = self.sections[section[6]]
            require(strings[1] == 3, 'ELF symbol names are not STRTAB')
            text = bounded(data, strings[4], strings[5], 'symbol strings')
            for pos in range(section[4], section[4]+section[5], 16):
                n, v, size, info, other, idx = struct.unpack_from('<IIIBBH', data, pos)
                name = cstring(text, n)
                require(idx < shcount or idx in (0xFFF1, 0xFFF2), 'ELF invalid symbol section index')
                if not size or idx == 0 or idx >= shcount:
                    continue
                source = self.sections[idx]
                address = v & ~1 if info & 15 == 2 else v
                require(source[3] <= address and size <= source[5]-(address-source[3]),
                        'ELF symbol outside its section')
                if source[1] == 8:
                    continue
                at = source[4]+address-source[3]
                require(name not in self.symbols, 'ELF duplicate sized symbol: '+name)
                self.symbols[name] = bounded(data, at, size, 'symbol')
                self.symbol_offsets[name] = at


def read_hex(data):
    try:
        lines = data.decode('ascii').splitlines()
    except UnicodeDecodeError as error:
        raise ValueError('HEX must be ASCII') from error
    image, base, entry, eof = {}, 0, None, False
    for line in lines:
        require(not eof, 'HEX record after EOF')
        require(bool(re.fullmatch(r':[0-9a-fA-F]+', line)), 'HEX malformed record')
        require(len(line) % 2 == 1, 'HEX odd byte encoding')
        record = bytes.fromhex(line[1:])
        require(len(record) >= 5 and len(record) == record[0]+5, 'HEX record length mismatch')
        require(sum(record) & 255 == 0, 'HEX checksum mismatch')
        size, kind = record[0], record[3]
        address = int.from_bytes(record[1:3], 'big')
        payload = record[4:-1]
        if kind == 0:
            require(size > 0 and address+size <= 0x10000, 'HEX invalid data record range')
            for i, byte in enumerate(payload):
                at = base+address+i
                require(FLASH_START <= at < FLASH_END, 'HEX load address outside flash')
                require(at not in image, 'HEX duplicate/overlapping address')
                image[at] = byte
        elif kind == 1:
            require(size == 0 and address == 0, 'HEX malformed EOF')
            eof = True
        elif kind == 4:
            require(size == 2 and address == 0, 'HEX malformed extended linear address')
            base = int.from_bytes(payload, 'big') << 16
        elif kind == 5:
            require(size == 4 and address == 0 and entry is None, 'HEX malformed/duplicate entry')
            entry = int.from_bytes(payload, 'big')
        else:
            raise ValueError('HEX unsupported record type (expected 00/01/04/05)')
    require(eof and image and entry is not None, 'HEX missing EOF, data or entry record')
    return image, entry


def compare_images(elf, hex_data):
    image, entry = read_hex(hex_data)
    require(entry == elf.entry, 'HEX/ELF entry mismatch')
    require(image == elf.load_bytes, 'HEX/ELF addressized load bytes mismatch')
