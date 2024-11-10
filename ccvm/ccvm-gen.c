
#if defined(TARGET_DEFS_ONLY) || defined(INTELLISENSE)
/* number of available registers */
#define NB_REGS 4

/* a register can belong to several classes. The classes must be
   sorted from more general to more precise (see gv2() code which does
   assumptions on it). */
#define RC_INT     0x0001 /* generic register R0-R3 */
#define RC_R0      0x0002
#define RC_R1      0x0004
#define RC_R2      0x0008
#define RC_R3      0x0010
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

ST_DATA const char * const target_machine_defs =
    "__ccvm__\0"
    "__ccvm\0"
    ;

ST_DATA const int reg_classes[NB_REGS] = {
    RC_INT | RC_R0, // R0
    RC_INT | RC_R1, // R1
    RC_INT | RC_R2, // R2
    RC_INT | RC_R3, // R3
};

const char *default_elfinterp(struct TCCState *s)
{
    return "/lib/ld-linux.so.3"; // TODO: may be removed
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
    //DEBUG_COMMENT("load r%d <- type:%d r:0x%04X r2:0x%04X C:0x%08lX", r, sv->type.t, sv->r, sv->r2, sv->c.i);

    int v, t, ft, fc, fr;
    SValue v1;

    fr = sv->r;
    ft = sv->type.t & ~VT_DEFSIGN;
    fc = sv->c.i;

    ft &= ~(VT_VOLATILE | VT_CONSTANT);

    v = fr & VT_VALMASK;

    if (fr & VT_LVAL) {
    
        if (v == VT_LLOCAL) {
            tcc_error("TODO: VT_LLOCAL");
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
            /* constant memory reference */
            if (fr & VT_SYM) {
                instrRWReloc(1, sv->sym, r, bits, sign_extend);
            } else {
                instrRWConst(1, r, fc, bits, sign_extend, 0);
            }
            return;
        } else if ((fr & VT_VALMASK) == VT_LOCAL) {
            instrRWConst(1, r, fc, bits, sign_extend, 1);
            return;
        } else {
            instrRWInd(1, r, sv->r & VT_VALMASK, bits, sign_extend);
            return;
        }
    } else {
        if (v == VT_CONST) {

            if (fr & VT_SYM) {
                instrMovReloc(r, sv->sym);
            } else {
                instrMovConst(r, fc);
            }
            return;

            #if 0
        } else if (v == VT_LOCAL) {
            if (fc) {
                o(0x8d); /* lea xxx(%ebp), r */
                gen_modrm(r, VT_LOCAL, sv->sym, fc);
            } else {
                o(0x89);
                o(0xe8 + r); /* mov %ebp, r */
            }
        } else if (v == VT_CMP) {
            o(0x0f); /* setxx %br */
            o(fc);
            o(0xc0 + r);
            o(0xc0b60f + r * 0x90000); /* movzbl %al, %eax */
        } else if (v == VT_JMP || v == VT_JMPI) {
            t = v & 1;
            oad(0xb8 + r, t); /* mov $1, r */
            o(0x05eb); /* jmp after */
            gsym(fc);
            oad(0xb8 + r, t ^ 1); /* mov $0, r */
            #endif
        } else if (v != r) {
            instrMovReg(r, v);
            return;
        } else {
            // Nothing to do here
            return;
        }
    }
    tcc_error("load unimplemented!");
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

    if (fr == VT_CONST || fr == VT_LOCAL || (v->r & VT_LVAL)) {
        if ((fr & VT_VALMASK) == VT_CONST) {
            /* constant memory reference */
            if (fr & VT_SYM) {
                instrRWReloc(0, v->sym, r, bits, 0);
            } else {
                instrRWConst(0, r, fc, bits, 0, 0);
            }
        } else if ((fr & VT_VALMASK) == VT_LOCAL) {
            instrRWConst(0, r, fc, bits, 0, 1);
        } else {
            instrRWInd(0, r, v->r & VT_VALMASK, bits, 0);
        }
    } else if (fr != r) {
        instrMovReg(r, fr);
    } else {
        // Copy the same register
    }
}

/* Return the number of registers needed to return the struct, or 0 if
   returning via struct pointer. */
ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ret_align, int *regsize) {
    tcc_error("gfunc_sret unimplemented");
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
        int size = type_size(&vtop->type, &align);
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
            instrPushBlockConst(r, offsets[arg_index + 1] - offsets[arg_index]);
            // generate code that copies the structure to newly allocated stack buffer
            vset(&vtop->type, r | VT_LVAL, 0);
            vswap();
            vstore();
        } else if (is_float(vtop->type.t)) {
            gv(RC_FLOAT); /* only one float register */
            DEBUG_COMMENT("TODO push float parameter");
            g8(0);
        } else {
            /* XXX: implicit cast ? */
            // put register to integer register
            int r = gv(RC_INT);
            // verify if we have correct size and alignment after conversion to register
            int align;
            int size = type_size(&vtop->type, &align);
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
            }
            instrPush(aligned_size * 8, vtop->r);
        }
        vtop--;
    }
    save_regs(0); /* save used temporary registers */

    gcall_or_jmp(0);

    // todo: clear parameters on stack if this is ellipsis function

    vtop--;
    FREE_OR_STACK(offsets);
}

static int epilogue_ret_cleanup;
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
    instrPushBlockLabel(0, prologue_push_label);

    for(param = sym->next; param; param = param->next) {
        // Get parameter information
        CType* type = &param->type;
        int align;
        int size = type_size(type, &align);
        // Align parameter address to its natural alignment
        addr = (addr + align - 1) & ~(align - 1);
        // Push parameter symbol
        sym_push(param->v & ~SYM_FIELD, type, VT_LOCAL | VT_LVAL, addr);
        // Calculate address for next parameter
        addr += size;
    }
    // Final alignment
    addr = (addr + 3) & ~3;

    epilogue_ret_cleanup = (addr - 8) / 4;
}

/* generate function epilog */
void gfunc_epilog(void)
{
    int loc_aligned = (-loc + 3) & -4;
    instrLabel(prologue_push_label, 0, loc_aligned);
    DEBUG_COMMENT("Adjusting function prologue to %d", loc_aligned);
    instrReturn(epilogue_ret_cleanup);
}

ST_FUNC void gen_fill_nops(int bytes)
{
    tcc_error("gen_fill_nops unimplemented");
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

const char* tok_to_str(int tok) {
    static char tmp[2] = "\0";
    switch (tok) {
    case TOK_SAR: return ">> (s)";
    case TOK_ULT: return "< (u)";
    case TOK_UGE: return ">= (u)";
    case TOK_EQ: return "==";
    case TOK_NE: return "!=";
    case TOK_ULE: return "<= (u)";
    case TOK_UGT: return "> (u)";
    case TOK_LT: return "<";
    case TOK_GE: return ">=";
    case TOK_LE: return "<=";
    case TOK_GT: return ">";
    case TOK_UMULL: return "**";
    default:
        if (tok > ' ' && tok <= '\x7F') {
            tmp[0] = tok;
            return tmp;
        } else {
            return "???";
        }
    }
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
            addLocalReloc(LOCAL_RELOC_ALIAS_LABEL, t, n, 0);
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
    case TOK_ADDC2:
    case TOK_SAR:
    case '+':
    case '*': {
        gv2(RC_INT, RC_INT);
        int a = vtop[-1].r;
        int b = vtop[0].r;
        vtop--;
        save_reg_upstack(a, 1);
        instrBinOp(op, a, b);
        return;
    }
    case TOK_ULT:
    case TOK_UGE:
    case TOK_EQ:
    case TOK_NE:
    case TOK_ULE:
    case TOK_UGT:
    case TOK_LT:
    case TOK_GE:
    case TOK_LE:
    case TOK_GT: {
        gv2(RC_INT, RC_INT);
        int a = vtop[-1].r;
        int b = vtop[0].r;
        vtop--;
        vset_VT_CMP(op);
        instrCmp(a, b);
        return;
    }

    case TOK_UMULL: {
        gv2(RC_INT, RC_INT);
        int a = vtop[-1].r;
        int b = vtop[0].r;
        vtop--;
        save_reg_upstack(a, 1);
        save_reg(b);
        instrBinOp(op, a, b);
        vtop[0].r2 = b;
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
    tcc_error("gen_opf unimplemented");
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

/* increment tcov counter */
ST_FUNC void gen_increment_tcov(SValue *sv)
{
    tcc_error("unimplemented gen_increment_tcov!");
}

/* computed goto support */
void ggoto(void)
{
    tcc_error("unimplemented ggoto!");
}

/* Save the stack pointer onto the stack and return the location of its address */
ST_FUNC void gen_vla_sp_save(int addr)
{
    tcc_error("unimplemented gen_vla_sp_save!");
}

/* Restore the SP from a location on the stack */
ST_FUNC void gen_vla_sp_restore(int addr)
{
    tcc_error("unimplemented gen_vla_sp_restore!");
}

/* Subtract from the stack pointer, and push the resulting value onto the stack */
ST_FUNC void gen_vla_alloc(CType *type, int align)
{
    tcc_error("unimplemented gen_vla_alloc!");
}

ST_FUNC void gsym_addr(int t, int a)
{
    addLocalReloc(LOCAL_RELOC_SET_LABEL, a, t, 0);
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