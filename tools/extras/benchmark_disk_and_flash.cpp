/***************************************************************************
 *  tools/extras/benchmark_disk_and_flash.cpp
 *
 *  Part of the STXXL. See http://stxxl.org
 *
 *  Copyright (C) 2008 Andreas Beckmann <beckmann@cs.uni-frankfurt.de>
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 **************************************************************************/

#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include <tlx/logger.hpp>

#include <foxxll/io.hpp>

#include <stxxl/seed>

using foxxll::request_ptr;
using foxxll::file;
using foxxll::timestamp;
using foxxll::external_size_type;

#ifdef BLOCK_ALIGN
 #undef BLOCK_ALIGN
#endif

#define BLOCK_ALIGN  4096

#define KB (1024)
#define MB (1024 * 1024)
#define GB (1024 * 1024 * 1024)

void run(char* buffer, file** disks, external_size_type offset, external_size_type length,
         unsigned hdd_blocks, unsigned hdd_bytes, unsigned ssd_blocks, unsigned ssd_bytes, unsigned repeats)
{
    unsigned i, j;
    double begin = timestamp(), end, elapsed;
    request_ptr* reqs = new request_ptr[std::max(hdd_blocks + ssd_blocks, 1U)];

    struct diskinfo {
        unsigned id;
        unsigned bytes;
        unsigned n;
    };

    diskinfo info[2];

    // HDD
    info[0].id = 0;
    info[0].bytes = hdd_bytes;
    info[0].n = hdd_blocks;

    // SSD
    info[1].id = 1;
    info[1].bytes = ssd_bytes;
    info[1].n = ssd_blocks;

    begin = timestamp();
    double volume = 0;

    std::mt19937 randgen(stxxl::seed_sequence::get_ref().get_next_seed());
    std::uniform_int_distribution<external_size_type> distr_pos(0, 0xffff);

    for (unsigned repeat = 0; repeat < repeats; ++repeat) {
        size_t r = 0;
        char* buf = buffer;
        for (i = 0; i < 2; i++)
        {
            for (j = 0; j < info[i].n; j++) {
                unsigned bytes = info[i].bytes;
                external_size_type position = (bytes * distr_pos(randgen)) % length;
                reqs[r++] = disks[info[i].id]->aread(buf, offset + position, bytes);
                buf += bytes;
                volume += static_cast<double>(bytes);
            }
        }

        wait_all(reqs, r);
    }

    end = timestamp();
    elapsed = end - begin;

    std::cout << "B_d = " << info[0].bytes << "  B_f = " << info[1].bytes << "  n_d = " << info[0].n << "  n_f = " << info[1].n; //<< std::endl;
    std::cout << " Transferred " << (volume / MB) << " MiB in " << elapsed << " seconds @ " << (volume / MB / elapsed) << " MiB/s" << std::endl;
    delete[] reqs;
}

void usage(const char* argv0)
{
    std::cout << "Usage: " << argv0 << " offset length diskfile flashfile" << std::endl;
    std::cout << "    starting 'offset' and 'length' are given in GiB" << std::endl;
    std::cout << "    length == 0 implies till end of space (please ignore the write error)" << std::endl;
    exit(-1);
}

int main(int argc, char* argv[])
{
    if (argc < 4)
        usage(argv[0]);

    external_size_type offset = external_size_type(GB) * external_size_type(atoi(argv[1]));
    external_size_type length = external_size_type(GB) * external_size_type(atoi(argv[2]));

    int first_disk_arg = 3;

    std::vector<std::string> disks_arr;

    if (!(first_disk_arg < argc))
        usage(argv[0]);

    for (int ii = first_disk_arg; ii < argc; ii++)
    {
        std::cout << "# Add disk: " << argv[ii] << std::endl;
        disks_arr.push_back(argv[ii]);
    }

    const size_t ndisks = disks_arr.size();
    size_t buffer_size = 1024 * MB;
    const size_t buffer_size_int = buffer_size / sizeof(int);

    size_t i;

    file** disks = new file*[ndisks];
    unsigned* buffer = static_cast<unsigned*>(foxxll::aligned_alloc<BLOCK_ALIGN>(buffer_size));

    for (i = 0; i < buffer_size_int; i++)
        buffer[i] = unsigned(i);

    for (i = 0; i < ndisks; i++)
    {
        disks[i] = new foxxll::syscall_file(disks_arr[i],
                                            file::CREAT | file::RDWR | file::DIRECT, static_cast<int>(i));
    }

    try {
        run(reinterpret_cast<char*>(buffer), disks, offset, length, 1, 2 * MB, 23, 128 * 1024, 100);
        run(reinterpret_cast<char*>(buffer), disks, offset, length, 1, 2 * MB, 42, 128 * 1024, 100);
        for (unsigned hdd_bytes = 4 * KB; hdd_bytes < 256 * MB; hdd_bytes <<= 1) {
            for (unsigned ssd_bytes = 128 * KB; ssd_bytes == 128 * KB; ssd_bytes <<= 1) {
                for (unsigned hdd_blocks = 1; hdd_blocks == 1; ++hdd_blocks) {
                    for (unsigned ssd_blocks = 0; ssd_blocks <= (std::max(16U, 2 * hdd_bytes * hdd_blocks / ssd_bytes)); ++ssd_blocks) {
                        run(reinterpret_cast<char*>(buffer), disks, offset, length, hdd_blocks, hdd_bytes, ssd_blocks, ssd_bytes, 100);
                    }
                }
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cout << std::endl;
        LOG1 << ex.what();
    }

    for (i = 0; i < ndisks; i++)
        delete disks[i];

    delete[] disks;
    foxxll::aligned_dealloc<BLOCK_ALIGN>(buffer);

    return 0;
}

