# =============================================================================
# The Lost Vikings Virtual Machine Compiler
#
# Ryan Mallon, 2017, <rmallon@gmail.com>
#
# This file is part of The Lost Vikings Library/Tools
#
# To the extent possible under law, the author(s) have dedicated all
# copyright and related and neighboring rights to this software to
# the public domain worldwide. This software is distributed without
# any warranty.  You should have received a copy of the CC0 Public
# Domain Dedication along with this software. If not, see
#
# <http://creativecommons.org/publicdomain/zero/1.0/>.
# =============================================================================
#
# This is very much a work in progress. Several parts of the compiler are not
# yet implemented (for example, the only working comparison operators are
# '==' and '!='. It should be relatively easy to fix these issues.
#
# Patching the Game Binary
# ------------------------
#
# The game stores the virtual machine programs chunk for the current world in
# extra segment (ES) at a location allocated by the DOS memory allocation API.
# The original game only allocates 0xc000 bytes (48K) for this region.
# Because the compiler does not generate efficient code it is easy to exceed
# this limit. The game binary can be patched to extend the size of the
# allocation.
#
# If you have VIKINGS.EXE with MD5 8807a952565c13e428988d87021095c8 you can
# edit the binary in hex editor and change the byte at offset 0x2d61 from
# 0x0c to 0x0d.
#
# If you have a different version of VIKINGS.EXE then you will need a
# disassembler such as IDA to find the correct offset to patch. Look for a
# function which uses DOS int 21h, operation 48h, and then for a call to that
# function which looks like:
#
#   mov  bx, 0x0c00
#   call alloc_mem
#   mov  <word>, ax
#
# The call should be in the third function which is called from the program
# start. Patch the first mov instruction to change the size to 0x0d00.
# Please share the patch offset for any alternative versions so I can include
# them here.
#
# Running the Compiler
# --------------------
#
# To run the compiler you first need to unpack the original programs chunk
# for the world you want to modify. The chunk indexes are listed in
# liblv/lv_level.c.
#
# For example, to extract the programs for the space world (chunk 449):
#
#   ./pack_tool DATA.DAT -d 449:orig_progs.bin
#
# The compiler requires a C preprocessor (cpp) to be in your path. The
# following arguments are required:
#
#   -b: Base address to patch the new program at. This should be a value
#       larger than the size of the orig_progs.bin file.
#
#   -p: Name of the extracted original programs chunk file.
#
#   -i: Index of the object to modify. See object_types.txt for a list.
#
#   -o: Output patched programs chunk file.
#
# For example, to compile a program to replace the turret object (0x04):
#
#   python ./compiler -b 0xc000 new_turret.lvc orig_progs.bin -o new_progs.bin
#
# Next, the modified chunk needs to be packed to create a new data file:
#
#  ./pack_tool DATA.DAT -r 449:new_progs.bin -o NEW.DAT
#
# Copy NEW.DAT to DATA.DAT in your Lost Vikings game directory (remember to
# keep a backup) and run the game.
#

import collections
import subprocess
import argparse
import struct
import sys
import re

class Token(object):
    def __init__(self, kind, value, line_num):
        self.kind = kind
        self.value = value
        self.line_num = line_num

    def __str__(self):
        string = "[{}] {}".format(self.line_num, self.kind)
        if self.kind != self.value:
            string += " ({})".format(self.value)
        return string

class Lexer(object):
    token_dict = collections.OrderedDict([
        ("//",                          "comment"),
        ("#",                           "comment"),
        ("==",                          "=="),
        ("=",                           "="),
        ("\+",                          "+"),
        ("-" ,                          "-"),
        ("<" ,                          "<"),
        (">" ,                          ">"),
        ("<=",                          "<="),
        (">=",                          ">="),
        ("!=",                          "!="),
        ("&",                           "&"),
        ("\^",                          "^"),
        (";" ,                          ";"),
        ("\(",                          "("),
        ("\)",                          ")"),
        ("{",                           "{"),
        ("}",                           "}"),
        (",",                           ","),
        ("if",                          "if"),
        ("else",                        "else"),
        ("elif",                        "elif"),
        ("function",                    "function"),
        ("while",                       "while"),
        ("call",                        "call"),
        ("0x[0-9a-fA-F]+",              "number"),
        ("[0-9]+",                      "number"),
        ("[a-zA-Z_][a-zA-Z0-9_.\[\]]+", "identifier"),
        ])

    def __init__(self, lines):
        self.lines = lines
        self.tokens = collections.deque()

    def match_token(self, line):
        for token_re, token_kind in Lexer.token_dict.iteritems():
            m = re.match(token_re, line)
            if m:
                return (token_kind, m.group(0))

        return (None, None)

    def lex_line(self, line, line_num):
        while True:
            line = line.lstrip()
            if len(line) == 0:
                break

            token_kind, token_value = self.match_token(line)
            if not token_kind:
                # FIXME - error message
                print("Unknown symbol on line {}: [{}]".format(line_num, line))
                sys.exit(0)

            if token_kind == "comment":
                break

            token_value_len = len(token_value)

            if token_kind == "number":
                if token_value.startswith("0x"):
                    token_value = int(token_value, 16)
                else:
                    token_value = int(token_value, 10)

            self.tokens.append(Token(token_kind, token_value, line_num))
            line = line[token_value_len:]

    def lex(self):
        for line_num, line in enumerate(self.lines):
            self.lex_line(line, line_num + 1)

class Instruction(object):
    # Operand indicies for patching jump instructions
    jump_operands = {
        0x03 : 0,
        0x1a : 1,
        0x30 : 1,
        0x32 : 1,
        0x37 : 1,
        0x38 : 1,
        0x74 : 1,
        0x79 : 1,
        }

    def __init__(self, addr, opcode, *args):
        self.addr = addr
        self.opcode = opcode
        self.operands = []

        for operand in args:
            self.operands.append(operand)

    def __len__(self):
        length = 1
        for _, operand_length in self.operands:
            length += operand_length

        return length

    def __str__(self):
        string = "[{:04x}] {:02x} ".format(self.addr, self.opcode)
        for value, size in self.operands:
            if size == 1:
                string += "{:02x} ".format(value)
            else:
                string += "{:04x} ".format(value)

        # Pretty print instructions
        strings = {
            0x00: (lambda self: "yield()"),
            0x03: (lambda self: "jump {:04x}".format(self.operands[0][0])),
            0x06: (lambda self: "return"),
            0x51: (lambda self: "var = 0x{:04x}".format(self.operands[0][0])),
            0x52: (lambda self: "var = this.field_{:02x}".format(self.operands[0][0])),
            0x53: (lambda self: "var = ds[{:04x}]".format(self.operands[0][0])),
            0x56: (lambda self: "this.field_{:02x} = var".format(self.operands[0][0])),
            0x57: (lambda self: "ds[{:04x}] = var".format(self.operands[0][0])),
            0x5a: (lambda self: "ds[{:04x}] += var".format(self.operands[0][0])),
            0x60: (lambda self: "ds[{:04x}] &= var".format(self.operands[0][0])),
            0x66: (lambda self: "ds[{:04x}] ^= var".format(self.operands[0][0])),
            0x74: (lambda self: "if (var == ds[{:04x}]) goto {:04x}".format(self.operands[0][0], self.operands[1][0])),
            0x79: (lambda self: "if (var != ds[{:04x}]) goto {:04x}".format(self.operands[0][0], self.operands[1][0])),
            }

        string += " " * (20 - len(string)) + "; "
        if self.opcode in strings:
            string += strings[self.opcode](self)

        return string

    def pack(self):
        string = struct.pack("B", self.opcode)
        for value, size in self.operands:
            if size == 1:
                string += struct.pack("B", value)
            else:
                string += struct.pack("<H", value)

        return string

    def patch_jump_addr(self, addr):
        try:
            index = Instruction.jump_operands[self.opcode]
        except:
            raise Exception("Bad jump opcode {:02x}".format(self.opcode))

        self.operands[index] = (addr, 2)

    @staticmethod
    def operand8(value):
        return (value, 1)

    @staticmethod
    def operand16(value):
        return (value, 2)

class CodeGen(object):
    NAME_FIELD        = 0
    NAME_TARGET_FIELD = 1
    NAME_DS           = 2

    DS_VAR            = 0x8a

    # Pseudo registers
    DS_REG_BASE       = 0xf000
    REG_ZERO          = 0
    REG_FLAG          = 1
    REG_GENERAL_BASE  = 2

    field_dict = {
        # Object fields
        "field_00" : 0x00,
        "field_02" : 0x02,
        "field_04" : 0x04,
        "field_06" : 0x06,

        "flags"    : 0x08,

        "field_0a" : 0x0a,
        "field_0c" : 0x0c,
        "field_0e" : 0x0e,
        "field_10" : 0x10,
        "field_12" : 0x12,
        "field_14" : 0x14,
        "field_16" : 0x16,
        "field_18" : 0x18,
        "field_1a" : 0x1a,
        "field_1c" : 0x1c,

        "x"        : 0x1e,
        "y"        : 0x20,

        "field_20" : 0x20,
        "field_22" : 0x22,
        "field_24" : 0x24,
        "field_26" : 0x26,
        "field_28" : 0x28,
        "field_2a" : 0x2a,
        "field_2c" : 0x2c,
        "field_2e" : 0x2e,
        "field_30" : 0x30,
        "field_32" : 0x32,
        "field_34" : 0x34,
        }

    global_dict = {
        # Data segment globals
        "g_tmp_a"            : 0x0206,
        "g_tmp_b"            : 0x0208,

        "erik_inv_slot[0]"   : 0x03e4,
        "erik_inv_slot[1]"   : 0x03e6,
        "erik_inv_slot[2]"   : 0x03e8,
        "erik_inv_slot[3]"   : 0x03ea,

        "baleog_inv_slot[0]" : 0x03ec,
        "baleog_inv_slot[1]" : 0x03ee,
        "baleog_inv_slot[2]" : 0x03f0,
        "baleog_inv_slot[3]" : 0x03f2,

        "olaf_inv_slot[0]"   : 0x03f4,
        "olaf_inv_slot[1]"   : 0x03f6,
        "olaf_inv_slot[2]"   : 0x03f8,
        "olaf_inv_slot[3]"   : 0x03fa,
        }

    def __init__(self, base_addr):
        self.instructions = []
        self.addr = base_addr

        self.call_flag_instrs = []

    def reg(self, reg):
        return self.DS_REG_BASE + (reg * 2)

    def get_name(self, name):
        parts = name.split(".")
        if len(parts) == 2 and parts[0] == "this":
            return (CodeGen.NAME_FIELD, self.field_dict[parts[1]])
        if len(parts) == 2 and parts[0] == "target":
            return (CodeGen.NAME_TARGET_FIELD, self.field_dict[parts[1]])

        return (CodeGen.NAME_DS, self.global_dict[name])

    def emit(self, opcode, *args):
        instr = Instruction(self.addr, opcode, *args)
        self.instructions.append(instr)
        self.addr += len(instr)
        return instr

    def emit_startup_code(self):
        # Clear the zero register
        self.emit_load_literal(0, CodeGen.REG_ZERO)

    def emit_helper_funcs(self):
        # Helper function to set the flag register
        func_addr = self.addr
        self.emit_load_literal(1, CodeGen.REG_FLAG)
        self.emit_return()

        # Patch calls to the set flag function
        for instr in self.call_flag_instrs:
            instr.patch_jump_addr(func_addr)

    def emit_load_literal(self, value, reg):
        # var = <value>;
        self.emit(0x51, Instruction.operand16(value))

        # ds[reg] = var;
        self.emit(0x57, Instruction.operand16(self.reg(reg)))

    def emit_load(self, name, reg):
        kind, offset = self.get_name(name)
        if kind == self.NAME_FIELD:
            # var = this.<field>;
            self.emit(0x52, Instruction.operand8(offset))

            # ds[reg] = var;
            self.emit(0x57, Instruction.operand16(self.reg(reg)))

        elif kind == self.NAME_TARGET_FIELD:
            # var = target.<field>;
            self.emit(0x54, Instruction.operand8(offset))

            # ds[reg] = var;
            self.emit(0x57, Instruction.operand16(self.reg(reg)))

        elif kind == self.NAME_DS:
            # var = ds[offset]
            self.emit(0x53, Instruction.operand16(offset))

            # ds[reg] = var;
            self.emit(0x57, Instruction.operand16(self.reg(reg)))

        else:
            raise Exception("FIXME")

    def emit_store(self, name, reg):
        kind, offset = self.get_name(name)
        if kind == self.NAME_FIELD:
            # var = ds[reg];
            self.emit(0x53, Instruction.operand16(self.reg(reg)))

            # this.<field> = var;
            self.emit(0x56, Instruction.operand8(offset))

        elif kind == self.NAME_TARGET_FIELD:
            # var = ds[reg];
            self.emit(0x53, Instruction.operand16(self.reg(reg)))

            # target.<field> = var;
            self.emit(0x58, Instruction.operand8(offset))

        elif kind == self.NAME_DS:
            # var = ds[reg];
            self.emit(0x53, Instruction.operand16(self.reg(reg)))

            # ds[offset] = var;
            self.emit(0x57, Instruction.operand16(offset))

        else:
            raise Exception("FIXME")

    def emit_jump(self, addr=0):
        return self.emit(0x03, Instruction.operand16(addr))

    #
    # Generates a jump for the opposite of the condition
    #
    def emit_cond_jump(self, op, reg_a, reg_b, addr=0, invert=False):
        invert_table = {
            "!=": "==",
            "==": "!=",
            }

        if invert:
            op = invert_table[op]

        # var = reg_a;
        self.emit(0x53, Instruction.operand16(self.reg(reg_a)))

        if op == "!=":
            # if (var != reg_b) goto <addr>;
            instr = self.emit(0x79, Instruction.operand16(self.reg(reg_b)),
                              Instruction.operand16(addr))

        elif op == "==":
            instr = self.emit(0x74, Instruction.operand16(self.reg(reg_b)),
                              Instruction.operand16(addr))

        else:
            raise Exception("FIXME")

        return instr

    def emit_return(self):
        self.emit(0x06)

    def emit_builtin_set_gfx_prog(self, reg_list):
        operand = reg_list[0]
        if operand.kind != "number":
            raise Exception("set_gfx_prog expects integer argument")

        self.emit(0x19, Instruction.operand16(operand.value))

    def emit_builtin_yield(self, reg_list):
        self.emit(0x00)

    # Returns (operand_type, operand)
    def get_packed_operand(self, arg):
        if arg.kind == "number":
            return (0, Instruction.operand16(arg.value))

        kind, offset = self.get_name(arg.value)
        if kind == CodeGen.NAME_FIELD:
            return (1, Instruction.operand8(offset))

        elif kind == CodeGen.NAME_DS:
            return (2, Instruction.operand16(offset))

        else:
            raise Exception("FIXME: can't pack operand {}".format(arg.value))

    def pack_args(self, args, expected_len):
        operands = []

        if len(args) != expected_len:
            raise Exception("Function called with {} arguments, expected {}".format(len(args), expected_len))

        while len(args) > 0:
            type_a, operand_a = self.get_packed_operand(args.pop(0))
            if len(args) > 0:
                type_b, operand_b = self.get_packed_operand(args.pop(0))
            else:
                type_b = 0
                operand_b = None

            operands.append(Instruction.operand8((type_b << 3) | type_a))
            operands.append(operand_a)
            if operand_b:
                operands.append(operand_b)

        return operands

    def emit_builtin_spawn_obj(self, reg_list):
        # spawn_obj(x, y, unknown1, unknown2, obj_type)
        operands = self.pack_args(reg_list[0:4], 4)
        operands.append(Instruction.operand8(reg_list[4].value))
        self.emit(0x14, *operands)

    def emit_builtin_vm_func_2f(self, reg_list):
        self.emit(0x2f)

    def emit_builtin_vm_func_0b(self, reg_list):
        self.emit(0x0b)

    def emit_builtin_collided_with_viking(self, reg_list):
        arg = reg_list[0].value & 0xff

        self.emit_load_literal(0, CodeGen.REG_FLAG)
        instr = self.emit(0x1a, Instruction.operand8(arg), Instruction.operand16(0))
        self.call_flag_instrs.append(instr)

    def emit_builtin_vm_func_30(self, reg_list):
        arg = reg_list[0].value & 0xff

        self.emit_load_literal(0, CodeGen.REG_FLAG)
        instr = self.emit(0x30, Instruction.operand8(arg), Instruction.operand16(0))
        self.call_flag_instrs.append(instr)

    def emit_builtin_vm_func_32(self, reg_list):
        arg = reg_list[0].value & 0xff

        self.emit_load_literal(0, CodeGen.REG_FLAG)
        instr = self.emit(0x32, Instruction.operand8(arg), Instruction.operand16(0))
        self.call_flag_instrs.append(instr)

    def emit_builtin_vm_func_37(self, reg_list):
        arg = reg_list[0].value & 0xff

        self.emit_load_literal(0, CodeGen.REG_FLAG)
        instr = self.emit(0x37, Instruction.operand8(arg), Instruction.operand16(0))
        self.call_flag_instrs.append(instr)

    def emit_builtin_vm_func_38(self, reg_list):
        arg = reg_list[0].value

        self.emit_load_literal(0, CodeGen.REG_FLAG)
        instr = self.emit(0x38, Instruction.operand16(arg), Instruction.operand16(0))
        self.call_flag_instrs.append(instr)

    def emit_builtin_show_dialog(self, reg_list):
        # show_dialog(dialog_index, unknown, xoffset, yoffset)
        operands = self.pack_args(reg_list, 4)
        self.emit(0x41, *operands)

    def emit_builtin_clear_dialog(self, reg_list):
        self.emit(0x42)

    def emit_builtin_wait_key(self, reg_list):
        self.emit(0xcb)

    def emit_builtin_vm_func_2b(self, reg_list):
        operands = self.pack_args(reg_list[0:2], 2)
        operands.append(Instruction.operand8(reg_list[2].value))
        self.emit(0x2b, *operands)

    def emit_builtin_vm_func_0e(self, reg_list):
        self.emit(0x0e)

    def emit_builtin_get_nearest_viking(self, reg_list):
        self.emit(0x34, Instruction.operand8(reg_list[0].value))

    # Builtin functions: name : (returns_bool, num_args, emit_func)
    function_dict = {
        "yield"                : (False, 0, emit_builtin_yield),
        "set_gfx_prog"         : (False, 1, emit_builtin_set_gfx_prog),
        "spawn_obj"            : (False, 5, emit_builtin_spawn_obj),
        "vm_func_2f"           : (False, 0, emit_builtin_vm_func_2f),

        "flip_horiz"           : (False, 0, emit_builtin_vm_func_0b),

        "collided_with_viking" : (True,  1, emit_builtin_collided_with_viking),
        "vm_func_30"           : (True,  1, emit_builtin_vm_func_30),
        "vm_func_32"           : (True,  1, emit_builtin_vm_func_32),
        "vm_func_37"           : (True,  1, emit_builtin_vm_func_37),

        # Player collision detection?
        "vm_func_38"           : (True,  1, emit_builtin_vm_func_38),

        "show_dialog"          : (False, 4, emit_builtin_show_dialog),
        "clear_dialog"         : (False, 0, emit_builtin_clear_dialog),
        "wait_key"             : (False, 0, emit_builtin_wait_key),

        "vm_func_2b"           : (False, 3, emit_builtin_vm_func_2b),
        "vm_func_0e"           : (False, 0, emit_builtin_vm_func_0e),

        "get_nearest_viking"   : (False, 1, emit_builtin_get_nearest_viking),
        }

    # Generates: a = a + b
    def emit_add(self, reg_a, reg_b):
        # var = ds[reg_b];
        self.emit(0x53, Instruction.operand16(self.reg(reg_a)))

        # ds[reg_a] += var;
        self.emit(0x5a, Instruction.operand16(self.reg(reg_b)))

    # Generates: a = a - b
    def emit_sub(self, reg_a, reg_b):
        # var = ds[reg_b];
        self.emit(0x53, Instruction.operand16(self.reg(reg_a)))

        # ds[reg_a] -= var;
        self.emit(0x5d, Instruction.operand16(self.reg(reg_b)))

    # Generates: a = a & b
    def emit_bitwise_and(self, reg_a, reg_b):
        # var = ds[reg_b];
        self.emit(0x53, Instruction.operand16(self.reg(reg_a)))

        # ds[reg_a] &= var;
        self.emit(0x60, Instruction.operand16(self.reg(reg_b)))

    def emit_bitwise_xor(self, reg_a, reg_b):
        # var = ds[reg_b];
        self.emit(0x53, Instruction.operand16(self.reg(reg_a)))

        # ds[reg_a] ^= var;
        self.emit(0x66, Instruction.operand16(self.reg(reg_b)))

class Parser(object):
    def __init__(self, tokens, codegen):
        self.tokens = tokens
        self.func_names = set()
        self.codegen = codegen

        self.reg = CodeGen.REG_GENERAL_BASE

    def error(self, msg, token=None):
        if not token:
            token = self.tokens.popleft()
        if token:
            string = "[{}] ".format(token.line_num)
        else:
            string = "[???] "

        string += msg
        if token:
            string += " (token = '{}':'{}')".format(token.kind, token.value)

        print(string)
        sys.exit(0)

    def consume_any(self, token_kinds):
        token = self.tokens.popleft()
        if not token:
            self.error("Unexpected end of file")

        if token.kind not in token_kinds:
            self.error("syntax error: got {}, expected '{}'".format(token.kind, ", ".join(token_kinds)), token=token)

        return token

    def consume(self, token_kind):
        return self.consume_any([token_kind])

    def consume_token(self, expect=None):
        token = self.tokens.popleft()
        if not token:
            self.error("Unexpected end of file")

        if expect:
            if isinstance(expect, basestring):
                if token.kind != expect:
                    self.error("syntax error: got '{}', expected '{}'".format(token.kind, expect), token=token)

            else:
                if token.kind not in expect:
                    self.error("syntax error: got '{}', expected '{}'".format(token.kind, ", ".join(expect)), token=token)

        return token

    def next_token_is(self, token_kind):
        if len(self.tokens) == 0:
            return False

        return self.tokens[0].kind == token_kind

    def next_token_kind(self):
        if len(self.tokens) == 0:
            return None

        return self.tokens[0].kind

    #
    # <term> ::= ( <expression> )
    #          | <identifier>
    #          | <number>
    #
    def parse_term(self):
        if self.next_token_is("("):
            self.consume_token()
            self.parse_expression()
            self.consume_token(expect=")")

        elif self.next_token_is("identifier"):
            token = self.consume_token()
            self.codegen.emit_load(token.value, self.reg)
            self.reg += 1

        elif self.next_token_is("number"):
            token = self.consume_token()
            self.codegen.emit_load_literal(token.value, self.reg)
            self.reg += 1

        else:
            self.error("syntax error")

    #
    # <sum> ::= <term> { [+|-|^] <term> }
    #
    def parse_sum(self):
        self.parse_term()

        if not self.next_token_kind() in ["+", "-", "^"]:
            return

        token_op = self.consume_token()

        self.parse_sum()

        if token_op.kind == "+":
            self.codegen.emit_add(self.reg - 1, self.reg - 2)
            self.reg -= 1
        elif token_op.kind == "-":
            self.codegen.emit_sub(self.reg - 1, self.reg - 2)
            self.reg -= 1
        elif token_op.kind == "^":
            self.codegen.emit_bitwise_xor(self.reg - 1, self.reg - 2)
            self.reg -= 1
        else:
            raise Exception("FIXME")

    #
    # <expression> ::= <sum>
    #
    def parse_expression(self):
        self.parse_sum()

    #
    # <condition> ::= <expression> [==|!=|<|>|<=|>=] <expression>
    #               | <expression> [&] <expression>
    #
    def parse_condition(self, invert=False):
        cond_ops = ["==", "!=", "<", ">", "<=", ">="]

        self.parse_expression()
        token_op = self.consume_token()
        self.parse_expression()

        if token_op.kind in cond_ops:
            instr = self.codegen.emit_cond_jump(token_op.value, self.reg - 1, self.reg - 2, invert=invert)
            self.reg -= 2
            return instr

        elif token_op.kind == "&":
            self.codegen.emit_bitwise_and(self.reg - 1, self.reg - 2)
            instr = self.codegen.emit_cond_jump("==", self.reg - 2, CodeGen.REG_ZERO)
            self.reg -= 2
            return instr

    #
    # <assignment> ::= <identifier> = <expression> ;
    #
    def parse_assignment(self):
        token_lhs = self.consume_token(expect="identifier")
        self.consume_token(expect="=")
        self.parse_expression()
        self.consume_token(expect=";")

        self.codegen.emit_store(token_lhs.value, self.reg - 1)
        self.reg -= 1

    #
    # <if_expr> ::= ( <condition> | <call> )
    #
    def parse_if_expr(self):
        self.consume("(")

        if self.next_token_is("call"):
            self.parse_call(requires_return=True)
            jump_instr = self.codegen.emit_cond_jump("==", CodeGen.REG_FLAG, CodeGen.REG_ZERO)
        else:
            jump_instr = self.parse_condition(invert=True)

        self.consume(")")

        return jump_instr

    #
    # <if> ::= if <if_expr> <block> { elif <if_expr> <block>  }* { else <block> }
    #
    def parse_if(self):
        jump_out_instrs = []

        self.consume("if")

        jump_instr = self.parse_if_expr()
        self.parse_block()

        while self.next_token_kind() == "elif":
            # Add a jump out for the previous if/elif
            jump_out_instrs.append(self.codegen.emit_jump())

            jump_instr.patch_jump_addr(self.codegen.addr)

            self.consume_token()
            jump_instr = self.parse_if_expr()
            self.parse_block()

        if self.next_token_kind() == "else":
            else_jump_instr = self.codegen.emit_jump()
            jump_instr.patch_jump_addr(self.codegen.addr)

            self.consume_token()
            self.parse_block()

            else_jump_instr.patch_jump_addr(self.codegen.addr)

        else:
            jump_instr.patch_jump_addr(self.codegen.addr)

        # Patch jump out instructions
        for instr in jump_out_instrs:
            instr.patch_jump_addr(self.codegen.addr)

    #
    # <while> ::= while ( <condition> ) <block>
    #
    def parse_while(self):
        start_addr = self.codegen.addr

        self.consume("while")
        self.consume("(")
        jump_instr = self.parse_condition(invert=True)
        self.consume(")")
        self.parse_block()

        self.codegen.emit_jump(addr=start_addr)
        jump_instr.patch_jump_addr(self.codegen.addr)

    #
    # <call> ::= call ( { [<identifer>|<number>] , } )
    #
    def parse_call(self, requires_return=False):
        num_args = 0

        self.consume("call")
        token_name = self.consume("identifier")
        self.consume("(")

        arg_list = []
        while self.next_token_kind() in ["identifier", "number"]:
            num_args += 1

            arg_list.append(self.consume_token())

            if self.next_token_kind() != ",":
                break
            self.consume(",")

        self.consume(")")

        if token_name.value in CodeGen.function_dict:
            returns_bool, expected_args, emit_func = CodeGen.function_dict[token_name.value]
            if num_args != expected_args:
                raise Exception("Builtin function {} expected {} arguments, got {}".format(token_name.value, expected_args, num_args))

            if requires_return and not returns_bool:
                raise Exception("Builtin function {} does return a value".format(token_name.value))

            emit_func(codegen, arg_list)

        else:
            raise Exception("FIXME: non-builtin functions not yet supported")

    #
    # <statement> ::= <assignment>
    #               | <if>
    #               | while ( <expression> ) <block>
    #               | <call> ;
    #
    def parse_statement(self):
        if self.next_token_is("identifier"):
            self.parse_assignment()

        elif self.next_token_is("if"):
            self.parse_if()

        elif self.next_token_is("while"):
            self.parse_while()

        elif self.next_token_is("call"):
            self.parse_call()
            self.consume(";")


        else:
            self.error("syntax error: expected statement")

    #
    # <block> ::= { <statement>* }
    #
    def parse_block(self):
        self.consume("{")

        while not self.next_token_is("}"):
            self.parse_statement()

        self.consume("}")

    #
    # <function> ::= func <identifier> <block>
    #
    def parse_function(self):
        self.consume("function")

        func_name = self.consume("identifier")
        if func_name.value in self.func_names:
            self.error("Duplicate function name '{}'".format(func_name.value))
        self.func_names.add(func_name.value)

        if func_name.value == "main":
            self.codegen.emit_startup_code()

        self.parse_block()

        self.codegen.emit_return()

    def parse(self):
        while len(self.tokens) > 0:
            self.parse_function()

        self.codegen.emit_helper_funcs()

def strtoul(value):
    if value.startswith("0x"):
        return int(value, 16)
    else:
        return int(value)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Lost Vikings VM compiler")
    parser.add_argument("-b", "--base-address", metavar="ADDRESS", default="0",
                        help="Base address for the program")
    parser.add_argument("-p", "--patch-file", metavar="FILENAME",
                        help="Chunk file to patch")
    parser.add_argument("-i", "--object-index", metavar="INDEX",
                        help="Object index to patch")
    parser.add_argument("-o", "--output-file", metavar="FILENAME",
                        help="Output file")
    parser.add_argument("-d", "--dump", action="store_true",
                        help="Dump the compiled instructions")
    parser.add_argument("filename", metavar="FILENAME",
                        help="Source file")
    parser.add_argument("-c", "--cpp", metavar="FILENAME", default="cpp",
                        help="Pre-processor command")
    args = parser.parse_args()

    # Fixup arguments
    args.base_address = strtoul(args.base_address)
    args.object_index = strtoul(args.object_index)

    # Preprocess the source
    process = subprocess.Popen([args.cpp, args.filename], stdout=subprocess.PIPE)
    lines, _ = process.communicate()
    lines = lines.split("\n")

    # Break the source into lexical tokens
    lexer = Lexer(lines)
    lexer.lex()

    codegen = CodeGen(args.base_address + 3)

    # Parse the source (generates code)
    parser = Parser(lexer.tokens, codegen)
    parser.parse()

    if args.dump:
        for instr in codegen.instructions:
            print(instr)

    if args.patch_file and args.object_index and args.output_file:
        data = open(args.patch_file).read()

        if len(data) >= args.base_address:
            print("Error: Base address for program is inside patch file (length={:x})".format(len(data)))
            sys.exit(-1)

        #
        # Patch the object header. Each object header is 21 bytes long.
        # The field at offset 3 specifies the base address of the program.
        #
        offset = (args.object_index * 21) + 3
        prog_start = struct.unpack("<H", data[offset:offset + 2])[0]
        prog_jump = data[prog_start:prog_start + 3]

        new_start = struct.pack("<H", args.base_address)

        patch_data = data[0:offset]
        patch_data += new_start
        patch_data += data[offset + 2:]

        # Pad data with zeros
        patch_data += "\x00" * (args.base_address - len(data))

        # Copy in the initial jump instruction
        patch_data += prog_jump

        # Append the new program
        for instr in codegen.instructions:
            patch_data += instr.pack()

        # FIXME - probably a bug in the compression/pack_tool.
        #         append zeros to fix.
        patch_data += "\x00\x00\x00\x00"

        # Write the patched chunk out
        open(args.output_file, "w").write(patch_data)

    num_instrs = len(codegen.instructions)
    prog_size = sum([len(instr) for instr in codegen.instructions])
    print("Compiled program is {} instrctions, {} bytes".format(num_instrs, prog_size))
