
#if defined(TARGET_DEFS_ONLY) || defined(INTELLISENSE)
/* number of available registers */
#define NB_REGS 8

/* a register can belong to several classes. The classes must be
   sorted from more general to more precise (see gv2() code which does
   assumptions on it). */
#define RC_INT     0x0001 /* generic register R0-R3 */
#define RC_R0      0x0002
#define RC_R1      0x0004
#define RC_R2      0x0008
#define RC_R3      0x0010
#define RC_X0      0x0020
#define RC_X1      0x0040
#define RC_X2      0x0080
#define RC_X3      0x0100
#define RC_IRET    RC_R0  /* function return: integer register */
#define RC_IRE2    RC_R1  /* function return: second integer register */
#define RC_FRET    RC_R0  /* function return: float register */
#define RC_FLOAT   RC_INT

/* pretty names for the registers */
enum {
    TREG_R0 = 0,
    TREG_R1,
    TREG_R2,
    TREG_R3,
    TREG_X0 = TREG_R0 + 4,
    TREG_X1,
    TREG_X2,
    TREG_X3,
};

/* return registers for function */
#define REG_IRET TREG_R0 /* single word int return register */
#define REG_IRE2 TREG_R1 /* second word return register (for long long) */
#define REG_FRET TREG_R0

/* defined if function parameters must be evaluated in reverse order */
#define INVERT_FUNC_PARAMS

/* defined if structures are passed as pointers. Otherwise structures
   are directly pushed on stack. */
/* #define FUNC_STRUCT_PARAM_AS_PTR */

/* pointer size, in bytes */
#define PTR_SIZE 4

/* long double size and alignment, in bytes */
#define LDOUBLE_SIZE  8
#define LDOUBLE_ALIGN 4

/* maximum alignment (for aligned attribute support) */
#define MAX_ALIGN     8

#define CHAR_IS_UNSIGNED


/******************************************************/
/* ! TARGET_DEFS_ONLY */
#endif
#if !defined(TARGET_DEFS_ONLY) || defined(INTELLISENSE)
/******************************************************/
#define USING_GLOBALS
#include "tcc.h"

#include "ccvm-instr.c"
#include "ccvm-reloc.c"
#include "ccvm-output.c"
#include "ccvm-link.c"

#define R0_ADDR (0 * 4)
#define X0_ADDR (1 * 4)
#define R1_ADDR (2 * 4)
#define X1_ADDR (3 * 4)
#define R2_ADDR (4 * 4)
#define X2_ADDR (5 * 4)
#define R3_ADDR (6 * 4)
#define X3_ADDR (7 * 4)
#define SP_ADDR (8 * 4)
#define PC_ADDR (9 * 4)
#define BP_ADDR (10 * 4)
#define FLAGS_ADDR (11 * 4)
#define STASH_ADDR (12 * 4)

int reg_addr(int reg) {
    switch (reg) {
    case TREG_R0: return R0_ADDR;
    case TREG_R1: return R1_ADDR;
    case TREG_R2: return R2_ADDR;
    case TREG_R3: return R3_ADDR;
    case TREG_X0: return X0_ADDR;
    case TREG_X1: return X1_ADDR;
    case TREG_X2: return X2_ADDR;
    case TREG_X3: return X3_ADDR;
    default: 
        tcc_error("INTERNAL ERROR: Using invalid register.");
        return 0;
    }
}

ST_DATA const char * const target_machine_defs =
    "__ccvm__\0"
    "__ccvm\0"
    ;

ST_DATA const int reg_classes[NB_REGS] = {
    RC_INT | RC_R0, // R0
    RC_INT | RC_R1, // R1
    RC_INT | RC_R2, // R2
    RC_INT | RC_R3, // R3
    RC_X0, // X0
    RC_X1, // X1
    RC_X2, // X2
    RC_X3, // X3
};

const char *default_elfinterp(struct TCCState *s)
{
    return "/lib/ld-linux.so.3"; // TODO: may be removed
}

static int my_type_size(CType *type, int *a)
{
    int r = type_size(type, a);
    if (a && *a > 4) *a = 4;
    return r;
}

int get_label(int t) {
    static int label_number = 1;
    if (t == 0) {
        return label_number++;
    }
    return t;
}

/* load 'r' from value 'sv' */
void load(int r, SValue *sv)
{
    int v, t, ft, fc, fr;
    SValue v1;

    fr = sv->r;
    ft = sv->type.t & ~VT_DEFSIGN;
    fc = sv->c.i;

    ft &= ~(VT_VOLATILE | VT_CONSTANT);

    v = fr & VT_VALMASK;

    if (fr & VT_LVAL) {

        // Load data from memory

        if (v == VT_LLOCAL) {
            v1.type.t = VT_INT;
            v1.r = VT_LOCAL | VT_LVAL;
            v1.c.i = fc;
            v1.sym = NULL;
            fr = r;
            if (!(reg_classes[fr] & RC_INT))
                fr = get_reg(RC_INT);
            load(fr, &v1);
        }

        int btype = ft & VT_BTYPE;
        int type = ft & VT_TYPE;
        int bits;
        int sign_extend = 0;

        if ((ft & VT_BTYPE) == VT_FLOAT) {
            bits = 32;
        } else if ((ft & VT_BTYPE) == VT_DOUBLE) {
            bits = 64;
        } else if ((ft & VT_BTYPE) == VT_LDOUBLE) {
            bits = 64;
        } else if ((ft & VT_TYPE) == VT_BYTE || (ft & VT_TYPE) == VT_BOOL) {
            bits = 8;
            sign_extend = 1;
        } else if ((ft & VT_TYPE) == (VT_BYTE | VT_UNSIGNED)) {
            bits = 8;
            sign_extend = 0;
        } else if ((ft & VT_TYPE) == VT_SHORT) {
            bits = 16;
            sign_extend = 1;
        } else if ((ft & VT_TYPE) == (VT_SHORT | VT_UNSIGNED)) {
            bits = 16;
            sign_extend = 0;
        } else {
            bits = 32;
        }

        if ((fr & VT_VALMASK) == VT_CONST) {
            // Constant memory reference
            if (fr & VT_SYM) {
                instrRWReloc(1, sv->sym, r, fc, bits, sign_extend);
            } else {
                instrRWConst(1, r, fc, bits, sign_extend, 0);
            }
        } else if ((fr & VT_VALMASK) == VT_LOCAL) {
            // Local variable
            instrRWConst(1, r, fc, bits, sign_extend, 1);
        } else {
            // Indirect access from fr register
            instrRWInd(1, r, fr & VT_VALMASK, bits, sign_extend);
        }

    } else if (v == VT_CONST) {

        // Load constant value into register either from symbol or absolute.
        if (fr & VT_SYM) {
            instrMovReloc(r, sv->sym);
        } else {
            instrMovConst(r, fc);
        }

    } else if (v == VT_LOCAL) {

        // Calculate address of local variable, e.g. int x; f(&x);
        instrRWConst(1, r, BP_ADDR, 32, 0, 0);
        if (fc) {
            instrBinOpConst(BIN_OP_ADD, r, fc);
        }

    } else if (v == VT_CMP) {

        // Load comparision result into register, e.g. int x = (a < b);
        int label = get_label(0);
        instrMovConst(r, 1);
        instrJumpCondLabel(vtop->cmp_op, label);
        instrMovConst(r, 0);
        instrLabel(label, 1, 0);

    } else if (v == VT_JMP || v == VT_JMPI) {

        // Load logic or/and result into register, e.g. int x = (a || b);
        int label = get_label(0);
        instrMovConst(r, v & 1);
        instrJumpLabel(label);
        gsym(fc);
        instrMovConst(r, (v & 1) ^ 1);
        instrLabel(label, 1, 0);

    } else if (v != r) {

        // Move between registers, Xn registers also supported
        if (v >= TREG_X0) {
            if (r >= TREG_X0) {
                int tmp_reg = 0;
                instrRWConst(0, tmp_reg, STASH_ADDR, 32, 0, 0);
                instrRWConst(1, tmp_reg, reg_addr(v), 32, 0, 0);
                instrRWConst(0, tmp_reg, reg_addr(r), 32, 0, 0);
                instrRWConst(1, tmp_reg, STASH_ADDR, 32, 0, 0);
            } else {
                instrRWConst(1, r, reg_addr(v), 32, 0, 0);
            }
        } else {
            if (r >= TREG_X0) {
                instrRWConst(0, v, reg_addr(r), 32, 0, 0);
            } else {
                instrMovReg(r, v);
            }
        }

    } else {

        // Nothing to do here, no need to move any thing if both registers are the same.

    }
}

/* store register 'r' in lvalue 'v' */
void store(int r, SValue *v)
{
    int fr, bt, ft, fc;

    ft = v->type.t;
    fc = v->c.i;
    fr = v->r & VT_VALMASK;
    ft &= ~(VT_VOLATILE | VT_CONSTANT);
    bt = ft & VT_BTYPE;

    if (fr == VT_CONST || fr == VT_LOCAL || (v->r & VT_LVAL)) {

        // Store value into memory

        int bits;
        if (bt == VT_FLOAT) {
            bits = 32;
        } else if (bt == VT_DOUBLE) {
            bits = 64;
        } else if (bt == VT_LDOUBLE) {
            bits = 64;
        } else if (bt == VT_SHORT) {
            bits = 16;
        } else if (bt == VT_BYTE || bt == VT_BOOL) {
            bits = 8;
        } else {
            bits = 32;
        }

        if ((fr & VT_VALMASK) == VT_CONST) {
            // Constant memory reference
            if (fr & VT_SYM) {
                instrRWReloc(0, v->sym, r, fc, bits, 0);
            } else {
                instrRWConst(0, r, fc, bits, 0, 0);
            }
        } else if ((fr & VT_VALMASK) == VT_LOCAL) {
            // Local variable
            instrRWConst(0, r, fc, bits, 0, 1);
        } else {
            // Indirect access from v->r register
            instrRWInd(0, r, v->r & VT_VALMASK, bits, 0);
        }

    } else if (fr != r) {

        // Move between registers, Xn registers also supported
        if (fr >= TREG_X0) {
            if (r >= TREG_X0) {
                int tmp_reg = 0;
                instrRWConst(0, tmp_reg, STASH_ADDR, 32, 0, 0);
                instrRWConst(1, tmp_reg, reg_addr(fr), 32, 0, 0);
                instrRWConst(0, tmp_reg, reg_addr(r), 32, 0, 0);
                instrRWConst(1, tmp_reg, STASH_ADDR, 32, 0, 0);
            } else {
                instrRWConst(1, r, reg_addr(fr), 32, 0, 0);
            }
        } else {
            if (r >= TREG_X0) {
                instrRWConst(0, fr, reg_addr(r), 32, 0, 0);
            } else {
                instrMovReg(r, fr);
            }
        }

    } else {

        // Nothing to do here, no need to move any thing if both registers are the same.

    }
}

/* Return the number of registers needed to return the struct, or 0 if
   returning via struct pointer. */
ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ret_align, int *regsize) {
    *ret_align = 1; // Never have to re-align return values
    return 0;
}

/* 'is_jmp' is '1' if it is a jump */
static void gcall_or_jmp(int is_jmp)
{
    if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST && (vtop->r & VT_SYM)) {
        instrJumpReloc(!is_jmp, vtop->sym);
    } else {
        int r = gv(RC_INT);
        instrJumpReg(!is_jmp, r);
    }
}

/* Generate function call. The function address is pushed first, then
   all the parameters in call order. This functions pops all the
   parameters and the function address. */
void gfunc_call(int nb_args)
{
    int i;
    Sym *func_sym;
    int *offsets;
    MALLOC_OR_STACK(offsets, sizeof(int) * (nb_args + 1));

    func_sym = vtop[-nb_args].type.ref;

    SValue *aaa = &vtop[-nb_args];

    DEBUG_COMMENT("Call %s", get_tok_str(aaa->sym->v, NULL));

    if (func_sym->f.func_type == FUNC_ELLIPSIS) {
        // todo: different calling convention for ellipsis:
        // - stack cleared by caller
        // - maybe 32-bit argument alignment will be also needed
        tcc_error("Ellipsis are not supported yet.");
    }

    // Calculate offsets
    int offset = 0;
    for(i = 0; i < nb_args; i++) {
        SValue* arg = vtop - nb_args + 1 + i;
        // calculate size, alignment, and offset
        int align;
        int size = my_type_size(&vtop->type, &align);
        int offset_aligned = (offset + align - 1) & ~(align - 1);
        offsets[i] = offset_aligned;
        offset = offset_aligned + size;
    }
    // Final alignment to 32-bits
    offsets[i] = (offset + 3) & ~3;

    for(i = 0; i < nb_args; i++) {
        int arg_index = nb_args - 1 - i;
        if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) {
            // allocate register to store the address
            int r = get_reg(RC_INT);
            // allocate the necessary size on stack
            instrPushBlockConst(r, offsets[arg_index + 1] - offsets[arg_index], 0);
            // generate code that copies the structure to newly allocated stack buffer
            vset(&vtop->type, r | VT_LVAL, 0);
            vswap();
            vstore();
        } else if (is_float(vtop->type.t)) {
            gv(RC_FLOAT); /* only one float register */
            DEBUG_COMMENT("TODO push float parameter");
            //g8(0);
        } else {
            /* XXX: implicit cast ? */
            // put register to integer register
            int r = gv(RC_INT);
            // verify if we have correct size and alignment after conversion to register
            int align;
            int size = my_type_size(&vtop->type, &align);
            int aligned_size = offsets[arg_index + 1] - offsets[arg_index];
            if ((offsets[arg_index] & (align - 1)) || size > aligned_size) {
                tcc_error("Internal error. Failed to evaluate expression to integer register.");
            }
            // push register according to its slot, which can be bigger than the type due to alignment
            if (aligned_size == 8) {
                if (size == 8) {
                    instrPush(32, vtop->r2);
                } else {
                    // Push dummy data, although this should never happen
                    instrPush(32, vtop->r);
                }
                instrPush(32, vtop->r);
            } else {
                instrPush(aligned_size * 8, vtop->r);
            }
        }
        vtop--;
    }
    save_regs(0); /* save used temporary registers */

    gcall_or_jmp(0);

    if (offset > 0) {
        instrPopBlockConst(offset);
    }

    vtop--;
    FREE_OR_STACK(offsets);
}

static int prologue_push_label;

/* generate function prolog of type 't' */
void gfunc_prolog(Sym *func_sym)
{
    CType *func_type = &func_sym->type;
    Sym *sym = func_type->ref;
    int func_call = sym->f.func_call;
    int addr = 8;
    Sym *param;

    /*
    call stack:
        return_address  <-- SP == BP
        return_bp       <-- SP + 4
        param0          <-- SP + 8
        param1          <-- SP + 8 + N
        ...
    after prologue:
        temp_value1     <-- SP
        temp_value0     <-- SP + M
        ...
        return_address  <-- BP
        return_bp       <-- BP + 4
        param0          <-- BP + 8
        param1          <-- BP + 8 + N
        ...
    */

    loc = 0;

    DEBUG_COMMENT("Function %s", get_tok_str(func_sym->v, NULL));

    prologue_push_label = get_label(0);
    instrPushBlockLabel(0, prologue_push_label, 1);

    for(param = sym->next; param; param = param->next) {
        // Get parameter information
        CType* type = &param->type;
        int align;
        int size = my_type_size(type, &align);
        // Align parameter address to its natural alignment
        addr = (addr + align - 1) & ~(align - 1);
        // Push parameter symbol
        sym_push(param->v & ~SYM_FIELD, type, VT_LOCAL | VT_LVAL, addr);
        // Calculate address for next parameter
        addr += size;
    }
}

/* generate function epilog */
void gfunc_epilog(void)
{
    int loc_aligned = (-loc + 3) & -4;
    instrLabel(prologue_push_label, 0, loc_aligned);
    DEBUG_COMMENT("Adjusting function prologue to %d", loc_aligned);
    instrReturn();
}

ST_FUNC void gen_fill_nops(int bytes)
{
    instrNoop(bytes);
}

/* generate a jump to a label */
ST_FUNC int gjmp(int t)
{
    t = get_label(t);
    instrJumpLabel(t);
    return t;
}

/* generate a jump to a fixed address */
ST_FUNC void gjmp_addr(int a)
{
    int label = get_label(0);
    instrLabel(label, 1, a - ind);
    instrJumpLabel(label);
}

ST_FUNC int gjmp_cond(int op, int t)
{
    instrJumpCondLabel(op, t);
    return t;
}

ST_FUNC int gjmp_append(int n, int t)
{
    int r;
    if (n) {
        if (t && t != n) {
            instrLabelAlias(t, n);
            return n;
        } else {
            return n;
        }
    } else {
        r = get_label(t);
    }
    DEBUG_COMMENT("gjmp_append 0x%04X 0x%04X -> 0x%04X", n, t, r);
    return r;
}

/* generate an integer binary operation */
void gen_opi(int op)
{
    DEBUG_COMMENT("0x%04X %c (0x%02X) 0x%04X", vtop[-1].r, op, op, vtop[0].r);
    switch (op)
    {
    case TOK_ADDC1:
        gen_opi(BIN_OP_ADD);
        return;
    case TOK_SUBC1:
        gen_opi(BIN_OP_SUB);
        return;
    case TOK_PDIV:
        gen_opi(BIN_OP_UDIV);
        return;
    case TOK_UMULL:
    case '%':
    case TOK_UMOD:
    case BIN_OP_ADD:
    case BIN_OP_ADDC:
    case BIN_OP_BITAND:
    case BIN_OP_BITXOR:
    case BIN_OP_BITOR:
    case BIN_OP_MUL:
    case BIN_OP_SUB:
    case BIN_OP_SUBC:
    case BIN_OP_SHL:
    case BIN_OP_SHR:
    case BIN_OP_SAR:
    case BIN_OP_DIV:
    case BIN_OP_UDIV:
    {
        int a, b;
        int gen_const = 0;
        if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
            gen_const = 1;
            vswap();
            a = gv(RC_INT);
            vswap();
            b = vtop->c.i;
        } else {
            gv2(RC_INT, RC_INT);
            a = vtop[-1].r;
            b = vtop[0].r;
        }

        vtop--;
        save_reg_upstack(a, 1);

        if (op == BIN_OP_MUL || op == BIN_OP_DIV || op == BIN_OP_UDIV || op == TOK_UMULL || op == '%' || op == TOK_UMOD) {
            save_reg(a + TREG_X0);
            if (op == '%' || op == TOK_UMOD) {
                vtop->r = a + TREG_X0;
                op = BIN_OP_DIV;
            }
            if (op == TOK_UMULL) {
                vtop->r2 = a + TREG_X0;
                op = BIN_OP_MUL;
            }
        }

        if (gen_const) {
            instrBinOpConst(op, a, b);
        } else {
            instrBinOp(op, a, b);
        }
        return;
    }

    case CMP_OP_ULT:
    case CMP_OP_UGE:
    case CMP_OP_EQ:
    case CMP_OP_NE:
    case CMP_OP_ULE:
    case CMP_OP_UGT:
    case CMP_OP_Nset:
    case CMP_OP_Nclear:
    case CMP_OP_LT:
    case CMP_OP_GE:
    case CMP_OP_LE:
    case CMP_OP_GT:
    {
        if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
            vswap();
            int a = gv(RC_INT);
            vswap();
            int value = vtop->c.i;
            instrBinOpConst(BIN_OP_CMP, a, value);
        } else {
            gv2(RC_INT, RC_INT);
            int a = vtop[-1].r;
            int b = vtop[0].r;
            instrBinOp(BIN_OP_CMP, a, b);
        }
        vtop--;
        vset_VT_CMP(op);
        return;
    }

    default:
        break;
    }
    tcc_error("gen_opi unimplemented 0x%02X", op);
}

/* generate a floating point operation 'v = t1 op t2' instruction. The
 *    two operands are guaranteed to have the same floating point type */
void gen_opf(int op)
{
    /*switch (op)
    {
    case '+':
    {
        int a, b;
        int gen_const = 0;

        gv2(RC_FLOAT, RC_FLOAT);
        a = vtop[-1].r;
        b = vtop[0].r;
        int type = vtop[0].type.t & VT_BTYPE;
        vtop--;
        save_reg_upstack(a, 1);
        if (type == VT_FLOAT) {
            instrBinOp(op, a, b); // TODO: Actual op code
        } else if (type == VT_DOUBLE || type == VT_LDOUBLE) {
            instrBinOp(op, a, b);
            instrBinOp(op, a, b); // TODO: Actual op code
        }
        return;
    }

    default:
        break;
    }*/
    tcc_error("gen_opf %d unimplemented", op);
}

/* convert integers to fp 't' type. Must handle 'int', 'unsigned int'
   and 'long long' cases. */
ST_FUNC void gen_cvt_itof(int t)
{
    tcc_error("unimplemented gen_cvt_itof %x!", vtop->type.t);
}

/* convert fp to int 't' type */
void gen_cvt_ftoi(int t)
{
    tcc_error("unimplemented gen_cvt_ftoi!");
}

/* convert from one floating point type to another */
void gen_cvt_ftof(int t)
{
    tcc_error("unimplemented gen_cvt_ftof!");
}

/* computed goto support */
void ggoto(void)
{
    gcall_or_jmp(1);
    vtop--;
}

/* Save the stack pointer onto the stack */
ST_FUNC void gen_vla_sp_save(int addr)
{
    DEBUG_COMMENT("gen_vla_sp_save %d", addr);
    int tmp_reg = 0;
    instrRWConst(0, tmp_reg, STASH_ADDR, 32, 0, 0);
    instrRWConst(1, tmp_reg, SP_ADDR, 32, 0, 0);
    instrRWConst(0, tmp_reg, addr, 32, 0, 1);
    instrRWConst(1, tmp_reg, STASH_ADDR, 32, 0, 0);
}

/* Restore the SP from a location on the stack */
ST_FUNC void gen_vla_sp_restore(int addr)
{
    DEBUG_COMMENT("gen_vla_sp_restore %d", addr);
    int tmp_reg = 0;
    instrRWConst(0, tmp_reg, STASH_ADDR, 32, 0, 0);
    instrRWConst(1, tmp_reg, addr, 32, 0, 1);
    instrRWConst(0, tmp_reg, SP_ADDR, 32, 0, 0);
    instrRWConst(1, tmp_reg, STASH_ADDR, 32, 0, 0);
}

/* Subtract from the stack pointer, and push the resulting value onto the stack */
ST_FUNC void gen_vla_alloc(CType *type, int align)
{
    DEBUG_COMMENT("gen_vla_alloc %d", align);
    int reg = gv(RC_INT); /* allocation size */
    save_reg_upstack(reg, 1);
    instrBinOpConst(BIN_OP_ADD, reg, 3);
    instrBinOpConst(BIN_OP_BITAND, reg, 0xFFFFFFFC);
    instrPushBlockReg(reg, reg);
    vpop();
}

ST_FUNC void gsym_addr(int t, int a)
{
    instrLabel(t, 1, a - ind);
}

/*************************************************************/
#endif
/*************************************************************/


/*

TODO:
* Multi-stage code generation allowing smaller jump instructions and link-time constants:
  * Generate special placeholder instructions that are not smaller than final instruction if address is unknown.
  * When linking:
    * Reduce size of placeholders (placeholder corruption is ok at this point) to correct size.
      Relocation information may be needed to calculate size.
    * Create map that maps old offsets to new ones.
    * Replace placeholders with actual instructions using relocation information.
  * Local jumps must use relocations even the relative offsets is known, because it may change during linking.
* Import and export VM interface with dllexport and dllimport attributes, but some of those attributes
  requires TCC_TARGET_PE enabled.
  * or better, use following:
    __attribute__((section(".ccvm.import.NNN"))) void f() {}
    __attribute__((section(".ccvm.export.NNN"))) void f() { ... }
    where NNN is function index
  * during function prologue generation, we can output parameters information to special section
  * during linking we have import/export function index and associated symbol name which is enough to link it.
  * we can add functionality that generates vm interface since we have parameters info.
  * this can be in form of files that can be included by the host.
* TCC should be able to write and load .o files in ELF format for later linking.
* Linker should handle constructor, destructor, weak, e.t.c. attributes.
* Linker should be able to remove unused functions and data since we have its size in symbols table.
* Linker should generate ordered list of actions: address => action
  * RELOCATION => type, actual address - for relocations
  * SKIP => size - for removing unused functions
  * when copying from sections to output bytecode it should walk at the same time over section data and this list
* Maybe write linker in C++, pass only the sections (and maybe some other data) and the rest will be done there.
* PUSH_BLOCK from prologue should be reduced to smallest possible size (removed if zero bytes allocated).
* User imports and exports starts at 1, index 0 is reserved.
  * import 0 - indicate exit from guest function
  * export 0 - call destructors
  * main function is called as any other exported function, but when generating
    sample host code, we can generate more sophisticated wrapper. It will
    get argc and argv as arguments, push them into the stack and call exported main.
* Why Ubuntu 20 on GH Actions doesn't work?
*/