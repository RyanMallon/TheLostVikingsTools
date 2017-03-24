#
# The Lost Vikings Virtual Machine Dissassembler
#
# This is a very basic, work in progress dissassembler for the virtual machine
# which controls object behaviour in The Lost Vikings.
#
# This is mostly a tool for assisting in reverse engineering the virtual
# machine. It is not guaranteed to work properly, in particular:
#
#  * Many opcodes are not yet implemented (operands are not known)
#  * Several opcodes are implemented, but have unknown use
#  * Logic for many conditional opcodes is probably incorrect
#  * Some implemented opcodes are probably incorrect
#
# To use this, use pack_tool to extract one of the game object chunks, e.g:
#
#   ./pack_tool ./pack_tool DATA.DAT -d 449:progs.bin
#
# Then run this dissassembler with the index of the program you want to
# dissassemble:
#
#   python ./lv_vm_disasm.py progs.bin 0x4
#
# If the program fails to dissassemble (probably because of an unimplemented
# opcode) then pass the -d argument to dump instructions as they are parsed
# and the -c argument to annotate each dissassembled sequence with a comment
# showing the original opcode and operands.
#
#

import argparse
import struct
import sys

class CodeStream(object):
    def __init__(self, filename):
        fd = open(filename, "r")
        self.data = fd.read()
        self.offset = 0

    def get_byte(self):
        byte = struct.unpack("<B", self.data[self.offset])[0]
        self.offset += 1
        return byte

    def get_word(self):
        word = struct.unpack("<H", self.data[self.offset:self.offset+2])[0]
        self.offset += 2
        return word

class Instruction(object):
    def __init__(self, code, func, addr):
        self.code = code
        self.addr = addr
        self.func = func

        self.operands = []
        self.lines = []

        self.code.offset = addr
        self.opcode = self.code.get_byte()

    def get_byte(self):
        byte = self.code.get_byte()
        self.operands.append((1, byte))
        return byte

    def get_word(self):
        word = self.code.get_word()
        self.operands.append((2, word))
        return word

    def emit(self, line, indent=0):
        self.lines.append((indent, line))

    def emit_call(self, addr, indent=0):
        self.func.ref_label(addr)
        self.func.call_labels.add(addr)
        self.emit("call sub_{:04x};".format(addr), indent=indent)

    def emit_jump(self, addr, indent=0):
        self.func.ref_label(addr)
        self.func.jump_labels.add(addr)
        self.emit("jump label_{:04x};".format(addr), indent=indent)

    def sets_var(self):
        for _, line in self.lines:
            if "<<<" in line and ">>>" in line:
                return True
        return False

    def gets_var(self):
        for _, line in self.lines:
            if "[[[VAR]]]" in line:
                return True
        return False

    def remove_var_set(self):
        for i, (_, line) in enumerate(self.lines):
            start = line.find("<<<")
            if start == -1:
                continue

            start += 3
            end = line.find(">>>", start)
            if end == -1:
                continue

            value = line[start:end]
            del self.lines[i]
            return value

        return None

    def replace_var_get(self, value):
        for i, (indent, line) in enumerate(self.lines):
            new_line = line.replace("[[[VAR]]]", value)
            self.lines[i] = (indent, new_line)

    def remove_markers(self):
        self.replace_var_get("var")
        for i, (indent, line) in enumerate(self.lines):
            new_line = line.replace("<<<", "")
            new_line = new_line.replace(">>>", "")
            self.lines[i] = (indent, new_line)

    def to_string(self):
        string = "[{:04x}] ({:02x}) ".format(self.addr, self.opcode)
        for size, operand in self.operands:
            if size == 1:
                string += "{:02x} ".format(operand)
            elif size == 2:
                string += "{:04x} ".format(operand)

        return string

    def dump(self):
        self.remove_markers()

        comment = "    // {}".format(self.to_string())

        if args.comments:
            print(comment)
        for indent, line in self.lines:
            print("{}{}".format("    " * (indent + 1), line))

class Function(object):
    def __init__(self, name, entry_addr):
        self.name = name
        self.entry_addr = entry_addr

        self.instructions = []
        self.labels = set()
        self.jump_labels = set()
        self.call_labels = set()

    def ref_label(self, addr):
        self.labels.add(addr)

    def squash_var_uses(self):
        for i, instr in enumerate(self.instructions):
            if i != 0 and instr.gets_var() and self.instructions[i - 1].sets_var():
                value = self.instructions[i - 1].remove_var_set()
                instr.replace_var_get(value)

    def dump(self):
        print("{}:".format(self.name))
        print("{")

        for instr in self.instructions:
            if instr.addr in self.labels:
                print("label_{:04x}:".format(instr.addr))
            instr.dump()

            if len(instr.lines) > 0:
                print("")

        print("}")
        print("")

class Disassembler(object):
    FIELD_FLAGS = 0x08

    def __init__(self, code):
        self.code = code
        self.funcs = []

        self.func_addrs = set()

    def __obj_field_name(self, offset):
        names = {
            0x08: "flags",
            0x0a: "argument",
            0x1a: "object_type",
            0x1e: "xoff",
            0x20: "yoff",
            0x3c: "target_obj",
            }
        if offset in names:
            return "{}".format(names[offset])
        else:
            return "field_{:02x}".format(offset)

    def obj_field_name(self, offset):
        return "this.{}".format(self.__obj_field_name(offset))

    def obj_field_name2(self, offset):
        return "this.target.{}".format(self.__obj_field_name(offset))

    def ds_name(self, offset):
        names = {
            0x0206: "g_tmp_a",
            0x0208: "g_tmp_b",

            # Nearest viking returned by VM function 34
            0x03ca: "func_34_viking",

            0x03e4: "erik_inv_slot[0]",
            0x03e6: "erik_inv_slot[1]",
            0x03e8: "erik_inv_slot[2]",
            0x03ea: "erik_inv_slot[3]",

            0x03ec: "baleog_inv_slot[0]",
            0x03ee: "baleog_inv_slot[1]",
            0x03f0: "baleog_inv_slot[2]",
            0x03f2: "baleog_inv_slot[3]",

            0x03f4: "olaf_inv_slot[0]",
            0x03f6: "olaf_inv_slot[1]",
            0x03f8: "olaf_inv_slot[2]",
            0x03fa: "olaf_inv_slot[3]",

            # 0x0418: "current_viking", ?
            }
        if offset in names:
            return "{}".format(names[offset])
        else:
            return "ds[0x{:04x}]".format(offset)

    def disasm_operand_set(self, instr, kind, var_name):
        if kind == 0x00 or kind == 0x04:
            # NOP
            pass

        elif kind == 0x01:
            operand = instr.get_byte()
            instr.emit("{} = {};".format(self.obj_field_name(operand), var_name))

        elif kind == 0x02:
            operand = instr.get_word()
            instr.emit("{} = {};".format(self.ds_name(operand), var_name))

        elif kind == 0x03:
            operand = instr.get_byte()
            instr.emit("{} = {};".format(self.obj_field_name2(operand), var_name))

        else:
            instr.emit("BAD = {};".formar(var_name))

    def disasm_operand_get(self, instr, kind, var_name):
        if kind == 0x00:
            operand = instr.get_word()
            instr.emit("{} = 0x{:04x};".format(var_name, operand))

        elif kind == 0x01:
            operand = instr.get_byte()
            instr.emit("{} = {};".format(var_name, self.obj_field_name(operand)))

        elif kind == 0x02:
            operand = instr.get_word()
            instr.emit("{} = {};".format(var_name, self.ds_name(operand)))

        elif kind == 0x03:
            operand = instr.get_byte()
            instr.emit("{} = {};".format(var_name, self.obj_field_name2(operand)))

        elif kind == 0x04:
            instr.emit("{} = UNKNOWN;".format(var_name))

        else:
            instr.emit("{} = BAD;".format(var_name))

    def disasm_jump_type(self, instr, kind, indent=0):
        if kind == 0x00:
            # jnb
            operand = instr.get_word()
            instr.emit("// jnb", indent=indent)
            instr.emit_jump(addr, indent=indent)

        elif kind == 0x01:
            # jb
            operand = instr.get_word()
            instr.emit("// jb", indent=indent)
            instr.emit_jump(addr, indent=indent)

        elif kind == 0x02 or kind == 0x04:
            # jmp
            pass

        elif kind == 0x03:
            # nop
            return

        else:
            instr.emit("BAD_JUMP_TYPE(0x{:02x});".format(kind))

    def parse_instruction(self, func):
        addr = self.code.offset
        if addr >= len(self.code.data):
            return None

        operands = {}
        instr = Instruction(self.code, func, self.code.offset)
        opcode = instr.opcode

        if opcode == 0x00:
            instr.emit("this.yield();")

        elif opcode == 0x01:
            instr.emit("nop();")

        elif opcode == 0x02:
            operands[0] = instr.get_word()
            instr.emit("func_02(0x{:04x});".format(operands[0]))

        elif opcode == 0x03:
            operands[0] = instr.get_word()
            instr.emit_jump(operands[0])

        elif opcode == 0x04:
            operands[0] = instr.get_byte()
            instr.emit("vm_func_04(0x{:02x});".format(operands[0]))

        elif opcode == 0x05:
            operands[0] = instr.get_word()
            instr.emit_call(operands[0])

        elif opcode == 0x06:
            instr.emit("return;")

        elif opcode == 0x07:
            instr.emit("if (!({} & 0x0040))".format(self.obj_field_name(Disassembler.FIELD_FLAGS)))
            instr.emit("this.flip_horiz();", indent=1)

        elif opcode == 0x08:
            instr.emit("if ({} & 0x0040)", self.obj_field_name(Disassembler.FIELD_FLAGS))
            instr.emit("this.flip_horiz();", indent=1)

        elif opcode == 0x09:
            instr.emit("if (!({} & 0x0080))", self.obj_field_name(Disassembler.FIELD_FLAGS))
            instr.emit("this.flip_vert();", indent=1)

        elif opcode == 0x0a:
            instr.emit("if ({} & 0x0080)", self.obj_field_name(Disassembler.FIELD_FLAGS))
            instr.emit("this.flip_vert();", indent=1)

        elif opcode == 0x0b:
            instr.emit("this.flip_horiz();")

        elif opcode == 0x0c:
            instr.emit("this.flip_vert();")

        elif opcode == 0x0e:
            # FIXME
            instr.emit("vm_func_0e();")

        elif opcode == 0x0f:
            # Breaks out of function
            instr.emit("vm_func_0f();")

        elif opcode == 0x10:
            instr.emit("this.update();")

        elif opcode == 0x12:
            instr.emit("vm_func_12();")

        elif opcode == 0x14:
            operands[0] = instr.get_byte()
            self.disasm_operand_get(instr, operands[0] & 0x7, "new.x")
            self.disasm_operand_get(instr, (operands[0] >> 3) & 0x7, "new.y")

            operands[1] = instr.get_byte()
            self.disasm_operand_get(instr, operands[1] & 0x7, "new.unknown_a")
            self.disasm_operand_get(instr, (operands[1] >> 3) & 0x7, "new.unknown_b")
            instr.emit("new.unknown_c &= 0x801;")

            operands[2] = instr.get_byte()
            instr.emit("new.type = 0x{:02x};".format(operands[2]))
            instr.emit("spawn_obj(new);")

        elif opcode == 0x16:
            # Checks horiz/vert directions of source and target object?
            operands[0] = instr.get_byte()
            self.disasm_operand_set(instr, operands[0] & 0x7, "tmp_a")
            self.disasm_operand_set(instr, (operands[0] >> 3) & 0x7, "tmp_b")

        elif opcode == 0x19:
            #
            # Sets graphics program to run (secondary VM)
            #
            operands[0] = instr.get_word()
            instr.emit("{} = 0x{:04x};".format(self.obj_field_name(0x21), operands[0]))
            instr.emit("{} = 0x0001;".format(self.obj_field_name(0x22)))

        elif opcode == 0x20:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            instr.emit("if (vm_func_20({} & 0x0040)".format(self.obj_field_name(0x4)))
            # self.disasm_jump_type(instr, operands[0], indent=1)
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x21:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()

            # FIXME
            instr.emit("if (vm_func_21({} & 0x0040))".format(self.obj_field_name(Disassembler.FIELD_FLAGS), operands[0]))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x1a:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            instr.emit("if (vm_func_1a(0x{:02x}))".format(operands[0]))
            instr.emit_call(operands[1], indent=1)

        elif opcode == 0x1c:
            operands[0] = instr.get_word()
            instr.emit("if ({} == 0)".format(self.obj_field_name(0x22)))
            instr.emit_jump(operands[0], indent=1)

        elif opcode == 0x1d:
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()
            instr.emit("if (vm_func_1d(0x{:04x}))".format(operands[0]))
            instr.emit_call(operands[1], indent=1)

        elif opcode == 0x23:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            # FIMXE - logic correct?
            instr.emit("if (sub_158d7(0x{:02x}))".format(operands[0]))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x26:
            # FIXME - not sure if correct
            operands[0] = instr.get_byte()
            self.disasm_operand_get(instr, operands[0] & 0x7, "tmp_a")
            instr.emit("tmp_a >>= 4")
            self.disasm_operand_get(instr, (operands[0] >> 3) & 0x7, "tmp_b"
)
            instr.emit("tmp_b >>= 4")

            operands[1] = instr.get_byte()
            self.disasm_operand_get(instr, operands[1] & 0x7, "tmp_c")
            self.disasm_operand_get(instr, (operands[1] >> 3) & 0x7, "tmp_d")

        elif opcode == 0x2b:
            operands[0] = instr.get_byte()
            self.disasm_operand_get(instr, operands[0] & 0x7, "tmp_a")
            self.disasm_operand_get(instr, (operands[0] >> 3) & 0x7, "tmp_b")

            operands[1] = instr.get_byte()
            self.disasm_operand_get(instr, operands[1] & 0x7, "tmp_c")

            instr.emit("vm_func_2b(tmp_a, tmp_b, tmp_c);")

        elif opcode == 0x2c:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            operands[2] = instr.get_byte() # Unused?
            instr.emit("if (vm_func_2c(0x{:02x}))".format(operands[0]))
            instr.emit_call(operands[1], indent=1)

        elif opcode == 0x2e:
            operands[0] = instr.get_word()
            instr.emit("vm_func_2e(0x{:04x});".format(operands[0]))

        elif opcode == 0x2f:
            instr.emit("vm_func_2f();")

        elif opcode == 0x30:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            instr.emit("if (vm_func_30(0x{:02x}))".format(operands[0]))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x32:
            #
            # Check for collisions against walls/edge of world.
            # Argument is a flag value in the object?
            #
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()

            instr.emit("if (this.collides_with_wall(0x{:02x}))".format(operands[0]))
            instr.emit_call(operands[1], indent=1)

        elif opcode == 0x34:
            #
            # Sets x, y as distance to nearest viking?
            #
            operands[0] = instr.get_byte()

            # FIXME - parts of code are shared with 0x15/0x16
            self.disasm_operand_set(instr, operands[0] & 0x7, "nearest_viking_dist_x()")
            self.disasm_operand_set(instr, (operands[0] >> 3) & 0x7, "nearest_viking_dist_y()")

        elif opcode == 0x37:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()

            # FIXME - same as 0x1a, but sets different globals?
            instr.emit("if (vm_func_37(0x{:02x}))".format(operands[0]))
            instr.emit_call(operands[1], indent=1)

        elif opcode == 0x38:
            #
            # Checks for object collison. Iterates over all objects.
            # Argument is a flag in the object database.
            #
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()

            instr.emit("if (this.collides_with_obj(0x{:04x}))".format(operands[0]))
            instr.emit_call(operands[1], indent=1)

        elif opcode == 0x3c:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            instr.emit("if (vm_func_3c(0x{:02x}))".format(operands[0]))
            instr.emit_call(operands[1], indent=1)

        elif opcode == 0x3f:
            instr.emit("vm_func_3f();")

        elif opcode == 0x40:
            instr.emit("vm_func_40();")

        elif opcode == 0x41:
            # FIXME - does lots of stuff
            operands[0] = instr.get_byte()

            self.disasm_operand_get(instr, operands[0] & 0x7, "tmp_a")
            self.disasm_operand_get(instr, (operands[0] >> 3) & 0x7, "tmp_b")

            operands[1] = instr.get_byte()
            self.disasm_operand_get(instr, operands[0] & 0x7, "tmp_c")
            self.disasm_operand_get(instr, (operands[0] >> 3) & 0x7, "tmp_d")

            instr.emit("vm_func_41(tmp_a, tmp_b, tmp_c, tmp_d);")

        elif opcode == 0x42:
            instr.emit("vm_func_42();")

        elif opcode == 0x43:
            # Same as 0xcb
            instr.emit("vm_func_43();")

        elif opcode == 0x46:
            operands[0] = instr.get_word()
            instr.emit("vm_func_46(0x{:04x});".format(operands[0]))

        elif opcode == 0x49:
            operands[0] = instr.get_byte()
            self.disasm_operand_get(instr, operands[0] & 0x7, "tmp_a")
            self.disasm_operand_get(instr, (operands[0] >> 3) & 0x7, "tmp_b")

            operands[1] = instr.get_byte()
            operands[2] = instr.get_word()

            instr.emit("if (vm_func_49(0x{:02x}))".format(operands[1]))
            instr.emit_jump(operands[2], indent=1)

        elif opcode == 0x4b:
            instr.emit("{} |= 0x2000;".format(self.obj_field_name(0x04)))

        elif opcode == 0x4e:
            operands[0] = instr.get_word()
            instr.emit("if (vm_func_4e())")
            instr.emit_jump(operands[0], indent=1)

        #
        # Opcodes 51..55: var = <x>;
        #
        elif opcode == 0x51:
            operands[0] = instr.get_word()
            instr.emit("var = <<<0x{:04x}>>>;".format(operands[0]))

        elif opcode == 0x52:
            operands[0] = instr.get_byte()
            instr.emit("var = <<<{}>>>;".format(self.obj_field_name(operands[0])))

        elif opcode == 0x53:
            operands[0] = instr.get_word()
            instr.emit("var = <<<{}>>>;".format(self.ds_name(operands[0])))

        elif opcode == 0x54:
            operands[0] = instr.get_byte()
            instr.emit("var = <<<{}>>>;".format(self.obj_field_name2(operands[0])))

        #
        # Opcodes 56..58: <x> = var;
        #
        elif opcode == 0x56:
            operands[0] = instr.get_byte()
            instr.emit("{} = [[[VAR]]];".format(self.obj_field_name(operands[0])))

        elif opcode == 0x57:
            operands[0] = instr.get_word()
            instr.emit("{} = [[[VAR]]];".format(self.ds_name(operands[0])))

        elif opcode == 0x58:
            operands[0] = instr.get_byte()
            instr.emit("{} = [[[VAR]]];".format(self.obj_field_name2(operands[0])))

        #
        # Opcodes 59..5b: <x> += var;
        #
        elif opcode == 0x5a:
            operands[0] = instr.get_word()
            instr.emit("{} += [[[VAR]]];".format(self.ds_name(operands[0])))

        elif opcode == 0x5b:
            operands[0] = instr.get_byte()
            instr.emit("{} += [[[VAR]]];".format(self.obj_field_name2(operands[0])))

        #
        # Opcodes 5c..5e: <x> -= var;
        #
        elif opcode == 0x5c:
            operands[0] = instr.get_byte()
            instr.emit("{} -= [[[VAR]]];".format(self.obj_field_name(operands[0])))

        elif opcode == 0x5d:
            operands[0] = instr.get_word()
            instr.emit("{} -= [[[VAR]]];".format(self.ds_name(operands[0])))

        elif opcode == 0x5e:
            operands[0] = instr.get_byte()
            instr.emit("{} -= [[[VAR]]];".format(self.obj_field_name2(operands[0])))

        #
        # Opcodes 5f..61: <x> &= var;
        #
        elif opcode == 0x5f:
            operands[0] = instr.get_byte()
            instr.emit("{} &= [[[VAR]]];".format(self.obj_field_name(operands[0])))

        elif opcode == 0x60:
            operands[0] = instr.get_word()
            instr.emit("{} &= [[[VAR]]];".format(self.ds_name(operands[0])))

        elif opcode == 0x62:
            operands[0] = instr.get_byte()
            instr.emit("{} |= [[[VAR]]];".format(self.obj_field_name(operands[0])))

        elif opcode == 0x63:
            operands[0] = instr.get_word()
            instr.emit("{} |= [[[VAR]]];".format(self.ds_name(operands[0])))

        elif opcode == 0x66:
            operands[0] = instr.get_word()
            instr.emit("{} ^= [[[VAR]]];".format(self.ds_name(operands[0])))

        elif opcode == 0x68:
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()
            instr.emit("if ({} > [[[VAR]]])".format(self.obj_field_name(operands[0])))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x69:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()

            # FIXME - condtion correct?
            instr.emit("if ({} < [[[VAR]]])".format(self.obj_field_name(operands[0])))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x6d:
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()
            instr.emit("if ([[[VAR]]] < 0x%.4x)".format(operands[0]))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x72:
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()
            instr.emit("if ([[[VAR]]] == 0x{:04x})".format(operands[0]))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x73:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            instr.emit("if ({} == [[[VAR]]])".format(self.obj_field_name(operands[0])))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x74:
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()
            instr.emit("if ([[[VAR]]] == {})".format(self.ds_name(operands[0])))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x75:
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()
            instr.emit("if ([[[VAR]]] != 0x%.4x)".format(operands[0]))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x77:
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()
            instr.emit("if ([[[VAR]]] != 0x{:04x})".format(operands[0]))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x78:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            instr.emit("if ([[[VAR]]] != {})".format(self.obj_field_name(operands[0])))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x79:
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()
            instr.emit("if ({} != [[[VAR]]])".format(self.ds_name(operands[0])))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x7c:
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()

            # FIXME - logic correct?
            instr.emit("if (abs(0x{:04x} - [[[VAR]]]) >= 0)".format(operands[0]))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x7e:
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()
            instr.emit("if ([[[VAR]]] - {} < 0)".format(self.ds_name(operands[0])))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x81:
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()
            instr.emit("if (abs(0x{:04x} - [[[VAR]]]) < 0)".format(operands[0]))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x82:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            # FIXME - logic?
            instr.emit("if ([[[VAR]]] - {})".format(self.obj_field_name(operands[0])))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x83:
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()
            # FIXME - additional checks for signedness?
            instr.emit("if ([[[VAR]]] - 0x{:04x} < 0)".format(operands[0]))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x84:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            # FIXME - logic?
            instr.emit("if ([[[VAR]]] - {})".format(self.obj_field_name2(operands[0])))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0x8b:
            operands[0] = instr.get_word()
            operands[1] = instr.get_word()
            instr.emit("if ([[[VAR]]] != 0x{:04x})".format(operands[0]))
            instr.emit_call(operands[1], indent=1)

        elif opcode == 0x8c:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            instr.emit("if ({} != [[[VAR]]])".format(self.obj_field_name(operands[0])))
            instr.emit_call(operands[1], indent=1)

        elif opcode == 0x91:
            operands[0] = instr.get_word()
            instr.emit("if ({} & 0x0040)".format(self.obj_field_name(Disassembler.FIELD_FLAGS)))
            instr.emit("{} -= [[[VAR]]];".format(self.ds_name(operands[0])), indent=1)
            instr.emit("else")
            instr.emit("{} += [[[VAR]]];".format(self.ds_name(operands[0])), indent=1)

        elif opcode == 0x94:
            operands[0] = instr.get_word()

            # FIXME - logic order?
            instr.emit("if ({} & 0x0040)".format(self.obj_field_name(0x04)))
            instr.emit("{} -= [[[VAR]]]".format(self.ds_name(operands[0])), indent=1)
            instr.emit("else")
            instr.emit("{} += [[[VAR]]];".format(self.ds_name(operands[0])), indent=1)

        elif opcode == 0x96:
            instr.emit("{} = [[[VAR]]];".format(self.obj_field_name(0x1e)))

        elif opcode == 0x97:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_byte()
            instr.emit("if (0x{:04x} & (1 << {}))".format(operands[1], operands[0] / 2))
            instr.emit("var = 0x0001;", indent=1)

        elif opcode == 0x98:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_byte()
            instr.emit("var = <<<!!({} & (1 << {}))>>>;".format(self.obj_field_name(operands[1]), operands[0] / 2))

        elif opcode == 0x99:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            instr.emit("if ({} & (1 << {}))".format(self.ds_name(operands[1]), operands[0] / 2))
            instr.emit("var = 1;", indent=1)

        elif opcode == 0x9a:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_byte()

            # FIXME
            instr.emit("if (vm_func_9a(0x{:02x}, 0x{:02x}))".format(operands[0], operands[1]))
            instr.emit("var = 1;", indent=1)

        elif opcode == 0x9c:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_byte()
            instr.emit("if (!([[[VAR]]]))")
            instr.emit("{} &= ~(1 << {});".format(self.obj_field_name(operands[1]), operands[0] / 2), indent=1)

        elif opcode == 0x9d:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()

            # FIXME
            instr.emit("if ([[[VAR]]] != 0)")
            instr.emit("var = (1 << {})".format(operands[0] / 2), indent=1)
            instr.emit("{} &= ~(1 << {});".format(self.ds_name(operands[1]), operands[0] / 2))
            instr.emit("{} |= var;".format(self.ds_name(operands[1])))

        elif opcode == 0x9e:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_byte()

            instr.emit("if ([[[VAR]]] != 0)")
            instr.emit("var = (1 << {});".format(operands[0] / 2), indent=1)

            # dx = mask[0]
            instr.emit("{} &= ~(1 << {});".format(self.obj_field_name2(operands[1]), operands[0] / 2))
            instr.emit("{} |= var;".format(self.obj_field_name2(operands[1])))

        elif opcode == 0xa8:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            operands[2] = instr.get_word()
            # FIXME - logic?
            instr.emit("if (0x{:04x} & (1 << {}))".format(operands[1], operands[0] / 2))
            instr.emit_jump(operands[2], indent=1)

        elif opcode == 0xa9:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_byte()
            operands[2] = instr.get_word()

            instr.emit("if ({} & (1 << {}))".format(self.obj_field_name(operands[1]), operands[0] / 2))
            instr.emit_jump(operands[2], indent=1)

        elif opcode == 0xab:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_byte()
            operands[2] = instr.get_word()
            instr.emit("if ({} & (1 << {}))".format(self.obj_field_name2(operands[1]), operands[0] / 2))
            instr.emit_jump(operands[2], indent=1)

        elif opcode == 0xae:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_byte()
            operands[2] = instr.get_word()
            instr.emit("if ({} & (1 << {}))".format(self.obj_field_name(operands[1]), operands[0] / 2))
            instr.emit_jump(operands[2], indent=1)

        elif opcode == 0xaf:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()
            operands[2] = instr.get_word()
            instr.emit("if (var == {} & (1 << {}))".format(self.ds_name(operands[1]), operands[0] / 2))
            instr.emit_jump(operands[2], indent=1)

        elif opcode == 0xb0:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_byte()
            operands[2] = instr.get_word()

            # FIXME - check logic order
            instr.emit("if ({} & (1 << {}))".format(self.obj_field_name2(operands[1]), operands[0] / 2))
            instr.emit_jump(operands[2], indent=1)

        elif opcode == 0xc2:
            operands[0] = instr.get_byte()
            operands[1] = instr.get_word()

            # FIXME
            instr.emit("if (vm_func_c2({} & 0x0040, 0x{:02x}))".format(self.obj_field_name(Disassembler.FIELD_FLAGS), operands[0]))
            instr.emit_jump(operands[1], indent=1)

        elif opcode == 0xc7:
            instr.emit("if (!({} & 0x8000))".format(self.obj_field_name(0x0c)))
            instr.emit("obj_hdr[{}].xoff = var;".format(self.obj_field_name(0x0c)))

        elif opcode == 0xc8:
            instr.emit("if (!({} & 0x8000))".format(self.obj_field_name(0x0c)))
            instr.emit("obj_hdr[{}].yoff = var;".format(self.obj_field_name(0x0c)))

        elif opcode == 0xc9:
            instr.emit("if (!({} & 0x8000))".format(self.obj_field_name(0x0c)))
            instr.emit("obj_hdr[{}].flags = var;".format(self.obj_field_name(0x0c)))

        elif opcode == 0xca:
            instr.emit("if (!({} & 0x8000))".format(self.obj_field_name(0x0c)))
            instr.emit("obj_hdr[{}].arg = var;".format(self.obj_field_name(0x0c)))

        elif opcode == 0xcb:
            instr.emit("vm_func_cb();")

        elif opcode == 0xce:
            operands[0] = instr.get_word()
            instr.emit("vm_func_ce();")
            instr.emit_jump(operands[0])

        elif opcode == 0xcf:
            operands[0] = instr.get_word()
            instr.emit_jump(operands[0])

        else:
            print("Unknown opcode {:02x} at {:04x}".format(opcode, instr.addr))
            sys.exit(1)

        if args.debug:
            instr.dump()
            print("")

        func.instructions.append(instr)
        return opcode

    def find_program_start(self, index):
        self.code.offset = (index * 21) + 3
        return self.code.get_word() + 3

    def parse_function(self, addr, func_name):
        end_opcodes = [
            0x03, # Unconditional jump
            0x06, # Return
            0x0f, # Returns
            0x10, # Object update (returns)
            ]

        print("Parsing function '{}' at {:04x}".format(func_name, addr))

        func = Function(func_name, addr)

        refed_labels = set()
        refed_labels.add(addr)

        while len(refed_labels) != 0:
            self.code.offset = refed_labels.pop()

            #
            # Parse instructions in linear order until hitting an instruction
            # which would exit control flow unconditionally (jump, return, etc)
            #
            while True:
                opcode = self.parse_instruction(func)
                if opcode is None or opcode in end_opcodes:
                    break

            #
            # Trim the set of referenced labels to the ones that have not
            # been visited.
            #
            for label in func.jump_labels:
                refed_labels.add(label)
            for label in refed_labels.copy():
                for instr in func.instructions:
                    if label == instr.addr and label in refed_labels:
                        refed_labels.remove(label)

        #
        # Post process:
        #
        #  * Sort the instructions
        #  * Squash variable assignments and uses
        #
        sorted(func.instructions, key=lambda instr: instr.addr)

        if not args.no_squash:
            func.squash_var_uses()

        return func

    def parse_program(self, start_addr):
        end_opcodes = [
            0x03, # Unconditional jump
            0x06, # Return
            0x0f, # Returns
            0x10, # Object update (returns)
            ]

        func_addrs = set()
        func_addrs.add(start_addr)

        while len(func_addrs) > 0:
            addr = func_addrs.pop()
            if addr == start_addr:
                func_name = "main_{:04x}".format(addr)
            else:
                func_name = "sub_{:04x}".format(addr)

            func = self.parse_function(addr, func_name)
            self.funcs.append(func)

            # Did this function add calls to functions we haven't parsed?
            for addr in func.call_labels:
                func_addrs.add(addr)
            for func in self.funcs:
                if func.entry_addr in func_addrs:
                    func_addrs.discard(func.entry_addr)

    def find_loops(self):
        for block in self.blocks:
            if block.jump_true_addr < block.addr and not block.jump_false_addr:
                # Loop
                loop_start_block = self.find_block(block.jump_true_addr)
                loop_start_block.lines.insert(0, "while (1) {")
                for i in xrange(1, len(loop_start_block.lines)):
                    loop_start_block.lines[i] = "\t" + loop_start_block.lines[i]

                block.jump_true_addr = None

                loop_block = self.find_block(loop_start_block.jump_true_addr)
                while loop_block:
                    if loop_block.lines:
                        for i in xrange(0, len(loop_block.lines)):
                            loop_block.lines[i] = "\t" + loop_block.lines[i]

                    loop_block = self.find_block(loop_block.jump_true_addr)

                block.lines = ["}"]

    def print_block(self, block):
        if block.lines:
            for line in block.lines:
                print(line)

        print("")

    def find_block(self, addr):
        for block in self.blocks:
            if block.addr == addr:
                return block

        return None

    def print_program(self):
        for func in self.funcs:
            func.dump()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Lost Vikings VM disassembler")
    parser.add_argument("-c", "--comments", action="store_true",
                        help="Display instruction comments")
    parser.add_argument("-d", "--debug", action="store_true",
                        help="Debug")
    parser.add_argument("filename", help="VM chunk filename")
    parser.add_argument("-n", "--no-squash", action="store_true",
                        help="Don't squash variable uses")
    parser.add_argument("record", help="VM object record index")

    args = parser.parse_args()

    code = CodeStream(args.filename)
    disasm = Disassembler(code)

    index = args.record

    addr = disasm.find_program_start(int(index, 16))

    disasm.parse_program(addr)
    disasm.print_program()
