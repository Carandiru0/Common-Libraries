/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

/*
    Copyright (c) 2005-2019 Intel Corporation

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef __TBB_scalable_aligned_allocator_H
#define __TBB_scalable_aligned_allocator_H
/** @file */

#include <stddef.h> /* Need ptrdiff_t and size_t from here. */
#if !_MSC_VER
#include <stdint.h> /* Need intptr_t from here. */
#endif

#include <tbb\tbb.h>
#include <new>      /* To use new with the placement argument */
#include <utility> // std::forward

namespace tbb {

//! Meets "allocator" requirements of ISO C++ Standard, Section 20.1.5
/** The members are ordered the same way they are in section 20.4.1
    of the ISO C++ standard.
    @ingroup memory_allocation */
template<typename T, size_t const alignment = alignof(T)>
class scalable_aligned_allocator {
public:
    typedef typename internal::allocator_type<T>::value_type value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    template<class U> struct rebind {
        typedef scalable_aligned_allocator<U, alignment> other;
    };

    scalable_aligned_allocator() throw() {}
    scalable_aligned_allocator( const scalable_aligned_allocator& ) throw() {}
    template<typename U> scalable_aligned_allocator(const scalable_aligned_allocator<U>&) throw() {}

    pointer address(reference x) const {return &x;}
    const_pointer address(const_reference x) const {return &x;}

    //! Allocate space for n objects.
    pointer allocate( size_type n, const void* /*hint*/ =0 ) {
        pointer p = static_cast<pointer>( scalable_aligned_malloc( n * sizeof(value_type), alignment ) );
        if (!p)
            internal::throw_exception(std::bad_alloc());
        return p;
    }

    //! Free previously allocated block of memory
    void deallocate( pointer p, size_type ) {
        scalable_aligned_free( p );
    }

    //! Largest value for which method allocate might succeed.
    size_type max_size() const throw() {
        size_type absolutemax = static_cast<size_type>(-1) / sizeof (value_type);
        return (absolutemax > 0 ? absolutemax : 1);
    }

    template<typename U, typename... Args>
    void construct(U *p, Args&&... args)
        { ::new((void *)p) U(std::forward<Args>(args)...); }

    void destroy( pointer p ) {p->~value_type();}
};

//! Analogous to std::allocator<void>, as defined in ISO C++ Standard, Section 20.4.1
/** @ingroup memory_allocation */
template<size_t const alignment>
class scalable_aligned_allocator<void, alignment> {
public:
    typedef void* pointer;
    typedef const void* const_pointer;
    typedef void value_type;
    template<class U> struct rebind {
        typedef scalable_aligned_allocator<U, alignment> other;
    };
};

template<typename T, typename U, size_t const alignment>
inline bool operator==( const scalable_aligned_allocator<T, alignment>&, const scalable_aligned_allocator<U, alignment>& ) {return true;}

template<typename T, typename U, size_t const alignment>
inline bool operator!=( const scalable_aligned_allocator<T, alignment>&, const scalable_aligned_allocator<U, alignment>& ) {return false;}

} // namespace tbb

#endif /* __TBB_scalable_aligned_allocator_H */


