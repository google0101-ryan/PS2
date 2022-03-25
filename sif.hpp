#pragma once

#include <queue>
#include <cstdint>

struct SIFRegs
{
    uint32_t mscom;
    uint32_t smcom;
    uint32_t msflg;
    uint32_t smflg;
    uint32_t ctrl;
    uint32_t padding;
    uint32_t bd6;
};

class SIF
{
public:
    SIF();

    uint32_t read(uint32_t addr);
    void write(uint32_t addr, uint32_t data);
private:
    SIFRegs regs = {};
public:
    std::queue<uint32_t> sif0_fifo, sif1_fifo;
};