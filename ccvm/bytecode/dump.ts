import { AbsoluteSymbol, collectAliasedLabels, DataInnerSymbol, DataSymbol, FunctionInnerSymbol, FunctionSymbol, ImportSymbol, InvalidSymbol, IRInstruction, IROpcode, Label, RWOpcodeFlags, SymbolBase, UndefinedSymbol, ValueFunction, WithIRSymbol } from "./ir";
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
                line += `:${instr.condition} ${getLabelStr(instr.label)}`;
                break;

            case IROpcode.INSTR_JUMP_COND_INSTR:
                line += `:${instr.condition} ${getID(instr.instruction)}`;
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

            case IROpcode.INSTR_CMP:              // srcReg, dstReg, op2 = comparison operator
                line += `:${instr.op} R${instr.srcReg} R${instr.dstReg}`;
                break;

            case IROpcode.INSTR_BIN_OP:           // srcReg, dstReg, op2 = operator
                if (instr.op !== 0x86) {
                    line += ` R${instr.dstReg} = R${instr.dstReg} ${binOpName(instr.op)} R${instr.srcReg}`;
                } else {
                    line += ` R${instr.dstReg}:R${instr.srcReg} = R${instr.dstReg} ${binOpName(instr.op)} R${instr.srcReg}`;
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
        } else if (symbol instanceof DataInnerSymbol) {
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
        case 0x2A:
        case 0x2B:
        case 0x2D:
            return String.fromCharCode(op);
        case 0x80: return 'DEC';
        case 0x82: return 'INC';
        case 0x83: return 'UDIV';
        case 0x84: return 'UMOD';
        case 0x85: return 'PDIV';
        case 0x86: return 'UMUL64';
        case 0x87: return '+ (keep carry)';
        case 0x88: return '+ carry +';
        case 0x89: return '- (keep carry)';
        case 0x8a: return '- carry -';
        case 0x3C: return '<<';
        case 0x3E: return '>>';
        case 0x8B: return '>>>';
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
