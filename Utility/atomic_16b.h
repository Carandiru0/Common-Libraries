/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#pragma once 
#include <type_traits>
#include <atomic>
#include <stdint.h>
#ifdef _WIN64
#include <intrin.h>
#endif

// custom compatibility class to force usage of 16 byte atomic instruction "cmpxchg16b"
template <typename T, typename = std::void_t<>>
struct atomic_16b : std::atomic<T> {};

#ifdef _WIN64
template <typename T>
struct atomic_16b<T, std::enable_if_t<
    sizeof(void*) == 8 &&
    sizeof(T) == 16 &&
    std::alignment_of_v<T> == 16
    >>
{
    atomic_16b() noexcept = default;
    atomic_16b(atomic_16b const&) = delete;

    /* implicit */ constexpr atomic_16b(T _Value) noexcept
        : m_value{ reinterpret_cast<storage&>(_Value) } {} // non-atomically initialize this atomic

    void __vectorcall store(const T _Value) noexcept { // store with sequential consistency
        (void)exchange(_Value);
    }

    _NODISCARD T __vectorcall load() noexcept { // load with sequential consistency

        storage _Result{}; // atomic CAS 0 with 0
        (void)_InterlockedCompareExchange128(m_value.data, 0, 0, _Result.data);
        return(reinterpret_cast<T&>(_Result));
    }

    T __vectorcall exchange(const T _Value) noexcept { // exchange with sequential consistency
        T _Result{ _Value };
        while (!compare_exchange_strong(_Result, _Value)) { // keep trying
        }

        return(_Result);
    }

    bool const __vectorcall compare_exchange_weak(T& expected, T desired, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        return(compare_exchange_strong(expected, desired, order));
    }

    bool const __vectorcall compare_exchange_strong(T& expected, T desired, std::memory_order = std::memory_order_seq_cst) noexcept
    {
        auto desired_ = reinterpret_cast<storage*>(&desired);
        auto expected_ = reinterpret_cast<storage*>(&expected);
        return( !!_InterlockedCompareExchange128(
            m_value.data,
            desired_->data[1],
            desired_->data[0],
            expected_->data) );
    }

    static constexpr bool is_always_lock_free = true;

private:
    struct storage
    {
        alignas(64) int64_t data[2];  // high = 1, low = 0
    };
    storage m_value;
};
#endif


