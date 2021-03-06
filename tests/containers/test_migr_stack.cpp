/***************************************************************************
 *  tests/containers/test_migr_stack.cpp
 *
 *  Part of the STXXL. See http://stxxl.org
 *
 *  Copyright (C) 2003 Roman Dementiev <dementiev@mpi-sb.mpg.de>
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 **************************************************************************/

//! \example containers/test_migr_stack.cpp
//! This is an example of how to use \c stxxl::STACK_GENERATOR class
//! to generate an \b migrating stack with critical size \c critical_size ,
//! external implementation \c normal_stack , \b four blocks per page,
//! block size \b 4096 bytes, and internal implementation
//! \c std::stack<int>

#include <tlx/die.hpp>
#include <tlx/logger.hpp>

#include <stxxl/stack>

// forced instantiation
const unsigned critical_size = 8 * 4096;
template class stxxl::STACK_GENERATOR<size_t, stxxl::migrating, stxxl::normal, 4, 4096, std::stack<size_t>, critical_size>;

int main()
{
    using migrating_stack_type = stxxl::STACK_GENERATOR<size_t, stxxl::migrating, stxxl::normal, 4, 4096, std::stack<size_t>, critical_size>::result;

    LOG1 << "Starting test.";

    migrating_stack_type my_stack;
    size_t test_size = 1 * 1024 * 1024 / sizeof(int);

    LOG1 << "Filling stack.";

    for (size_t i = 0; i < test_size; i++)
    {
        my_stack.push(i);
        die_unless(my_stack.top() == i);
        die_unless(my_stack.size() == i + 1);
        die_unless((my_stack.size() >= critical_size) == my_stack.external());
    }

    LOG1 << "Testing swap.";
    // test swap
    migrating_stack_type my_stack2;
    std::swap(my_stack2, my_stack);
    std::swap(my_stack2, my_stack);

    LOG1 << "Removing elements from " <<
    (my_stack.external() ? "external" : "internal") << " stack";
    for (size_t i = test_size; i > 0; )
    {
        --i;
        die_unless(my_stack.top() == i);
        die_unless(my_stack.size() == i + 1);
        my_stack.pop();
        die_unless(my_stack.size() == i);
        die_unless(my_stack.external() == (test_size >= int(critical_size)));
    }

    LOG1 << "Test passed.";

    return 0;
}
