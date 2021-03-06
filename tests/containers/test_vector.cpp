/***************************************************************************
 *  tests/containers/test_vector.cpp
 *
 *  Part of the STXXL. See http://stxxl.org
 *
 *  Copyright (C) 2002, 2003, 2006 Roman Dementiev <dementiev@mpi-sb.mpg.de>
 *  Copyright (C) 2010 Johannes Singler <singler@kit.edu>
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 **************************************************************************/

#define STXXL_DEFAULT_BLOCK_SIZE(T) 4096

//! \example containers/test_vector.cpp
//! This is an example of use of \c stxxl::vector. Vector type is configured
//! to store 64-bit integers and have 2 pages each of 1 block

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <random>

#include <tlx/die.hpp>
#include <tlx/logger.hpp>

#include <stxxl/scan>
#include <stxxl/vector>

using key_type = uint64_t;

struct element  // 24 bytes, not a power of 2 intentionally
{
    key_type key;
    uint64_t load0;
    uint64_t load1;

    element& operator = (uint64_t i)
    {
        key = i;
        load0 = i + 42;
        load1 = i ^ 42;
        return *this;
    }

    bool operator == (const element& e2) const
    {
        return key == e2.key && load0 == e2.load0 && load1 == e2.load1;
    }
};

template <class my_vec_type>
void test_const_iterator(const my_vec_type& x)
{
    typename my_vec_type::const_iterator i = x.begin();
    i = x.end() - 1;
    i.block_externally_updated();
    i.flush();
    i++;
    ++i;
    --i;
    i--;
    *i;
}

void test_vector1()
{
    constexpr size_t magic1 = 0xdeafbeaf;
    constexpr size_t magic2 = 0xbadc0ffee;

    // use non-randomized striping to avoid side effects on random generator
    using vector_type = stxxl::vector<element, 2, stxxl::lru_pager<2>, STXXL_DEFAULT_BLOCK_SIZE(
                                          element), foxxll::striping>;
    vector_type v(32 * STXXL_DEFAULT_BLOCK_SIZE(element) / sizeof(element));

    // test assignment const_iterator = iterator
    vector_type::const_iterator c_it = v.begin();
    tlx::unused(c_it);

    constexpr size_t big_size = 2 * 32 * STXXL_DEFAULT_BLOCK_SIZE(double);
    using vec_big = stxxl::vector<double>;
    vec_big my_vec(big_size);

    vec_big::iterator big_it = my_vec.begin();
    big_it += 6;

    test_const_iterator(v);

    std::mt19937_64 randgen;
    std::uniform_int_distribution<key_type> distr;

    const auto offset = distr(randgen);

    LOG1 << "write " << v.size() << " elements";
    {
        randgen.seed(magic1);

        // fill the vector with increasing sequence of integer numbers
        for (size_t i = 0; i < v.size(); ++i) {
            v[i].key = i + offset;
            die_unless(v[i].key == uint64_t(i + offset));
        }

        // fill the vector with random numbers
        stxxl::generate(v.begin(), v.end(), std::bind(distr, std::ref(randgen)), 4);
        v.flush();

        LOG1 << "seq read of " << v.size() << " elements";

        // testing swap
        vector_type a;
        std::swap(v, a);
        std::swap(v, a);

        randgen.seed(magic1);
        for (size_t i = 0; i < v.size(); i++)
            die_unless(v[i].key == distr(randgen));
    }

    LOG1 << "clear";
    v.clear();

    // check again
    {
        randgen.seed(magic2);

        v.resize(32 * STXXL_DEFAULT_BLOCK_SIZE(element) / sizeof(element));

        LOG1 << "write " << v.size() << " elements";
        stxxl::generate(v.begin(), v.end(), [&]() { return distr(randgen); }, 4);

        randgen.seed(magic2);

        LOG1 << "seq read of " << v.size() << " elements";

        for (size_t i = 0; i < v.size(); i++)
            die_unless(v[i].key == distr(randgen));
    }

    LOG1 << "copy vector of " << v.size() << " elements";

    vector_type v_copy0(v);
    die_unless(v == v_copy0);

    vector_type v_copy1;
    v_copy1 = v;
    die_unless(v == v_copy1);
}

//! check vector::resize(n,true)
void test_resize_shrink()
{
    using vector_type = stxxl::vector<int, 2, stxxl::lru_pager<4>, 4096>;
    vector_type vector;

    int n = 1 << 16;
    vector.resize(n);

    for (int i = 0; i < n; i += 100)
        vector[i] = i;

    vector.resize(1, true);
    vector.flush();
}

int main()
{
    test_vector1();
    test_resize_shrink();

    return 0;
}

static_assert(std::is_same<stxxl::vector<double>, stxxl::vector<double>::iterator::vector_type>::value, "Vector iterator has inconsistent vector type");
static_assert(std::is_same<stxxl::vector<double>, stxxl::vector<double>::const_iterator::vector_type>::value, "Vector const iterator has inconsistent vector type");

// forced instantiation
template class stxxl::vector<element, 2, stxxl::lru_pager<2>, (1024* 1024), foxxll::striping>;
template class stxxl::vector<double>;
template class stxxl::vector_iterator<stxxl::vector<double>::configuration_type>;
template class stxxl::const_vector_iterator<stxxl::vector<double>::configuration_type>;

//-tb bufreader instantiation work only for const_iterator!
using const_vector_iterator = stxxl::vector<double>::const_iterator;
template class stxxl::vector_bufreader<const_vector_iterator>;
template class stxxl::vector_bufreader_reverse<const_vector_iterator>;
template class stxxl::vector_bufreader_iterator<stxxl::vector_bufreader<const_vector_iterator> >;

using vector_iterator = stxxl::vector<double>::iterator;
template class stxxl::vector_bufwriter<vector_iterator>;
