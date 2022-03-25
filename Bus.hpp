#pragma once

#include <cstdint>
#include <string>
#include <gs/gs.hpp>
#include <intc.hpp>
#include <ee_timers.hpp>
#include <sif.hpp>

class Bus
{
private:
    uint8_t bios[0x400000];
    uint8_t eeRam[0x2000000];
    uint8_t eeScratchpad[1024*16];

    uint32_t KUSEG_MASKS[8] = 
    {
        /* KUSEG: Don't touch the address, it's fine */
        0xffffffff, 0xfffffff, 0xfffffff, 0xffffffff,
        /* KSEG0: Strip the MSB (0x8 -> 0x0 and 0x9 -> 0x1) */
        0x7fffffff,
        /* KSEG1: Strip the 3 MSB's (0xA -> 0x0 and 0xB -> 0x1) */
        0x1fffffff,
        /* KSEG2: Don't touch the address, it's fine */
        0xffffffff, 0x1fffffff,
    };

    gs::GraphicsSynthesizer *gs;
    INTC* intc;
    Timers* timers;
    SIF* sif;
    std::ofstream console;

    uint32_t MCH_RICM = 0, MCH_DRD = 0;
    uint8_t rdram_sdevid = 0;
public:
    Bus(std::string biosFilePath, gs::GraphicsSynthesizer *gs);
    void attachIntc(INTC* _intc) {intc = _intc;}
    void attachTimers(Timers* _timer) {timers = _timer;}

    uint8_t Read8(uint32_t addr, bool ee);
    uint16_t Read16(uint32_t addr, bool ee);
    uint32_t Read32(uint32_t addr, bool ee);
    uint64_t Read64(uint32_t addr, bool ee);
    void Write8(uint32_t addr, uint8_t data, bool ee);
    void Write16(uint32_t addr, uint16_t data, bool ee);
    void Write32(uint32_t addr, uint32_t data, bool ee);
    void Write64(uint32_t addr, uint64_t data, bool ee);
};