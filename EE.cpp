#include <EE.hpp>
#include <cstring>

EmotionEngine::EmotionEngine(Bus* _bus)
: bus(_bus),
intc(new INTC(this)),
timer(new Timers(intc))
{
    pc = 0xBFC00000;
    std::memset(regs, 0, sizeof(regs));

    next_instr = {};
    next_instr.value = bus->Read32(pc, true);
    next_instr.pc = pc;

    lookup.reserve(0x100);

    lookup[0x00] = std::bind(&EmotionEngine::special, this);
    lookup[0x01] = std::bind(&EmotionEngine::regimm, this);
    lookup[0x02] = std::bind(&EmotionEngine::j, this);
    lookup[0x03] = std::bind(&EmotionEngine::jal, this);
    lookup[0x04] = std::bind(&EmotionEngine::beq, this);
    lookup[0x05] = std::bind(&EmotionEngine::bne, this);
    lookup[0x06] = std::bind(&EmotionEngine::blez, this);
    lookup[0x07] = std::bind(&EmotionEngine::bgtz, this);
    lookup[0x09] = std::bind(&EmotionEngine::addiu, this);
    lookup[0x0A] = std::bind(&EmotionEngine::slti, this);
    lookup[0x0B] = std::bind(&EmotionEngine::sltiu, this);
    lookup[0x0C] = std::bind(&EmotionEngine::andi, this);
    lookup[0x0D] = std::bind(&EmotionEngine::ori, this);
    lookup[0x0F] = std::bind(&EmotionEngine::lui, this);
    lookup[0x10] = std::bind(&EmotionEngine::cop0, this);
    lookup[0x14] = std::bind(&EmotionEngine::beql, this);
    lookup[0x15] = std::bind(&EmotionEngine::bnel, this);
    lookup[0x1C] = std::bind(&EmotionEngine::mmi, this);
    lookup[0x20] = std::bind(&EmotionEngine::lb, this);
    lookup[0x23] = std::bind(&EmotionEngine::lw, this);
    lookup[0x24] = std::bind(&EmotionEngine::lbu, this);
    lookup[0x25] = std::bind(&EmotionEngine::lhu, this);
    lookup[0x28] = std::bind(&EmotionEngine::sb, this);
    lookup[0x29] = std::bind(&EmotionEngine::sh, this);
    lookup[0x2B] = std::bind(&EmotionEngine::sw, this);
    lookup[0x37] = std::bind(&EmotionEngine::ld, this);
    lookup[0x39] = std::bind(&EmotionEngine::swc1, this);
    lookup[0x3F] = std::bind(&EmotionEngine::sd, this);
}

std::string regNames[] =
{
    "$zero",
    "$at",
    "$v0",
    "$v1",
    "$a0",
    "$a1",
    "$a2",
    "$a3",
    "$t0",
    "$t1",
    "$t2",
    "$t3",
    "$t4",
    "$t5",
    "$t6",
    "$t7",
    "$s0",
    "$s1",
    "$s2",
    "$s3",
    "$s4",
    "$s5",
    "$s6",
    "$s7",
    "$t8",
    "$t9",
    "$k0",
    "$k1",
    "$gp",
    "$sp",
    "$fp",
    "$ra"
};

void EmotionEngine::Clock(uint32_t cycles)
{
    cycles_to_execute = cycles;
    for (int cycle = cycles; cycle > 0; cycle--)
    {
        instr = next_instr;

        next_instr = {};
        next_instr.value = bus->Read32(pc, true);
        next_instr.pc = pc;
        pc += 4;

        skip_branch_delay = false;
        branch_taken = false;

        if (instr.value == 0)
        {
            printf("[EE]: NOP\n");
            return;
        }

        if (!lookup[instr.opcode])
        {
            printf("[EE]: Unimplemented instruction 0x%02X\n", instr.opcode);
            exit(1);
        }
        lookup[instr.opcode]();
        regs[0].i.hi = 0;
        regs[0].i.lo = 0;
        cycles_to_execute--;
    }
    
    COP0.regs[9] += cycles + std::abs(cycles_to_execute);
}

void EmotionEngine::special()
{
    switch (instr.r_type.funct)
    {
    case 0x00:
        sll();
        break;
    case 0x02:
        srl();
        break;
    case 0x03:
        sra();
        break;
    case 0x08:
        jr();
        break;
    case 0x09:
        jalr();
        break;
    case 0x0B:
        movn();
        break;
    case 0x0F:
        sync();
        break;
    case 0x10:
        mfhi();
        break;
    case 0x12:
        mflo();
        break;
    case 0x18:
        mult();
        break;
    case 0x1A:
        div();
        break;
    case 0x1B:
        divu();
        break;
    case 0x21:
        addu();
        break;
    case 0x23:
        subu();
        break;
    case 0x24:
        op_and();
        break;
    case 0x25:
        op_or();
        break;
    case 0x2A:
        slt();
        break;
    case 0x2B:
        sltu();
        break;
    case 0x2D:
        daddu();
        break;
    case 0x38:
        dsll();
        break;
    case 0x3C:
        dsll32();
        break;
    case 0x3E:
        dsrl32();
        break;
    case 0x3F:
        dsra32();
        break;
    default:
        printf("[EE]: Unimplemented special opcode 0x%02X\n", instr.r_type.funct);
        exit(1);
    }
}

void EmotionEngine::regimm()
{
    uint16_t type = (instr.value >> 16) & 0x1F;
    switch (type)
    {
    case 0x00:
        bltz();
        break;
    case 0x01:
        bgez();
        break;
    default:
        printf("[EE]: Unimplemented regimm opcode 0x%02X\n", type);
        exit(1);
    }
}

void EmotionEngine::bltz()
{
    int32_t imm = (int16_t)instr.i_type.immediate;
    uint16_t rs = instr.i_type.rs;

    int32_t offset = imm << 2;
    int64_t reg = regs[rs].ud[0];
    printf("[EE]: BLTZ %s, 0x%08X\n", regNames[rs].c_str(), instr.pc + 4 + offset);
    if (reg < 0)
    {
        pc = instr.pc + 4 + offset;
        branch_taken = true;
    }

    next_instr.is_delay_slot = true;
}

void EmotionEngine::bgez()
{
    int32_t imm = (int16_t)instr.i_type.immediate;
    uint16_t rs = instr.i_type.rs;

    int32_t offset = imm << 2;
    int64_t reg = (int64_t)regs[rs].ud[0];
    next_instr.is_delay_slot = true;
    printf("[EE]: BGEZ %s, 0x%08X\n", regNames[rs].c_str(), instr.pc + 4 + offset);
    if (reg >= 0)
    {
        pc = instr.pc + 4 + offset;
        branch_taken = true;
    }
}

void EmotionEngine::j()
{
    uint32_t instr_index = instr.j_type.target;
    pc = ((instr.pc + 4) & 0xF0000000) | (instr_index << 2);

    next_instr.is_delay_slot = true;
    branch_taken = true;
    printf("[EE]: J 0x%08X\n", pc);
}

void EmotionEngine::jal()
{
    uint32_t instr_index = instr.j_type.target;

    regs[31].ud[0] = instr.pc + 8;
    pc = ((instr.pc + 4) & 0xF0000000) | (instr_index << 2);

    next_instr.is_delay_slot = true;
    branch_taken = true;

    printf("[EE]: JAL 0x%08X\n", pc);
}

void EmotionEngine::beq()
{
    uint8_t rs = instr.i_type.rs;
    uint8_t rt = instr.i_type.rt;

    int64_t imm = (int16_t)instr.i_type.immediate;

    int32_t offset = imm << 2;
    printf("[EE]: BEQ %s (0x%08lX), %s, 0x%08X\n", regNames[rs].c_str(), regs[rs].ud[0], regNames[rt].c_str(), instr.pc + offset + 4);
    if (regs[rs].ud[0] == regs[rt].ud[0])
    {
        pc = instr.pc + offset + 4;
        branch_taken = true;
    }
    next_instr.is_delay_slot = true;
}

void EmotionEngine::bne()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t rs = instr.i_type.rs;
    int32_t imm = (int16_t)instr.i_type.immediate;

    printf("[EE]: BNE %s, %s, 0x%08X (%s)\n", regNames[rt].c_str(), regNames[rs].c_str(), instr.pc + 4 + (imm << 2), regs[rs].ud[0] != regs[rt].ud[0] ? "taken" : "ignored");

    int32_t offset = imm << 2;
    if (regs[rs].ud[0] != regs[rt].ud[0])
    {
        pc = instr.pc + 4 + offset;
        branch_taken = true;
    }

    next_instr.is_delay_slot = true;
}

void EmotionEngine::blez()
{
    int32_t imm = (int16_t)instr.i_type.immediate;
    uint16_t rs = instr.i_type.rs;

    int32_t offset = imm << 2;
    printf("[EE]: BLEZ 0x%08X\n", instr.pc + 4 + offset);
    int64_t reg = regs[rs].ud[0];
    if (reg <= 0)
    {
        pc = instr.pc + 4 + offset;
        branch_taken = true;
    }
    next_instr.is_delay_slot = true;
}

void EmotionEngine::bgtz()
{
    int32_t imm = (int16_t)instr.i_type.immediate;
    uint16_t rs = instr.i_type.rs;

    int32_t offset = imm << 2;
    printf("[EE]: BGTZ 0x%08X\n", instr.pc + 4 + offset);
    int64_t reg = regs[rs].ud[0];
    if (reg > 0)
    {
        pc = instr.pc + 4 + offset;
        branch_taken = true;
    }
    next_instr.is_delay_slot = true;
}

void EmotionEngine::addiu()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t rs = instr.i_type.rs;
    int16_t imm = (int16_t)instr.i_type.immediate;

    int32_t result = regs[rs].ud[0] + imm;
    regs[rt].ud[0] = result;

    printf("[EE]: ADDIU %s, %s, 0x%08X\n", regNames[rt].c_str(), regNames[rs].c_str(), imm);
}

void EmotionEngine::slti()
{
    uint8_t rs = instr.i_type.rs;
    uint8_t rt = instr.i_type.rt;

    int64_t imm = (int16_t)instr.i_type.immediate;

    int64_t reg = regs[rs].ud[0];
    regs[rt].ud[0] = reg < imm;

    printf("[EE]: SLTI %s, %s, 0x%08lX\n", regNames[rt].c_str(), regNames[rs].c_str(), imm);
}

void EmotionEngine::sltiu()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t rs = instr.i_type.rs;
    uint64_t imm = (int16_t)instr.i_type.immediate;

    regs[rt].ud[0] = regs[rs].ud[0] < imm;

    printf("[EE]: SLTIU %s, %s, 0x%08lX\n", regNames[rt].c_str(), regNames[rs].c_str(), imm);
}

void EmotionEngine::andi()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t rs = instr.i_type.rs;
    uint64_t imm = instr.i_type.immediate;

    regs[rt].ud[0] = regs[rs].ud[0] & imm;
    printf("[EE]: ANDI %s, %s, 0x%08lX\n", regNames[rt].c_str(), regNames[rs].c_str(), imm);
}

void EmotionEngine::ori()
{
    uint16_t rs = instr.i_type.rs;
    uint16_t rt = instr.i_type.rt;
    uint16_t imm = instr.i_type.immediate;

    printf("[EE]: ORI %s, %s, 0x%08X\n", regNames[rt].c_str(), regNames[rs].c_str(), imm);

    regs[rt].ud[0] = regs[rs].ud[0] | (uint64_t)imm;
}

void EmotionEngine::lui()
{
    uint16_t rt = instr.i_type.rt;
    uint32_t imm = instr.i_type.immediate;

    regs[rt].ud[0] = (int32_t)(imm << 16);

    printf("[EE]: LUI %s, 0x%08X\n", regNames[rt].c_str(), imm << 16);
}

void EmotionEngine::cop0()
{
    uint8_t fmt = instr.value >> 21 & 0x1F;

    switch (fmt)
    {
    case 0:
    {
        uint8_t rd = (instr.value >> 11) & 0x1F;
        uint8_t rt = (instr.value >> 16) & 0x1F;
        regs[rt] = COP0.regs[rd];
        printf("[EE]: MFC0 %s, $%d\n", regNames[rt].c_str(), rd);
        break;
    }
    case 4:
    {
        uint16_t rt = (instr.value >> 16) & 0x1F;
        uint16_t rd = (instr.value >> 11) & 0x1F;

        COP0.regs[rd] = regs[rt].uw[0];
        printf("[EE]: MTC0 $%d, %s\n", rd, regNames[rt].c_str());
        break;
    }
    case 0x10:
    {
        uint8_t fmt = instr.value & 0x3f;
        switch (fmt)
        {
        case 0x02:
            printf("[EE]: TLBWI\n");
            break;
        default:
            printf("[EE]: Unknown COP0 TLB opcode 0x%02X\n", fmt);
            exit(1);
        }
        break;
    }
    default:
        printf("[EE]: Unknown cop0 opcode 0x%02X\n", fmt);
        exit(1);
    }
}

void EmotionEngine::beql()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t rs = instr.i_type.rs;
    int32_t imm = (int16_t)instr.i_type.immediate;

    int32_t offset = imm << 2;

    printf("[EE]: BEQL %s, %s, 0x%08X\n", regNames[rs].c_str(), regNames[rt].c_str(), instr.pc + 4 + offset);

    if (regs[rs].ud[0] == regs[rt].ud[0])
    {
        pc = instr.pc + 4 + offset;
        branch_taken = true;
    }
    else
    {
        next_instr = {};
        next_instr.value = bus->Read32(pc, true);
        next_instr.pc = pc;
        pc += 4;
        skip_branch_delay = 1;
    }
}

void EmotionEngine::bnel()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t rs = instr.i_type.rs;
    int32_t imm = (int16_t)instr.i_type.immediate;

    int32_t offset = imm << 2;

    printf("[EE]: BNEL %s, %s, 0x%08X\n", regNames[rs].c_str(), regNames[rt].c_str(), instr.pc + 4 + offset);

    if (regs[rs].ud[0] != regs[rt].ud[0])
    {
        pc = instr.pc + 4 + offset;
        branch_taken = true;
    }
    else
    {
        next_instr = {};
        next_instr.value = bus->Read32(pc, true);
        next_instr.pc = pc;
        pc += 4;
        skip_branch_delay = 1;
    }
}

void EmotionEngine::div()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t rs = instr.i_type.rs;

    int32_t reg1 = regs[rs].uw[0];
    int32_t reg2 = regs[rt].uw[0];

    if (reg2 == 0)
    {
        hi0 = reg1;
        lo0 = reg1 >= 0 ? (int32_t)0xFFFFFFFF : 1;
    }
    else if (reg1 == 0x80000000 && reg2 == -1)
    {
        hi0 = 0;
        lo0 = (int32_t)0x80000000;
    }
    else
    {
        hi0 = (int32_t)(reg1 % reg2);
        lo0 = (int32_t)(reg1 / reg2);
    }

    printf("[EE]: DIV %s, %s\n", regNames[rs].c_str(), regNames[rt].c_str());
}

void EmotionEngine::mmi()
{
    uint16_t type = instr.value & 0x1F;
    switch (type)
    {
    case 0x08:
    {
        uint8_t subtype = (instr.value >> 6) & 0x1F;
        switch (subtype)
        {
        default:
            printf("[EE]: Unknown MMI0 opcode 0x%02X\n", subtype);
            exit(1);
        }
        break;
    }
    case 0x12:
        mflo1();
        break;
    case 0x1A:
        div1();
        break;
    default:
        printf("[EE]: Unimplemented MMI opcode 0x%02X\n", type);
        exit(1);
    }
}

void EmotionEngine::mflo1()
{
    uint16_t rd = instr.r_type.rd;

    regs[rd].ud[0] = lo1;

    printf("[EE]: MFLO1 %s\n", regNames[rd].c_str());
}

void EmotionEngine::div1()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t rs = instr.i_type.rs;

    int32_t reg1 = regs[rs].uw[0];
    int32_t reg2 = regs[rt].uw[0];

    if (reg2 == 0)
    {
        hi0 = reg1;
        lo0 = reg1 >= 0 ? (int32_t)0xFFFFFFFF : 1;
    }
    else if (reg1 == 0x80000000 && reg2 == -1)
    {
        hi1 = 0;
        lo1 = (int32_t)0x80000000;
    }
    else
    {
        hi1 = (int32_t)(reg1 % reg2);
        lo1 = (int32_t)(reg1 / reg2);
    }

    printf("[EE]: DIV1 %s, %s\n", regNames[rs].c_str(), regNames[rt].c_str());
}

void EmotionEngine::lb()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t base = instr.i_type.rs;
    int16_t offset = (int16_t)instr.i_type.immediate;

    uint32_t vaddr = offset + regs[base].uw[0];
    regs[rt].ud[0] = bus->Read8(vaddr, true);

    printf("[EE]: LB %s, 0x%04X(%s) 0x%08X\n", regNames[rt].c_str(), offset, regNames[base].c_str(), vaddr);
}

void EmotionEngine::lw()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t base = instr.i_type.rs;
    int16_t offset = (int16_t)instr.i_type.immediate;

    uint32_t vaddr = offset + regs[base].uw[0];

    if (vaddr & 0x3)
    {
        printf("[EE]: LW: Address 0x%08X is not aligned\n", vaddr);
        exception(Exception::AddrErrorLoad, true);
    }
    else
    {
        regs[rt].ud[0] = (int32_t)bus->Read32(vaddr, true);
        printf("[EE]: LW %s, 0x%X(%s) 0x%08X\n", regNames[rt].c_str(), offset, regNames[base].c_str(), vaddr);
    }
}

void EmotionEngine::lbu()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t base = instr.i_type.rs;
    int16_t offset = (int16_t)instr.i_type.immediate;

    uint32_t vaddr = offset + regs[base].uw[0];
    regs[rt].ud[0] = bus->Read8(vaddr, true);

    printf("[EE]: LBU %s, 0x%X(%s)\n", regNames[rt].c_str(), offset, regNames[base].c_str());
}

void EmotionEngine::lhu()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t base = instr.i_type.rs;
    int16_t offset = (int16_t)instr.i_type.immediate;

    uint32_t vaddr = offset + regs[base].uw[0];
    if (vaddr & 0x1)
    {
        printf("[EE]: LHU: Address 0x%08X is not aligned\n", vaddr);
        exception(Exception::AddrErrorLoad, true);
    }
    else
        regs[rt].ud[0] = bus->Read16(vaddr, true);
    printf("[EE]: LHU %s, 0x%02X(%s)\n", regNames[rt].c_str(), offset, regNames[base].c_str());
}

void EmotionEngine::sb()
{
    uint16_t base = instr.i_type.rs;
    uint16_t rt = instr.i_type.rt;
    int16_t offset = (int16_t)instr.i_type.immediate;

    uint32_t vaddr = offset + regs[base].uw[0];
    uint16_t data = regs[rt].uw[0] & 0xFF;

    printf("[EE]: SB %s, 0x%02X(%s)\n", regNames[rt].c_str(), offset, regNames[base].c_str());
    bus->Write8(vaddr, data, true);
}

void EmotionEngine::sh()
{
    uint16_t base = instr.i_type.rs;
    uint16_t rt = instr.i_type.rt;
    int16_t offset = (int16_t)instr.i_type.immediate;

    uint32_t vaddr = offset + regs[base].uw[0];
    uint16_t data = regs[rt].uw[0] & 0xFFFF;

    printf("[EE]: SH %s, 0x%X(%s)\n", regNames[rt].c_str(), offset, regNames[base].c_str());
    if (vaddr & 0x1)
    {
        printf("[EE]: SH: Address 0x%08X is not aligned\n", vaddr);
        exception(Exception::AddrErrorStore, true);
    }
    else
        bus->Write16(vaddr, data, true);
}

void EmotionEngine::sw()
{
    uint16_t base = instr.i_type.rs;
    uint16_t rt = instr.i_type.rt;
    int16_t offset = (int16_t)instr.i_type.immediate;

    uint32_t vaddr = offset + regs[base].uw[0];
    uint32_t data = regs[rt].uw[0];

    printf("[EE]: SW %s, 0x%08X(%s)\n", regNames[rt].c_str(), offset, regNames[base].c_str());
    if (vaddr & 0x3)
    {
        printf("[EE]: SW: Address 0x%08X is not aligned\n", vaddr);
        exception(Exception::AddrErrorStore, true);
    }
    else
        bus->Write32(vaddr, data, true);
}

void EmotionEngine::ld()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t base = instr.i_type.rs;
    int16_t offset = (int16_t)instr.i_type.immediate;

    uint32_t vaddr = offset + regs[base].uw[0];

    if (vaddr & 0x7)
    {
        printf("[EE]: LD: Address 0x%08X is not aligned\n", vaddr);
        exception(Exception::AddrErrorLoad, true);
    }
    else
    {
        regs[rt].ud[0] = bus->Read64(vaddr, true);
        printf("[EE]: LD %s, 0x%02X(%s)\n", regNames[rt].c_str(), offset, regNames[base].c_str());
    }
}

void EmotionEngine::swc1()
{
    uint16_t base = instr.i_type.rs;
    uint16_t ft = instr.i_type.rt;
    int16_t offset = (int16_t)instr.i_type.immediate;

    uint32_t vaddr = offset + regs[base].uw[0];
    uint32_t data = cop1.fpr[ft].uint;

    printf("[EE]: SWC1 $%d, 0x%04X(%s)\n", ft, offset, regNames[base].c_str());
}

void EmotionEngine::sd()
{
    uint16_t base = instr.i_type.rs;
    uint16_t rt = instr.i_type.rt;
    int16_t offset = (int16_t)instr.i_type.immediate;

    uint32_t vaddr = offset + regs[base].uw[0];
    uint64_t data = regs[rt].ud[0];

    printf("[EE]: SW %s, 0x%08X(%s)\n", regNames[rt].c_str(), offset, regNames[base].c_str());

    if (vaddr & 0x7)
    {
        printf("[EE]: SW: Address 0x%08X is not aligned\n", vaddr);
        exception(Exception::AddrErrorStore, true);
    }
    else
        bus->Write64(vaddr, data, true);
}

// Special

void EmotionEngine::sll()
{
    uint16_t rt = instr.r_type.rt;
    uint16_t rd = instr.r_type.rd;
    uint16_t sa = instr.r_type.sa;

    regs[rd].ud[0] = (uint64_t)(int32_t)(regs[rt].uw[0] << sa);

    printf("[EE]: SLL %s, %s, 0x%0X\n", regNames[rd].c_str(), regNames[rt].c_str(), sa);
}

void EmotionEngine::srl()
{
    uint16_t sa = instr.r_type.sa;
    uint16_t rd = instr.r_type.rd;
    uint16_t rt = instr.r_type.rt;

    regs[rd].ud[0] = (int32_t)(regs[rt].uw[0] >> sa);

    printf("[EE]: SRL %s, %s, 0x%02X\n", regNames[rd].c_str(), regNames[rt].c_str(), sa);
}

void EmotionEngine::sra()
{
    uint16_t sa = instr.r_type.sa;
    uint16_t rd = instr.r_type.rd;
    uint16_t rt = instr.r_type.rt;

    int32_t reg = (int32_t)regs[rt].uw[0];
    regs[rd].ud[0] = reg >> sa;

    printf("[EE]: SRA %s, %s, 0x%X\n", regNames[rd].c_str(), regNames[rt].c_str(), sa);
}

void EmotionEngine::jr()
{
    uint16_t rs = instr.i_type.rs;
    pc = regs[rs].uw[0];

    next_instr.is_delay_slot = true;
    branch_taken = true;

    printf("[EE]: JR %s\n", regNames[rs].c_str());
}

void EmotionEngine::jalr()
{
    uint16_t rs = instr.r_type.rs;
    uint16_t rd = instr.r_type.rd;

    pc = regs[rs].uw[0];
    regs[rd].ud[0] = instr.pc + 8;

    printf("[EE]: JALR %s, %s\n", regNames[rd].c_str(), regNames[rs].c_str());
}

void EmotionEngine::movn()
{
    uint16_t rt = instr.r_type.rt;
    uint16_t rs = instr.r_type.rs;
    uint16_t rd = instr.r_type.rd;

    if (regs[rt].ud[0] != 0) regs[rd].ud[0] = regs[rs].ud[0];

    printf("[EE]: MOVN %s, %s, %s\n", regNames[rt].c_str(), regNames[rd].c_str(), regNames[rs].c_str());
}

void EmotionEngine::sync()
{
    printf("[EE]: SYNC\n");
}

void EmotionEngine::mult()
{
    uint16_t rt = instr.r_type.rt;
    uint16_t rs = instr.r_type.rs;
    uint16_t rd = instr.r_type.rd;

    int64_t reg1 = (int64_t)regs[rs].ud[0];
    int64_t reg2 = (int64_t)regs[rt].ud[0];
    int64_t result = reg1 * reg2;

    regs[rd].ud[0] = lo0 = (int32_t)(result & 0xFFFFFFFF);
    hi0 = (int32_t)(result >> 32);

    printf("[EE]: MULT %s, %s, %s\n", regNames[rd].c_str(), regNames[rs].c_str(), regNames[rt].c_str());
}

void EmotionEngine::mfhi()
{
    uint16_t rd = instr.r_type.rd;

    regs[rd].ud[0] = hi0;
    printf("[EE]: MFHI %s\n", regNames[rd].c_str());
}

void EmotionEngine::mflo()
{
    uint16_t rd = instr.r_type.rd;
    regs[rd].ud[0] = lo0;

    printf("[EE]: MFLO %s\n", regNames[rd].c_str());
}

void EmotionEngine::divu()
{
    uint16_t rt = instr.i_type.rt;
    uint16_t rs = instr.i_type.rs;

    if (regs[rt].uw[0] == 0)
    {
        hi0 = (int32_t)regs[rs].uw[0];
        lo0 = (int32_t)0xFFFFFFFF;
    }
    else
    {
        hi0 = (int32_t)(regs[rs].uw[0] % regs[rt].uw[0]);
        lo0 = (int32_t)(regs[rs].uw[0] / regs[rt].uw[0]);
    }

    printf("[EE]: DIVU %s, %s\n", regNames[rs].c_str(), regNames[rt].c_str());
}

void EmotionEngine::subu()
{
    uint16_t rt = instr.r_type.rt;
    uint16_t rs = instr.r_type.rs;
    uint16_t rd = instr.r_type.rd;

    int32_t reg1 = regs[rs].ud[0];
    int32_t reg2 = regs[rt].ud[0];
    regs[rd].ud[0] = reg1 - reg2;
    printf("[EE]: SUBU %s, %s, %s\n", regNames[rd].c_str(), regNames[rs].c_str(), regNames[rt].c_str());
}

void EmotionEngine::addu()
{
    uint16_t rt = instr.r_type.rt;
    uint16_t rs = instr.r_type.rs;
    uint16_t rd = instr.r_type.rd;
    
    int32_t result = regs[rs].ud[0] + regs[rt].ud[0];
    regs[rd].ud[0] = result;

    printf("[EE]: ADDU %s, %s, %s\n", regNames[rd].c_str(), regNames[rs].c_str(), regNames[rt].c_str());
}

void EmotionEngine::op_and()
{
    uint16_t rt = instr.r_type.rt;
    uint16_t rs = instr.r_type.rs;
    uint16_t rd = instr.r_type.rd;

    regs[rd].ud[0] = regs[rs].ud[0] & regs[rt].ud[0];

    printf("[EE]: AND %s, %s, %s\n", regNames[rd].c_str(), regNames[rs].c_str(), regNames[rt].c_str());
}

void EmotionEngine::op_or()
{
    uint16_t rt = instr.r_type.rt;
    uint16_t rs = instr.r_type.rs;
    uint16_t rd = instr.r_type.rd;

    printf("[EE]: OR %s, %s, %s\n", regNames[rd].c_str(), regNames[rs].c_str(), regNames[rt].c_str());

    regs[rd].ud[0] = regs[rs].ud[0] | regs[rt].ud[0];
}

void EmotionEngine::slt()
{
    uint16_t rt = instr.r_type.rt;
    uint16_t rs = instr.r_type.rs;
    uint16_t rd = instr.r_type.rd;

    int64_t reg1 = regs[rs].ud[0];
    int64_t reg2 = regs[rt].ud[0];
    regs[rd].ud[0] = reg1 < reg2;

    printf("[EE]: SLT %s, %s, %s\n", regNames[rd].c_str(), regNames[rs].c_str(), regNames[rt].c_str());
}

void EmotionEngine::sltu()
{
    uint16_t rt = instr.r_type.rt;
    uint16_t rs = instr.r_type.rs;
    uint16_t rd = instr.r_type.rd;

    printf("[EE]: SLTU %s, %s, %s\n", regNames[rd].c_str(), regNames[rs].c_str(), regNames[rt].c_str());
    regs[rd].ud[0] = regs[rs].ud[0] < regs[rt].ud[0];
}

void EmotionEngine::daddu()
{
    uint16_t rt = instr.r_type.rt;
    uint16_t rs = instr.r_type.rs;
    uint16_t rd = instr.r_type.rd;

    int64_t reg1 = regs[rs].ud[0];
    int64_t reg2 = regs[rt].ud[0];

    regs[rd].ud[0] = reg1 + reg2;
    printf("[EE]: DADDU %s, %s, %s\n", regNames[rd].c_str(), regNames[rs].c_str(), regNames[rt].c_str());
}

void EmotionEngine::dsll()
{
    uint16_t sa = instr.r_type.sa;
    uint16_t rd = instr.r_type.rd;
    uint16_t rt = instr.r_type.rt;

    regs[rd].ud[0] = (int32_t)(regs[rt].uw[0] >> sa);

    printf("[EE]: DSLL %s, %s, 0x%02X\n", regNames[rd].c_str(), regNames[rt].c_str(), sa);
}

void EmotionEngine::dsll32()
{
    uint16_t sa = instr.r_type.sa;
    uint16_t rd = instr.r_type.rd;
    uint16_t rt = instr.r_type.rt;

    printf("[EE]: DSLL32 %s, %s, 0x%02X\n", regNames[rd].c_str(), regNames[rt].c_str(), sa);
    regs[rd].ud[0] = regs[rt].ud[0] << (sa + 32);
}

void EmotionEngine::dsrl32()
{
    uint16_t sa = instr.r_type.sa;
    uint16_t rd = instr.r_type.rd;
    uint16_t rt = instr.r_type.rt;
    
    regs[rd].ud[0] = regs[rt].ud[0] >> (sa + 32);

    printf("[EE]: DSRL32 %s, %s, 0x%02X\n", regNames[rd].c_str(), regNames[rt].c_str(), sa);
}

void EmotionEngine::dsra32()
{
    uint16_t sa = instr.r_type.sa;
    uint16_t rd = instr.r_type.rd;
    uint16_t rt = instr.r_type.rt;

    int64_t reg = (int64_t)regs[rt].ud[0];
    regs[rd].ud[0] = reg >> (sa + 32);

    printf("[EE]: DSRA32 %s, %s, 0x%02X\n", regNames[rd].c_str(), regNames[rt].c_str(), sa);
}
