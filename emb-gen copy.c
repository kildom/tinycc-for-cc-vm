
#ifdef TARGET_DEFS_ONLY
/* number of available registers */
#define NB_REGS 8

/* a register can belong to several classes. The classes must be
   sorted from more general to more precise (see gv2() code which does
   assumptions on it). */
#define RC_INT     0x0001 /* generic register R0-R5 */
#define RC_R0      0x0002
#define RC_R1      0x0004
#define RC_R2      0x0008
#define RC_R3      0x0010
#define RC_R4      0x0020
#define RC_R5      0x0040
#define RC_LR      0x0080
#define RC_SP      0x0100
#define RC_IRET    RC_R0  /* function return: integer register */
#define RC_IRE2    RC_R1  /* function return: second integer register */
#define RC_FRET    RC_R0  /* function return: integer register */
#define RC_FLOAT   RC_INT

/* pretty names for the registers */
enum {
    TREG_R0 = 0,
    TREG_R1,
    TREG_R2,
    TREG_R3,
    TREG_R4,
    TREG_R5,
    TREG_LR,
    TREG_SP,
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
#else /* ! TARGET_DEFS_ONLY */
/******************************************************/
#define USING_GLOBALS
#include "tcc.h"

ST_DATA const char * const target_machine_defs =
    "__emb__\0"
    "__emb\0"
    ;

ST_DATA const int reg_classes[NB_REGS] = {
    RC_INT | RC_R0, // R0
    RC_INT | RC_R1, // R1
    RC_INT | RC_R2, // R2
    RC_INT | RC_R3, // R3
    RC_INT | RC_R4, // R4
    RC_INT | RC_R5, // R5
    RC_LR, // LR
    RC_SP, // SP
};

const char *default_elfinterp(struct TCCState *s)
{
    return "/lib/ld-linux.so.3"; // TODO: may be removed
}

/* load 'r' from value 'sv' */
void load(int r, SValue *sv)
{
    printf("load r%d <- type:%d r:0x%04X r2:0x%04X\n", r, sv->type.t, sv->r, sv->r2);

    int v, t, ft, fc, fr;
    SValue v1;

    fr = sv->r;
    ft = sv->type.t & ~VT_DEFSIGN;
    fc = sv->c.i;

    ft &= ~(VT_VOLATILE | VT_CONSTANT);

    v = fr & VT_VALMASK;
    if (fr & VT_LVAL) {
        #if 0
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
        if ((ft & VT_BTYPE) == VT_FLOAT) {
            o(0xd9); /* flds */
            r = 0;
        } else if ((ft & VT_BTYPE) == VT_DOUBLE) {
            o(0xdd); /* fldl */
            r = 0;
        } else if ((ft & VT_BTYPE) == VT_LDOUBLE) {
            o(0xdb); /* fldt */
            r = 5;
        } else if ((ft & VT_TYPE) == VT_BYTE || (ft & VT_TYPE) == VT_BOOL) {
            o(0xbe0f);   /* movsbl */
        } else if ((ft & VT_TYPE) == (VT_BYTE | VT_UNSIGNED)) {
            o(0xb60f);   /* movzbl */
        } else if ((ft & VT_TYPE) == VT_SHORT) {
            o(0xbf0f);   /* movswl */
        } else if ((ft & VT_TYPE) == (VT_SHORT | VT_UNSIGNED)) {
            o(0xb70f);   /* movzwl */
        } else {
            o(0x8b);     /* movl */
        }
        gen_modrm(r, fr, sv->sym, fc);
        #endif
    } else {
        if (v == VT_CONST) {

            if (fr & VT_SYM) {
                //greloc(cur_text_section, sv->sym, ind, R_386_32);
                printf("r%d = sym\n", r);
                return;
            } else {
                printf("r%d = %d\n", r, fc);
                return;
            }
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
            printf("R%d = R%d\n", r, v);
        } else {
            printf("noop # R%d = R%d\n", r, v);
        }
    }
    tcc_error("load unimplemented!");
}

/* store register 'r' in lvalue 'v' */
void store(int r, SValue *sv)
{
    tcc_error("store unimplemented");
}

/* Return the number of registers needed to return the struct, or 0 if
   returning via struct pointer. */
ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ret_align, int *regsize) {
    tcc_error("gfunc_sret unimplemented");
    return 0;
}

/* Generate function call. The function address is pushed first, then
   all the parameters in call order. This functions pops all the
   parameters and the function address. */
void gfunc_call(int nb_args)
{
    tcc_error("gfunc_call unimplemented");
}

/* generate function prolog of type 't' */
void gfunc_prolog(Sym *func_sym)
{
    //tcc_error("gfunc_prolog unimplemented");
}

/* generate function epilog */
void gfunc_epilog(void)
{
    tcc_error("gfunc_epilog unimplemented");
}

ST_FUNC void gen_fill_nops(int bytes)
{
    tcc_error("gen_fill_nops unimplemented");
}

/* generate a jump to a label */
ST_FUNC int gjmp(int t)
{
    tcc_error("gjmp unimplemented");
  int r;
  if (nocode_wanted)
    return t;
  r=ind;
  //o(0xE0000000|encbranch(r,t,1));
  return r;
}

/* generate a jump to a fixed address */
ST_FUNC void gjmp_addr(int a)
{
    tcc_error("gjmp_addr unimplemented");
  gjmp(a);
}

ST_FUNC int gjmp_cond(int op, int t)
{
    tcc_error("gjmp_cond unimplemented");
  int r;
  if (nocode_wanted)
    return t;
  r=ind;
  //op=mapcc(op);
  //op|=encbranch(r,t,1);
  //o(op);
  return r;
}

ST_FUNC int gjmp_append(int n, int t)
{
    tcc_error("gjmp_append unimplemented");
  return t;
}

/* generate an integer binary operation */
void gen_opi(int op)
{
    switch (op)
    {
    case '*':
        gv2(RC_INT, RC_INT);
        printf("OPI R%d %c R%d\n", vtop[-1].r, op, vtop[0].r);
        vtop[-1].r = vtop[0].r;
        vtop--;
        return;

    case '+':
        gv2(RC_INT, RC_INT);
        printf("OPI R%d %c R%d\n", vtop[-1].r, op, vtop[0].r);
        vtop[-1].r = vtop[0].r;
        vtop--;
        return;

    default:
        break;
    }
    tcc_error("gen_opi unimplemented");
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
    tcc_error("unimplemented gsym_addr!");
}

/*************************************************************/
#endif
/*************************************************************/
