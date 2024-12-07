import { dumpIRFromSymbols } from "./dump";
import { AbsoluteSymbol, DataInnerSymbol, DataSymbol, ExportEntry, FunctionInnerSymbol, FunctionSymbol, ImportSymbol, InnerSymbol, IRInstruction, IRMarkerInstruction, IROpcode, PredefinedSymbols, Section, SymbolBase, ValueFunction, WithIRSymbol } from "./ir";
import { Parser } from "./parser";


const sectionsNameRegExp = {
    registers: /^\.ccvm\.registers(\..*)?$/,
    rodata: /^\.data(\.*)?\.ro|\.data\.ro(\.*)?|\.rodata(\..*)?$/,
    data: /^\.data(\..*)?$/,
    bss: /^\.(bss|common)(\..*)?$/,
    stack: /^\.ccvm\.stack(\..*)?$/,
    heap: /^\.ccvm\.heap(\..*)?$/,
    entry: /^\.text\.ccvm\.entry(\..*)?$/,
    text: /^\.text(\..*)?$/,
    init: /^\.init_array(\..+)?$/,
    fini: /^\.fini_array(\..+)?$/,
    predefined: /^\$predefined_symbols_section$/,
    removed: /.*/,
};

type OutputSections = { [key in keyof typeof sectionsNameRegExp]: WithIRSymbol[] };

class SymbolOrganizer {

    private outputSections: OutputSections;
    private minStackSize: number;
    private minHeapSize: number;
    private anySection: Section | undefined;
    private traveled!: Set<WithIRSymbol>;

    constructor(
        private symbols: SymbolBase[],
        private predefinedSymbols: PredefinedSymbols,
        private exports: ExportEntry[],
    ) { }

    public organize() {

        let invalidExportSymbol: WithIRSymbol | undefined = undefined;

        this.outputSections = Object.fromEntries(Object.keys(sectionsNameRegExp).map(name => [name, []])) as any;

        this.anySection = undefined;
        // Assign symbols to output sections
        for (let symbol of this.symbols) {
            if (symbol instanceof WithIRSymbol) {

                this.anySection = this.anySection ?? symbol.section;

                for (let [nameStr, re] of Object.entries(sectionsNameRegExp)) {
                    let name = nameStr as keyof typeof sectionsNameRegExp;
                    if (re.test(symbol.section.name)) {
                        this.outputSections[name].push(symbol);
                        break;
                    }
                }

                if (symbol.name === '__ccvm_invalid_export__') {
                    invalidExportSymbol = symbol;
                }

            } else if (symbol instanceof ImportSymbol) {

                let wrapper = new FunctionSymbol(symbol.name, symbol.section, 0, 0);
                wrapper.ir = [
                    {
                        opcode: IROpcode.INSTR_HOST,
                        value: new ValueFunction(symbol.importIndex),
                    },
                    {
                        opcode: IROpcode.INSTR_RETURN,
                    },
                ];
                this.outputSections.text.push(wrapper);

            } else {

                // Nothing to do, this symbol does not generate any code

            }
        }
        if (!this.anySection) {
            throw new Error('No sections found');
        }
        this.minStackSize = this.maxSizeSymbolCombine(this.outputSections.stack);
        this.minHeapSize = this.maxSizeSymbolCombine(this.outputSections.heap);

        let exportTableSymbol = new DataSymbol(`__ccvm_export_table__`, this.anySection, 0, 4 * this.exports.length);
        exportTableSymbol.used = true;
        exportTableSymbol.ir = [];
        for (let i = 0; i < this.exports.length; i++) {
            let exp = this.exports[i];
            if (exp) {
                exportTableSymbol.ir.push({
                    opcode: IROpcode.INSTR_WORD,
                    value: new ValueFunction(0, exp.symbol),
                    references: exp.symbol ? [exp.symbol] : undefined,
                });
            } else {
                exportTableSymbol.ir.push({
                    opcode: IROpcode.INSTR_WORD,
                    value: new ValueFunction(0, invalidExportSymbol),
                    references: invalidExportSymbol ? [invalidExportSymbol] : undefined,
                });
            }
        }

        this.usedSection(this.outputSections.registers);
        this.usedSection(this.outputSections.entry);
        this.usedSection([exportTableSymbol]);
        this.usedSection(this.outputSections.init);
        this.usedSection(this.outputSections.fini);

        let outputSymbols = [
            this.predefinedSymbols.__ccvm_section_registers_begin__,
            ...sortSymbols(this.outputSections.registers, false),
            this.predefinedSymbols.__ccvm_section_registers_end__,

            this.predefinedSymbols.__ccvm_section_data_begin__,
            this.createMarker('dataBegin'),
            ...sortSymbols(this.outputSections.data, true),
            this.createMarker('dataEnd'),
            this.predefinedSymbols.__ccvm_section_data_end__,

            this.predefinedSymbols.__ccvm_section_bss_begin__,
            ...sortSymbols(this.outputSections.bss, true),
            this.predefinedSymbols.__ccvm_section_bss_end__,

            this.predefinedSymbols.__ccvm_section_heap_begin__,
            ...this.outputSections.heap,
            this.createMarker('heap'),
            this.predefinedSymbols.__ccvm_section_heap_end__,

            this.predefinedSymbols.__ccvm_section_stack_begin__,
            ...this.outputSections.stack,
            this.createMarker('stack'),
            this.predefinedSymbols.__ccvm_section_stack_end__,

            this.createMarker('program'),

            this.predefinedSymbols.__ccvm_section_entry_begin__,
            ...sortSymbols(this.outputSections.entry, false),
            this.predefinedSymbols.__ccvm_section_entry_end__,

            this.predefinedSymbols.__ccvm_section_rodata_begin__,
            ...sortSymbols(this.outputSections.rodata, true),
            this.predefinedSymbols.__ccvm_export_table_begin__,
            exportTableSymbol,
            this.predefinedSymbols.__ccvm_export_table_end__,
            this.predefinedSymbols.__ccvm_section_rodata_end__,

            this.predefinedSymbols.__ccvm_section_init_begin__,
            ...sortSymbols(this.outputSections.init, false),
            this.predefinedSymbols.__ccvm_section_init_end__,

            this.predefinedSymbols.__ccvm_section_fini_begin__,
            ...sortSymbols(this.outputSections.fini, false),
            this.predefinedSymbols.__ccvm_section_fini_end__,

            this.predefinedSymbols.__ccvm_section_text_begin__,
            ...sortSymbols(this.outputSections.text, false),
            this.predefinedSymbols.__ccvm_section_text_end__,

            this.predefinedSymbols.__ccvm_load_section_data_begin__,
            this.createMarker('dataLoad'),
            this.predefinedSymbols.__ccvm_load_section_data_end__,
        ];

        this.findUsedSymbols(outputSymbols);

        dumpIRFromSymbols(outputSymbols, true);
        console.log('===========================\n    REMOVED\n===========================');
        dumpIRFromSymbols(this.outputSections.removed, true);
        console.log('===========================\n    PREDEFINED\n===========================');
        dumpIRFromSymbols(this.outputSections.predefined, true);
    }

    findUsedSymbols(outputSymbols: WithIRSymbol[]) {
        let done = new Set<WithIRSymbol>();
        let toTravel: WithIRSymbol[] = [];
        for (let symbol of outputSymbols) {
            if (symbol.used) {
                toTravel.push(symbol);
                done.add(symbol);
            }
        }
        while (toTravel.length > 0) {
            let symbol = toTravel.pop()!;
            symbol.used = true;
            for (let instr of symbol.ir ?? []) {
                if (instr.references) {
                    for (let ref of instr.references) {
                        if (ref instanceof WithIRSymbol && !done.has(ref)) {
                            toTravel.push(ref);
                            done.add(ref);
                        } else if (ref instanceof InnerSymbol && !done.has(ref.parentSymbol)) {
                            toTravel.push(ref.parentSymbol);
                            done.add(ref.parentSymbol);
                        }
                    }
                }
            }
        }
    }

    usedSection(symbols: WithIRSymbol[]) {
        for (let symbol of symbols) {
            symbol.used = true;
        }
    }

    private createMarker(marker: IRMarkerInstruction['marker']) {
        let stackSymbol = new DataSymbol(`__ccvm_marker_${marker}__`, this.anySection!, 0, 0);
        stackSymbol.used = true;
        stackSymbol.ir = [{
            opcode: IROpcode.INSTR_MARKER,
            marker: marker as any,
        }];
        return stackSymbol;
    }

    private maxSizeSymbolCombine(symbols: WithIRSymbol[]): number {
        let result = 0;
        for (let symbol of symbols) {
            result = Math.max(result, symbol.size);
            symbol.addr = 0;
            symbol.size = 0;
            symbol.ir = [];
        }
        return result;
    }

}


let parser = new Parser();
let { symbols, predefinedSymbols, exports } = parser.parse('../bin/sample.bin');
let org = new SymbolOrganizer(symbols, predefinedSymbols, exports);
org.organize();


function sortSymbols(symbols: WithIRSymbol[], sortByAlignment: boolean): WithIRSymbol[] {
    symbols.sort(sortByAlignment ? compareWithAlignment : compare);
    return symbols;
}


function compareWithAlignment(a: WithIRSymbol, b: WithIRSymbol): number {
    let res = compareSectionNames(a.section.name, b.section.name);
    if (res !== 0) return res;
    let aAlignKey = alignKeyFromAddr(a.addr);
    let bAlignKey = alignKeyFromAddr(b.addr);
    res = aAlignKey - bAlignKey;
    if (res !== 0) return res;
    res = a.addr - b.addr;
    if (res !== 0) return res;
    return a.name.localeCompare(b.name, 'en');
}

function compare(a: WithIRSymbol, b: WithIRSymbol): number {
    let res = compareSectionNames(a.section.name, b.section.name);
    if (res !== 0) return res;
    res = a.addr - b.addr;
    if (res !== 0) return res;
    return a.name.localeCompare(b.name, 'en');
}

function compareSectionNames(a: string, b: string) {
    if (a === b) return 0;
    let aa = a.split('.');
    let bb = b.split('.');
    let n = Math.min(aa.length, bb.length);
    for (let i = 0; i < n; i++) {
        let pa = aa[i];
        let pb = bb[i];
        if (pa === pb) continue;
        let ia = parseInt(pa);
        if (ia.toString() === pa) {
            let ib = parseInt(pb);
            if (ib.toString() === pb) {
                return ia - ib;
            }
        }
        return pa.localeCompare(pb, 'en');
    }
    return aa.length > bb.length ? 1 : -1;
}

function alignKeyFromAddr(addr: number) {
    switch (addr & 3) {
        case 0: return 0;
        case 2: return 1;
        default: return 2;
    }
}

