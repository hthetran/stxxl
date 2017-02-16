/***************************************************************************
 *  tests/containers/btree/test_btree_insert_scan.cpp
 *
 *  Part of the STXXL. See http://stxxl.sourceforge.net
 *
 *  Copyright (C) 2006 Roman Dementiev <dementiev@ira.uka.de>
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 **************************************************************************/

#include <iostream>
#include <ctime>

#include <stxxl/bits/containers/btree/btree.h>
#include <stxxl/scan>
#include <stxxl/sort>

struct comp_type : public std::less<int>
{
    static int max_value()
    {
        return std::numeric_limits<int>::max();
    }
    static int min_value()
    {
        return std::numeric_limits<int>::min();
    }
};

typedef stxxl::btree::btree<
        int, double, comp_type, 4096, 4096, stxxl::simple_random> btree_type;
//typedef stxxl::btree::btree<int,double,comp_type,10,11,stxxl::simple_random> btree_type;

std::ostream& operator << (std::ostream& o, const std::pair<int, double>& obj)
{
    o << obj.first << " " << obj.second;
    return o;
}

struct rnd_gen
{
    stxxl::random_number32 rnd;
    int operator () ()
    {
        return (rnd() >> 2);
    }
};

bool operator == (const std::pair<int, double>& a, const std::pair<int, double>& b)
{
    return a.first == b.first;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        STXXL_MSG("Usage: " << argv[0] << " #log_ins");
        return -1;
    }

    const int log_nins = atoi(argv[1]);
    if (log_nins > 31) {
        STXXL_ERRMSG("This test can't do more than 2^31 operations, you requested 2^" << log_nins);
        return -1;
    }

    btree_type BTree(1024 * 128, 1024 * 128);

    const size_t nins = 1ULL << log_nins;

    stxxl::ran32State = (unsigned int)time(NULL);

    stxxl::vector<int> Values(nins);
    STXXL_MSG("Generating " << nins << " random values");
    stxxl::generate(Values.begin(), Values.end(), rnd_gen(), 4);

    stxxl::vector<int>::const_iterator it = Values.begin();
    STXXL_MSG("Inserting " << nins << " random values into btree");
    for ( ; it != Values.end(); ++it)
        BTree.insert(std::pair<int, double>(*it, double(*it) + 1.0));

    STXXL_MSG("Sorting the random values");
    stxxl::sort(Values.begin(), Values.end(), comp_type(), 128 * 1024 * 1024);

    STXXL_MSG("Deleting unique values");
    stxxl::vector<int>::iterator NewEnd = std::unique(Values.begin(), Values.end());
    Values.resize(NewEnd - Values.begin());

    STXXL_CHECK(BTree.size() == Values.size());
    STXXL_MSG("Size without duplicates: " << Values.size());

    STXXL_MSG("Comparing content");

    stxxl::vector<int>::const_iterator vIt = Values.begin();
    btree_type::iterator bIt = BTree.begin();

    for ( ; vIt != Values.end(); ++vIt, ++bIt)
    {
        STXXL_CHECK(*vIt == bIt->first);
        STXXL_CHECK(double(bIt->first) + 1.0 == bIt->second);
        STXXL_CHECK(bIt != BTree.end());
    }

    STXXL_CHECK(bIt == BTree.end());

    STXXL_MSG("Test passed.");

    return 0;
}
