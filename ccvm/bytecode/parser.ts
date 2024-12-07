

import * as fs from 'node:fs';
import { AbsoluteSymbol, collectAliasedLabels, DataSymbol, ExportEntry, FunctionInnerSymbol, FunctionSymbol, ImportSymbol, InnerSymbol, InvalidSymbol, IRInstruction, IROpcode, Label, PredefinedSymbols, predefinedSymbols, Relocation, RelocationType, RWOpcodeFlags, Section, SymbolBase, UndefinedSymbol, ValueFunction, WithIRSymbol } from './ir';
import { Dict, error, newDict, replaceObjectContent, warning } from './utils';
import { dumpIRFromSymbols } from './dump';

const SHN_LORESERVE = 0xff00;
const SHN_ABS = 0xfff1;
const RELOCATION_MAX_SIZE = 12;

const exportSectionRegExp = /^\.ccvm\.export\.([0-9]+)\.(.+)$/;
const importSectionRegExp = /^\.ccvm\.import\.([0-9]+)\.(.+)$/;
const initFiniSectionRegExp = /^\.(init|fini)_array(\..+)?$/;

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

function getRelocationSize(type: RelocationType): number {
    switch (type) {
        case RelocationType.DATA:
            return 4;
        case RelocationType.INSTR:
            return 12;
    }
}

enum SymbolKind {
    DATA,
    FUNCTION,
    SKIPPED,
}

function getSymbolKind(section: Section): SymbolKind {
    if (section.name.startsWith('.text.') || section.name === '.text') {
        return SymbolKind.FUNCTION;
    } else if (section.name.match(exportSectionRegExp) || section.name.match(importSectionRegExp)) {
        return SymbolKind.SKIPPED;
    } else {
        return SymbolKind.DATA;
    }
}

export class Parser {

    private sections!: Section[];
    private sectionById!: Dict<Section>;
    private sectionByIndex!: Section[];
    private symtab!: Section;
    private strtab!: Section;
    private initFiniSections: Section[];
    private exports!: ExportEntry[];
    private imports!: ImportSymbol[];
    private importByName!: Dict<ImportSymbol>;
    private strings!: string;
    private symbols!: SymbolBase[];
    private predefinedSymbols!: PredefinedSymbols;
    private labels: Label[] = [];
    private customSectionIndex: number | undefined;

    public parse(fileName: string): { symbols: SymbolBase[], predefinedSymbols: PredefinedSymbols, exports: ExportEntry[] } {

        this.sections = [];
        this.sectionById = newDict();
        this.sectionByIndex = [];
        this.exports = [];
        this.imports = [];
        this.importByName = newDict();
        this.strings = '\0';
        this.symbols = [];
        this.customSectionIndex = undefined;

        this.createPredefinedSymbols();
        this.readSections(fileName);
        this.findSpecialSections();
        this.parseStrtab();
        this.parseSymtab();
        this.createInitFiniSymbols();
        this.findInnerSymbols();
        this.generateIR();
        //dumpIRFromSymbols(this.symbols);
        this.resolveLabels();
        //dumpIRFromSymbols(this.symbols);
        return {
            symbols: this.symbols,
            predefinedSymbols: this.predefinedSymbols,
            exports: this.exports,
        };
    }

    private createPredefinedSymbols() {
        let section = this.createSection({ name: '$predefined_symbols_section' });
        this.predefinedSymbols = {} as any;
        for (let name in predefinedSymbols) {
            this.predefinedSymbols[name] = new DataSymbol(name, section, 0, 0);
        }
    }

    private findInnerSymbols() {
        let symbolsBySection = newDict<WithIRSymbol[]>();
        let replacements = new Map<SymbolBase, SymbolBase>();
        for (let symbol of this.symbols) {
            if (symbol instanceof WithIRSymbol) {
                symbolsBySection[symbol.section.id] = symbolsBySection[symbol.section.id] ?? [];
                symbolsBySection[symbol.section.id].push(symbol);
            }
        }
        for (let symbols of Object.values(symbolsBySection)) {
            let currentSymbol: WithIRSymbol | undefined = undefined;
            symbols.sort((a, b) => {
                if (a.addr === b.addr) return b.size - a.size;
                return a.addr - b.addr;
            });
            for (let symbol of symbols) {
                if (!currentSymbol) {
                    currentSymbol = symbol;
                } else if (symbol.addr >= currentSymbol.addr + currentSymbol.size) {
                    currentSymbol = symbol;
                } else if (symbol.addr + symbol.size > currentSymbol.addr + currentSymbol.size) {
                    throw new Error(`Overlapping top-level symbols "${symbol.name}" and "${currentSymbol.name}"`);
                } else {
                    let cls = (symbol instanceof DataSymbol) ? InnerSymbol : FunctionInnerSymbol;
                    let innerSymbol = new cls(symbol.name, currentSymbol, symbol.addr - currentSymbol.addr);
                    currentSymbol.innerSymbols = currentSymbol.innerSymbols ?? [];
                    currentSymbol.innerSymbols.push(innerSymbol);
                    replacements.set(symbol, innerSymbol);
                }
            }
        }
        for (let i = 0; i < this.symbols.length; i++) {
            if (replacements.has(this.symbols[i])) {
                this.symbols[i] = replacements.get(this.symbols[i])!;
            }
        }
    }

    private createInitFiniSymbols() {
        for (let section of this.initFiniSections) {
            let symbol = new DataSymbol(`_ccvm_init_fini_auto_${section.name}`, section, 0, section.size);
            symbol.indexes.push(this.symbols.length);
            this.symbols.push(symbol);
        }
    }

    private resolveLabels() {
        for (let symbol of this.symbols) {
            if (symbol instanceof WithIRSymbol && symbol.ir) {
                this.resolveLabelsInIR(symbol.ir);
            }
        }
    }

    private resolveLabelsInIR(ir: IRInstruction[]) {
        for (let instr of ir) {

            switch (instr.opcode) {

                case IROpcode.INSTR_JUMP_LABEL:
                    instr.label = this.resolveLabel(ir, instr.label);
                    if (instr.label.instruction === undefined) throw new Error('Absolute label address not allowed in JUMP instructions');
                    replaceObjectContent<IRInstruction>(instr, {
                        opcode: IROpcode.INSTR_JUMP_INSTR,
                        instruction: instr.label.instruction,
                    });
                    break;

                case IROpcode.INSTR_JUMP_COND_LABEL:
                    instr.label = this.resolveLabel(ir, instr.label);
                    if (instr.label.instruction === undefined) throw new Error('Absolute label address not allowed in JUMP instructions');
                    replaceObjectContent<IRInstruction>(instr, {
                        opcode: IROpcode.INSTR_JUMP_COND_INSTR,
                        instruction: instr.label.instruction,
                        condition: instr.condition,
                    });
                    break;

                case IROpcode.INSTR_PUSH_BLOCK_LABEL:
                    instr.label = this.resolveLabel(ir, instr.label);
                    if (instr.label.absoluteValue === undefined) throw new Error('Relative label address not allowed in PUSH_BLOCK instruction');
                    if (instr.optional && instr.label.absoluteValue === 0) {
                        replaceObjectContent<IRInstruction>(instr, {
                            opcode: IROpcode.INSTR_EMPTY
                        });
                    } else {
                        replaceObjectContent<IRInstruction>(instr, {
                            opcode: IROpcode.INSTR_PUSH_BLOCK_CONST,
                            optional: instr.optional,
                            reg: instr.reg,
                            value: new ValueFunction(instr.label.absoluteValue),
                        });
                    }
                    break;

                case IROpcode.INSTR_LABEL_RELATIVE:
                case IROpcode.INSTR_LABEL_ABSOLUTE:
                case IROpcode.INSTR_LABEL_ALIAS:
                    replaceObjectContent<IRInstruction>(instr, {
                        opcode: IROpcode.INSTR_EMPTY,
                    });
                    break;
            }
        }
    }

    private resolveLabel(ir: IRInstruction[], label: Label): Label {
        let all = collectAliasedLabels(label);
        let definitions = all.filter(label =>
            label.instruction !== undefined
            || label.instructionOffset !== undefined
            || label.absoluteValue !== undefined
        );
        let main = definitions[0];
        if (definitions.length < 1) {
            throw new Error('Undefined label.');
        } else if (definitions.length > 1) {
            throw new Error('Multiple definitions of the same label.');
        } else if (main.absoluteValue !== undefined || main.instruction !== undefined) {
            return main;
        } else if (main.instructionOffset! < 0 || main.instructionOffset! >= ir.length) {
            throw new Error('Invalid label instruction offset.');
        } else {
            main.instruction = ir[main.instructionOffset!];
            delete main.instructionOffset;
            return main;
        }
    }

    private createSection(options: Partial<Section>): Section {
        if (this.customSectionIndex === undefined) {
            this.customSectionIndex = this.sections.length;
        }
        let index = this.customSectionIndex;
        this.customSectionIndex++;
        return {
            id: `custom_section_${index}`, name: `.__ccvm_custom_section_${index}`, index,
            link: undefined, reloc: undefined, prev: undefined,
            size: 0, data: new Uint8Array(),
            addr: 0, entsize: 0, flags: 0, info: 0, type: 1,
            known: true,
            ...options,
        };
    }

    private findSpecialSections() {

        this.symtab = this.createSection({ name: '.symtab', entsize: 16, type: 2 });
        this.strtab = this.createSection({ name: '.strtab', entsize: 0, type: 3, size: 1, data: new Uint8Array([0]) });

        this.initFiniSections = [];
        let m: RegExpMatchArray | null;
        for (let section of this.sections) {
            if (section.name === '.symtab') {
                section.known = true;
                this.symtab = section;
            } else if (section.name === '.strtab') {
                section.known = true;
                this.strtab = section;
            } else if ((m = section.name.match(exportSectionRegExp))) {
                section.known = true;
                let index = parseInt(m[1]);
                let name = m[2];
                if (this.exports[index] && this.exports[index].name !== name) {
                    error(`Multiple exported symbols for index ${index}, "${name}" and "${this.exports[index].name}".`);
                }
                this.exports[index] = { index, name, symbol: undefined };
            } else if ((m = section.name.match(importSectionRegExp))) {
                section.known = true;
                let index = parseInt(m[1]);
                let name = m[2];
                if (this.imports[index] && this.imports[index].name !== name) {
                    error(`Multiple imported symbols for index ${index}, "${name}" and "${this.imports[index].name}".`);
                }
                if (this.importByName[name] && this.importByName[name].importIndex !== index) {
                    error(`Multiple indexes for symbol "${name}", ${index} and ${this.importByName[name].importIndex}.`);
                }
                let symbol = new ImportSymbol(name, index, section);
                this.imports[index] = symbol;
                this.importByName[name] = symbol;
            } else if (section.name.match(initFiniSectionRegExp)) {
                this.initFiniSections.push(section);
            }
        }
    }

    private getString(offset: number): string {
        if (offset < 0 || offset >= this.strings.length) throw new Error('Invalid strtab reference.');
        let end = this.strings.indexOf('\0', offset);
        if (end < 0) throw new Error('Invalid strtab.');
        return this.strings.substring(offset, end);
    }

    private readSections(fileName: string): void {

        let decoder = new TextDecoder('ascii');
        let file = fs.readFileSync(fileName) as Uint8Array;
        let view = new DataView(file.buffer, file.byteOffset, file.byteLength);
        let offset = 0;

        while (offset < file.length) {

            if (offset + 72 > file.length) throw new Error('Corrupted input file.');
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

            this.sections.push({
                id: id.toString(16).padStart(16, '0'),
                index, name,
                link: link !== 0n ? link.toString(16).padStart(16, '0') : undefined,
                reloc: reloc !== 0n ? reloc.toString(16).padStart(16, '0') : undefined,
                prev: prev !== 0n ? prev.toString(16).padStart(16, '0') : undefined,
                size, data, addr, entsize, flags, info, type,
                known: false,
            });
        }

        for (let section of this.sections) {
            if (this.sectionById[section.id]) throw new Error(`Repeating section id ${section.id}`);
            this.sectionById[section.id] = section;
            if (this.sectionByIndex[section.index]) throw new Error(`Repeating section index ${section.index}`);
            this.sectionByIndex[section.index] = section;
        }
    }

    private parseStrtab() {
        this.strings = new TextDecoder('ascii').decode(this.strtab.data);
    }

    private parseSymtab() {

        if (!this.symtab.data) throw new Error("No symbols");

        let exportByName = newDict<ExportEntry[]>();
        for (let exp of this.exports) {
            if (!exp) continue;
            exportByName[exp.name] = exportByName[exp.name] ?? [];
            exportByName[exp.name].push(exp);
        }

        let view = new DataView(this.symtab.data.buffer, this.symtab.data.byteOffset, this.symtab.data.byteLength);
        let offset = 0;

        while (offset < view.byteLength) {

            let nameIndex = view.getUint32(offset + 0, true);
            let addr = view.getUint32(offset + 4, true);
            let size = view.getUint32(offset + 8, true);
            let bind = view.getUint8(offset + 12) >> 4;
            //let type = view.getUint8(offset + 12) & 0xF;
            //let other = view.getUint8(offset + 13);
            let shndx = view.getUint16(offset + 14, true);
            offset += 16;
            let name = this.getString(nameIndex);

            let symbol: SymbolBase;

            if (shndx === 0) {

                // Symbol is undefined
                if (this.importByName[name]) {
                    symbol = this.importByName[name];
                } else if (this.predefinedSymbols[name] !== undefined) {
                    symbol = this.predefinedSymbols[name];
                } else {
                    symbol = new UndefinedSymbol(name);
                }

            } else if (shndx === SHN_ABS || offset === 0) {

                // Absolute reference symbol
                symbol = new AbsoluteSymbol(name, addr);

            } else if (shndx >= SHN_LORESERVE) {

                // Unknown (reserved) symbol
                warning(`Unknown reserved section index ${shndx}, skipping symbol "${name}".`);
                symbol = new InvalidSymbol(name);

            } else if (this.sectionByIndex[shndx]) {

                // Normal symbol that refers to a section
                let section = this.sectionByIndex[shndx];
                switch (getSymbolKind(section)) {
                    case SymbolKind.DATA:
                        symbol = new DataSymbol(name, section, addr, size);
                        break;
                    case SymbolKind.FUNCTION:
                        symbol = new FunctionSymbol(name, section, addr, size);
                        break;
                    default:
                        symbol = new UndefinedSymbol(name);
                        break;
                }

            } else {

                // Invalid section reference
                throw new Error(`Missing section index ${shndx}.`);

            }

            if (bind !== SymbolBinding.LOCAL && exportByName[name] && !(symbol instanceof UndefinedSymbol)) {
                for (let exported of exportByName[name]) {
                    if (!exported.symbol) {
                        exported.symbol = symbol;
                    } else if (bind === SymbolBinding.WEAK) {
                        exported.symbol = symbol;
                    } else {
                        error(`Multiple global symbols matching exported name "${name}.`);
                    }
                }
            }

            symbol.indexes.push(this.symbols.length);
            this.symbols.push(symbol);
        }
    }

    private getRelocationsInRange(section: Section, addr: number, size: number): Relocation[] {
        let all = this.getRelocations(section);
        for (let i = addr - RELOCATION_MAX_SIZE + 1; i < addr; i++) {
            if (all[i] !== undefined && i + getRelocationSize(all[i].type) > addr)
                throw new Error(`Relocation spans over symbol boundary at ${addr}, size ${size}`);
        }
        for (let i = addr + size - RELOCATION_MAX_SIZE + 1; i < addr + size; i++) {
            if (all[i] !== undefined && i + getRelocationSize(all[i].type) > addr + size) {
                console.log(`Symbol at ${addr}, size ${size}, relocation ${i}, size ${getRelocationSize(all[i].type)}`);
                throw new Error(`Relocation spans over symbol boundary at ${addr}, size ${size}`);
            }
        }
        return all.slice(addr, addr + size);
    }

    private getRelocations(section: Section): Relocation[] {

        if (section.relocations === undefined) {
            section.relocations = [];

            // Find relocations section
            let reloc: Section | undefined = undefined;
            if (section.reloc) {
                reloc = this.sectionById[section.reloc];
                if (!reloc) throw new Error('Invalid relocation section index.');
            } else {
                for (let sec of this.sections) {
                    if (sec.name === `rel.${section.name}`) {
                        reloc = sec;
                        break;
                    }
                }
            }

            let view = (reloc && reloc.data)
                ? new DataView(reloc.data.buffer, reloc.data.byteOffset, reloc.data.byteLength)
                : new DataView(new ArrayBuffer(0));
            let offset = 0;
            while (offset < view.byteLength) {
                let addr = view.getUint32(offset + 0, true);
                let info = view.getUint32(offset + 4, true);
                let symbolIndex = info >> 8;
                let type = info & 0xFF;
                if (!RelocationType[type]) throw new Error(`Unknown relocation type: ${type}.`);
                if (!this.symbols[symbolIndex]) throw new Error('Unknown relocation symbol index');
                offset += 8;
                section.relocations[addr] = {
                    type,
                    symbol: this.symbols[symbolIndex],
                };
            }
        }

        return section.relocations;
    }

    generateIR() {
        for (let symbol of this.symbols) {
            if (symbol instanceof DataSymbol) {
                this.generateDataIR(symbol);
            } else if (symbol instanceof FunctionSymbol) {
                this.generateFunctionIR(symbol);
            }
        }
    }

    generateFunctionIR(symbol: FunctionSymbol) {
        let innerByAddr = new Map<number, FunctionInnerSymbol>();
        for (let inner of symbol.innerSymbols ?? []) {
            if (inner.parentOffset < 0 || inner.parentOffset > symbol.size || inner.parentOffset % 12 !== 0) {
                throw new Error(`Invalid inner symbol "${inner.name}" offset for function "${symbol.name}"`);
            }
            innerByAddr.set(inner.parentOffset, inner);
        }
        this.labels.splice(0);
        let ir: IRInstruction[] = [];
        //console.log('>>', symbol.name);
        let relocations = this.getRelocationsInRange(symbol.section, symbol.addr, symbol.size);
        let data = symbol.section.data;
        if (!data) throw new Error(`No bits generated for symbol ${symbol.name}`); // todo: normal error
        let view = new DataView(data.buffer, data.byteOffset, data.byteLength);
        if (data.byteLength % 12 != 0) throw new Error(`Invalid size of function ${symbol.name}`); // todo: normal error
        for (let funcOffset = 0; funcOffset < symbol.size; funcOffset += 12) {
            let relocation: Relocation | undefined = undefined;
            if (relocations[funcOffset]) {
                relocation = relocations[funcOffset];
                if (relocation.type !== RelocationType.INSTR) throw new Error(`Invalid relocation type in function ${symbol.name}`);
            }
            if (relocations[funcOffset + 4]) {
                if (relocation) throw new Error(`Two relocations in the same instruction in function ${symbol.name}`);
                if (relocations[funcOffset + 4].type !== RelocationType.DATA) throw new Error(`Invalid relocation type in function ${symbol.name}`);
                relocation = {
                    type: RelocationType.INSTR,
                    symbol: relocations[funcOffset + 4].symbol,
                };
            }
            for (let i of [/* 0, */ 1, 2, 3, /* 4, */ 5, 6, 7, 8, 9, 10, 11]) {
                if (relocations[funcOffset + i]) throw new Error(`Invalid relocation position in function ${symbol.name}`);
            }
            let instr = this.generateInstruction(symbol, view, funcOffset, relocation);
            ir.push(instr);
            if (innerByAddr.has(funcOffset)) {
                let inner = innerByAddr.get(funcOffset)!;
                inner.instruction = instr;
            }
        }
        symbol.ir = ir;
    }

    generateInstruction(symbol: FunctionSymbol, view: DataView, funcOffset: number, relocation: Relocation | undefined): IRInstruction {
        let secOffset = symbol.addr + funcOffset;
        let opcode = view.getUint8(secOffset);
        let op2 = view.getUint8(secOffset + 1);
        let reg = view.getUint8(secOffset + 2);
        let dstReg = reg;
        let srcReg = view.getUint8(secOffset + 3);
        let addrReg = srcReg;
        let uintValue = view.getUint32(secOffset + 4, true);
        let uintValue2 = view.getUint32(secOffset + 8, true);
        let value: ValueFunction;
        let references: SymbolBase[] | undefined = undefined;

        if (relocation) {
            value = new ValueFunction(uintValue, relocation.symbol);
            references = [relocation.symbol];
        } else {
            value = new ValueFunction(uintValue);
        }

        switch (opcode) {

            case IROpcode.INSTR_MOV_REG:          // dstReg = srcReg
                this.noRelocation(relocation);
                return { opcode, references, dstReg, srcReg };

            case IROpcode.INSTR_MOV_CONST:        // reg = value
                return { opcode, references, reg, value };

            case IROpcode.INSTR_LABEL_RELATIVE: {   // label, address_offset
                this.noRelocation(relocation);
                let label = this.getLabel(uintValue);
                let address = (uintValue2 + funcOffset) & 0xFFFFFFFF;
                if (address % 12 !== 0) throw new Error(`Invalid label address ${address}`);
                if (address < 0) throw new Error('Label offset out of range');
                if (address > symbol.size) throw new Error('Label offset out of range');
                label.instructionOffset = address / 12;
                return { opcode, references, label, value: address };
            }

            case IROpcode.INSTR_LABEL_ABSOLUTE: {   // label, address_offset
                this.noRelocation(relocation);
                let label = this.getLabel(uintValue);
                label.absoluteValue = uintValue2;
                return { opcode, references, label, value: uintValue2 };
            }

            case IROpcode.INSTR_WRITE_CONST:      // reg => [value]
            case IROpcode.INSTR_READ_CONST:       // reg <= [value]
                return { opcode, references, reg, value, op: op2 }

            case IROpcode.INSTR_WRITE_REG:        // reg => [addrReg]
            case IROpcode.INSTR_READ_REG:         // reg <= [addrReg]
                this.noRelocation(relocation);
                return { opcode, references, reg, addrReg };

            case IROpcode.INSTR_JUMP_COND_LABEL:  // label, op2 = condition
                this.noRelocation(relocation);
                return { opcode, references, label: this.getLabel(uintValue), condition: op2 };

            case IROpcode.INSTR_JUMP_CONST:       // address
            case IROpcode.INSTR_CALL_CONST:       // address
                return { opcode, references, value };

            case IROpcode.INSTR_JUMP_LABEL:       // label
                this.noRelocation(relocation);
                return { opcode, references, label: this.getLabel(uintValue) };

            case IROpcode.INSTR_JUMP_REG:         // reg
            case IROpcode.INSTR_CALL_REG:         // reg
                this.noRelocation(relocation);
                return { opcode, references, reg };

            case IROpcode.INSTR_PUSH:             // reg, op2 = 1..4 bytes
                this.noRelocation(relocation);
                return { opcode, references, reg, bytes: op2 as any };

            case IROpcode.INSTR_POP:             // reg, op2 = 1..4 bytes
                this.noRelocation(relocation);
                return { opcode, references, reg, bytes: op2 as any };

            case IROpcode.INSTR_PUSH_BLOCK_CONST: // reg, value = block size
                return { opcode, references, reg, value, optional: op2 !== 0 };

            case IROpcode.INSTR_PUSH_BLOCK_LABEL: // reg, label = label containing block size
                this.noRelocation(relocation);
                return { opcode, references, reg, label: this.getLabel(uintValue), optional: op2 !== 0 };

            case IROpcode.INSTR_CMP:              // srcReg, dstReg, op2 = comparison operator
            case IROpcode.INSTR_BIN_OP:           // srcReg, dstReg, op2 = operator
                this.noRelocation(relocation);
                return { opcode, references, dstReg, srcReg, op: op2 };

            case IROpcode.INSTR_RETURN:           //
                return { opcode, references }

            case IROpcode.INSTR_HOST:           //
                return { opcode, references, value }

            case IROpcode.INSTR_POP_BLOCK_CONST:           //
                return { opcode, references, value }

            case IROpcode.INSTR_LABEL_ALIAS: {      // labelAlias = label
                this.noRelocation(relocation);
                let label1 = this.getLabel(uintValue);
                let label2 = this.getLabel(uintValue2);
                if (!label1.aliases) label1.aliases = [];
                if (!label2.aliases) label2.aliases = [];
                label1.aliases.push(label2);
                label2.aliases.push(label1);
                return { opcode, references, label1, label2 };
            }

            case IROpcode.INSTR_DATA: {
                this.noRelocation(relocation);
                let v = new DataView(new ArrayBuffer(8));
                v.setUint32(0, uintValue, true);
                v.setUint32(4, uintValue2, true);
                return { opcode, references, data: new Uint8Array(v.buffer, 0, Math.min(8, op2)) };
            }

            case IROpcode.INSTR_WORD:
                return { opcode, references, value };

            case IROpcode.INSTR_FILL:
                this.noRelocation(relocation);
                return { opcode, references, size: uintValue };

            default:
                throw new Error(`Unknown IR opcode ${opcode}`);
        }
    }

    getLabel(labelIndex: number): Label {
        if (!this.labels[labelIndex]) {
            this.labels[labelIndex] = {};
        }
        return this.labels[labelIndex];
    }

    private noRelocation(relocation: Relocation | undefined) {
        if (relocation) throw new Error('Relocation in instruction not supporting relocations.');
    }

    generateDataIR(symbol: DataSymbol) {
        let ir: IRInstruction[] = [];
        let relocations: (Relocation | null)[] = this.getRelocationsInRange(symbol.section, symbol.addr, symbol.size);
        if (relocations.find(rel => rel && rel.type !== RelocationType.DATA))
            throw new Error(`Non-data relocation in symbol "${symbol.name}"`);
        let offset = symbol.addr;
        let data = symbol.section.data;
        let view = data ? new DataView(data.buffer, data.byteOffset, data.byteLength) : undefined;
        relocations[symbol.size] = null;
        relocations.forEach((rel, relAddr) => {
            relAddr += symbol.addr;
            if (offset < relAddr) {
                if (data) {
                    ir.push({
                        opcode: IROpcode.INSTR_DATA,
                        data: data.slice(offset, relAddr),
                    });
                } else {
                    ir.push({
                        opcode: IROpcode.INSTR_FILL,
                        size: relAddr - offset,
                    });
                }
            }
            offset = relAddr;
            if (!rel) return;
            let addValue = view?.getUint32(offset, true) ?? 0;
            ir.push({
                opcode: IROpcode.INSTR_WORD,
                references: [rel.symbol],
                value: new ValueFunction(addValue, rel.symbol),
            });
            offset += 4;
        });
        symbol.ir = ir;
        //console.log(symbol.section.name, symbol.name, symbol.addr, symbol.size, ir);
        //process.exit(0);
    }

}
