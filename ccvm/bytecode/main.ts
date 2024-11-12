
import * as fs from 'node:fs';

export interface Section {
    id: string;
    index: number;
    name: string;
    link: string | undefined;
    reloc: string | undefined;
    prev: string | undefined;
    size: number;
    data: Uint8Array;
    addr: number;
    entsize: number;
    flags: number;
    info: number;
    type: number;
}

export enum SymbolBinding {
    LOCAL = 0,
    GLOBAL = 1,
    WEAK = 2,
}

export enum SymbolType {
    NOTYPE = 0,
    OBJECT = 1,
    FUNC = 2,
    SECTION = 3,
    FILE = 4,
    COMMON = 5,
    TLS = 6,
}

export interface Symbol {
    name: string;
    section: Section | undefined;
    absoluteAddr: boolean;
    addr: number;
    size: number;
    binding: SymbolBinding;
    type: SymbolType;
    imported?: ImportEntry;
}

type GetStringType = (offset: number) => string;
type Dict<T> = { [key: string]: T };

export interface ExportEntry {
    index: number;
    name: string;
    symbol: Symbol | undefined;
};

export interface ImportEntry {
    index: number;
    name: string;
};

export interface Program {
    sections: Section[];
    sectionById: Dict<Section>;
    sectionByIndex: Section[];
    getString: GetStringType;
    symbols: (Symbol | undefined)[];
    exports: ExportEntry[];
    imports: ImportEntry[];
    importByName: Dict<ImportEntry>;
};


function parseStrtab(strtab: Section): GetStringType {
    let strings = new TextDecoder('ascii').decode(strtab.data);
    return function (offset: number): string {
        if (offset < 0 || offset >= strings.length) throw new Error('Invalid strtab reference.');
        let end = strings.indexOf('\0', offset);
        if (end < 0) throw new Error('Invalid strtab.');
        return strings.substring(offset, end);
    }
}

function error(message: string) {
    console.error('ERROR:', message);
}

function warning(message: string) {
    console.warn('WARNING:', message);
}

const SHN_LORESERVE = 0xff00;
const SHN_ABS = 0xfff1;

function parseSymtab(program: Program, symtab: Section) {
    let offset = 0;
    let view = new DataView(symtab.data.buffer, symtab.data.byteOffset, symtab.data.byteLength);
    program.symbols.splice(0);
    let exportByName: Dict<ExportEntry[]> = Object.create(null);
    for (let exp of program.exports) {
        if (!exp) continue;
        exportByName[exp.name] = exportByName[exp.name] ?? [];
        exportByName[exp.name].push(exp);
    }
    while (offset < view.byteLength) {
        let section: Section | undefined = undefined;
        let absoluteAddr = (offset === 0);
        let nameIndex = view.getUint32(offset + 0, true);
        let addr = view.getUint32(offset + 4, true);
        let size = view.getUint32(offset + 8, true);
        let bind = view.getUint8(offset + 12) >> 4;
        let type = view.getUint8(offset + 12) & 0xF;
        //let other = view.getUint8(offset + 13);
        let shndx = view.getUint16(offset + 14, true);
        offset += 16;
        let name = program.getString(nameIndex);
        if (shndx === 0) {
            // nothing to do here
        } else if (shndx === SHN_ABS) {
            absoluteAddr = true;
        } else if (shndx >= SHN_LORESERVE) {
            warning(`Unknown reserved section index ${shndx}, skipping symbol "${name}".`);
            program.symbols.push(undefined);
            continue;
        } else if (program.sectionByIndex[shndx]) {
            section = program.sectionByIndex[shndx];
        } else {
            throw new Error(`Missing section index ${shndx}.`);
        }
        if (!SymbolBinding[bind]) throw new Error(`Unknown symbol binding ${bind}.`);
        //if (!SymbolType[type]) throw new Error(`Unknown symbol type ${type}.`); - uncomment if actually used
        let symbol: Symbol = {
            name,
            section,
            absoluteAddr,
            addr,
            size,
            binding: bind as SymbolBinding,
            type: type as SymbolType,
        };
        program.symbols.push(symbol);
        if (symbol.binding !== SymbolBinding.LOCAL && symbol.section === undefined && symbol.absoluteAddr === false && program.importByName[symbol.name]) {
            symbol.imported = program.importByName[symbol.name];
        }
        if (symbol.binding !== SymbolBinding.LOCAL && symbol.section !== undefined && exportByName[symbol.name]) {
            for (let exported of exportByName[symbol.name]) {
                if (!exported.symbol) {
                    exported.symbol = symbol;
                } else if (exported.symbol.binding === SymbolBinding.WEAK) {
                    exported.symbol = symbol;
                } else {
                    error(`Multiple global symbols matching exported name "${symbol.name}.`);
                }
            }
        }
    }
}

function checkInterface(program: Program) {
    for (let exported of program.exports) {
        if (exported && !exported.symbol) {
            error(`Undefined export symbol "${exported.name}`);
        }
    }
}

const exportSectionRegExp = /^\.ccvm\.export\.([0-9]+)\.(.+)$/;
const importSectionRegExp = /^\.ccvm\.import\.([0-9]+)\.(.+)$/;

function parseSections(program: Program) {
    let symtab: Section = {
        id: 'empty_symtab', name: '.symtab', index: program.sections.length,
        link: undefined, reloc: undefined, prev: undefined,
        size: 0, data: new Uint8Array(),
        addr: 0, entsize: 16, flags: 0, info: 0, type: 2,
    };
    let strtab: Section = {
        id: 'empty_symtab', name: '.strtab', index: program.sections.length + 1,
        link: undefined, reloc: undefined, prev: undefined,
        size: 0, data: new Uint8Array(),
        addr: 0, entsize: 0, flags: 0, info: 0, type: 3,
    };
    let m: RegExpMatchArray | null;
    for (let section of program.sections) {
        if (section.name === '.symtab') {
            symtab = section;
        } else if (section.name === '.strtab') {
            strtab = section;
        } else if ((m = section.name.match(exportSectionRegExp))) {
            let index = parseInt(m[1]);
            let name = m[2];
            if (program.exports[index] && program.exports[index].name !== name) {
                error(`Multiple exported symbols for index ${index}, "${name}" and "${program.exports[index].name}".`);
            }
            program.exports[index] = { index, name, symbol: undefined };
        } else if ((m = section.name.match(importSectionRegExp))) {
            let index = parseInt(m[1]);
            let name = m[2];
            if (program.imports[index] && program.imports[index].name !== name) {
                error(`Multiple imported symbols for index ${index}, "${name}" and "${program.imports[index].name}".`);
            }
            if (program.importByName[name] && program.importByName[name].index !== index) {
                error(`Multiple indexes for symbol "${name}", ${index} and ${program.importByName[name].index}.`);
            }
            program.imports[index] = { index, name };
            program.importByName[name] = program.imports[index];
        }
    }
    program.getString = parseStrtab(strtab);
    parseSymtab(program, symtab);
    checkInterface(program);
    console.log(program);
}


function readCompiledFile(fileName: string) {
    let decoder = new TextDecoder('ascii');
    let file = fs.readFileSync(fileName) as Uint8Array;
    let view = new DataView(file.buffer, file.byteOffset, file.byteLength);
    let offset = 0;
    let sections: Section[] = [];
    while (offset < file.length) {

        if (offset + 64 > file.length) throw new Error('Corrupted input file.');
        let id = view.getBigUint64(offset + 0, true);      // uint64_t id;
        let link = view.getBigUint64(offset + 8, true);    // uint64_t link;
        let reloc = view.getBigUint64(offset + 16, true);  // uint64_t reloc;
        let prev = view.getBigUint64(offset + 24, true);   // uint64_t prev;
        let size = view.getUint32(offset + 32, true);      // uint32_t size;
        let data_size = view.getUint32(offset + 36, true); // uint32_t data_size;
        let addr = view.getUint32(offset + 40, true);      // uint32_t addr;
        let entsize = view.getUint32(offset + 44, true);   // uint32_t entsize;
        let flags = view.getUint32(offset + 48, true);     // uint32_t flags;
        let info = view.getUint32(offset + 52, true);      // uint32_t info;
        let type = view.getUint32(offset + 56, true);      // uint32_t type;
        let name_len = view.getUint32(offset + 60, true);  // uint32_t name_len;
        let index = view.getUint32(offset + 64, true);     // uint32_t index;
        // uint32_t _reserved, 68
        offset += 72;

        if (id === 0n) {
            if (offset !== file.length) throw new Error('Corrupted input file.');
            break;
        }

        if (offset + name_len > file.length) throw new Error('Corrupted input file.');
        let name = decoder.decode(file.slice(offset, offset + name_len));
        offset += name_len;

        if (offset + data_size > file.length) throw new Error('Corrupted input file.');
        let data = file.slice(offset, offset + data_size);
        offset += data_size;

        sections.push({
            id: id.toString(16).padStart(16, '0'),
            index,
            name,
            link: link !== 0n ? link.toString(16).padStart(16, '0') : undefined,
            reloc: reloc !== 0n ? reloc.toString(16).padStart(16, '0') : undefined,
            prev: prev !== 0n ? prev.toString(16).padStart(16, '0') : undefined,
            size,
            data,
            addr,
            entsize,
            flags,
            info,
            type,
        });
    }
    let sectionById: Dict<Section> = Object.create(null);
    let sectionByIndex: Section[] = [];
    for (let section of sections) {
        if (sectionById[section.id]) throw new Error(`Repeating section id ${section.id}`);
        sectionById[section.id] = section;
        if (sectionByIndex[section.index]) throw new Error(`Repeating section index ${section.index}`);
        sectionByIndex[section.index] = section;
    }
    let result: Program = {
        sections,
        sectionById,
        sectionByIndex,
        getString: () => '',
        symbols: [],
        exports: [],
        imports: [],
        importByName: Object.create(null),
    };
    return result;
}

let p = readCompiledFile('../bin/sample.bin');
parseSections(p);

