/***************************************************************************
 *  tests/containers/test_vector_move.cpp
 *
 *  Part of the STXXL. See http://stxxl.sourceforge.net
 *
 *  Copyright (C) 2017 Michael Hamann <michael.hamann@kit.edu>
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 **************************************************************************/

#define STXXL_DEFAULT_BLOCK_SIZE(T) 4096

#include <stxxl/vector>

typedef stxxl::VECTOR_GENERATOR<int, 4, 4>::result vector_type;

int main()
{
    vector_type vector;

    for (int i = 0; i < 1<<20; ++i) {
        vector.push_back(i);
    }

    static_assert(std::is_nothrow_move_constructible<vector_type>::value, "Vector cannot be move constructed!");
    static_assert(std::is_nothrow_move_assignable<vector_type>::value, "Vector cannot be move assigned!");

    vector_type moved_vector = std::move(vector);

    for (int i = 0; i < 1<<20; ++i) {
        STXXL_CHECK_EQUAL(i, moved_vector[static_cast<size_t>(i)]);
    }

    STXXL_CHECK(vector.empty());

    vector_type target_vector;

    target_vector.push_back(0);
    target_vector.emplace_back(12);
    target_vector.emplace_back(42);

    STXXL_CHECK(target_vector.size() == 3);

    STXXL_CHECK_EQUAL(target_vector[2], 42);
    STXXL_CHECK_EQUAL(target_vector[1], 12);
    STXXL_CHECK_EQUAL(target_vector[0], 0);

    target_vector = std::move(moved_vector);

    for (int i = 0; i < 1<<20; ++i) {
        STXXL_CHECK_EQUAL(i, target_vector[static_cast<size_t>(i)]);
    }

    STXXL_CHECK(moved_vector.empty());

    return 0;
}
