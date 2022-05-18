#pragma once

#include <int128.h>

class EmotionEngine;
struct Instruction;

union Vector
{
    uint128_t qword;
    uint32_t word[4];
    float fword[4];
    struct {float x, y, z, w;};
};

struct Registers
{
    uint32_t vi[16];
    uint32_t control[16];
    Vector vf[32];
};

union VUInstr
{
    uint32_t value;
	struct
	{
		uint32_t : 6;
		uint32_t id : 5;
		uint32_t is : 5;
		uint32_t it : 5;
		/* This acts as a bit mask that tells the CPU which fields
		   to write (X, Y, Z, W) */
		uint32_t dest : 4;
		uint32_t : 7;
	};
	struct
	{
		uint32_t : 6;
		uint32_t fd : 5;
		uint32_t fs : 5;
		uint32_t ft : 5;
		uint32_t : 11;
	};
};

enum Memory
{
    Code = 0,
    Data = 1
};

class VectorUnit
{
public:
    VectorUnit(EmotionEngine* parent);
    ~VectorUnit() = default;

    template<Memory mem, typename T>
    T read(uint32_t addr);

    template<Memory mem, typename T>
    void write(uint32_t addr, T value);

    void qmfc2(Instruction instr); // 0x01
    void cfc2(Instruction instr); // 0x02
    void qmtc2(Instruction instr); // 0x05
    void ctc2(Instruction instr); // 0x06
    void special1(Instruction instr);
    void viadd(VUInstr instr);
    void vsub(VUInstr instr);
    void special2(Instruction instr); // 0x3x
    void vsqi(VUInstr instr); // 0x35
    void viswr(VUInstr instr); // 0x3F

    Registers regs = {};
    Vector acc;

    uint8_t code[16*1024] = {};
    uint8_t data[16*1024] = {};
private:
    EmotionEngine* cpu;
};

template<Memory mem, typename T>
inline T VectorUnit::read(uint32_t addr)
{
    if constexpr (mem == Memory::Data)
        return *(T*)&data[addr & 0x3FFF];
    else
        return *(T*)&code[addr & 0x3FFF];
}

template<Memory mem, typename T>
inline void VectorUnit::write(uint32_t addr, T value)
{
    if constexpr (mem == Memory::Data)
		*(T*)&data[addr & 0x3fff] = value;
	else
		*(T*)&code[addr & 0x3fff] = value;
}