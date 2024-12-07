
// #region Op Codes

export enum IROpcode {
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
    INSTR_PUSH_BLOCK_CONST, // reg, op2 = optional, value = block size
    INSTR_PUSH_BLOCK_LABEL, // reg, op2 = optional, label = label containing block size
    INSTR_BIN_OP,           // srcReg, dstReg, op2 = operator
    INSTR_RETURN,           //
    INSTR_LABEL_ALIAS,      // labelAlias = label
    INSTR_HOST,             // value = host function index
    INSTR_POP,              // reg, op2 = 1..4 bytes, TODO: is signed needed?
    INSTR_POP_BLOCK_CONST,  // value = bytes
    INSTR_BIN_OP_CONST,     // reg = reg ?? value

    INSTR_JUMP_COND_INSTR,
    INSTR_JUMP_INSTR,

    INSTR_DATA,
    INSTR_WORD,
    INSTR_FILL,
    INSTR_EMPTY,
    INSTR_MARKER,
    /*INSTR_DISPOSABLE_BEGIN,
    INSTR_DISPOSABLE_END,
    INSTR_LABEL,*/
};

export enum IRBinOpcode {
    BIN_OP_ADD = 0x2B,
    BIN_OP_SUB = 0x2D,
    BIN_OP_ADDC = 0x88,
    BIN_OP_SUBC = 0x8a,
    BIN_OP_BITAND = 0x26,
    BIN_OP_BITXOR = 0x5E,
    BIN_OP_BITOR = 0x7C,
    BIN_OP_MUL = 0x2A,
    BIN_OP_SHL = 0x3C,
    BIN_OP_SHR = 0x8b,
    BIN_OP_SAR = 0x3E,
    BIN_OP_DIV = 0x2F,
    BIN_OP_UDIV = 0x83,
    BIN_OP_MOD = 0x25,
    BIN_OP_UMOD = 0x84,
    BIN_OP_UMULL = 0x86,
    BIN_OP_CMP = 0xFF,
};

export enum IRCmpOpcode {
    CMP_OP_ULT = 0x92,
    CMP_OP_UGE = 0x93,
    CMP_OP_EQ = 0x94,
    CMP_OP_NE = 0x95,
    CMP_OP_ULE = 0x96,
    CMP_OP_UGT = 0x97,
    CMP_OP_Nset = 0x98,
    CMP_OP_Nclear = 0x99,
    CMP_OP_LT = 0x9c,
    CMP_OP_GE = 0x9d,
    CMP_OP_LE = 0x9e,
    CMP_OP_GT = 0x9f,
};

// #endregion


// #region Instructions

export enum RWOpcodeFlags {
    SIGNED = 0x80,
    BP = 0x40,
    BITS_MASK = 0x03,
    BITS8 = 0x00,
    BITS16 = 0x01,
    BITS32 = 0x02,
    BITS64 = 0x03,
};

export class ValueFunction {
    public constructor(
        public value: number,
        public relocation?: SymbolBase
    ) { }
    public getValue(): number {
        if (this.relocation !== undefined) {
            return (this.value + this.relocation.getValue()) & 0xFFFFFFFF;
        } else {
            return this.value;
        }
    }
}

export interface Label {
    instruction?: IRInstruction;
    instructionOffset?: number;
    absoluteValue?: number;
    aliases?: Label[];
    main?: boolean;
}

interface IRInstructionBase {
    opcode: IROpcode;
    references?: SymbolBase[];
    addr?: number; // TODO: Clean it up
};

interface IREmptyInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_EMPTY | IROpcode.INSTR_RETURN;
}

interface IRTwoRegInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_MOV_REG;
    dstReg: number;
    srcReg: number;
}

interface IRTwoRegOpInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_BIN_OP;
    dstReg: number;
    srcReg: number;
    op: number; // TODO: Split into two interfaces and use enums in op
}

interface IRConstRWInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_READ_CONST | IROpcode.INSTR_WRITE_CONST | IROpcode.INSTR_BIN_OP_CONST;
    reg: number;
    value: ValueFunction;
    op: number;
}

interface IRWithValueInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_WORD | IROpcode.INSTR_CALL_CONST | IROpcode.INSTR_JUMP_CONST | IROpcode.INSTR_HOST | IROpcode.INSTR_POP_BLOCK_CONST;
    value: ValueFunction;
}

interface IRRegRWInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_READ_REG | IROpcode.INSTR_WRITE_REG;
    reg: number;
    addrReg: number;
}

interface IRRegInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_JUMP_REG | IROpcode.INSTR_CALL_REG;
    reg: number;
}

interface IRRegPushInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_PUSH | IROpcode.INSTR_POP;
    reg: number;
    bytes: 1 | 2 | 3 | 4;
}

interface IRRegValueInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_MOV_CONST;
    reg: number;
    value: ValueFunction;
}

interface IRPushBlockConstInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_PUSH_BLOCK_CONST;
    reg: number;
    value: ValueFunction;
    optional: boolean;
}

interface IRDataInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_DATA;
    data: Uint8Array;
};

interface IRLabelInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_JUMP_LABEL;
    label: Label;
};

interface IRJumpInstrInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_JUMP_INSTR;
    instruction: IRInstruction;
};

interface IRPushBlockLabelInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_PUSH_BLOCK_LABEL;
    reg: number;
    label: Label;
    optional: boolean;
};

interface IRLabelCondInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_JUMP_COND_LABEL;
    label: Label;
    condition: number;
};

interface IRLabelCondInstrInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_JUMP_COND_INSTR;
    instruction: IRInstruction;
    condition: number;
};

interface IRLabelValueInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_LABEL_ABSOLUTE | IROpcode.INSTR_LABEL_RELATIVE;
    label: Label;
    value: number;
};

interface IRAliasInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_LABEL_ALIAS;
    label1: Label;
    label2: Label;
};

interface IRFillInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_FILL;
    size: number;
};

export interface IRMarkerInstruction extends IRInstructionBase {
    opcode: IROpcode.INSTR_MARKER;
    marker: 'heap' | 'stack' | 'program' | 'dataLoad' | 'dataBegin' | 'dataEnd';
};

export type IRInstruction = IRTwoRegOpInstruction | IRPushBlockLabelInstruction | IRRegPushInstruction
    | IRRegInstruction | IRLabelCondInstruction | IRLabelInstruction | IRConstRWInstruction
    | IRRegRWInstruction | IRTwoRegInstruction | IRRegValueInstruction | IREmptyInstruction
    | IRWithValueInstruction | IRDataInstruction | IRFillInstruction | IRLabelValueInstruction
    | IRAliasInstruction | IRPushBlockConstInstruction | IRLabelCondInstrInstruction
    | IRJumpInstrInstruction | IRMarkerInstruction
    ;


// #endregion


// #region Symbols


export class SymbolBase {
    getValue(): number {
        return 0; // TODO: Implement this
    }
    public indexes: number[] = [];
    public constructor(
        public name: string
    ) { }
}

export class AbsoluteSymbol extends SymbolBase {
    public constructor(
        name: string,
        public address: number
    ) {
        super(name);
    }
}

export class ImportSymbol extends SymbolBase {
    public constructor(
        name: string,
        public importIndex: number,
        public section: Section
    ) {
        super(name);
    }
}

export class WithIRSymbol extends SymbolBase {
    public innerSymbols?: InnerSymbol[];
    public ir?: IRInstruction[];
    public used: boolean = false;
    public constructor(
        name: string,
        public section: Section,
        public addr: number,
        public size: number
    ) {
        super(name);
    }
}

export class FunctionSymbol extends WithIRSymbol {
    declare public innerSymbols?: FunctionInnerSymbol[];
}

export class DataSymbol extends WithIRSymbol {
    declare public innerSymbols?: InnerSymbol[];
}

export class InnerSymbol extends SymbolBase {
    public constructor(
        name: string,
        public parentSymbol: WithIRSymbol,
        public parentOffset: number
    ) {
        super(name);
    }
}

export class FunctionInnerSymbol extends InnerSymbol {
    public instruction?: IRInstruction;
    public constructor(
        name: string,
        parentSymbol: WithIRSymbol,
        parentOffset: number
    ) {
        super(name, parentSymbol, parentOffset);
    }
}


export class InvalidSymbol extends SymbolBase {
}

export class UndefinedSymbol extends SymbolBase {
}

// #endregion


// #region Import Export


export interface ExportEntry {
    index: number;
    name: string;
    symbol: SymbolBase | undefined;
};


// #endregion


// #region Section

export enum RelocationType {
    DATA = 1,
    INSTR = 2,
};

export interface Relocation {
    type: RelocationType;
    symbol: SymbolBase;
}

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
    known: boolean;
    relocations?: Relocation[];
    //symbols: Symbol[];
    //relocations: Relocation[];
}

// #endregion


// #region Predefined symbols

export const predefinedSymbols = {
    __ccvm_section_registers_begin__: false,
    __ccvm_section_registers_end__: false,
    __ccvm_section_data_begin__: false,
    __ccvm_section_data_end__: false,
    __ccvm_section_bss_begin__: false,
    __ccvm_section_bss_end__: false,
    __ccvm_section_stack_begin__: false,
    __ccvm_section_stack_end__: false,
    __ccvm_section_heap_begin__: false,
    __ccvm_section_heap_end__: false,
    __ccvm_section_entry_begin__: false,
    __ccvm_section_entry_end__: false,
    __ccvm_section_rodata_begin__: false,
    __ccvm_section_rodata_end__: false,
    __ccvm_section_text_begin__: false,
    __ccvm_section_text_end__: false,
    __ccvm_section_init_begin__: false,
    __ccvm_section_init_end__: false,
    __ccvm_section_fini_begin__: false,
    __ccvm_section_fini_end__: false,
    __ccvm_load_section_data_begin__: false,
    __ccvm_load_section_data_end__: false,
    __ccvm_export_table_begin__: false,
    __ccvm_export_table_end__: false,
}

export type PredefinedSymbols = { [key in keyof typeof predefinedSymbols]: DataSymbol };

// #endregion


// #region IR Utilities

function collectAllLabelsInSet(all: Set<Label>, label: Label) {
    if (!all.has(label)) {
        all.add(label);
        for (let a of label.aliases ?? []) {
            collectAllLabelsInSet(all, a);
        }
    }
}

export function collectAliasedLabels(label: Label) {
    let all = new Set<Label>();
    collectAllLabelsInSet(all, label);
    return [...all];
}

// #endregion
