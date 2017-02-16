/***************************************************************************
 *  tests/containers/test_pqueue_equal_key.cpp
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

//! \example containers/test_pqueue_equal_key.cpp
//! This is an example of how *not* to use \c stxxl::PRIORITY_QUEUE_GENERATOR
//! and \c stxxl::priority_queue

#include <stxxl/priority_queue>

#include <limits>

struct myElement
{
    int64_t sortKey;
    uint64_t id;
};

inline std::ostream& operator << (std::ostream& os, myElement const& m)
{
    return os << "{sortKey:" << m.sortKey << ", id:" << m.id << "}";
}

struct MyCompareLess : public std::binary_function<myElement, myElement, bool>
{
    bool operator()(const myElement& a, const myElement& b) const
    {
        return a.sortKey < b.sortKey;
    }

    myElement min_value() const
    {
        return {std::numeric_limits<int64_t>::min(),
                std::numeric_limits<uint64_t>::min()};
    }
};

int main(int argc, char ** argv)
{
    uint64_t numElements = argc > 1 ? atoi(argv[1]) : 270593;

    typedef stxxl::PRIORITY_QUEUE_GENERATOR<myElement, MyCompareLess, 8*1024*1024, 1024*1024>::result pqueue_type;
    typedef pqueue_type::block_type block_type;
    stxxl::read_write_pool<block_type> pool(
        1024 * 1024 / block_type::raw_size, 1024 * 1024 / block_type::raw_size);
    pqueue_type prioQueue(pool);

    // generate elements which have sometimes equal sorting keys but never equal ids.
    for (uint64_t i = 0; i < numElements; ++i) {
        prioQueue.push(myElement{static_cast<int64_t>(i % 2), i});
    }

    std::cout << "inserted " << prioQueue.size() << " elements with 2 keys into PQ" << std::endl;

    std::vector<bool> idCheck(numElements, false);

    // check that no two elements that are removed from the queue have the same id.
    while (!prioQueue.empty()) {
        //STXXL_CHECK(!idCheck[prioQueue.top().id]); // this fails for some elements!
	if (idCheck[prioQueue.top().id])
		std::cout << "dupe: " << prioQueue.top() << std::endl;
        idCheck[prioQueue.top().id] = true;
        prioQueue.pop();
    }

    return 0;
}
