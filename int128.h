#pragma once

#include <cstdint>

union Register
{
    struct
    {
        uint64_t lo;
        uint64_t hi;
    } i;

    uint64_t ud[2];
    uint32_t uw[2];

    Register inline operator |(Register val)
    {
        Register data;
        data.i.lo = i.lo | val.i.lo;
        data.i.hi = i.hi | val.i.hi;

        return data;
    }

    Register& operator=(const int value)
    {
        uw[0] = value;
        return *this;
    }
};

using uint128_t = unsigned __int128;