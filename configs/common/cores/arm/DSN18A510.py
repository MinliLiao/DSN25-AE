#-----------------------------------------------------------------------
#                DSN18A510 little core from
# https://developer.arm.com/documentation/PJDOC-466751330-536816/latest
# https://en.wikichip.org/wiki/arm_holdings/microarchitectures/cortex-a510
#-----------------------------------------------------------------------

from m5.objects import *



# Simple function to allow a string of [01x_] to be converted into a
# mask and value for use with MinorFUTiming
def make_implicant(implicant_string):
    ret_mask = 0
    ret_match = 0

    shift = False
    for char in implicant_string:
        char = char.lower()
        if shift:
            ret_mask <<= 1
            ret_match <<= 1

        shift = True
        if char == '_':
            shift = False
        elif char == '0':
            ret_mask |= 1
        elif char == '1':
            ret_mask |= 1
            ret_match |= 1
        elif char == 'x':
            pass
        else:
            print("Can't parse implicant character", char)

    return (ret_mask, ret_match)

#                          ,----- 36 thumb
#                          | ,--- 35 bigThumb
#                          | |,-- 34 aarch64
a64_inst = make_implicant('0_01xx__xxxx_xxxx_xxxx_xxxx__xxxx_xxxx_xxxx_xxxx')
a32_inst = make_implicant('0_00xx__xxxx_xxxx_xxxx_xxxx__xxxx_xxxx_xxxx_xxxx')
t32_inst = make_implicant('1_10xx__xxxx_xxxx_xxxx_xxxx__xxxx_xxxx_xxxx_xxxx')
t16_inst = make_implicant('1_00xx__xxxx_xxxx_xxxx_xxxx__xxxx_xxxx_xxxx_xxxx')
any_inst = make_implicant('x_xxxx__xxxx_xxxx_xxxx_xxxx__xxxx_xxxx_xxxx_xxxx')
#                          | ||
any_a64_inst = \
           make_implicant('x_x1xx__xxxx_xxxx_xxxx_xxxx__xxxx_xxxx_xxxx_xxxx')
any_non_a64_inst = \
           make_implicant('x_x0xx__xxxx_xxxx_xxxx_xxxx__xxxx_xxxx_xxxx_xxxx')

def encode_opcode(pattern):
    def encode(opcode_string):
        a64_mask, a64_match = pattern
        mask, match = make_implicant(opcode_string)
        return (a64_mask | mask), (a64_match | match)
    return encode

a64_opcode = encode_opcode(a64_inst)
a32_opcode = encode_opcode(a32_inst)
t32_opcode = encode_opcode(t32_inst)
t16_opcode = encode_opcode(t16_inst)

# These definitions (in some form) should probably be part of TimingExpr

def literal(value):
    def body(env):
        ret = TimingExprLiteral()
        ret.value = value
        return ret
    return body

def bin(op, left, right):
    def body(env):
        ret = TimingExprBin()
        ret.op = 'timingExpr' + op
        ret.left = left(env)
        ret.right = right(env)
        return ret
    return body

def un(op, arg):
    def body(env):
        ret = TimingExprUn()
        ret.op = 'timingExpr' + op
        ret.arg = arg(env)
        return ret
    return body

def ref(name):
    def body(env):
        if name in env:
            ret = TimingExprRef()
            ret.index = env[name]
        else:
            print("Invalid expression name", name)
            ret = TimingExprNull()
        return ret
    return body

def if_expr(cond, true_expr, false_expr):
    def body(env):
        ret = TimingExprIf()
        ret.cond = cond(env)
        ret.trueExpr = true_expr(env)
        ret.falseExpr = false_expr(env)
        return ret
    return body

def src(index):
    def body(env):
        ret = TimingExprSrcReg()
        ret.index = index
        return ret
    return body

def int_reg(reg):
    def body(env):
        ret = TimingExprReadIntReg()
        ret.reg = reg(env)
        return ret
    return body

def let(bindings, expr):
    def body(env):
        ret = TimingExprLet()
        let_bindings = []
        new_env = {}
        i = 0

        # Make the sub-expression as null to start with
        for name, binding in bindings:
            new_env[name] = i
            i += 1

        defns = []
        # Then apply them to the produced new env
        for i in range(0, len(bindings)):
            name, binding_expr = bindings[i]
            defns.append(binding_expr(new_env))

        ret.defns = defns
        ret.expr = expr(new_env)

        return ret
    return body

def expr_top(expr):
    return expr([])

class DSN18A510_DefaultInt(MinorFUTiming):
    description = 'DSN18A510_DefaultInt'
    mask, match = any_non_a64_inst
    srcRegsRelativeLats = [3, 3, 2, 2, 2, 1, 0]

class DSN18A510_DefaultA64Int(MinorFUTiming):
    description = 'DSN18A510_DefaultA64Int'
    mask, match = any_a64_inst
    # r, l, (c)
    srcRegsRelativeLats = [2, 2, 2, 0]

class DSN18A510_DefaultMul(MinorFUTiming):
    description = 'DSN18A510_DefaultMul'
    mask, match = any_non_a64_inst
    # f, f, f, r, l, a?
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0, 0]

class DSN18A510_DefaultA64Mul(MinorFUTiming):
    description = 'DSN18A510_DefaultA64Mul'
    mask, match = any_a64_inst
    # a (zr for mul), l, r
    srcRegsRelativeLats = [0, 0, 0, 0]
    # extraCommitLat = 1

class DSN18A510_DefaultVfp(MinorFUTiming):
    description = 'DSN18A510_DefaultVfp'
    mask, match = any_non_a64_inst
    # cpsr, z, z, z, cpacr, fpexc, l_lo, r_lo, l_hi, r_hi (from vadd2h)
    srcRegsRelativeLats = [5, 5, 5, 5, 5, 5,  2, 2, 2, 2, 2, 2, 2, 2, 0]

class DSN18A510_DefaultA64Vfp(MinorFUTiming):
    description = 'DSN18A510_DefaultA64Vfp'
    mask, match = any_a64_inst
    # cpsr, cpacr_el1, fpscr_exc, ...
    srcRegsRelativeLats = [5, 5, 5, 2]

class DSN18A510_FMADD_A64(MinorFUTiming):
    description = 'DSN18A510_FMADD_A64'
    mask, match = a64_opcode('0001_1111_0x0x_xxxx__0xxx_xxxx_xxxx_xxxx')
    #                                    t
    # cpsr, cpacr_el1, fpscr_exc, 1, 1, 2, 2, 3, 3, fpscr_exc, d, d, d, d
    srcRegsRelativeLats = [5, 5, 5,  0, 0,  0, 0,  1, 1,  0,  0, 0, 0, 0]

class DSN18A510_FMSUB_D_A64(MinorFUTiming):
    description = 'DSN18A510_FMSUB_D_A64'
    mask, match = a64_opcode('0001_1111_0x0x_xxxx__1xxx_xxxx_xxxx_xxxx')
    #                                    t
    # cpsr, cpacr_el1, fpscr_exc, 1, 1, 2, 2, 3, 3, fpscr_exc, d, d, d, d
    srcRegsRelativeLats = [5, 5, 5,  0, 0,  0, 0,  1, 1,  0,  0, 0, 0, 0]

class DSN18A510_FMOV_A64(MinorFUTiming):
    description = 'DSN18A510_FMOV_A64'
    mask, match = a64_opcode('0001_1110_0x10_0000__0100_00xx_xxxx_xxxx')
    # cpsr, cpacr_el1, fpscr_exc, 1, 1, 2, 2, 3, 3, fpscr_exc, d, d, d, d
    srcRegsRelativeLats = [5, 5, 5, 0]

class DSN18A510_ADD_SUB_vector_scalar_A64(MinorFUTiming):
    description = 'DSN18A510_ADD_SUB_vector_scalar_A64'
    mask, match = a64_opcode('01x1_1110_xx1x_xxxx__1000_01xx_xxxx_xxxx')
    # cpsr, z, z, z, cpacr, fpexc, l0, r0, l1, r1, l2, r2, l3, r3 (for vadd2h)
    srcRegsRelativeLats = [5, 5, 5, 4]


class DSN18A510_ADD_SUB_vector_vector_A64(MinorFUTiming):
    description = 'DSN18A510_ADD_SUB_vector_vector_A64'
    mask, match = a64_opcode('0xx0_1110_xx1x_xxxx__1000_01xx_xxxx_xxxx')
    # cpsr, z, z, z, cpacr, fpexc, l0, r0, l1, r1, l2, r2, l3, r3 (for vadd2h)
    srcRegsRelativeLats = [5, 5, 5, 4]

class DSN18A510_FDIV_scalar_32_A64(MinorFUTiming):
    description = 'DSN18A510_FDIV_scalar_32_A64'
    mask, match = a64_opcode('0001_1110_001x_xxxx__0001_10xx_xxxx_xxxx')
    extraCommitLat = 6
    srcRegsRelativeLats = [0, 0, 0, 20,  4]

class DSN18A510_FDIV_scalar_64_A64(MinorFUTiming):
    description = 'DSN18A510_FDIV_scalar_64_A64'
    mask, match = a64_opcode('0001_1110_011x_xxxx__0001_10xx_xxxx_xxxx')
    extraCommitLat = 15
    srcRegsRelativeLats = [0, 0, 0, 20,  4]

# CINC CINV CSEL CSET CSETM CSINC CSINC CSINV CSINV CSNEG
class DSN18A510_Cxxx_A64(MinorFUTiming):
    description = 'DSN18A510_Cxxx_A64'
    mask, match = a64_opcode('xx01_1010_100x_xxxx_xxxx__0xxx_xxxx_xxxx')
    srcRegsRelativeLats = [3, 3, 3, 2, 2]

class DSN18A510_DefaultMem(MinorFUTiming):
    description = 'DSN18A510_DefaultMem'
    mask, match = any_non_a64_inst
    srcRegsRelativeLats = [1, 1, 1, 1, 1, 2]
    # Assume that LDR/STR take 2 cycles for resolving dependencies
    # (1 + 1 of the FU)
    extraAssumedLat = 2

class DSN18A510_DefaultMem64(MinorFUTiming):
    description = 'DSN18A510_DefaultMem64'
    mask, match = any_a64_inst
    srcRegsRelativeLats = [2]
    # Assume that LDR/STR take 2 cycles for resolving dependencies
    # (1 + 1 of the FU)
    extraAssumedLat = 3

class DSN18A510_DataProcessingMovShiftr(MinorFUTiming):
    description = 'DSN18A510_DataProcessingMovShiftr'
    mask, match = a32_opcode('xxxx_0001_101x_xxxx__xxxx_xxxx_xxx1_xxxx')
    srcRegsRelativeLats = [3, 3, 2, 2, 2, 1, 0]

class DSN18A510_DataProcessingMayShift(MinorFUTiming):
    description = 'DSN18A510_DataProcessingMayShift'
    mask, match = a32_opcode('xxxx_000x_xxxx_xxxx__xxxx_xxxx_xxxx_xxxx')
    srcRegsRelativeLats = [3, 3, 2, 2, 1, 1, 0]

class DSN18A510_DataProcessingNoShift(MinorFUTiming):
    description = 'DSN18A510_DataProcessingNoShift'
    mask, match = a32_opcode('xxxx_000x_xxxx_xxxx__xxxx_0000_0xx0_xxxx')
    srcRegsRelativeLats = [3, 3, 2, 2, 2, 1, 0]

class DSN18A510_DataProcessingAllowShifti(MinorFUTiming):
    description = 'DSN18A510_DataProcessingAllowShifti'
    mask, match = a32_opcode('xxxx_000x_xxxx_xxxx__xxxx_xxxx_xxx0_xxxx')
    srcRegsRelativeLats = [3, 3, 2, 2, 1, 1, 0]

class DSN18A510_DataProcessingSuppressShift(MinorFUTiming):
    description = 'DSN18A510_DataProcessingSuppressShift'
    mask, match = a32_opcode('xxxx_000x_xxxx_xxxx__xxxx_xxxx_xxxx_xxxx')
    srcRegsRelativeLats = []
    suppress = True

class DSN18A510_DataProcessingSuppressBranch(MinorFUTiming):
    description = 'DSN18A510_DataProcessingSuppressBranch'
    mask, match = a32_opcode('xxxx_1010_xxxx_xxxx__xxxx_xxxx_xxxx_xxxx')
    srcRegsRelativeLats = []
    suppress = True

class DSN18A510_BFI_T1(MinorFUTiming):
    description = 'DSN18A510_BFI_T1'
    mask, match = t32_opcode('1111_0x11_0110_xxxx__0xxx_xxxx_xxxx_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 1, 0]

class DSN18A510_BFI_A1(MinorFUTiming):
    description = 'DSN18A510_BFI_A1'
    mask, match = a32_opcode('xxxx_0111_110x_xxxx__xxxx_xxxx_x001_xxxx')
    # f, f, f, dest, src
    srcRegsRelativeLats = [0, 0, 0, 1, 1, 0]

class DSN18A510_CLZ_T1(MinorFUTiming):
    description = 'DSN18A510_CLZ_T1'
    mask, match = t32_opcode('1111_1010_1011_xxxx__1111_xxxx_1000_xxxx')
    srcRegsRelativeLats = [3, 3, 2, 2, 2, 1, 0]

class DSN18A510_CLZ_A1(MinorFUTiming):
    description = 'DSN18A510_CLZ_A1'
    mask, match = a32_opcode('xxxx_0001_0110_xxxx__xxxx_xxxx_0001_xxxx')
    srcRegsRelativeLats = [3, 3, 2, 2, 2, 1, 0]

class DSN18A510_CMN_immediate_A1(MinorFUTiming):
    description = 'DSN18A510_CMN_immediate_A1'
    mask, match = a32_opcode('xxxx_0011_0111_xxxx__xxxx_xxxx_xxxx_xxxx')
    srcRegsRelativeLats = [3, 3, 3, 2, 2, 3, 3, 3, 0]

class DSN18A510_CMN_register_A1(MinorFUTiming):
    description = 'DSN18A510_CMN_register_A1'
    mask, match = a32_opcode('xxxx_0001_0111_xxxx__xxxx_xxxx_xxx0_xxxx')
    srcRegsRelativeLats = [3, 3, 3, 2, 2, 3, 3, 3, 0]

class DSN18A510_CMP_immediate_A1(MinorFUTiming):
    description = 'DSN18A510_CMP_immediate_A1'
    mask, match = a32_opcode('xxxx_0011_0101_xxxx__xxxx_xxxx_xxxx_xxxx')
    srcRegsRelativeLats = [3, 3, 3, 2, 2, 3, 3, 3, 0]

class DSN18A510_CMP_register_A1(MinorFUTiming):
    description = 'DSN18A510_CMP_register_A1'
    mask, match = a32_opcode('xxxx_0001_0101_xxxx__xxxx_xxxx_xxx0_xxxx')
    srcRegsRelativeLats = [3, 3, 3, 2, 2, 3, 3, 3, 0]

class DSN18A510_MLA_T1(MinorFUTiming):
    description = 'DSN18A510_MLA_T1'
    mask, match = t32_opcode('1111_1011_0000_xxxx__xxxx_xxxx_0000_xxxx')
    # z, z, z, a, l?, r?
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 2, 0]

class DSN18A510_MLA_A1(MinorFUTiming):
    description = 'DSN18A510_MLA_A1'
    mask, match = a32_opcode('xxxx_0000_001x_xxxx__xxxx_xxxx_1001_xxxx')
    # z, z, z, a, l?, r?
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 2, 0]

class DSN18A510_MADD_A64(MinorFUTiming):
    description = 'DSN18A510_MADD_A64'
    mask, match = a64_opcode('x001_1011_000x_xxxx__0xxx_xxxx_xxxx_xxxx')
    # a, l?, r?
    srcRegsRelativeLats = [1, 1, 1, 0]
    extraCommitLat = 1

class DSN18A510_MLS_T1(MinorFUTiming):
    description = 'DSN18A510_MLS_T1'
    mask, match = t32_opcode('1111_1011_0000_xxxx__xxxx_xxxx_0001_xxxx')
    # z, z, z, l?, a, r?
    srcRegsRelativeLats = [0, 0, 0, 2, 0, 0, 0]

class DSN18A510_MLS_A1(MinorFUTiming):
    description = 'DSN18A510_MLS_A1'
    mask, match = a32_opcode('xxxx_0000_0110_xxxx__xxxx_xxxx_1001_xxxx')
    # z, z, z, l?, a, r?
    srcRegsRelativeLats = [0, 0, 0, 2, 0, 0, 0]

class DSN18A510_MOVT_A1(MinorFUTiming):
    description = 'DSN18A510_MOVT_A1'
    mask, match = t32_opcode('xxxx_0010_0100_xxxx__xxxx_xxxx_xxxx_xxxx')

class DSN18A510_MUL_T1(MinorFUTiming):
    description = 'DSN18A510_MUL_T1'
    mask, match = t16_opcode('0100_0011_01xx_xxxx')
class DSN18A510_MUL_T2(MinorFUTiming):
    description = 'DSN18A510_MUL_T2'
    mask, match = t32_opcode('1111_1011_0000_xxxx_1111_xxxx_0000_xxxx')

class DSN18A510_PKH_T1(MinorFUTiming):
    description = 'DSN18A510_PKH_T1'
    mask, match = t32_opcode('1110_1010_110x_xxxx__xxxx_xxxx_xxxx_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 2, 1, 0]

class DSN18A510_PKH_A1(MinorFUTiming):
    description = 'DSN18A510_PKH_A1'
    mask, match = a32_opcode('xxxx_0110_1000_xxxx__xxxx_xxxx_xx01_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 2, 1, 0]

class DSN18A510_QADD_QSUB_T1(MinorFUTiming):
    description = 'DSN18A510_QADD_QSUB_T1'
    mask, match = t32_opcode('1111_1010_1000_xxxx__1111_xxxx_10x0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 1, 0]

class DSN18A510_QADD_QSUB_A1(MinorFUTiming):
    description = 'DSN18A510_QADD_QSUB_A1'
    mask, match = a32_opcode('xxxx_0001_00x0_xxxx__xxxx_xxxx_0101_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 1, 0]

# T1 QADD16 QADD8 QSUB16 QSUB8 UQADD16 UQADD8 UQSUB16 UQSUB8
class DSN18A510_QADD_ETC_T1(MinorFUTiming):
    description = 'DSN18A510_QADD_ETC_T1'
    mask, match = t32_opcode('1111_1010_1x0x_xxxx__1111_xxxx_0x01_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 1, 0]

# A1 QADD16 QADD8 QSAX QSUB16 QSUB8 UQADD16 UQADD8 UQASX UQSAX UQSUB16 UQSUB8
class DSN18A510_QADD_ETC_A1(MinorFUTiming):
    description = 'DSN18A510_QADD_ETC_A1'
    mask, match = a32_opcode('xxxx_0110_0x10_xxxx__xxxx_xxxx_xxx1_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 1, 0]

class DSN18A510_QASX_QSAX_UQASX_UQSAX_T1(MinorFUTiming):
    description = 'DSN18A510_QASX_QSAX_UQASX_UQSAX_T1'
    mask, match = t32_opcode('1111_1010_1x10_xxxx__1111_xxxx_0x01_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 1, 0]

class DSN18A510_QDADD_QDSUB_T1(MinorFUTiming):
    description = 'DSN18A510_QDADD_QDSUB_T1'
    mask, match = t32_opcode('1111_1010_1000_xxxx__1111_xxxx_10x1_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 1, 0]

class DSN18A510_QDADD_QDSUB_A1(MinorFUTiming):
    description = 'DSN18A510_QDADD_QSUB_A1'
    mask, match = a32_opcode('xxxx_0001_01x0_xxxx__xxxx_xxxx_0101_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 1, 0]

class DSN18A510_RBIT_A1(MinorFUTiming):
    description = 'DSN18A510_RBIT_A1'
    mask, match = a32_opcode('xxxx_0110_1111_xxxx__xxxx_xxxx_0011_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 0]

class DSN18A510_REV_REV16_A1(MinorFUTiming):
    description = 'DSN18A510_REV_REV16_A1'
    mask, match = a32_opcode('xxxx_0110_1011_xxxx__xxxx_xxxx_x011_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 0]

class DSN18A510_REVSH_A1(MinorFUTiming):
    description = 'DSN18A510_REVSH_A1'
    mask, match = a32_opcode('xxxx_0110_1111_xxxx__xxxx_xxxx_1011_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 0]

class DSN18A510_ADD_ETC_A1(MinorFUTiming):
    description = 'DSN18A510_ADD_ETC_A1'
    mask, match = a32_opcode('xxxx_0110_0xx1_xxxx__xxxx_xxxx_x001_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 2, 2, 0]

class DSN18A510_ADD_ETC_T1(MinorFUTiming):
    description = 'DSN18A510_ADD_ETC_A1'
    mask, match = t32_opcode('1111_1010_100x_xxxx__1111_xxxx_0xx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 2, 2, 0]

class DSN18A510_SASX_SHASX_UASX_UHASX_A1(MinorFUTiming):
    description = 'DSN18A510_SASX_SHASX_UASX_UHASX_A1'
    mask, match = a32_opcode('xxxx_0110_0xx1_xxxx__xxxx_xxxx_0011_xxxx')
    srcRegsRelativeLats = [3, 3, 2, 2, 2, 1, 0]

class DSN18A510_SBFX_UBFX_A1(MinorFUTiming):
    description = 'DSN18A510_SBFX_UBFX_A1'
    mask, match = a32_opcode('xxxx_0111_1x1x_xxxx__xxxx_xxxx_x101_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 0]

### SDIV

sdiv_lat_expr = expr_top(let([
    ('left', un('SignExtend32To64', int_reg(src(4)))),
    ('right', un('SignExtend32To64', int_reg(src(3)))),
    ('either_signed', bin('Or',
        bin('SLessThan', ref('left'), literal(0)),
        bin('SLessThan', ref('right'), literal(0)))),
    ('left_size', un('SizeInBits', un('Abs', ref('left')))),
    ('signed_adjust', if_expr(ref('either_signed'), literal(1), literal(0))),
    ('right_size', un('SizeInBits',
        bin('UDiv', un('Abs', ref('right')),
            if_expr(ref('either_signed'), literal(4), literal(2))))),
    ('left_minus_right', if_expr(
        bin('SLessThan', ref('left_size'), ref('right_size')),
        literal(0),
        bin('Sub', ref('left_size'), ref('right_size'))))
    ],
    bin('Add',
        ref('signed_adjust'),
        if_expr(bin('Equal', ref('right'), literal(0)),
            literal(0),
            bin('UDiv', ref('left_minus_right'), literal(4))))
    ))

sdiv_lat_expr64 = expr_top(let([
    ('left', un('SignExtend32To64', int_reg(src(0)))),
    ('right', un('SignExtend32To64', int_reg(src(1)))),
    ('either_signed', bin('Or',
        bin('SLessThan', ref('left'), literal(0)),
        bin('SLessThan', ref('right'), literal(0)))),
    ('left_size', un('SizeInBits', un('Abs', ref('left')))),
    ('signed_adjust', if_expr(ref('either_signed'), literal(1), literal(0))),
    ('right_size', un('SizeInBits',
        bin('UDiv', un('Abs', ref('right')),
            if_expr(ref('either_signed'), literal(4), literal(2))))),
    ('left_minus_right', if_expr(
        bin('SLessThan', ref('left_size'), ref('right_size')),
        literal(0),
        bin('Sub', ref('left_size'), ref('right_size'))))
    ],
    bin('Add',
        ref('signed_adjust'),
        if_expr(bin('Equal', ref('right'), literal(0)),
            literal(0),
            bin('UDiv', ref('left_minus_right'), literal(4))))
    ))

class DSN18A510_SDIV_A1(MinorFUTiming):
    description = 'DSN18A510_SDIV_A1'
    mask, match = a32_opcode('xxxx_0111_0001_xxxx__xxxx_xxxx_0001_xxxx')
    extraCommitLat = 0
    srcRegsRelativeLats = []
    extraCommitLatExpr = sdiv_lat_expr

class DSN18A510_SDIV_A64(MinorFUTiming):
    description = 'DSN18A510_SDIV_A64'
    mask, match = a64_opcode('x001_1010_110x_xxxx__0000_11xx_xxxx_xxxx')
    extraCommitLat = 0
    srcRegsRelativeLats = []
    extraCommitLatExpr = sdiv_lat_expr64

### SEL

class DSN18A510_SEL_A1(MinorFUTiming):
    description = 'DSN18A510_SEL_A1'
    mask, match = a32_opcode('xxxx_0110_1000_xxxx__xxxx_xxxx_1011_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 2, 2, 0]

class DSN18A510_SEL_A1_Suppress(MinorFUTiming):
    description = 'DSN18A510_SEL_A1_Suppress'
    mask, match = a32_opcode('xxxx_0110_1000_xxxx__xxxx_xxxx_1011_xxxx')
    srcRegsRelativeLats = []
    suppress = True

class DSN18A510_SHSAX_SSAX_UHSAX_USAX_A1(MinorFUTiming):
    description = 'DSN18A510_SHSAX_SSAX_UHSAX_USAX_A1'
    mask, match = a32_opcode('xxxx_0110_0xx1_xxxx__xxxx_xxxx_0101_xxxx')
    # As Default
    srcRegsRelativeLats = [3, 3, 2, 2, 2, 1, 0]

class DSN18A510_USUB_ETC_A1(MinorFUTiming):
    description = 'DSN18A510_USUB_ETC_A1'
    mask, match = a32_opcode('xxxx_0110_0xx1_xxxx__xxxx_xxxx_x111_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 2, 2, 0]

class DSN18A510_SMLABB_T1(MinorFUTiming):
    description = 'DSN18A510_SMLABB_T1'
    mask, match = t32_opcode('1111_1011_0001_xxxx__xxxx_xxxx_00xx_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 2, 0]

class DSN18A510_SMLABB_A1(MinorFUTiming):
    description = 'DSN18A510_SMLABB_A1'
    mask, match = a32_opcode('xxxx_0001_0000_xxxx__xxxx_xxxx_1xx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 2, 0]

class DSN18A510_SMLAD_T1(MinorFUTiming):
    description = 'DSN18A510_SMLAD_T1'
    mask, match = t32_opcode('1111_1011_0010_xxxx__xxxx_xxxx_000x_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 2, 0]

class DSN18A510_SMLAD_A1(MinorFUTiming):
    description = 'DSN18A510_SMLAD_A1'
    mask, match = a32_opcode('xxxx_0111_0000_xxxx__xxxx_xxxx_00x1_xxxx')
    # z, z, z, l, r, a
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 2, 0]

class DSN18A510_SMLAL_T1(MinorFUTiming):
    description = 'DSN18A510_SMLAL_T1'
    mask, match = t32_opcode('1111_1011_1100_xxxx__xxxx_xxxx_0000_xxxx')
class DSN18A510_SMLAL_A1(MinorFUTiming):
    description = 'DSN18A510_SMLAL_A1'
    mask, match = a32_opcode('xxxx_0000_111x_xxxx__xxxx_xxxx_1001_xxxx')

class DSN18A510_SMLALBB_T1(MinorFUTiming):
    description = 'DSN18A510_SMLALBB_T1'
    mask, match = t32_opcode('1111_1011_1100_xxxx__xxxx_xxxx_10xx_xxxx')
class DSN18A510_SMLALBB_A1(MinorFUTiming):
    description = 'DSN18A510_SMLALBB_A1'
    mask, match = a32_opcode('xxxx_0001_0100_xxxx__xxxx_xxxx_1xx0_xxxx')

class DSN18A510_SMLALD_T1(MinorFUTiming):
    description = 'DSN18A510_SMLALD_T1'
    mask, match = t32_opcode('1111_1011_1100_xxxx__xxxx_xxxx_110x_xxxx')
class DSN18A510_SMLALD_A1(MinorFUTiming):
    description = 'DSN18A510_SMLALD_A1'
    mask, match = a32_opcode('xxxx_0111_0100_xxxx__xxxx_xxxx_00x1_xxxx')

class DSN18A510_SMLAWB_T1(MinorFUTiming):
    description = 'DSN18A510_SMLAWB_T1'
    mask, match = t32_opcode('1111_1011_0011_xxxx__xxxx_xxxx_000x_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 2, 0]

class DSN18A510_SMLAWB_A1(MinorFUTiming):
    description = 'DSN18A510_SMLAWB_A1'
    mask, match = a32_opcode('xxxx_0001_0010_xxxx__xxxx_xxxx_1x00_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 2, 0]

class DSN18A510_SMLSD_A1(MinorFUTiming):
    description = 'DSN18A510_SMLSD_A1'
    mask, match = a32_opcode('xxxx_0111_0000_xxxx__xxxx_xxxx_01x1_xxxx')

class DSN18A510_SMLSLD_T1(MinorFUTiming):
    description = 'DSN18A510_SMLSLD_T1'
    mask, match = t32_opcode('1111_1011_1101_xxxx__xxxx_xxxx_110x_xxxx')
class DSN18A510_SMLSLD_A1(MinorFUTiming):
    description = 'DSN18A510_SMLSLD_A1'
    mask, match = a32_opcode('xxxx_0111_0100_xxxx__xxxx_xxxx_01x1_xxxx')

class DSN18A510_SMMLA_T1(MinorFUTiming):
    description = 'DSN18A510_SMMLA_T1'
    mask, match = t32_opcode('1111_1011_0101_xxxx__xxxx_xxxx_000x_xxxx')
    #                                              ^^^^ != 1111
    srcRegsRelativeLats = [0, 0, 0, 2, 0, 0, 0]

class DSN18A510_SMMLA_A1(MinorFUTiming):
    description = 'DSN18A510_SMMLA_A1'
    # Note that this must be after the encoding for SMMUL
    mask, match = a32_opcode('xxxx_0111_0101_xxxx__xxxx_xxxx_00x1_xxxx')
    #                                              ^^^^ != 1111
    srcRegsRelativeLats = [0, 0, 0, 2, 0, 0, 0]

class DSN18A510_SMMLS_T1(MinorFUTiming):
    description = 'DSN18A510_SMMLS_T1'
    mask, match = t32_opcode('1111_1011_0110_xxxx__xxxx_xxxx_000x_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 2, 0, 0, 0]

class DSN18A510_SMMLS_A1(MinorFUTiming):
    description = 'DSN18A510_SMMLS_A1'
    mask, match = a32_opcode('xxxx_0111_0101_xxxx__xxxx_xxxx_11x1_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 2, 0, 0, 0]

class DSN18A510_SMMUL_T1(MinorFUTiming):
    description = 'DSN18A510_SMMUL_T1'
    mask, match = t32_opcode('1111_1011_0101_xxxx__1111_xxxx_000x_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0]

class DSN18A510_SMMUL_A1(MinorFUTiming):
    description = 'DSN18A510_SMMUL_A1'
    mask, match = a32_opcode('xxxx_0111_0101_xxxx__1111_xxxx_00x1_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0]

class DSN18A510_SMUAD_T1(MinorFUTiming):
    description = 'DSN18A510_SMUAD_T1'
    mask, match = t32_opcode('1111_1011_0010_xxxx__1111_xxxx_000x_xxxx')
class DSN18A510_SMUAD_A1(MinorFUTiming):
    description = 'DSN18A510_SMUAD_A1'
    mask, match = a32_opcode('xxxx_0111_0000_xxxx__1111_xxxx_00x1_xxxx')

class DSN18A510_SMULBB_T1(MinorFUTiming):
    description = 'DSN18A510_SMULBB_T1'
    mask, match = t32_opcode('1111_1011_0001_xxxx__1111_xxxx_00xx_xxxx')
class DSN18A510_SMULBB_A1(MinorFUTiming):
    description = 'DSN18A510_SMULBB_A1'
    mask, match = a32_opcode('xxxx_0001_0110_xxxx__xxxx_xxxx_1xx0_xxxx')

class DSN18A510_SMULL_T1(MinorFUTiming):
    description = 'DSN18A510_SMULL_T1'
    mask, match = t32_opcode('1111_1011_1000_xxxx__xxxx_xxxx_0000_xxxx')
class DSN18A510_SMULL_A1(MinorFUTiming):
    description = 'DSN18A510_SMULL_A1'
    mask, match = a32_opcode('xxxx_0000_110x_xxxx__xxxx_xxxx_1001_xxxx')

class DSN18A510_SMULWB_T1(MinorFUTiming):
    description = 'DSN18A510_SMULWB_T1'
    mask, match = t32_opcode('1111_1011_0011_xxxx__1111_xxxx_000x_xxxx')
class DSN18A510_SMULWB_A1(MinorFUTiming):
    description = 'DSN18A510_SMULWB_A1'
    mask, match = a32_opcode('xxxx_0001_0010_xxxx__xxxx_xxxx_1x10_xxxx')

class DSN18A510_SMUSD_T1(MinorFUTiming):
    description = 'DSN18A510_SMUSD_T1'
    mask, match = t32_opcode('1111_1011_0100_xxxx__1111_xxxx_000x_xxxx')
class DSN18A510_SMUSD_A1(MinorFUTiming):
    description = 'DSN18A510_SMUSD_A1'
    mask, match = a32_opcode('xxxx_0111_0000_xxxx__1111_xxxx_01x1_xxxx')

class DSN18A510_SSAT_USAT_no_shift_A1(MinorFUTiming):
    description = 'DSN18A510_SSAT_USAT_no_shift_A1'
    # Order *before* shift
    mask, match = a32_opcode('xxxx_0110_1x1x_xxxx__xxxx_0000_0001_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 2, 0]

class DSN18A510_SSAT_USAT_shift_A1(MinorFUTiming):
    description = 'DSN18A510_SSAT_USAT_shift_A1'
    # Order after shift
    mask, match = a32_opcode('xxxx_0110_1x1x_xxxx__xxxx_xxxx_xx01_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 0]

class DSN18A510_SSAT16_USAT16_A1(MinorFUTiming):
    description = 'DSN18A510_SSAT16_USAT16_A1'
    mask, match = a32_opcode('xxxx_0110_1x10_xxxx__xxxx_xxxx_0011_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 2, 0]

class DSN18A510_SXTAB_T1(MinorFUTiming):
    description = 'DSN18A510_SXTAB_T1'
    mask, match = t32_opcode('1111_1010_0100_xxxx__1111_xxxx_1xxx_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 2, 0]

class DSN18A510_SXTAB_SXTAB16_SXTAH_UXTAB_UXTAB16_UXTAH_A1(MinorFUTiming):
    description = 'DSN18A510_SXTAB_SXTAB16_SXTAH_UXTAB_UXTAB16_UXTAH_A1'
    # Place AFTER DSN18A510_SXTB_SXTB16_SXTH_UXTB_UXTB16_UXTH_A1
    # e6[9d][^f]0070 are undefined
    mask, match = a32_opcode('xxxx_0110_1xxx_xxxx__xxxx_xxxx_0111_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 2, 0]

class DSN18A510_SXTAB16_T1(MinorFUTiming):
    description = 'DSN18A510_SXTAB16_T1'
    mask, match = t32_opcode('1111_1010_0010_xxxx__1111_xxxx_1xxx_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 2, 0]

class DSN18A510_SXTAH_T1(MinorFUTiming):
    description = 'DSN18A510_SXTAH_T1'
    mask, match = t32_opcode('1111_1010_0000_xxxx__1111_xxxx_1xxx_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 2, 0]

class DSN18A510_SXTB_T1(MinorFUTiming):
    description = 'DSN18A510_SXTB_T1'
    mask, match = t16_opcode('1011_0010_01xx_xxxx')
class DSN18A510_SXTB_T2(MinorFUTiming):
    description = 'DSN18A510_SXTB_T2'
    mask, match = t32_opcode('1111_1010_0100_1111__1111_xxxx_1xxx_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 2, 0]

class DSN18A510_SXTB_SXTB16_SXTH_UXTB_UXTB16_UXTH_A1(MinorFUTiming):
    description = 'DSN18A510_SXTB_SXTB16_SXTH_UXTB_UXTB16_UXTH_A1'
    # e6[9d]f0070 are undefined
    mask, match = a32_opcode('xxxx_0110_1xxx_1111__xxxx_xxxx_0111_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 2, 0]

class DSN18A510_SXTB16_T1(MinorFUTiming):
    description = 'DSN18A510_SXTB16_T1'
    mask, match = t32_opcode('1111_1010_0010_1111__1111_xxxx_1xxx_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 2, 0]

class DSN18A510_SXTH_T1(MinorFUTiming):
    description = 'DSN18A510_SXTH_T1'
    mask, match = t16_opcode('1011_0010_00xx_xxxx')
class DSN18A510_SXTH_T2(MinorFUTiming):
    description = 'DSN18A510_SXTH_T2'
    mask, match = t32_opcode('1111_1010_0000_1111__1111_xxxx_1xxx_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 1, 2, 0]

class DSN18A510_UDIV_T1(MinorFUTiming):
    description = 'DSN18A510_UDIV_T1'
    mask, match = t32_opcode('1111_1011_1011_xxxx__xxxx_xxxx_1111_xxxx')

udiv_lat_expr = expr_top(let([
    ('left', int_reg(src(4))),
    ('right', int_reg(src(3))),
    ('left_size', un('SizeInBits', ref('left'))),
    ('right_size', un('SizeInBits',
        bin('UDiv', ref('right'), literal(2)))),
    ('left_minus_right', if_expr(
        bin('SLessThan', ref('left_size'), ref('right_size')),
        literal(0),
        bin('Sub', ref('left_size'), ref('right_size'))))
    ],
    if_expr(bin('Equal', ref('right'), literal(0)),
        literal(0),
        bin('UDiv', ref('left_minus_right'), literal(4)))
    ))

class DSN18A510_UDIV_A1(MinorFUTiming):
    description = 'DSN18A510_UDIV_A1'
    mask, match = a32_opcode('xxxx_0111_0011_xxxx__xxxx_xxxx_0001_xxxx')
    extraCommitLat = 0
    srcRegsRelativeLats = []
    extraCommitLatExpr = udiv_lat_expr

class DSN18A510_UMAAL_T1(MinorFUTiming):
    description = 'DSN18A510_UMAAL_T1'
    mask, match = t32_opcode('1111_1011_1110_xxxx__xxxx_xxxx_0110_xxxx')
    # z, z, z, dlo, dhi, l, r
    extraCommitLat = 1
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0, 0, 0]

class DSN18A510_UMAAL_A1(MinorFUTiming):
    description = 'DSN18A510_UMAAL_A1'
    mask, match = a32_opcode('xxxx_0000_0100_xxxx__xxxx_xxxx_1001_xxxx')
    # z, z, z, dlo, dhi, l, r
    extraCommitLat = 1
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0, 0, 0]

class DSN18A510_UMLAL_T1(MinorFUTiming):
    description = 'DSN18A510_UMLAL_T1'
    mask, match = t32_opcode('1111_1011_1110_xxxx__xxxx_xxxx_0000_xxxx')

class DSN18A510_UMLAL_A1(MinorFUTiming):
    description = 'DSN18A510_UMLAL_A1'
    mask, match = t32_opcode('xxxx_0000_101x_xxxx__xxxx_xxxx_1001_xxxx')

class DSN18A510_UMULL_T1(MinorFUTiming):
    description = 'DSN18A510_UMULL_T1'
    mask, match = t32_opcode('1111_1011_1010_xxxx__xxxx_xxxx_0000_xxxx')

class DSN18A510_UMULL_A1(MinorFUTiming):
    description = 'DSN18A510_UMULL_A1'
    mask, match = a32_opcode('xxxx_0000_100x_xxxx__xxxx_xxxx_1001_xxxx')

class DSN18A510_USAD8_USADA8_A1(MinorFUTiming):
    description = 'DSN18A510_USAD8_USADA8_A1'
    mask, match = a32_opcode('xxxx_0111_1000_xxxx__xxxx_xxxx_0001_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 2, 0]

class DSN18A510_USAD8_USADA8_A1_Suppress(MinorFUTiming):
    description = 'DSN18A510_USAD8_USADA8_A1_Suppress'
    mask, match = a32_opcode('xxxx_0111_1000_xxxx__xxxx_xxxx_0001_xxxx')
    srcRegsRelativeLats = []
    suppress = True

class DSN18A510_VMOV_immediate_A1(MinorFUTiming):
    description = 'DSN18A510_VMOV_register_A1'
    mask, match = a32_opcode('1111_0010_0x10_xxxx_xxxx_0001_xxx1_xxxx')
    # cpsr, z, z, z, hcptr, nsacr, cpacr, fpexc, scr
    srcRegsRelativeLats = [5, 5, 5, 5, 5, 5, 5, 5, 5, 0]

class DSN18A510_VMRS_A1(MinorFUTiming):
    description = 'DSN18A510_VMRS_A1'
    mask, match = a32_opcode('xxxx_1110_1111_0001_xxxx_1010_xxx1_xxxx')
    # cpsr,z,z,z,hcptr,nsacr,cpacr,scr,r42
    srcRegsRelativeLats = [5, 5, 5, 5, 5, 5, 5, 5, 5, 0]

class DSN18A510_VMOV_register_A2(MinorFUTiming):
    description = 'DSN18A510_VMOV_register_A2'
    mask, match = a32_opcode('xxxx_1110_1x11_0000_xxxx_101x_01x0_xxxx')
    # cpsr, z, r39, z, hcptr, nsacr, cpacr, fpexc, scr, f4, f5, f0, f1
    srcRegsRelativeLats = \
        [5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 0]

# VADD.I16 D/VADD.F32 D/VADD.I8 D/VADD.I32 D
class DSN18A510_VADD2H_A32(MinorFUTiming):
    description = 'Vadd2hALU'
    mask, match = a32_opcode('1111_0010_0xxx_xxxx__xxxx_1000_xxx0_xxxx')
    # cpsr, z, z, z, cpacr, fpexc, l0, r0, l1, r1, l2, r2, l3, r3 (for vadd2h)
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 0]

# VAQQHN.I16 Q/VAQQHN.I32 Q/VAQQHN.I64 Q
class DSN18A510_VADDHN_A32(MinorFUTiming):
    description = 'VaddhnALU'
    mask, match = a32_opcode('1111_0010_1xxx_xxxx__xxxx_0100_x0x0_xxxx')
    # cpsr, z, z, z, cpacr, fpexc, l0, l1, l2, l3, r0, r1, r2, r3
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  3, 3, 3, 3, 3, 3, 3, 3, 0]

class DSN18A510_VADDL_A32(MinorFUTiming):
    description = 'VaddlALU'
    mask, match = a32_opcode('1111_001x_1xxx_xxxx__xxxx_0000_x0x0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  3, 3, 3, 3, 3, 3, 3, 3, 0]

class DSN18A510_VADDW_A32(MinorFUTiming):
    description = 'DSN18A510_VADDW_A32'
    mask, match = a32_opcode('1111_001x_1xxx_xxxx__xxxx_0001_x0x0_xxxx')
    # cpsr, z, z, z, cpacr, fpexc, l0, l1, l2, l3, r0, r1
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 3, 3, 0]

# VHADD/VHSUB S8,S16,S32,U8,U16,U32 Q and D
class DSN18A510_VHADD_A32(MinorFUTiming):
    description = 'DSN18A510_VHADD_A32'
    mask, match = a32_opcode('1111_001x_0xxx_xxxx__xxxx_00x0_xxx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 0]

class DSN18A510_VPADAL_A32(MinorFUTiming):
    description = 'VpadalALU'
    mask, match = a32_opcode('1111_0011_1x11_xx00__xxxx_0110_xxx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  2, 2, 2, 2, 2, 2, 2, 2, 0]

# VPADDH.I16
class DSN18A510_VPADDH_A32(MinorFUTiming):
    description = 'VpaddhALU'
    mask, match = a32_opcode('1111_0010_0xxx_xxxx__xxxx_1011_xxx1_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  3, 3, 3, 3, 3, 3, 3, 3, 0]

# VPADDH.F32
class DSN18A510_VPADDS_A32(MinorFUTiming):
    description = 'VpaddsALU'
    mask, match = a32_opcode('1111_0011_0x0x_xxxx__xxxx_1101_xxx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  2, 2, 2, 2, 2, 2, 2, 2, 0]

# VPADDL.S16
class DSN18A510_VPADDL_A32(MinorFUTiming):
    description = 'VpaddlALU'
    mask, match = a32_opcode('1111_0011_1x11_xx00__xxxx_0010_xxx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  3, 3, 3, 3, 3, 3, 3, 3, 0]

# VRADDHN.I16
class DSN18A510_VRADDHN_A32(MinorFUTiming):
    description = 'DSN18A510_VRADDHN_A32'
    mask, match = a32_opcode('1111_0011_1xxx_xxxx__xxxx_0100_x0x0_xxxx')
    # cpsr, z, z, z, cpacr, fpexc, l0, l1, l2, l3, r0, r1, r2, r3
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 0]

class DSN18A510_VRHADD_A32(MinorFUTiming):
    description = 'VrhaddALU'
    mask, match = a32_opcode('1111_001x_0xxx_xxxx__xxxx_0001_xxx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 0]

class DSN18A510_VQADD_A32(MinorFUTiming):
    description = 'VqaddALU'
    mask, match = a32_opcode('1111_001x_0xxx_xxxx__xxxx_0000_xxx1_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  3, 3, 3, 3, 3, 3, 3, 3, 0]

class DSN18A510_VANDQ_A32(MinorFUTiming):
    description = 'VandqALU'
    mask, match = a32_opcode('1111_0010_0x00_xxxx__xxxx_0001_xxx1_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  5, 5, 5, 5, 5, 5, 5, 5, 0]

# VMUL (integer)
class DSN18A510_VMULI_A32(MinorFUTiming):
    description = 'VmuliALU'
    mask, match = a32_opcode('1111_001x_0xxx_xxxx__xxxx_1001_xxx1_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  2, 2, 2, 2, 2, 2, 2, 2, 0]

# VBIC (reg)
class DSN18A510_VBIC_A32(MinorFUTiming):
    description = 'VbicALU'
    mask, match = a32_opcode('1111_0010_0x01_xxxx__xxxx_0001_xxx1_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  5, 5, 5, 5, 5, 5, 5, 5, 0]

# VBIF VBIT VBSL
class DSN18A510_VBIF_ETC_A32(MinorFUTiming):
    description = 'VbifALU'
    mask, match = a32_opcode('1111_0011_0xxx_xxxx__xxxx_0001_xxx1_xxxx')
    srcRegsRelativeLats = \
        [0, 0, 0, 0, 0, 0,  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0]

class DSN18A510_VACGE_A32(MinorFUTiming):
    description = 'VacgeALU'
    mask, match = a32_opcode('1111_0011_0xxx_xxxx__xxxx_1110_xxx1_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 4, 0]

# VCEQ.F32
class DSN18A510_VCEQ_A32(MinorFUTiming):
    description = 'VceqALU'
    mask, match = a32_opcode('1111_0010_0x0x_xxxx__xxxx_1110_xxx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 4, 0]

# VCEQ.[IS]... register
class DSN18A510_VCEQI_A32(MinorFUTiming):
    description = 'VceqiALU'
    mask, match = a32_opcode('1111_0011_0xxx_xxxx__xxxx_1000_xxx1_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 4, 0]

# VCEQ.[IS]... immediate
class DSN18A510_VCEQII_A32(MinorFUTiming):
    description = 'DSN18A510_VCEQII_A32'
    mask, match = a32_opcode('1111_0011_1x11_xx01__xxxx_0x01_0xx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 4, 0]

class DSN18A510_VTST_A32(MinorFUTiming):
    description = 'DSN18A510_VTST_A32'
    mask, match = a32_opcode('1111_0010_0xxx_xxxx__xxxx_1000_xxx1_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  3, 3, 3, 3, 3, 3, 3, 3, 3, 0]

class DSN18A510_VCLZ_A32(MinorFUTiming):
    description = 'DSN18A510_VCLZ_A32'
    mask, match = a32_opcode('1111_0011_1x11_xx00__xxxx_0100_1xx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 4, 0]

class DSN18A510_VCNT_A32(MinorFUTiming):
    description = 'DSN18A510_VCNT_A32'
    mask, match = a32_opcode('1111_0011_1x11_xx00__xxxx_0101_0xx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 4, 0]

class DSN18A510_VEXT_A32(MinorFUTiming):
    description = 'DSN18A510_VCNT_A32'
    mask, match = a32_opcode('1111_0010_1x11_xxxx__xxxx_xxxx_xxx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 4, 0]

# VMAX VMIN integer
class DSN18A510_VMAXI_A32(MinorFUTiming):
    description = 'DSN18A510_VMAXI_A32'
    mask, match = a32_opcode('1111_001x_0xxx_xxxx__xxxx_0110_xxxx_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 4, 0]

# VMAX VMIN float
class DSN18A510_VMAXS_A32(MinorFUTiming):
    description = 'DSN18A510_VMAXS_A32'
    mask, match = a32_opcode('1111_0010_0xxx_xxxx__xxxx_1111_xxx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  2, 2, 2, 2, 2, 2, 2, 2, 2, 0]

# VNEG integer
class DSN18A510_VNEGI_A32(MinorFUTiming):
    description = 'DSN18A510_VNEGI_A32'
    mask, match = a32_opcode('1111_0011_1x11_xx01__xxxx_0x11_1xx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 4, 0]

# VNEG float
class DSN18A510_VNEGF_A32(MinorFUTiming):
    description = 'DSN18A510_VNEGF_A32'
    mask, match = a32_opcode('xxxx_1110_1x11_0001__xxxx_101x_01x0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  2, 2, 2, 2, 2, 2, 2, 2, 2, 0]

# VREV16 VREV32 VREV64
class DSN18A510_VREVN_A32(MinorFUTiming):
    description = 'DSN18A510_VREVN_A32'
    mask, match = a32_opcode('1111_0011_1x11_xx00__xxxx_000x_xxx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 4, 0]

class DSN18A510_VQNEG_A32(MinorFUTiming):
    description = 'DSN18A510_VQNEG_A32'
    mask, match = a32_opcode('1111_0011_1x11_xx00__xxxx_0111_1xx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  3, 3, 3, 3, 3, 3, 3, 3, 3, 0]

class DSN18A510_VSWP_A32(MinorFUTiming):
    description = 'DSN18A510_VSWP_A32'
    mask, match = a32_opcode('1111_0011_1x11_xx10__xxxx_0000_0xx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 0]

class DSN18A510_VTRN_A32(MinorFUTiming):
    description = 'DSN18A510_VTRN_A32'
    mask, match = a32_opcode('1111_0011_1x11_xx10__xxxx_0000_1xx0_xxxx')
    # cpsr, z, z, z, cpact, fpexc, o0, d0, o1, d1, o2, d2, o3, d3
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  2, 2, 2, 2, 2, 2, 2, 2, 0]

# VQMOVN VQMOVUN
class DSN18A510_VQMOVN_A32(MinorFUTiming):
    description = 'DSN18A510_VQMOVN_A32'
    mask, match = a32_opcode('1111_0011_1x11_xx10__xxxx_0010_xxx0_xxxx')
    # cpsr, z, z, z, cpact, fpexc, o[0], o[1], o[2], o[3], fpscr
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  2, 2, 2, 2,  2, 0]

# VUZP double word
class DSN18A510_VUZP_A32(MinorFUTiming):
    description = 'DSN18A510_VUZP_A32'
    mask, match = a32_opcode('1111_0011_1x11_xx10__xxxx_0001_00x0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  3, 3, 3, 3, 3, 3, 3, 3, 0]

# VDIV.F32
class DSN18A510_VDIV32_A32(MinorFUTiming):
    description = 'DSN18A510_VDIV32_A32'
    mask, match = a32_opcode('xxxx_1110_1x00_xxxx__xxxx_1010_x0x0_xxxx')
    # cpsr, z, z, z, cpact, fpexc, fpscr_exc, l, r
    extraCommitLat = 9
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0, 20,  4, 4, 0]

# VDIV.F64
class DSN18A510_VDIV64_A32(MinorFUTiming):
    description = 'DSN18A510_VDIV64_A32'
    mask, match = a32_opcode('xxxx_1110_1x00_xxxx__xxxx_1011_x0x0_xxxx')
    # cpsr, z, z, z, cpact, fpexc, fpscr_exc, l, r
    extraCommitLat = 18
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0, 20,  4, 4, 0]

class DSN18A510_VZIP_A32(MinorFUTiming):
    description = 'DSN18A510_VZIP_A32'
    mask, match = a32_opcode('1111_0011_1x11_xx10__xxxx_0001_1xx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 4, 4, 4, 4, 0]

# VPMAX integer
class DSN18A510_VPMAX_A32(MinorFUTiming):
    description = 'DSN18A510_VPMAX_A32'
    mask, match = a32_opcode('1111_001x_0xxx_xxxx__xxxx_1010_xxxx_xxxx')
    # cpsr, z, z, z, cpact, fpexc, l0, r0, l1, r1, fpscr
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4,  4, 0]

# VPMAX float
class DSN18A510_VPMAXF_A32(MinorFUTiming):
    description = 'DSN18A510_VPMAXF_A32'
    mask, match = a32_opcode('1111_0011_0xxx_xxxx__xxxx_1111_xxx0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  2, 2, 2, 2, 0]

class DSN18A510_VMOVN_A32(MinorFUTiming):
    description = 'DSN18A510_VMOVN_A32'
    mask, match = a32_opcode('1111_0011_1x11_xx10__xxxx_0010_00x0_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 0]

class DSN18A510_VMOVL_A32(MinorFUTiming):
    description = 'DSN18A510_VMOVL_A32'
    mask, match = a32_opcode('1111_001x_1xxx_x000__xxxx_1010_00x1_xxxx')
    srcRegsRelativeLats = [0, 0, 0, 0, 0, 0,  4, 4, 4, 4, 0]

# VSQRT.F64
class DSN18A510_VSQRT64_A32(MinorFUTiming):
    description = 'DSN18A510_VSQRT64_A32'
    mask, match = a32_opcode('xxxx_1110_1x11_0001__xxxx_1011_11x0_xxxx')
    extraCommitLat = 18
    srcRegsRelativeLats = []

# VSQRT.F32
class DSN18A510_VSQRT32_A32(MinorFUTiming):
    description = 'DSN18A510_VSQRT32_A32'
    mask, match = a32_opcode('xxxx_1110_1x11_0001__xxxx_1010_11x0_xxxx')
    extraCommitLat = 9
    srcRegsRelativeLats = []

class DSN18A510_VALU(MinorFU):
    opClasses = minorMakeOpClassSet([
        'FloatAdd', 'FloatCmp', 'FloatCvt', 'FloatMult', 'FloatDiv',
        'FloatSqrt', 'FloatMisc', 'FloatMultAcc',
        'SimdAdd', 'SimdAddAcc', 'SimdAlu', 'SimdCmp', 'SimdCvt',
        'SimdMisc', 'SimdMult', 'SimdMultAcc', 'SimdShift', 'SimdShiftAcc',
        'SimdDiv', 'SimdReduceAdd', 'SimdReduceAlu', 'SimdReduceCmp',
        'SimdSqrt', 'SimdFloatAdd', 'SimdFloatAlu', 'SimdFloatCmp',
        'SimdFloatCvt', 'SimdFloatDiv', 'SimdFloatMisc', 'SimdFloatMult',
        'SimdFloatSqrt', 'SimdFloatReduceAdd', 'SimdFloatReduceCmp'])

    timings = [
        # VUZP and VZIP must be before VADDW/L
        DSN18A510_VUZP_A32(), DSN18A510_VZIP_A32(),
        DSN18A510_VADD2H_A32(), DSN18A510_VADDHN_A32(),
        DSN18A510_VADDL_A32(), DSN18A510_VADDW_A32(),
        DSN18A510_VHADD_A32(), DSN18A510_VPADAL_A32(),
        DSN18A510_VPADDH_A32(), DSN18A510_VPADDS_A32(),
        DSN18A510_VPADDL_A32(), DSN18A510_VRADDHN_A32(),
        DSN18A510_VRHADD_A32(), DSN18A510_VQADD_A32(),
        DSN18A510_VANDQ_A32(), DSN18A510_VBIC_A32(),
        DSN18A510_VBIF_ETC_A32(), DSN18A510_VACGE_A32(),
        DSN18A510_VCEQ_A32(), DSN18A510_VCEQI_A32(),
        DSN18A510_VCEQII_A32(), DSN18A510_VTST_A32(),
        DSN18A510_VCLZ_A32(), DSN18A510_VCNT_A32(),
        DSN18A510_VEXT_A32(), DSN18A510_VMAXI_A32(),
        DSN18A510_VMAXS_A32(), DSN18A510_VNEGI_A32(),
        DSN18A510_VNEGF_A32(), DSN18A510_VREVN_A32(),
        DSN18A510_VQNEG_A32(), DSN18A510_VSWP_A32(),
        DSN18A510_VTRN_A32(), DSN18A510_VMOVN_A32(),
        DSN18A510_VMRS_A1(),
        DSN18A510_VMOV_immediate_A1(),
        DSN18A510_VMOV_register_A2(),
        DSN18A510_VQMOVN_A32(), DSN18A510_VMOVL_A32(),
        DSN18A510_VDIV32_A32(), DSN18A510_VDIV64_A32(),
        DSN18A510_VSQRT32_A32(), DSN18A510_VSQRT64_A32(),
        DSN18A510_VMULI_A32(),
        DSN18A510_VPMAX_A32(), DSN18A510_VPMAXF_A32(),
        # Add before here
        DSN18A510_FMSUB_D_A64(),
        DSN18A510_FMOV_A64(),
        DSN18A510_ADD_SUB_vector_scalar_A64(),
        DSN18A510_ADD_SUB_vector_vector_A64(),
        DSN18A510_FDIV_scalar_32_A64(), DSN18A510_FDIV_scalar_64_A64(),
        DSN18A510_DefaultA64Vfp(),
        DSN18A510_DefaultVfp()]

    opLat = 5

class DSN18A510_PALU(MinorFU):
    opClasses = minorMakeOpClassSet(['SimdPredAlu'])
    opLat = 1

class DSN18A510_VMAC(MinorFU):
    opClasses = minorMakeOpClassSet(['SimdFloatMultAcc'])
    timings = [DSN18A510_FMADD_A64()]
    opLat = 4

class DSN18A510_Branch(MinorFU):
    opClasses = minorMakeOpClassSet(['Branch'])
    opLat = 1

class DSN18A510_ALU(MinorFU):
    opClasses = minorMakeOpClassSet(['IntAlu', 'IntMult'])
    # IMPORTANT! Keep the order below, add new entries *at the head*
    timings = [
        DSN18A510_SSAT_USAT_no_shift_A1(),
        DSN18A510_SSAT_USAT_shift_A1(),
        DSN18A510_SSAT16_USAT16_A1(),
        DSN18A510_QADD_QSUB_A1(),
        DSN18A510_QADD_QSUB_T1(),
        DSN18A510_QADD_ETC_A1(),
        DSN18A510_QASX_QSAX_UQASX_UQSAX_T1(),
        DSN18A510_QADD_ETC_T1(),
        DSN18A510_USUB_ETC_A1(),
        DSN18A510_ADD_ETC_A1(),
        DSN18A510_ADD_ETC_T1(),
        DSN18A510_QDADD_QDSUB_A1(),
        DSN18A510_QDADD_QDSUB_T1(),
        DSN18A510_SASX_SHASX_UASX_UHASX_A1(),
        DSN18A510_SHSAX_SSAX_UHSAX_USAX_A1(),
        DSN18A510_SXTB_SXTB16_SXTH_UXTB_UXTB16_UXTH_A1(),

        # Must be after DSN18A510_SXTB_SXTB16_SXTH_UXTB_UXTB16_UXTH_A1
        DSN18A510_SXTAB_SXTAB16_SXTAH_UXTAB_UXTAB16_UXTAH_A1(),

        DSN18A510_SXTAB_T1(),
        DSN18A510_SXTAB16_T1(),
        DSN18A510_SXTAH_T1(),
        DSN18A510_SXTB_T2(),
        DSN18A510_SXTB16_T1(),
        DSN18A510_SXTH_T2(),

        DSN18A510_PKH_A1(),
        DSN18A510_PKH_T1(),
        DSN18A510_SBFX_UBFX_A1(),
        DSN18A510_SEL_A1_Suppress(),
        DSN18A510_RBIT_A1(),
        DSN18A510_REV_REV16_A1(),
        DSN18A510_REVSH_A1(),
        DSN18A510_USAD8_USADA8_A1_Suppress(),
        DSN18A510_BFI_A1(),
        DSN18A510_BFI_T1(),

        DSN18A510_CMN_register_A1(), # Need to check for shift
        DSN18A510_CMN_immediate_A1(),
        DSN18A510_CMP_register_A1(), # Need to check for shift
        DSN18A510_CMP_immediate_A1(),

        DSN18A510_DataProcessingNoShift(),
        DSN18A510_DataProcessingAllowShifti(),
        # DSN18A510_DataProcessingAllowMovShiftr(),

        # Data processing ops that match SuppressShift but are *not*
        # to be suppressed here
        DSN18A510_CLZ_A1(),
        DSN18A510_CLZ_T1(),
        DSN18A510_DataProcessingSuppressShift(),
        # Can you dual issue a branch?
        # DSN18A510_DataProcessingSuppressBranch(),
        DSN18A510_Cxxx_A64(),

        DSN18A510_DefaultA64Int(),
        DSN18A510_DefaultInt(),

        DSN18A510_MLA_A1(), DSN18A510_MLA_T1(),
        DSN18A510_MLS_A1(), DSN18A510_MLS_T1(),
        DSN18A510_SMLABB_A1(), DSN18A510_SMLABB_T1(),
        DSN18A510_SMLAWB_A1(), DSN18A510_SMLAWB_T1(),
        DSN18A510_SMLAD_A1(), DSN18A510_SMLAD_T1(),
        DSN18A510_SMMUL_A1(), DSN18A510_SMMUL_T1(),
        # SMMUL_A1 must be before SMMLA_A1
        DSN18A510_SMMLA_A1(), DSN18A510_SMMLA_T1(),
        DSN18A510_SMMLS_A1(), DSN18A510_SMMLS_T1(),
        DSN18A510_UMAAL_A1(), DSN18A510_UMAAL_T1(),

        DSN18A510_MADD_A64(),
        DSN18A510_DefaultA64Mul(),
        DSN18A510_DefaultMul()]       
    opLat = 1

class DSN18A510_Crypto(MinorFU):
    opClasses = minorMakeOpClassSet(['SimdAes', 'SimdAesMix',
        'SimdSha1Hash', 'SimdSha1Hash2', 'SimdSha256Hash',
        'SimdSha256Hash2', 'SimdShaSigma2', 'SimdShaSigma3'])
    opLat = 1

class DSN18A510_DIV(MinorFU):
    opClasses = minorMakeOpClassSet(['IntDiv'])
    timings = [DSN18A510_SDIV_A1(), DSN18A510_UDIV_A1(),
        DSN18A510_SDIV_A64()]
    issueLat = 3
    opLat = 8

class DSN18A510_Load_Store(MinorFU):
    opClasses = minorMakeOpClassSet(['MemRead', 'MemWrite', 'FloatMemRead',
                                     'FloatMemWrite'])
    timings = [DSN18A510_DefaultMem(), DSN18A510_DefaultMem64()]
    opLat = 1
    cantForwardFromFUIndices = [5] # Mem (this FU)

class DSN18A510_Load(MinorFU):
    opClasses = minorMakeOpClassSet(['MemRead', 'FloatMemRead'])
    timings = [DSN18A510_DefaultMem(), DSN18A510_DefaultMem64()]
    opLat = 1
    cantForwardFromFUIndices = [5] # Mem (this FU)

class DSN18A510_MiscFU(MinorFU):
    opClasses = minorMakeOpClassSet(['IprAccess', 'InstPrefetch'])
    opLat = 1

class DSN18A510_FUPool(MinorFUPool):
    funcUnits = [
            DSN18A510_ALU(),
            DSN18A510_ALU(),
            DSN18A510_ALU(),
            DSN18A510_Branch(),
            DSN18A510_DIV(),
            DSN18A510_Load_Store(),
            DSN18A510_Load(),
            DSN18A510_PALU(),
            DSN18A510_Crypto(),
            DSN18A510_VALU(),
            DSN18A510_VMAC(),
            DSN18A510_Crypto(),
            DSN18A510_VALU(),
            DSN18A510_VMAC(),
            DSN18A510_MiscFU()
            ]

class DSN18A510_MMU(ArmMMU):
    itb = ArmTLB(entry_type="instruction", size=128)
    dtb = ArmTLB(entry_type="data", size=128)

class DSN18A510_ICache(Cache):
    data_latency = 1
    tag_latency = 1
    response_latency = 1
    mshrs = 6
    tgts_per_mshr = 8
    size = '2kB'
    assoc = 2
    # No prefetcher, this is handled by the core

class DSN18A510_DCache(Cache):
    data_latency = 1
    tag_latency = 1
    response_latency = 1
    mshrs = 6
    tgts_per_mshr = 8
    size = '2kB'
    assoc = 4
    write_buffers = 4

class DSN18A510_L2(Cache):
    data_latency = 2
    tag_latency = 2
    response_latency = 2
    mshrs = 6
    tgts_per_mshr = 8
    size = '16kB'
    assoc = 4
    write_buffers = 4
    # Simple stride prefetcher
    prefetch_on_access = True
    prefetcher = StridePrefetcher(degree=1, latency = 1)
    tags = BaseSetAssoc()
    replacement_policy = RandomRP()

class DSN18A510(ArmMinorCPU):
    # Inherit the doc string from the module to avoid repeating it
    # here.
    __doc__ = __doc__

    fetch1FetchLimit = 1
    #fetch1LineSnapWidth = 16
    decodeInputWidth = 1
    executeInputWidth = 1
    executeIssueLimit = 1
    executeMemoryIssueLimit = 1
    executeCommitLimit = 1
    executeMemoryCommitLimit = 1
    executeMemoryWidth = 16
    executeMaxAccessesInMemory = 1
    executeLSQRequestsQueueSize = 8
    executeLSQTransfersQueueSize = 8
    executeLSQStoreBufferSize = 16
    #executeAllowEarlyMemoryIssue = True
    executeFuncUnits = DSN18A510_FUPool()
    branchPred = MultiperspectivePerceptronTAGE8KB()
    mmu = DSN18A510_MMU()

__all__ = [
   "DSN18A510_ITB", "DSN18A510_DTB",
    "DSN18A510_ICache", "DSN18A510_DCache", "DSN18A510_L2",
    "DSN18A510",
]
