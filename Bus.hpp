#pragma once

#include <cstdint>
#include <string>
#include <gs/gs.hpp>
#include <intc.hpp>
#include <ee_timers.hpp>
#include <sif.hpp>
#include <int128.h>
#include <gs/gif.hpp>
#include <dmac.hpp>
#include <vu.hpp>
#include <vif.h>
#include <ipu.h>
#include <sio2.h>
#include <iop/iop_intc.hpp>

class Bus
{
private:
    uint8_t bios[0x400000];
    uint8_t eeScratchpad[1024*16];
    uint8_t iopScratchpad[1024];
    uint32_t iop_scratchpad_start = 0x1F800000;

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
    std::ofstream console;

    uint32_t MCH_RICM = 0, MCH_DRD = 0;
    uint8_t rdram_sdevid = 0;
    uint32_t ram_setting = 0;

    uint32_t TranslateAddr(uint32_t addr)
    {
        if (addr >= 0x70000000 && addr <= 0x70004000)
            return addr;
        else if (addr >= 0x30100000 && addr <= 0x32000000)
            return addr & 0x1FFFFFF;
        else
            return addr & 0x1FFFFFFF;
    }
public:
    uint8_t eeRam[0x2000000];
    uint8_t iopRam[0x200000];
    gs::GraphicsSynthesizer *gs;
    INTC* intc;
    Timers* timers;
    SIF* sif;
    GIF* gif;
    DMAController* dmac;
    VectorUnit* vu[2];
    VIF* vif[2];
    IPU* ipu;
    SIO2* sio2;
    IOP_INTC* iop_intc;
    Bus(std::string biosFilePath, gs::GraphicsSynthesizer *gs);
    void attachIntc(INTC* _intc) {intc = _intc;}
    void attachTimers(Timers* _timer) {timers = _timer;}
    void attachGIF(GIF* _gif) {gif = _gif;}

    uint8_t Read8(uint32_t addr, bool ee);
    uint16_t Read16(uint32_t addr, bool ee);
    uint32_t Read32(uint32_t addr, bool ee);
    uint64_t Read64(uint32_t addr, bool ee);
    Register Read128(uint32_t addr);
    void Write8(uint32_t addr, uint8_t data, bool ee);
    void Write16(uint32_t addr, uint16_t data, bool ee);
    void Write32(uint32_t addr, uint32_t data, bool ee);
    void Write64(uint32_t addr, uint64_t data, bool ee);
    void Write128(uint32_t addr, Register data);

    uint8_t iop_read8(uint32_t addr)
    {
        if (addr >= 0x1FC00000 && addr <= 0x20000000)
            return bios[addr - 0x1FC00000];
        if (addr < 0x00200000)
            return iopRam[addr];
        printf("[BUS]: Read8 from unknown addr 0x%08X\n", addr);
        exit(1);
    }

    uint16_t iop_read16(uint32_t addr)
    {
        if (addr >= 0x1FC00000 && addr <= 0x20000000)
            return *(uint16_t*)&bios[addr - 0x1FC00000];
        if (addr < 0x00200000)
            return *(uint16_t*)&iopRam[addr];
        printf("[BUS]: Read16 from unknown addr 0x%08X\n", addr);
        exit(1);
    }

    uint32_t iop_read32(uint32_t addr)
    {
        if (addr >= 0x1FC00000 && addr <= 0x20000000)
            return *(uint32_t*)&bios[addr - 0x1FC00000];
        if (addr < 0x00200000)
            return *(uint32_t*)&iopRam[addr];
        switch (addr)
        {
        case 0x1F801450:
            return 0x20;
        case 0x1F801010:
            return 0x20;
        case 0x1F801070:
            return iop_intc->read_istat();
        case 0x1F801074:
            return iop_intc->read_imask();
        case 0x1F801078:
            return iop_intc->read_ictrl();
        }
        printf("[BUS]: Read32 from unknown addr 0x%08X\n", addr);
        exit(1);
    }

    void iop_write8(uint32_t addr, uint8_t data)
    {
        if (addr < 0x00200000)
        {
            iopRam[addr] = data;
            return;
        }
        switch (addr)
        {
        case 0x1F802070:
            return;
        }
        printf("[BUS]: Write8 to unknown addr 0x%08X\n", addr);
        exit(1);
    }

    void iop_write16(uint32_t addr, uint16_t data)
    {
        printf("[BUS]: Write16 to unknown addr 0x%08X\n", addr);
        exit(1);
    }

    void iop_write32(uint32_t addr, uint32_t data)
    {
        switch (addr)
        {
        case 0x1F802070: // POST2 (BIOS POST value)
            return;
        case 0xFFFE0130: // Cache control, irrelevant
            return;
        case 0x1F801070:
            iop_intc->write_istat(data);
            return;
        case 0x1F801074:
            iop_intc->write_imask(data);
            return;
        case 0x1F801078:
            iop_intc->write_ictrl(data);
            return;
        }
        printf("[BUS]: Write32 to unknown addr 0x%08X\n", addr);
        exit(1);
    }
};