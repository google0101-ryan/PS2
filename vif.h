#pragma once

#include <int128.h>
#include <gs/queue.h>
#include <array>

class Bus;

union VIFSTAT
{
    uint32_t value = 0;
    struct
    {
        uint32_t vif_status : 2;
        uint32_t e_wait : 1;
        uint32_t waiting_for_gif : 1;
        uint32_t : 2;
        uint32_t mark : 1;
        uint32_t double_buffer_flag : 1;
        uint32_t stalled_after_stop : 1;
        uint32_t stalled_on_intr : 1;
        uint32_t intr_detected : 1;
        uint32_t dmatag_mismatch : 1;
        uint32_t invalid_cmd : 1;
        uint32_t : 9;
        uint32_t fifo_detection : 1;
        uint32_t fifo_count : 5;
    };
};

union VIFFBRST
{
    uint8_t value = 0;
    struct
    {
        uint8_t reset : 1;
        uint8_t force_break : 1;
        uint8_t stop_vif : 1;
        uint8_t stall_cancel : 1;
    };
};

union VIFCYCLE
{
    uint16_t value;
    struct
    {
        uint16_t cycle_length : 8;
        uint16_t write_cycle_length;
    };
};

union VIFCommand
{
    uint32_t value;
    struct
    {
        union
        {
            uint16_t immediate;
            struct
            {
                uint16_t address : 10;
                uint16_t : 4;
                uint16_t sign_extend : 1;
                uint16_t flg : 1;
            };
        };
        uint8_t num;
        union
        {
            uint8_t command;
            struct
            {
                uint8_t vl : 2;
                uint8_t vn : 2;
                uint8_t mask : 1;
            };
        };
    };
};

enum VIFCommands
{
    VNOP = 0x0,
    STCYCL = 0x1,
    OFFSET = 0x2,
    BASE = 0x3,
    ITOP = 0x4,
    STMOD = 0x5,
    MSKPATH3 = 0x6,
    MARK = 0x7,
    FLUSHE = 0x10,
    FLUSH = 0x11,
    FLUSHA  = 0x13,
    MSCAL = 0x14,
    MSCALF = 0x15,
    MSCNT = 0x17,
    STMASK = 0x20,
    STROW = 0x30,
    STCOL = 0x31,
    MPG = 0x4A,
    DIRECT = 0x50,
    DIRECTHL = 0x51,
    UNPACKSTART = 0x60,
    UNPACKEND = 0x7F
};

enum VIFUFormat
{
	S_32 = 0,
	S_16 = 1,
	S_8 = 2,
	V2_32 = 4,
	V2_16 = 5,
	V2_8 = 6,
	V3_32 = 8,
	V3_16 = 9,
	V3_8 = 10,
	V4_32 = 12,
	V4_16 = 13,
	V4_8 = 14,
	V4_5 = 15
};

enum WriteMode : bool
{
    Skipping = 0,
    Filling = 1
};

class VIF
{
public:
    VIF(Bus* parent, int n);
    ~VIF() = default;

    void tick(uint32_t cycles);
    void reset();

    template<typename T>
    bool write_fifo(uint32_t, T data);

    uint32_t read(uint32_t address);
    void write(uint32_t address, uint32_t data);
private:
    void process_command();
    void process_unpack();

    void execute_command();
    void unpack_packet();
private:
    Bus* bus;
    int id;

    VIFSTAT status = {};
    VIFFBRST fbrst = {};
    uint32_t err = 0, mark = 0;
    VIFCYCLE cycle;
    uint32_t mode = 0, num = 0;
    uint32_t mask = 0, code = 0, itops = 0;

    // VIF1 only
    uint32_t base = 0, ofst = 0;
    uint32_t tops = 0, itop = 0, top = 0;
    std::array<uint32_t, 4> rn = {}, cn = {};
    // End VIF1 only

    util::Queue<uint32_t, 64> fifo;
    WriteMode write_mode = WriteMode::Skipping;

    VIFCommand command = {};
    uint32_t subpacket_count = 0, address = 0;
    uint32_t qwords_written = 0, word_cycles = 0;
};

template<typename T>
inline bool VIF::write_fifo(uint32_t, T data)
{
    return fifo.push<T>(data);
}