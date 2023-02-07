// this mutex is way too big, > 512 bytes per mutex is nuts
#pragma once

#include "..\Random\superrandom.hpp"
#include <tbb/scalable_allocator.h>

#include <iostream>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_constructive_interference_size;
using std::hardware_destructive_interference_size;
#else
 // 64 bytes on x86-64
constexpr std::size_t hardware_constructive_interference_size = 64;
constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

// contention free shared mutex (same-lock-type is recursive for X->X, X->S or S->S locks), but (S->X - is UB)
// see: https://www.codeproject.com/Articles/1183423/We-Make-a-std-shared-mutex-10-Times-Faster

template<unsigned int contention_free_count = 36, bool const shared_flag = false>
class contention_free_shared_mutex {
    
    alignas(std::hardware_destructive_interference_size) std::atomic<bool> want_x_lock;
    
    struct cont_free_flag_t { alignas(std::hardware_destructive_interference_size) std::atomic<int> value; cont_free_flag_t() { value = 0; } }; // C++17
    typedef std::array<cont_free_flag_t, contention_free_count> array_slock_t;

    alignas(std::hardware_destructive_interference_size) std::shared_ptr<array_slock_t> const shared_locks_array_ptr;  // 0 - unregistred, 1 registred & free, 2... - busy
    alignas(std::hardware_destructive_interference_size) array_slock_t& shared_locks_array;

    int recursive_xlock_count;


    enum index_op_t { unregister_thread_op, get_index_op, register_thread_op };

    typedef std::thread::id thread_id_t;
    std::atomic<std::thread::id> owner_thread_id;
    std::thread::id const get_fast_this_thread_id() const { return(std::this_thread::get_id()); }

    struct unregister_t {
        int thread_index;
        std::shared_ptr<array_slock_t> array_slock_ptr;
        unregister_t(int const index, std::shared_ptr<array_slock_t> const& ptr) : thread_index(index), array_slock_ptr(ptr) {}
        unregister_t(unregister_t&& src) : thread_index(src.thread_index), array_slock_ptr(std::move(src.array_slock_ptr)) {}
        ~unregister_t() { if (array_slock_ptr.use_count() > 0) (*array_slock_ptr)[thread_index].value--; }
    };

    int get_or_set_index(index_op_t index_op = get_index_op, int set_index = -1) {
        thread_local static std::unordered_map<void*, unregister_t> thread_local_index_hashmap;
        // get thread index - in any cases
        auto it = thread_local_index_hashmap.find(this);
        if (it != thread_local_index_hashmap.cend())
            set_index = it->second.thread_index;

        if (index_op == unregister_thread_op) {  // unregister thread
            if (shared_locks_array[set_index].value == 1) // if isn't shared_lock now
                thread_local_index_hashmap.erase(this);
            else
                return -1;
        }
        else if (index_op == register_thread_op) {  // register thread
            thread_local_index_hashmap.emplace(this, unregister_t(set_index, shared_locks_array_ptr));

            // remove info about deleted contfree-mutexes
            for (auto it = thread_local_index_hashmap.begin(), ite = thread_local_index_hashmap.end(); it != ite;) {
                if (it->second.array_slock_ptr->at(it->second.thread_index).value < 0)    // if contfree-mtx was deleted
                    it = thread_local_index_hashmap.erase(it);
                else
                    ++it;
            }
        }
        return set_index;
    }

public:
    contention_free_shared_mutex() :
        shared_locks_array_ptr(std::make_shared<array_slock_t>()), shared_locks_array(*shared_locks_array_ptr), want_x_lock(false), recursive_xlock_count(0),
        owner_thread_id(thread_id_t()) {}

    ~contention_free_shared_mutex() {
        for (auto& i : shared_locks_array) i.value = -1;
    }


    bool unregister_thread() { return get_or_set_index(unregister_thread_op) >= 0; }

    int register_thread() {
        int cur_index = get_or_set_index();

        if (cur_index == -1) {
            if (shared_locks_array_ptr.use_count() <= (int)shared_locks_array.size())  // try once to register thread
            {
                for (size_t i = 0; i < shared_locks_array.size(); ++i) {
                    int unregistred_value = 0;
                    if (shared_locks_array[i].value == 0)
                        if (shared_locks_array[i].value.compare_exchange_strong(unregistred_value, 1)) {
                            cur_index = i;
                            get_or_set_index(register_thread_op, cur_index);   // thread registred success
                            break;
                        }
                }
            }
        }
        return(cur_index);
    }

    void lock_shared() {
        int const register_index = register_thread();

        if (register_index >= 0) {
            int const recursion_depth = shared_locks_array[register_index].value.load(std::memory_order_acquire);

            if (recursion_depth > 1)
                shared_locks_array[register_index].value.store(recursion_depth + 1, std::memory_order_release); // if recursive -> release
            else {
                shared_locks_array[register_index].value.store(recursion_depth + 1, std::memory_order_seq_cst); // if first -> sequential
                while (want_x_lock.load(std::memory_order_seq_cst)) {
                    shared_locks_array[register_index].value.store(recursion_depth, std::memory_order_seq_cst);
                    for (volatile size_t i = 0; want_x_lock.load(std::memory_order_seq_cst); ++i)
                        if (i % 100000 == 0) std::this_thread::yield();
                    shared_locks_array[register_index].value.store(recursion_depth + 1, std::memory_order_seq_cst);
                }
            }
            // (shared_locks_array[register_index] == 2 && want_x_lock == false) ||     // first shared lock
            // (shared_locks_array[register_index] > 2)                                 // recursive shared lock
        }
        else {
            if (owner_thread_id.load(std::memory_order_acquire) != get_fast_this_thread_id()) {
                size_t i = 0;
                for (bool flag = false; !want_x_lock.compare_exchange_weak(flag, true, std::memory_order_seq_cst); flag = false)
                    if (++i % 100000 == 0) std::this_thread::yield();
                owner_thread_id.store(get_fast_this_thread_id(), std::memory_order_release);
            }
            ++recursive_xlock_count;
        }
    }

    void unlock_shared() {
        int const register_index = get_or_set_index();

        if (register_index >= 0) {
            int const recursion_depth = shared_locks_array[register_index].value.load(std::memory_order_acquire);

            shared_locks_array[register_index].value.store(recursion_depth - 1, std::memory_order_release);
        }
        else {
            if (--recursive_xlock_count == 0) {
                owner_thread_id.store(decltype(owner_thread_id)(), std::memory_order_release);
                want_x_lock.store(false, std::memory_order_release);
            }
        }
    }

    void lock() {
        // forbidden upgrade S-lock to X-lock - this is an excellent opportunity to get deadlock
        int const register_index = get_or_set_index();
        if (register_index >= 0) {
          auto const r = shared_locks_array[register_index].value.load(std::memory_order_acquire);
        }

        if (owner_thread_id.load(std::memory_order_acquire) != get_fast_this_thread_id()) {
            size_t i = 0;
            for (bool flag = false; !want_x_lock.compare_exchange_weak(flag, true, std::memory_order_seq_cst); flag = false)
                if (++i % 1000000 == 0) std::this_thread::yield();

            owner_thread_id.store(get_fast_this_thread_id(), std::memory_order_release);

            for (auto& i : shared_locks_array)
                while (i.value.load(std::memory_order_seq_cst) > 1);
        }

        ++recursive_xlock_count;
    }

    void unlock() {

        if (--recursive_xlock_count == 0) {
            owner_thread_id.store(decltype(owner_thread_id)(), std::memory_order_release);
            want_x_lock.store(false, std::memory_order_release);
        }
    }
};

template<typename mutex_t, bool const bExclusive = false> // default is reader mode, set true for writer mode "start"
struct scoped_lock {
    mutex_t& ref_mtx;

    void lock() { // upgrade-to-writer
        ref_mtx.lock();
    }
    void unlock() { // downgrade-to-reader
        ref_mtx.unlock();
    }

    scoped_lock(mutex_t& mtx) : ref_mtx(mtx) { 
        
        if constexpr (bExclusive) {
            ref_mtx.lock();
        }
        else {
            ref_mtx.lock_shared();
        }
    }
    ~scoped_lock() { 
        ref_mtx.unlock_shared(); // use unlock_shared() when current state is unknown (exclusive or not) and its time to unlock. unlock() should only be used when the last state is known to be exclusive.  :(  its backwards and unlock() should be the one to use in this case.
    }
};

using default_contention_free_shared_mutex = contention_free_shared_mutex<>;


