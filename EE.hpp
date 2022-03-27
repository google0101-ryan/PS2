#pragma once

#include <Bus.hpp>
#include <int128.h>
#include <unordered_map>
#include <functional>
#include <intc.hpp>
#include <ee_timers.hpp>
#include <fstream>

union FPR
{
    uint32_t uint = 0;
    float fint;
};

union FCR0
{
    uint32_t value = 0;
    struct
    {
        uint32_t revision : 8;
        uint32_t impl : 8;
        uint32_t : 16;
    };
};

union FCR31
{
    uint32_t value = 0;
    struct
    {
        uint32_t round_mode : 2;
        uint32_t flags : 5;
        uint32_t enable : 5;
        uint32_t cause : 6;
        uint32_t : 5;
        uint32_t condition : 1;
        uint32_t flush_denormals : 1;
        uint32_t : 7;
    };
};

struct Instruction
{
    union
    {
        uint32_t value;
        struct
        { /* Used when polling for the opcode */
            uint32_t : 26;
            uint32_t opcode : 6;
        };
        struct
        {
            uint32_t immediate : 16;
            uint32_t rt : 5;
            uint32_t rs : 5;
            uint32_t opcode : 6;
        } i_type;
        struct
        {
            uint32_t target : 26;
            uint32_t opcode : 6;
        } j_type;
        struct
        {
            uint32_t funct : 6;
            uint32_t sa : 5;
            uint32_t rd : 5;
            uint32_t rt : 5;
            uint32_t rs : 5;
            uint32_t opcode : 6;
        } r_type;
    };

    uint32_t pc;
    bool is_delay_slot = false;
};

struct EE_COP1
{
    void execute(Instruction instr);
    void op_adda(Instruction instr);
    void op_madd(Instruction instr);

    FPR fpr[32];
    FCR0 fcr0;
    FCR31 fcr31;
    FPR acc;
};

class EmotionEngine
{
private:
    void fetch_next()
    {
        next_instr = {};
        next_instr.value = bus->Read32(pc, true);
        next_instr.pc = pc;
        pc += 4;
    }
    uint32_t hi0 = 0, hi1 = 0;
    uint32_t lo0 = 0, lo1 = 0;
    uint32_t pc;
    uint32_t sa = 0;
    Bus* bus;

    Instruction instr, next_instr;
    bool skip_branch_delay = false;
    bool branch_taken = false;

    /* The status register fields */
    union COP0Status
    {
        uint32_t value;
        struct
        {
            uint32_t ie : 1; /* Interrupt Enable */
            uint32_t exl : 1; /* Exception Level */
            uint32_t erl : 1; /* Error Level */
            uint32_t ksu : 2; /* Kernel/Supervisor/User Mode bits */
            uint32_t : 5;
            uint32_t im0 : 1; /* Int[1:0] signals */
            uint32_t im1 : 1;
            uint32_t bem : 1; /* Bus Error Mask */
            uint32_t : 2;
            uint32_t im7 : 1; /* Internal timer interrupt  */
            uint32_t eie : 1; /* Enable IE */
            uint32_t edi : 1; /* EI/DI instruction Enable */
            uint32_t ch : 1; /* Cache Hit */
            uint32_t : 3;
            uint32_t bev : 1; /* Location of TLB refill */
            uint32_t dev : 1; /* Location of Performance counter */
            uint32_t : 2;
            uint32_t fr : 1; /* Additional floating point registers */
            uint32_t : 1;
            uint32_t cu : 4; /* Usability of each of the four coprocessors */
        };
    };

    union COP0Cause
    {
        uint32_t value;
        struct
        {
            uint32_t : 2;
            uint32_t exccode : 5;
            uint32_t : 3;
            uint32_t ip0_pending : 1;
            uint32_t ip1_pending : 1;
            uint32_t siop : 1;
            uint32_t : 2;
            uint32_t timer_ip_pending : 1;
            uint32_t exc2 : 3;
            uint32_t : 9;
            uint32_t ce : 2;
            uint32_t bd2 : 1;
            uint32_t bd : 1;
        };
    };

    enum OperatingMode 
    {
        USER_MODE = 0b10,
        SUPERVISOR_MODE = 0b01,
        KERNEL_MODE = 0b00
    };

    /* The COP0 registers */
    union COP0
    {
        COP0() { status.value = 0x400004; /* BEV, ERL = 1 by default */ }

        uint32_t regs[32] = {};
        struct
        {
            uint32_t index;
            uint32_t random;
            uint32_t entry_lo0;
            uint32_t entry_lo1;
            uint32_t context;
            uint32_t page_mask;
            uint32_t wired;
            uint32_t reserved0[1];
            uint32_t bad_vaddr;
            uint32_t count;
            uint32_t entryhi;
            uint32_t compare;
            COP0Status status;
            COP0Cause cause;
            uint32_t epc;
            uint32_t prid;
            uint32_t config;
            uint32_t reserved1[6];
            uint32_t bad_paddr;
            uint32_t debug;
            uint32_t perf;
            uint32_t reserved2[2];
            uint32_t tag_lo;
            uint32_t tag_hi;
            uint32_t error_epc;
            uint32_t reserved3[1];
        };

        OperatingMode get_operating_mode()
        {
            if (status.exl || status.erl) /* Setting one of these enforces kernel mode */
                return OperatingMode::KERNEL_MODE;
            else
                return (OperatingMode)status.ksu;
        }
    };

    uint32_t exception_addr[2] = { 0x80000000, 0xBFC00200 };

    enum ExceptionVector
    {
        V_TLB_REFILL = 0x0,
        V_COMMON = 0x180,
        V_INTERRUPT = 0x200
    };

    void special(); // 0x00
    void regimm(); // 0x01
    void bltz();
    void bgez();
    void j(); // 0x02
    void jal(); // 0x03
    void beq(); // 0x04
    void bne(); // 0x05
    void blez(); // 0x06
    void bgtz(); // 0x07
    void addiu(); // 0x09
    void slti(); // 0x0A
    void sltiu(); // 0x0B
    void andi(); // 0x0C
    void ori(); // 0x0D
    void xori(); // 0x0E
    void lui(); // 0x0F
    void op_cop0(); // 0x10
    void op_cop2(); // 0x12
    void beql(); // 0x14
    void bnel(); // 0x15
    void daddiu(); // 0x19
    void div(); // 0x1A
    void mmi(); // 0x1C
    void mmi2(); // 0x09 (MMI)
    void pand(); // 0x12 (MMI2)
    void mflo1();
    void mult1();
    void div1();
    void divu1();
    void lq(); // 0x1E
    void sq(); // 0x1F
    void lb(); // 0x20
    void lh(); // 0x21
    void lw(); // 0x23
    void lbu(); // 0x24
    void lhu(); // 0x25
    void sb(); // 0x28
    void sh(); // 0x29
    void sw(); // 0x2B
    void cache() {} // 0x2F
    void ld(); // 0x37
    void swc1(); // 0x39
    void sd(); // 0x3F

    void sll(); // 0x00
    void srl(); // 0x02
    void sra(); // 0x03
    void sllv(); // 0x04
    void srav(); // 0x07
    void jr(); // 0x08
    void jalr(); // 0x09
    void movz(); // 0x0A
    void movn(); // 0x0B
    void sync(); // 0x0F
    void mfhi(); // 0x10
    void mflo(); // 0x12
    void dsllv(); // 0x14
    void dsrav(); // 0x17
    void mult(); // 0x18
    void divu(); // 0x1B
    void addu(); // 0x21
    void subu(); // 0x23
    void op_and(); // 0x24
    void op_or(); // 0x25
    void nor(); // 0x27
    void slt(); // 0x2A
    void sltu(); // 0x2B
    void dadd(); // 0x2C
    void daddu(); // 0x2D
    void dsll(); // 0x38
    void dsll32(); // 0x3C
    void dsrl32(); // 0x3E
    void dsra32(); // 0x3F
    INTC intc;
    Timers timers;
    int cycles_to_execute = 0;
public:
    Register regs[32];

    enum Exception
    {
        Interrupt = 0,
        TLBModified = 1,
        TLBLoad = 2,
        TLBStore = 3,
        AddrErrorLoad = 4,
        AddrErrorStore = 5,
        Syscall = 8,
        Break = 9,
        Reserved = 10,
        CopUnusable = 11,
        Overflow = 12,
        Trap = 13,
    };
    COP0 cop0;
    EE_COP1 cop1;
    EmotionEngine(Bus* _bus);
    INTC* getIntc() {return &intc;}
    Timers* getTimers() {return &timers;}

    void Clock(uint32_t cycles);
    void exception(Exception exception, bool log)
    {
        if (log)
        {
            printf("[EE]: Exception occured of type %d\n", (uint32_t)exception);
        }

        ExceptionVector vector = ExceptionVector::V_COMMON;
        cop0.cause.exccode = (uint32_t)exception;
        if (!cop0.status.exl)
        {
            cop0.epc = instr.pc - 4 * instr.is_delay_slot;
            cop0.cause.bd = instr.is_delay_slot;

            switch (exception)
            {
            case Exception::TLBLoad:
            case Exception::TLBStore:
                vector = ExceptionVector::V_TLB_REFILL;
                break;;
            case Exception::Interrupt:
                vector = ExceptionVector::V_INTERRUPT;
                break;;
            default:
                vector = ExceptionVector::V_COMMON;
                break;;
            }

            cop0.status.exl = true;
        }

        pc = exception_addr[cop0.status.bev] + vector;

        fetch_next();
    }
};