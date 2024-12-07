import { AbsoluteSymbol, collectAliasedLabels, DataSymbol, FunctionInnerSymbol, FunctionSymbol, ImportSymbol, InnerSymbol, InvalidSymbol, IRBinOpcode, IRCmpOpcode, IRInstruction, IROpcode, Label, RWOpcodeFlags, SymbolBase, UndefinedSymbol, ValueFunction, WithIRSymbol } from "./ir";
import { assertUnreachable } from "./utils";

const map = new Map<any, string>();

export function getID(value: any): string {
    if (value === undefined) {
        return 'undefined';
    } else if (map.has(value)) {
        return map.get(value)!;
    } else {
        let id = (value?.name ?? '') + '@' + map.size;
        map.set(value, id);
        return id;
    }
}


export function dumpIR(ir: IRInstruction[] | undefined, ind: string) {
    if (ir === undefined) {
        console.log(ind, '# No IR instructions');
        return;
    } else if (ir.length === 0) {
        console.log(ind, '# Empty');
    }

    for (let [i, instr] of ir.entries()) {

        let line = `0x${i.toString(16).padStart(4, '0')} ${IROpcode[instr.opcode]?.substring(6)}`;
        let comment = getID(instr);

        switch (instr.opcode) {

            case IROpcode.INSTR_MOV_REG:          // dstReg = srcReg
                line += ` R${instr.dstReg} = R${instr.srcReg}`;
                break;

            case IROpcode.INSTR_MOV_CONST:        // reg = value
                line += ` R${instr.reg} = ${getValueStr(instr.value)}`;
                break;

            case IROpcode.INSTR_LABEL_RELATIVE:   // label, address_offset
                line += ` ${getLabelStr(instr.label)} = ${instr.value}`;
                break;

            case IROpcode.INSTR_LABEL_ABSOLUTE:   // label, address_offset
                line += ` ${getLabelStr(instr.label)} = ${instr.value}`;
                break;

            case IROpcode.INSTR_WRITE_CONST:       // reg <= [value]
            case IROpcode.INSTR_READ_CONST: {      // reg => [value]
                let type = (instr.op & RWOpcodeFlags.SIGNED) ? 'int' : 'uint';
                switch (instr.op & RWOpcodeFlags.BITS_MASK) {
                    case RWOpcodeFlags.BITS8: type += '8'; break;
                    case RWOpcodeFlags.BITS16: type += '16'; break;
                    case RWOpcodeFlags.BITS32: type += '32'; break;
                    case RWOpcodeFlags.BITS64: type += '64'; break;
                }
                let reg = `R${instr.reg}`;
                let mem = `${type}[${getValueStr(instr.value, true, !!(instr.op & RWOpcodeFlags.BP))}]`;
                if (instr.opcode === IROpcode.INSTR_WRITE_CONST) {
                    line += ` ${mem} = ${reg}`;
                } else {
                    line += ` ${reg} = ${mem}`;
                }
                break;
            }

            case IROpcode.INSTR_WRITE_REG:        // reg => [addrReg]
                line += ` [R${instr.addrReg}] = R${instr.reg}`;
                break;

            case IROpcode.INSTR_READ_REG:         // reg <= [addrReg]
                line += ` R${instr.reg} = [R${instr.addrReg}]`;
                break;

            case IROpcode.INSTR_JUMP_COND_LABEL:  // label, op2 = condition
                line += ` IF ${getCondStr(instr.condition)} ${getLabelStr(instr.label)}`;
                break;

            case IROpcode.INSTR_JUMP_COND_INSTR:
                line += ` IF ${getCondStr(instr.condition)} ${getID(instr.instruction)}`;
                break;

            case IROpcode.INSTR_JUMP_CONST:       // address
                line += ` ${getValueStr(instr.value)}`;
                break;

            case IROpcode.INSTR_CALL_CONST:       // address
                line += ` ${getValueStr(instr.value)}`;
                break;

            case IROpcode.INSTR_JUMP_LABEL:       // label
                line += ` ${getLabelStr(instr.label)}`;
                break;

            case IROpcode.INSTR_JUMP_INSTR:
                line += ` ${getID(instr.instruction)}`;
                break;

            case IROpcode.INSTR_JUMP_REG:         // reg
                line += ` [R${instr.reg}]`;
                break;

            case IROpcode.INSTR_CALL_REG:         // reg
                line += ` [R${instr.reg}]`;
                break;

            case IROpcode.INSTR_PUSH:             // reg, op2 = 1..4 bytes
            case IROpcode.INSTR_POP:             // reg, op2 = 1..4 bytes
                line += ` uint${instr.bytes * 8} R${instr.reg}`;
                break;

            case IROpcode.INSTR_PUSH_BLOCK_CONST: // reg, value = block size
                line += ` R${instr.reg} = BLOCK ${getValueStr(instr.value)}`;
                if (instr.optional) line += ' (OPTIONAL)';
                break;

            case IROpcode.INSTR_PUSH_BLOCK_LABEL: // reg, label = label containing block size
                line += ` R${instr.reg} = BLOCK ${getLabelStr(instr.label)}`;
                if (instr.optional) line += ' (OPTIONAL)';
                break;

            case IROpcode.INSTR_POP_BLOCK_CONST: // reg, value = block size
                line += ` ${getValueStr(instr.value)}`;
                break;

            case IROpcode.INSTR_BIN_OP:           // srcReg, dstReg, op2 = operator
                if (instr.op === IRBinOpcode.BIN_OP_UMULL) {
                    line += ` R${instr.dstReg}:R${instr.srcReg} = R${instr.dstReg} ${binOpName(instr.op)} R${instr.srcReg}`;
                } else if (instr.op === IRBinOpcode.BIN_OP_CMP) {
                    line += ` R${instr.dstReg} ${binOpName(instr.op)} R${instr.srcReg}`;
                } else {
                    line += ` R${instr.dstReg} = R${instr.dstReg} ${binOpName(instr.op)} R${instr.srcReg}`;
                }
                break;

            case IROpcode.INSTR_BIN_OP_CONST:
                if (instr.op === IRBinOpcode.BIN_OP_UMULL) {
                    throw new Error('Invalid instruction');
                } else if (instr.op === IRBinOpcode.BIN_OP_CMP) {
                    line += ` R${instr.reg} ${binOpName(instr.op)} ${getValueStr(instr.value)}`;
                } else {
                    line += ` R${instr.reg} = R${instr.reg} ${binOpName(instr.op)} ${getValueStr(instr.value)}`;
                }
                break;

            case IROpcode.INSTR_RETURN:           // value = cleanup words
                break;

            case IROpcode.INSTR_LABEL_ALIAS:      // labelAlias = label
                line += ` ${getLabelStr(instr.label1)} == ${getLabelStr(instr.label2)}`;
                break;

            case IROpcode.INSTR_DATA:
                if (instr.data.length > 40) {
                    line += ' ' + [...instr.data.slice(0, 18)].map(x => x.toString(16).padStart(2, '0')).join('');
                    line += '...';
                    line += [...instr.data.slice(instr.data.length - 19)].map(x => x.toString(16).padStart(2, '0')).join('');
                } else {
                    line += ' ' + [...instr.data].map(x => x.toString(16).padStart(2, '0')).join('');
                }
                break;

            case IROpcode.INSTR_WORD:
                line += ` ${getValueStr(instr.value)}`;
                break;

            case IROpcode.INSTR_FILL:
                line += ` ${instr.size}`;
                break;

            case IROpcode.INSTR_EMPTY:
                break;

            case IROpcode.INSTR_MARKER:
                line += ' ' + instr.marker;
                break;

            case IROpcode.INSTR_HOST:
                line += ` ${getValueStr(instr.value)}`;
                break;

            default:
                assertUnreachable(instr);
        }

        if (instr.references && instr.references.length > 0) {
            comment += ', references ' + instr.references.map(ref => getID(ref)).join(', ');
        }

        console.log(ind, line, '#', comment);
    }
}

export function dumpIRFromSymbols(symbols: SymbolBase[], commentUnused: boolean = false): void {
    for (let symbol of symbols) {
        let prefix = (commentUnused && (symbol instanceof WithIRSymbol) && !symbol.used) ? '# ' : '';
        console.log(`${prefix}${symbol.name}: # ${getID(symbol)} ${symbol.indexes.join(', ')}`);
        if (symbol instanceof AbsoluteSymbol) {
            console.log(`${prefix}    == 0x${symbol.address.toString(16)}`);
        } else if (symbol instanceof ImportSymbol) {
            console.log(`${prefix}    == import index ${symbol.importIndex}`);
        } else if (symbol instanceof FunctionSymbol) {
            console.log(`${prefix}    # FUNCTION, section ${symbol.section.name} at 0x${symbol.addr.toString(16)}, size ${symbol.size}, ${symbol.used ? 'used' : 'UNUSED'}`);
            for (let inner of symbol.innerSymbols ?? [])
                console.log(`${prefix}    #    INNER ${getID(inner)} at ${getID(inner.instruction)} (0x${(inner.parentOffset / 12).toString(16).padStart(4, '0')})`);
        } else if (symbol instanceof DataSymbol) {
            console.log(`${prefix}    # DATA, section ${symbol.section.name} at 0x${symbol.addr.toString(16)}, size ${symbol.size}, ${symbol.used ? 'used' : 'UNUSED'}`);
            for (let inner of symbol.innerSymbols ?? [])
                console.log(`${prefix}    #    INNER ${getID(inner)} at ${inner.parentOffset}`);
        } else if (symbol instanceof InvalidSymbol) {
            console.log(`${prefix}    # INVALID`);
        } else if (symbol instanceof UndefinedSymbol) {
            console.log(`${prefix}    # UNDEFINED`);
        } else if (symbol instanceof FunctionInnerSymbol) {
            console.log(`${prefix}    # INNER FUNCTION IN ${getID(symbol.parentSymbol)} at ${getID(symbol.instruction)} (0x${(symbol.parentOffset / 12).toString(16).padStart(4, '0')})`);
        } else if (symbol instanceof InnerSymbol) {
            console.log(`${prefix}    # INNER DATA IN ${getID(symbol.parentSymbol)} at ${symbol.parentOffset}`);
        } else {
            console.log(`${prefix}    # UNKNOWN KIND OF SYMBOL`);
        }
        if (symbol instanceof WithIRSymbol) {
            dumpIR(symbol.ir, `${prefix}   `);
        }
        console.log();
    }
}


function binOpName(op: number): string {
    switch (op) {
        case IRBinOpcode.BIN_OP_ADD: return '+';
        case IRBinOpcode.BIN_OP_SUB: return '-';
        case IRBinOpcode.BIN_OP_ADDC: return '+ carry +';
        case IRBinOpcode.BIN_OP_SUBC: return '- carry -';
        case IRBinOpcode.BIN_OP_BITAND: return '&';
        case IRBinOpcode.BIN_OP_BITXOR: return '^';
        case IRBinOpcode.BIN_OP_BITOR: return '|';
        case IRBinOpcode.BIN_OP_MUL: return '*';
        case IRBinOpcode.BIN_OP_SHL: return '<<';
        case IRBinOpcode.BIN_OP_SHR: return '(unsigned) >>';
        case IRBinOpcode.BIN_OP_SAR: return '(signed) >>';
        case IRBinOpcode.BIN_OP_DIV: return '(signed) /';
        case IRBinOpcode.BIN_OP_UDIV: return '(unsigned) /';
        case IRBinOpcode.BIN_OP_MOD: return '(signed) %';
        case IRBinOpcode.BIN_OP_UMOD: return '(unsigned) %';
        case IRBinOpcode.BIN_OP_UMULL: return '**';
        case IRBinOpcode.BIN_OP_CMP: return '(compare) ?';
        default: return `(?0x${op.toString(16)}?)`;
    }
}

function getValueStr(value: ValueFunction, signed: boolean = false, bp: boolean = false): string {
    let parts: string[] = [];
    if (bp) parts.push('BP');
    if (value.relocation) parts.push(getID(value.relocation));
    if (value.value !== 0) {
        if (signed && value.value > 0x7FFFFFFF) {
            parts.push((value.value - 0x100000000).toString());
        } else {
            parts.push(value.value.toString());
        }
    }
    return parts.length ? parts.join(' + ').replace('+ -', '- ') : '0';
}

function getLabelStr(label: Label) {
    let all = collectAliasedLabels(label);
    let parts: string[] = [];
    for (let a of all) {
        parts.push(`label${getID(a)}`);
        if (a.instruction !== undefined) parts.push(getID(a.instruction));
        if (a.instructionOffset !== undefined) parts.push(`offset 0x${a.instructionOffset.toString(16).padStart(4, '0')}`);
        if (a.absoluteValue !== undefined) parts.push(`value ${a.absoluteValue}`);
    }
    return parts.join(', ');
}

function getCondStr(op: number) {
    switch (op) {
        case IRCmpOpcode.CMP_OP_ULT: return '(unsigned) <';
        case IRCmpOpcode.CMP_OP_UGE: return '(unsigned) >=';
        case IRCmpOpcode.CMP_OP_EQ: return '==';
        case IRCmpOpcode.CMP_OP_NE: return '!=';
        case IRCmpOpcode.CMP_OP_ULE: return '(unsigned) <=';
        case IRCmpOpcode.CMP_OP_UGT: return '(unsigned) >';
        case IRCmpOpcode.CMP_OP_Nset: return 'negative';
        case IRCmpOpcode.CMP_OP_Nclear: return 'positive';
        case IRCmpOpcode.CMP_OP_LT: return '(signed) <';
        case IRCmpOpcode.CMP_OP_GE: return '(signed) >=';
        case IRCmpOpcode.CMP_OP_LE: return '(signed) <=';
        case IRCmpOpcode.CMP_OP_GT: return '(signed) >';
        default: return `unknown ${op.toString(16)}`;
    }
}

