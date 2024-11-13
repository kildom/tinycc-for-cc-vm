
// TODO: Symbol-based approach
/*
 * 1. Extract symbol contents from sections
 *    * Symbol must be self-contained, e.g. cannot reference address from the same section,
 *      but different symbol. Relocation must be used instead.
 *    * Data content and relocations are extracted to the symbol.
 *    * Symbol type is determined based on ".text" prefix
 * 2. Add predefined symbols
 * 3. Generate export table
 * 4. Make symbol that covers entire "export", "init", and "fini" section.
 * 5. Add IR label object for each symbol.
 * 6. Translate each symbol to IR (IR never references a label by name, but it holds
 *    a reference to label object).
 * 7. Generate import wrappers. Direct calls to imported functions should be converted into
 *    HOST instruction, so most wrappers will be removed as unused.
 * 8. Mark symbols that belong to specific sections as used: registers, entry, export, init, fini.
 *    (todo: check if "used" attribute can be used?)
 * 9. Mark all referenced symbols as used.
 * 10. Remove unused symbols.
 * 12. Sort symbols by: 1) output section location, 2) input section name, 3) address
 * 13. Combine IR from all symbols
 * 14. Compile IR into bytecode
 * 
 * Maximum size of all symbols from ".ccvm.heap" is taken as a output heap size. All ".ccvm.heap"
 * will point to the beginning of the heap.
 * The same for ".ccvm.stack", but actual stack area end will be at the end of data memory making
 * it bigger than declared (but never smaller).
 *
 * import_wrapper:
 *   .REF entry_preserve_R3_1 # Referencing those labels will enable R3 preserving in entry.
 *   .REF entry_preserve_R3_2 # If all import_wrappers are removed preserving will be also removed.
 *   POP R3    # Pop return address, so host will not see it
 *   HOST ???  # Execute host code, if host calls vm again, entry function should push R3
 *   PUSH R3
 *   RETURN
 *    // TODO: if callee is responsible for parameter cleanup, should the wrappers always be present?
 *    // TODO: SP must be aligned to 32-bits when calling host, failing to do so causes an error
 *             and host function will not be called.
 * 
 * entry_handler: # section .text.ccvm.entry_handler.0
 *   .REF entry_handler_continue1
 *   .REF entry_handler_continue2
 *   .REF entry_handler_continue3
 * entry_preserve_R3_1: # section .text.ccvm.entry_handler.1
 *   PUSH R3
 * entry_handler_continue2: # section .text.ccvm.entry_handler.2
 *   MUL R0, 4  # or 2 for small model
 *   MOV R3, __ccvm_export_table_begin
 *   ADD R3, R0
 *   READ32 R3, [R3]  # or READ16U for small model
 *   CALL R3
 * entry_preserve_R3_2: # section .text.ccvm.entry_handler.3
 *   POP R3
 * entry_handler_continue3: # section .text.ccvm.entry_handler.4
 *   HOST -1 # Return to host
 * 
 * Getting pointers to functions should result in address relative to beginning of program memory
 * (all with ".text" prefix), but both formats are allowed: 0x4aaaaaaa and 0x0aaaaaaa.
 * 
 */

/*
import * as fs from 'node:fs';
import * as util from 'node:util';

enum RelocationType {
    DATA = 1,
    INSTR = 2,
};

interface Relocation {
    addr: number;
    type: RelocationType;
    symbol: Symbol | undefined;
}

enum IROpcode {
    INSTR_MOV_REG,          // dstReg = srcReg
    INSTR_MOV_CONST,        // reg = value
    INSTR_LABEL_RELATIVE,   // label, address_offset
    INSTR_LABEL_ABSOLUTE,   // label, address_offset
    INSTR_WRITE_CONST,      // reg => [value]
    INSTR_READ_CONST,       // reg <= [value]
    INSTR_WRITE_REG,        // reg => [addrReg]
    INSTR_READ_REG,         // reg <= [addrReg]
    INSTR_JUMP_COND_LABEL,  // label, op2 = condition
    INSTR_JUMP_CONST,       // address
    INSTR_CALL_CONST,       // address
    INSTR_JUMP_LABEL,       // label
    INSTR_JUMP_REG,         // reg
    INSTR_CALL_REG,         // reg
    INSTR_PUSH,             // reg, op2 = 1..4 bytes
    INSTR_PUSH_BLOCK_CONST, // reg, value = block size
    INSTR_PUSH_BLOCK_LABEL, // reg, label = label containing block size
    INSTR_CMP,              // srcReg, dstReg, op2 = comparison operator
    INSTR_BIN_OP,           // srcReg, dstReg, op2 = operator
    INSTR_RETURN,           // value = cleanup words
    INSTR_LABEL_ALIAS,      // labelAlias = label

    INSTR_DATA,
    INSTR_WORD,
    INSTR_FILL,
    INSTR_DISPOSABLE_BEGIN,
    INSTR_DISPOSABLE_END,
    INSTR_LABEL,
    INSTR_PROGRAM,
};

interface CompileContext {

}

type ValueFunction = (ctx: CompileContext) => number;

interface IRInstructionBase {
    opcode: IROpcode;
    addr?: number; // TODO: Clean it up
};

interface IREmptyInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_DISPOSABLE_BEGIN | IROpcode.INSTR_DISPOSABLE_END | IROpcode.INSTR_PROGRAM;
}
interface IRWithValueInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_WORD;
    value: ValueFunction;
}

interface IRNamedInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_LABEL;
    name: string;
}

interface IRDataInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_DATA;
    data: Uint8Array;
};

interface IRFillInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_FILL;
    size: number;
};

type IRInstruction = IREmptyInstruction | IRWithValueInstruction | IRNamedInstruction | IRDataInstruction | IRFillInstruction;

export interface Section {
    id: string;
    index: number;
    name: string;
    link: string | undefined;
    reloc: string | undefined;
    prev: string | undefined;
    size: number;
    data: Uint8Array | undefined;
    addr: number;
    entsize: number;
    flags: number;
    info: number;
    type: number;
    symbols: Symbol[];
    relocations: Relocation[];
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
    index: number;
    outputName: string;
    section: Section | undefined;
    defined: boolean;
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
    predefinedSymbols: Dict<boolean>;
    outputSections: {
        registers: Section[];
        data: Section[];
        bss: Section[];
        heap: Section[];
        entry: Section[];
        rodata: Section[];
        exportTable: Section[];
        initTable: Section[];
        finiTable: Section[];
        text: Section[];
        removed: Section[];
    }
    instructions: IRInstruction[];
};

const predefinedSymbols: Dict<boolean> = {
    __ccvm_section_ccvm_registers_begin: false,
    __ccvm_section_ccvm_registers_end: false,
    __ccvm_section_data_begin: false,
    __ccvm_section_data_end: false,
    __ccvm_section_bss_begin: false,
    __ccvm_section_bss_end: false,
    __ccvm_section_ccvm_heap_begin: false,
    __ccvm_section_ccvm_heap_end: false,
    __ccvm_section_ccvm_entry_begin: false,
    __ccvm_section_ccvm_entry_end: false,
    __ccvm_section_rodata_begin: false,
    __ccvm_section_rodata_end: false,
    __ccvm_section_ccvm_export_table_begin: false,
    __ccvm_section_ccvm_export_table_end: false,
    __ccvm_section_init_array_begin: false,
    __ccvm_section_init_array_end: false,
    __ccvm_section_fini_array_begin: false,
    __ccvm_section_fini_array_end: false,
    __ccvm_section_text_begin: false,
    __ccvm_section_text_end: false,
    __ccvm_section_data_load_begin: false,
    __ccvm_section_data_load_end: false,
    __ccvm_data_memory_end: false,
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
    program.symbols.splice(0);
    if (!symtab.data) return;
    let view = new DataView(symtab.data.buffer, symtab.data.byteOffset, symtab.data.byteLength);
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
            index: program.symbols.length,
            outputName: `${name}@${section?.id}_${program.symbols.length}`,
            section,
            defined: !!(section || absoluteAddr),
            addr,
            size,
            binding: bind as SymbolBinding,
            type: type as SymbolType,
        };
        program.symbols.push(symbol);
        section?.symbols.push(symbol);
        if (symbol.binding !== SymbolBinding.LOCAL && symbol.section === undefined && !symbol.defined) {
            if (program.importByName[symbol.name]) {
                symbol.imported = program.importByName[symbol.name];
                symbol.defined = true;
                symbol.outputName = `${symbol.imported.name}@imported_${symbol.imported.index}`;
            }
            if (program.predefinedSymbols[symbol.name] !== undefined) {
                symbol.defined = true;
                symbol.outputName = symbol.name;
                program.predefinedSymbols[symbol.name] = true;
            }
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

function findRelocation(program: Program, section: Section) {
    if (section.reloc) {
        if (!program.sectionById[section.reloc]) throw new Error(`Missing section ${section.reloc} which is relocation section for "${section.name}".`);
        return program.sectionById[section.reloc];
    } else {
        let name = '.rel' + section.name;
        return program.sections.filter(sec => sec.name === name)[0];
    }
}

function parseRelocations(program: Program) {
    for (let section of program.sections) {
        let rel = findRelocation(program, section);
        if (!rel || !rel.data) continue;
        let view = new DataView(rel.data.buffer, rel.data.byteOffset, rel.data.byteLength);
        let offset = 0;
        while (offset < view.byteLength) {
            let addr = view.getUint32(offset + 0, true);
            let info = view.getUint32(offset + 4, true);
            let symbolIndex = info >> 8;
            let type = info & 0xFF;
            if (!RelocationType[type]) throw new Error(`Unknown relocation type: ${type}.`);
            offset += 8;
            section.relocations[addr] = {
                addr,
                type,
                symbol: program.symbols[symbolIndex],
            };
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
        symbols: [], relocations: [],
    };
    let strtab: Section = {
        id: 'empty_symtab', name: '.strtab', index: program.sections.length + 1,
        link: undefined, reloc: undefined, prev: undefined,
        size: 0, data: new Uint8Array(),
        addr: 0, entsize: 0, flags: 0, info: 0, type: 3,
        symbols: [], relocations: [],
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
    parseRelocations(program);
    createExportTable(program);
    assignSections(program);
    parseSectionsData(program, program.outputSections.registers, false);
    parseSectionsData(program, program.outputSections.data, true);
    parseSectionsData(program, program.outputSections.bss, true);
    parseSectionsData(program, program.outputSections.heap, true);
    program.instructions.push({ opcode: IROpcode.INSTR_PROGRAM });
    console.log('PROGRAM MEMORY');
    parseSectionsData(program, program.outputSections.entry, false);
    parseSectionsData(program, program.outputSections.rodata, true);
    parseSectionsData(program, program.outputSections.exportTable, false);
    parseSectionsData(program, program.outputSections.initTable, false);
    parseSectionsData(program, program.outputSections.finiTable, false);
    //program.outputSections.text.forEach(sec => parseSectionCode(program, sec));
}

function createVarExp(variableName: string) {
    return () => 0; // TODO;
}

function createAddExp(a: ValueFunction, b: ValueFunction) {
    return (ctx: CompileContext) => (a(ctx) + b(ctx)) & 0xFFFFFFFF;
}

function parseSectionsData(program: Program, sections: Section[], allowDisposable: boolean) {
    for (let section of sections) {
        parseSectionData(program, section, allowDisposable);
    }
}

function parseSectionData(program: Program, section: Section, allowDisposable: boolean) {
    let output = program.instructions;
    let text = {
        push(text: string) {
            console.log(text);
        }
    }
    let symbolByAddr: Symbol[] = [];
    let boundaries = new Set<number>();
    section.relocations.forEach(rel => {
        boundaries.add(rel.addr);
        boundaries.add(rel.addr + 4);
    });
    section.symbols.forEach(symbol => {
        boundaries.add(symbol.addr);
        boundaries.add(symbol.addr + symbol.size);
        symbolByAddr[symbol.addr] = symbol;
    });
    boundaries.add(section.size);
    boundaries.delete(0);
    let begin = 0;
    let boundariesArr = [...boundaries];
    boundariesArr.sort((a, b) => a - b);
    let disposableEndings: number[] = [];
    for (let i = 0; i < boundariesArr.length; i++) {
        let end = boundariesArr[i];

        // Add symbol information
        let symbol = symbolByAddr[begin];
        if (symbol) {
            if (symbol.size > 0 && allowDisposable) {
                output.push({
                    opcode: IROpcode.INSTR_DISPOSABLE_BEGIN,
                });
                text.push(`BEGIN DISPOSABLE`);
                let symbolEnd = begin + symbol.size;
                disposableEndings[symbolEnd] = (disposableEndings[symbolEnd] ?? 0) + 1;
            }
            output.push({
                opcode: IROpcode.INSTR_LABEL,
                name: symbol.outputName,
            });
            text.push(`${symbol.outputName}:`);
        }

        let rel = section.relocations[begin];

        if (rel) {
            if (end - begin != 4) {
                //console.log(boundariesArr);
                //console.log(section.relocations);
                throw new Error('Overlapping relocation and symbol boundaries or other relocation.');
            }
            if (rel.type !== RelocationType.DATA) {
                throw new Error(`Section "${section.name}" allows only "DATA" relocation, found "${RelocationType[rel.type]}".`);
            }
            let add = 0;
            let buf = section.data?.slice(begin, end);
            if (buf) {
                add = new DataView(buf.buffer, buf.byteOffset, buf.byteLength).getUint32(0, true);
            }
            output.push({
                opcode: IROpcode.INSTR_WORD,
                value: rel.symbol ? createAddExp(createVarExp(rel.symbol.outputName), () => add) : () => 0,
            });
            text.push(`WORD ${rel.symbol?.outputName ?? 0} + ${add}`);
        } else if (section.data) {
            output.push({
                opcode: IROpcode.INSTR_DATA,
                data: section.data.slice(begin, end),
            });
            text.push(`DATA ${[...section.data.slice(begin, end)]}`);
        } else {
            output.push({
                opcode: IROpcode.INSTR_FILL,
                size: end - begin,
            });
            text.push(`FILL ${end - begin}`);
        }

        if (disposableEndings[end]) {
            for (let i = 0; i < disposableEndings[end]; i++) {
                output.push({
                    opcode: IROpcode.INSTR_DISPOSABLE_END,
                });
                text.push(`END DISPOSABLE`);
            }
        }
        begin = end;
    }
}

function createExportTable(program: Program) {
    let relocations: Relocation[] = [];
    for (let i = 0; i < program.exports.length; i++) {
        let exported = program.exports[i];
        if (!exported) {
            // Skip
        } else if (!exported.symbol) {
            error(`Undefined symbol "${exported.name}" for exported index ${i}.`);
        } else {
            relocations[4 * i] = {
                addr: 4 * i,
                type: RelocationType.DATA,
                symbol: exported.symbol,
            };
        }
    }
    let section: Section = {
        id: 'generated_export_table',
        index: program.sections.length + 2,
        name: '.ccvm.export.table',
        link: undefined,
        reloc: undefined,
        prev: undefined,
        size: 4 * program.exports.length,
        data: undefined,
        addr: 0,
        entsize: 0,
        flags: 0,
        info: 0,
        type: 1,
        symbols: [],
        relocations,
    };
    program.sections.push(section);
    program.sectionById[section.id] = section;
    program.sectionByIndex[section.index] = section;
}


const outputSectionsRexExp: { [key in keyof Program['outputSections']]: RegExp } = {
    registers: /^\.ccvm\.registers(\..*)?$/,
    bss: /^\.(bss|common)(\..*)?$/,
    heap: /^\.ccvm\.heap(\..*)?$/,
    entry: /^\.ccvm\.entry(\..*)?$/,
    rodata: /^\.(rodata(\..*)?|data(\..*)?\.ro)$/,
    data: /^\.data(\..*)?$/,
    exportTable: /^\.ccvm\.export\.table$/,
    initTable: /^\.init_array(\..*)?$/,
    finiTable: /^\.fini_array(\..*)?$/,
    text: /^\.text(\..*)?$/,
    removed: /^.*$/,
};


function assignSections(program: Program) {
    for (let section of program.sections) {
        for (let [bucketName, re] of Object.entries(outputSectionsRexExp) as [keyof typeof outputSectionsRexExp, RegExp][]) {
            if (re.test(section.name)) {
                program.outputSections[bucketName].push(section);
                break;
            }
        }
    }
    for (let bucket of Object.values(program.outputSections)) {
        bucket.sort((a, b) => {
            let aa = a.name.split('.');
            let bb = b.name.split('.');
            for (let i = 0; i < Math.max(aa.length, bb.length); i++) {
                if (aa[i] === undefined) return -1;
                if (bb[i] === undefined) return 1;
                let ia = parseInt(aa[i]);
                let ib = parseInt(bb[i]);
                if (ia.toString() === aa[i] && ib.toString() === bb[i]) {
                    if (ia !== ib) return ia - ib;
                } else {
                    let r = aa[i].localeCompare(bb[i], 'en-US');
                    if (r < 0) return -1;
                    if (r > 0) return 1;
                }
            }
            return 0;
        });
    }
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
        let data: Uint8Array | undefined;
        if (data_size !== size) {
            data = undefined;
        } else {
            data = file.slice(offset, offset + data_size);
        }
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
            symbols: [],
            relocations: [],
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
        predefinedSymbols: {...predefinedSymbols},
        outputSections: {
            registers: [],
            data: [],
            bss: [],
            heap: [],
            entry: [],
            rodata: [],
            exportTable: [],
            initTable: [],
            finiTable: [],
            text: [],
            removed: [],
        },
        instructions: [],
    };
    return result;
}


function refs(a: any, m: Map<any, any>) {
    if (typeof a === 'object' && !(a.buffer instanceof ArrayBuffer)) {
        m.set(a, `*** REF ${m.size} ***`);
        if (Array.isArray(a)) {
            return a.map((value) => {
                if (m.has(value)) return m.get(value);
                return refs(value, m);
            });
        } else {
            return {
                '_': m.get(a),
                ...Object.fromEntries(Object.entries(a)
                    .map(([key, value]) => {
                        if (m.has(value)) return [key, m.get(value)];
                        else return [key, refs(value, m)];
                    }))
            };
        }
    }
    return a;
}

let p = readCompiledFile('../bin/sample.bin');
parseSections(p);

console.log(util.inspect(refs(p, new Map()), false, null, true));
*/
