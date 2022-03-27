#include <vu.hpp>
#include <EE.hpp>

VectorUnit::VectorUnit(EmotionEngine* parent)
: cpu(parent)
{
    regs.vf[0].w = 1.0f;
}

void VectorUnit::cfc2(Instruction instr)
{
    uint16_t id = instr.r_type.rd;
    uint16_t rt = instr.r_type.rt;
    auto ptr = (uint32_t*)&regs + id;

    cpu->regs[rt].ud[0] = (int32_t)*ptr;
}

void VectorUnit::qmtc2(Instruction instr)
{
    uint16_t fd = instr.r_type.rd;
    uint16_t rt = instr.r_type.rt;

    regs.vf[fd].qword = *(uint128_t*)&cpu->regs[rt];
}

void VectorUnit::ctc2(Instruction instr)
{
    uint16_t id = instr.r_type.rd;
    uint16_t rt = instr.r_type.rt;
    auto ptr = (uint32_t*)&regs + id;
    *ptr = cpu->regs[rt].uw[0];
}

void VectorUnit::special1(Instruction instr)
{
    uint32_t function = instr.value & 0x3F;
    VUInstr vu_instr = {.value = instr.value};
    switch (function)
    {
    case 0x2C:
        vsub(vu_instr);
        break;
    case 0x3C ... 0x3F: 
        special2(instr);
        break;
    default:
        printf("[VU0]: Unimplemented special 0x%X\n", function);
        exit(1);
    }
}

void VectorUnit::vsub(VUInstr instr)
{
    uint16_t fd = instr.fd;
    uint16_t fs = instr.fs;
    uint16_t ft = instr.ft;

    for (int i = 0; i < 4; i++)
    {
        if (instr.dest & (1 << (3 - i)))
        {
            regs.vf[fd].fword[i] = regs.vf[fs].fword[i] - regs.vf[ft].fword[i];
        }
    }
}

void VectorUnit::special2(Instruction instr)
{
    uint32_t flo = instr.value & 0x3;
    uint32_t fhi = (instr.value >> 6) & 0x1F;
    uint32_t opcode = flo | (fhi * 4);

    VUInstr vu_instr = {.value = instr.value};
    switch (opcode)
    {
    case 0x35:
        vsqi(vu_instr);
        break;
    case 0x3F: 
        viswr(vu_instr);
        break;
    
    default:
        printf("[VU0]: Unimplemented special2 0x%X\n", opcode);
        exit(1);
    }
}

void VectorUnit::vsqi(VUInstr instr)
{
    uint16_t fs = instr.fs;
    uint16_t it = instr.it;

    uint32_t address = regs.vi[it] * 16;
    auto ptr = (uint32_t*)&data[address];

    if (address > 0xFF0)
        return;
    
    for (int i = 0; i < 4; i++)
    {
        if (instr.dest & (1 << (3 - i)))
        {
            *(ptr + i) = regs.vf[fs].word[i];
        }
    }
}

void VectorUnit::viswr(VUInstr instr)
{
    uint16_t is = instr.is;
    uint16_t it = instr.it;

    uint32_t address = regs.vi[is] * 16;

    if (address > 0xFF0)
        return;
    
    auto ptr = (uint32_t*)&data[address];

    for (int i = 0; i < 4; i++)
    {
        if (instr.dest & (1 << i))
        {
            *(ptr + i) = regs.vi[it] & 0xFFFF;
        }
    }
}

void VectorUnit::qmfc2(Instruction instr)
{
    uint16_t fd = instr.r_type.rd;
    uint16_t rt = instr.r_type.rt;

    *(uint128_t*)&cpu->regs[rt] = regs.vf[fd].qword;
}