/***************************************************************************
 *  examples/applications/skew3.cpp
 *
 *  Implementation of the external memory suffix sorting algorithm DC3 aka
 *  skew3 as described in Roman Dementiev, Juha Kaerkkaeinen, Jens Mehnert and
 *  Peter Sanders. "Better External Memory Suffix Array Construction". Journal
 *  of Experimental Algorithmics (JEA), volume 12, 2008.
 *
 *  Part of the STXXL. See http://stxxl.org
 *
 *  Copyright (C) 2004 Jens Mehnert <jmehnert@mpi-sb.mpg.de>
 *  Copyright (C) 2012-2013 Timo Bingmann <tb@panthema.net>
 *  Copyright (C) 2012-2013 Daniel Feist <daniel.feist@student.kit.edu>
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 **************************************************************************/

#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdlib>

#include <algorithm>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>
#include <tuple>
#include <stxxl/bits/common/comparator.h>

#include <foxxll/common/uint_types.hpp>
#include <foxxll/io.hpp>
#include <stxxl/algorithm>
#include <stxxl/bits/common/cmdline.h>
#include <stxxl/sorter>
#include <stxxl/stream>
#include <stxxl/vector>

using foxxll::external_size_type;
namespace stream = stxxl::stream;

// 1 GiB ram used by external data structures / 1 MiB block size
size_t ram_use = 1024 * 1024 * 1024;

// alphabet data type
using alphabet_type = unsigned char;

// calculation data type
using size_type = external_size_type;

/// Suffix Array checker for correctness verification

/**
 * Algorithm to check whether the suffix array is correct. Loosely based on the
 * ideas of Kaerkkaeinen und Burghardt, originally implemented in STXXL by Jens
 * Mehnert (2004), reimplemented using triples by Timo Bingmann (2012).
 *
 * \param InputT is the original text, from which the suffix array was build
 * \param InputSA is the suffix array from InputT
 *
 * Note: ISA := The inverse of SA
 */
template <typename InputT, typename InputSA>
bool sacheck(InputT& inputT, InputSA& inputSA)
{
    using offset_type = typename InputSA::value_type;
    using pair_type = std::tuple<offset_type, offset_type>;
    using triple_type = std::tuple<offset_type, offset_type, offset_type>;

    // *** Pipeline Declaration ***

    // Build tuples with index: (SA[i]) -> (i, SA[i])
    using index_counter_type = stxxl::stream::counter<offset_type>;
    index_counter_type index_counter;

    using tuple_index_sa_type = stream::make_tuplestream<index_counter_type, InputSA>;
    tuple_index_sa_type tuple_index_sa(index_counter, inputSA);

    // take (i, SA[i]) and sort to (ISA[i], i)
    using pair_less_type = stxxl::comparator<pair_type, stxxl::direction::DontCare, stxxl::direction::Less>;
    using build_isa_type = typename stream::sort<tuple_index_sa_type, pair_less_type>;

    build_isa_type build_isa(tuple_index_sa, pair_less_type(), ram_use / 3);

    // build (ISA[i], T[i], ISA[i+1]) and sort to (i, T[SA[i]], ISA[SA[i]+1])
    using triple_less_type = stxxl::comparator<triple_type, stxxl::direction::Less, stxxl::direction::DontCare, stxxl::direction::DontCare>;      // comparison relation

    typedef typename stream::use_push<triple_type> triple_push_type; // indicator use push()
    using triple_rc_type = typename stream::runs_creator<triple_push_type, triple_less_type>;
    using triple_rm_type = typename stream::runs_merger<typename triple_rc_type::sorted_runs_type, triple_less_type>;

    triple_rc_type triple_rc(triple_less_type(), ram_use / 3);

    // ************************* Process ******************************
    // loop 1: read ISA and check for a permutation. Simultaneously create runs
    // of triples by iterating ISA and T.

    size_type totalSize;
    {
        offset_type prev_isa = std::get<0>(*build_isa);
        offset_type counter = 0;
        while (!build_isa.empty())
        {
            if (std::get<1>(*build_isa) != counter) {
                std::cout << "Error: suffix array is not a permutation of 0..n-1." << std::endl;
                return false;
            }

            ++counter;
            ++build_isa; // ISA is one in front of T

            if (!build_isa.empty()) {
                triple_rc.push(triple_type(prev_isa, *inputT, std::get<0>(*build_isa)));
                prev_isa = std::get<0>(*build_isa);
            }
            ++inputT;
        }

        totalSize = counter;
    }

    if (totalSize == 1) return true;

    // ************************************************************************
    // loop 2: read triples (i,T[SA[i]],ISA[SA[i]+1]) and check for correct
    // ordering.

    triple_rm_type triple_rm(triple_rc.result(), triple_less_type(), ram_use / 3);

    {
        triple_type prev_triple = *triple_rm;
        size_type counter = 0;

        ++triple_rm;

        while (!triple_rm.empty())
        {
            const triple_type& this_triple = *triple_rm;

            if (std::get<1>(prev_triple) > std::get<1>(this_triple))
            {
                // simple check of first character of suffix
                std::cout << "Error: suffix array position " << counter << " ordered incorrectly." << std::endl;
                return false;
            }
            else if (std::get<1>(prev_triple) == std::get<1>(this_triple))
            {
                if (std::get<2>(this_triple) == static_cast<offset_type>(totalSize)) {
                    // last suffix of string must be first among those with same
                    // first character
                    std::cout << "Error: suffix array position " << counter << " ordered incorrectly." << std::endl;
                    return false;
                }
                if (std::get<2>(prev_triple) != static_cast<offset_type>(totalSize) && std::get<2>(prev_triple) > std::get<2>(this_triple)) {
                    // positions SA[i] and SA[i-1] has same first character but
                    // their suffixes are ordered incorrectly: the suffix
                    // position of SA[i] is given by ISA[SA[i]]
                    std::cout << "Error: suffix array position " << counter << " ordered incorrectly." << std::endl;
                    return false;
                }
            }

            prev_triple = this_triple;

            ++triple_rm;
            ++counter;
        }
    }
    return true;
}

template <typename InputT, typename InputSA>
bool sacheck_vectors(InputT& inputT, InputSA& inputSA)
{
    typename stream::streamify_traits<typename InputT::iterator>::stream_type streamT
        = stream::streamify(inputT.begin(), inputT.end());

    typename stream::streamify_traits<typename InputSA::iterator>::stream_type streamSA
        = stream::streamify(inputSA.begin(), inputSA.end());

    return sacheck(streamT, streamSA);
}

/// DC3 aka skew algorithm

/*
 * DC3 aka skew algorithm a short description. T := input string
 * The recursion works as follows:
 * Step 1: a) pick all mod1/mod2 triples (i.e. triples T[i,i+2] at position i mod 3 != 0) (-> extract_mod12 class)
 *         b) sort mod1/mod2 triples lexicographically (-> build_sa class)
 *         c) give mod1/mod2 triples lexicographical ascending names n (-> naming class)
 *         d) check lexicographical names for uniqueness (-> naming class)
 *            If yes: proceed to next Step, If no: set T := lexicographical names and run Step 1 again
 * Step 2: a) by sorting the lexicographical names n we receive ranks r
 *         b) construct mod0-quints, mod1-quads and mod2-quints  (-> build_sa class)
 *         c) prepare for merging by:
 *            sort mod0-quints by 2 components, sort mod1-quads / mod2-quints by one component (-> build_sa class)
 *         c) merge mod0-quints, mod1-quads and mod2-quints (-> merge_sa class)
 * Step 3: a) return Suffix Array of T
 *
 * \param offset_type later suffix array data type
 */
template <typename offset_type>
class skew
{
public:
    // 2-tuple, 3-tuple, 4-tuple (=quads), 5-tuple(=quints) definition
    using skew_pair_type = std::tuple<offset_type, offset_type>;
    using skew_triple_type = std::tuple<offset_type, offset_type, offset_type>;
    using skew_quad_type = std::tuple<offset_type, offset_type, offset_type, offset_type>;
    using skew_quint_type = std::tuple<offset_type, offset_type, offset_type, offset_type, offset_type>;

    using offset_array_type = typename stxxl::vector<offset_type, 1, stxxl::lru_pager<2> >;
    using offset_array_it_rg = stream::vector_iterator2stream<typename offset_array_type::iterator>;

    /** Comparison function for the mod0 tuples. */
    using less_mod0 = stxxl::comparator<skew_quint_type, stxxl::direction::DontCare, stxxl::direction::Less, stxxl::direction::DontCare, stxxl::direction::Less, stxxl::direction::DontCare>;

    using less_mod1 = stxxl::comparator<skew_quad_type, stxxl::direction::DontCare, stxxl::direction::Less, stxxl::direction::DontCare, stxxl::direction::DontCare>;
    using less_mod2 = stxxl::comparator<skew_quint_type, stxxl::direction::DontCare, stxxl::direction::Less, stxxl::direction::DontCare, stxxl::direction::DontCare, stxxl::direction::DontCare>;

    /** Check, if last two components of tree quads are equal. */
    template <class quad_type>
    static inline bool quad_eq(const quad_type& a, const quad_type& b)
    {
        return (std::get<1>(a) == std::get<1>(b)) && (std::get<2>(a) == std::get<2>(b)) && (std::get<3>(a) == std::get<3>(b));
    }

    /** Naming pipe for the conventional skew algorithm without discarding. */
    template <class Input>
    class naming
    {
    public:
        using quad_type = typename Input::value_type;

        using value_type = skew_pair_type;

    private:
        Input& A;

        bool& unique;
        offset_type lexname;
        quad_type prev;
        skew_pair_type result;

    public:
        naming(Input& A_, bool& unique_)
            : A(A_), unique(unique_), lexname(0)
        {
            assert(!A.empty());
            unique = true;

            prev = *A;
            std::get<0>(result) = std::get<0>(prev);
            std::get<1>(result) = lexname;
        }

        const value_type& operator * () const
        {
            return result;
        }

        naming& operator ++ ()
        {
            assert(!A.empty());

            ++A;
            if (A.empty())
                return *this;

            quad_type curr = *A;
            if (!quad_eq(prev, curr)) {
                ++lexname;
            }
            else {
                if (!A.empty() && std::get<1>(curr) != offset_type(0)) {
                    unique = false;
                }
            }

            std::get<0>(result) = std::get<0>(curr);
            std::get<1>(result) = lexname;

            prev = curr;
            return *this;
        }

        bool empty() const
        {
            return A.empty();
        }
    };

    /** Create tuples of 2 components until one of the input streams are empty. */
    template <class InputA, class InputB, const int add_alphabet = 0>
    class make_pairs
    {
    public:
        using value_type = std::tuple<typename InputA::value_type, offset_type>;

    private:
        InputA& A;
        InputB& B;
        value_type result;

    public:
        make_pairs(InputA& a, InputB& b)
            : A(a), B(b)
        {
            assert(!A.empty());
            assert(!B.empty());
            if (!empty()) {
                result = value_type(*A, *B + add_alphabet);
            }
        }

        const value_type& operator * () const
        { return result; }

        make_pairs& operator ++ ()
        {
            assert(!A.empty());
            assert(!B.empty());

            ++A;
            ++B;

            if (!A.empty() && !B.empty()) {
                result = value_type(*A, *B + add_alphabet);
            }

            return *this;
        }

        bool empty() const
        { return (A.empty() || B.empty()); }
    };

    /**
     * Collect three characters t_i, t_{i+1}, t_{i+2} beginning at the index
     * i. Since we need at least one unique endcaracter, we free the first
     * characters i.e. we map (t_i) -> (i,t_i,t_{i+1},t_{i+2})
     *
     * \param Input holds all characters t_i from input string t
     * \param alphabet_type
     * \param add_alphabet
     */
    template <class Input, typename alphabet_type, const int add_alphabet = 0>
    class make_quads
    {
    public:
        using value_type = std::tuple<offset_type, alphabet_type, alphabet_type, alphabet_type>;

    private:
        Input& A;
        value_type current;
        offset_type counter;
        unsigned int z3z;  // = counter mod 3, ("+",Z/3Z) is cheaper than %
        bool finished;

        offset_array_type& text;

    public:
        make_quads(Input& data_in_, offset_array_type& text_)
            : A(data_in_),
              current(0, 0, 0, 0),
              counter(0),
              z3z(0),
              finished(false),
              text(text_)
        {
            assert(!A.empty());

            std::get<0>(current) = counter;
            std::get<1>(current) = std::get<1>(*A) + add_alphabet;
            ++A;

            if (!A.empty()) {
                std::get<2>(current) = std::get<1>(*A) + add_alphabet;
                ++A;
            }
            else {
                std::get<2>(current) = 0;
                std::get<3>(current) = 0;
            }

            if (!A.empty()) {
                std::get<3>(current) = std::get<1>(*A) + add_alphabet;
            }
            else {
                std::get<3>(current) = 0;
            }
        }

        const value_type& operator * () const
        { return current; }

        make_quads& operator ++ ()
        {
            assert(!A.empty() || !finished);

            if (std::get<1>(current) != offset_type(0)) {
                text.push_back(std::get<1>(current));
            }

            // Calculate module
            if (++z3z == 3) z3z = 0;

            std::get<0>(current) = ++counter;
            std::get<1>(current) = std::get<2>(current);
            std::get<2>(current) = std::get<3>(current);

            if (!A.empty())
                ++A;

            if (!A.empty()) {
                std::get<3>(current) = std::get<1>(*A) + add_alphabet;
            }
            else {
                std::get<3>(current) = 0;
            }

            // Inserts a dummy tuple for input sizes of n%3==1
            if ((std::get<1>(current) == offset_type(0)) && (z3z != 1)) {
                finished = true;
            }

            return *this;
        }

        bool empty() const
        { return (A.empty() && finished); }
    };

    /** Drop 1/3 of the input. More exactly the offsets at positions (0 mod
     * 3). Index begins with 0. */
    template <class Input>
    class extract_mod12
    {
    public:
        using value_type = typename Input::value_type;

    private:
        Input& A;
        offset_type counter;
        offset_type output_counter;
        value_type result;

    public:
        explicit extract_mod12(Input& A_)
            : A(A_),
              counter(0),
              output_counter(0)
        {
            assert(!A.empty());
            ++A, ++counter;  // skip 0 = mod0 offset
            if (!A.empty()) {
                result = *A;
                std::get<0>(result) = output_counter;
            }
        }

        const value_type& operator * () const
        { return result; }

        extract_mod12& operator ++ ()
        {
            assert(!A.empty());

            ++A, ++counter, ++output_counter;

            if (!A.empty() && (counter % 3) == 0) {
                // skip mod0 offsets
                ++A, ++counter;
            }
            if (!A.empty()) {
                result = *A;
                std::get<0>(result) = output_counter;
            }

            return *this;
        }

        bool empty() const
        { return A.empty(); }
    };

    /** Create the suffix array from the current sub problem by simple
     *  comparison-based merging.  More precisely: compare characters(out of
     *  text t) and ranks(out of ISA12) of the following constellation:
     *  Input constellation:
     *  \param Mod0 5-tuple (quint): <i, t_i, t_{i+1}, ISA12[i+1], ISA12[i+2]>
     *  \param Mod1 4-tuple (quad): <i, ISA12[i], t_i, ISA12[i+1]>
     *  \param Mod2 5-tuple (quint): <i, ISA[i], t_i, t_{i+1}, ISA12[i+1]>
     */
    template <class Mod0, class Mod1, class Mod2>
    class merge_sa
    {
    public:
        using value_type = offset_type;

    private:
        Mod0& A;
        Mod1& B;
        Mod2& C;

        skew_quint_type s0;
        skew_quad_type s1;
        skew_quint_type s2;

        int selected;
        bool done[3];

        offset_type index;
        offset_type merge_result;

        bool cmp_mod1_less_mod2()
        {
            assert(!done[1] && !done[2]);

            return std::get<1>(s1) < std::get<1>(s2);
        }

        bool cmp_mod0_less_mod2()
        {
            assert(!done[0] && !done[2]);

            if (std::get<1>(s0) == std::get<2>(s2)) {
                if (std::get<2>(s0) == std::get<3>(s2))
                    return std::get<4>(s0) < std::get<4>(s2);
                else
                    return std::get<2>(s0) < std::get<3>(s2);
            }
            else
                return std::get<1>(s0) < std::get<2>(s2);
        }

        bool cmp_mod0_less_mod1()
        {
            assert(!done[0] && !done[1]);

            if (std::get<1>(s0) == std::get<2>(s1))
                return std::get<3>(s0) < std::get<3>(s1);
            else
                return std::get<1>(s0) < std::get<2>(s1);
        }

        void merge()
        {
            assert(!done[0] || !done[1] || !done[2]);

            if (done[0])
            {
                if (done[2] || (!done[1] && cmp_mod1_less_mod2()))
                {
                    selected = 1;
                    merge_result = std::get<0>(s1);
                }
                else
                {
                    selected = 2;
                    merge_result = std::get<0>(s2);
                }
            }
            else if (done[1] || cmp_mod0_less_mod1())
            {
                if (done[2] || cmp_mod0_less_mod2())
                {
                    selected = 0;
                    merge_result = std::get<0>(s0);
                }
                else
                {
                    selected = 2;
                    merge_result = std::get<0>(s2);
                }
            }
            else
            {
                if (done[2] || cmp_mod1_less_mod2())
                {
                    selected = 1;
                    merge_result = std::get<0>(s1);
                }
                else
                {
                    selected = 2;
                    merge_result = std::get<0>(s2);
                }
            }

            assert(!done[selected]);
        }

    public:
        bool empty() const
        {
            return (A.empty() && B.empty() && C.empty());
        }

        merge_sa(Mod0& x1, Mod1& x2, Mod2& x3)
            : A(x1), B(x2), C(x3), selected(-1), index(0)
        {
            assert(!A.empty());
            assert(!B.empty());
            assert(!C.empty());
            done[0] = false;
            done[1] = false;
            done[2] = false;
            s0 = *A;
            s1 = *B;
            s2 = *C;

            merge();
        }

        const value_type& operator * () const
        {
            return merge_result;
        }

        merge_sa& operator ++ ()
        {
            if (selected == 0) {
                assert(!A.empty());
                ++A;
                if (!A.empty())
                    s0 = *A;
                else
                    done[0] = true;
            }
            else if (selected == 1) {
                assert(!B.empty());
                ++B;
                if (!B.empty())
                    s1 = *B;
                else
                    done[1] = true;
            }
            else {
                assert(!C.empty());
                assert(selected == 2);
                ++C;
                if (!C.empty())
                    s2 = *C;
                else
                    done[2] = true;
            }

            ++index;
            if (!empty())
                merge();

            return *this;
        }
    };

    /** Helper function for computing the size of the 2/3 subproblem. */
    static inline size_type subp_size(size_type n)
    {
        return (n / 3) * 2 + ((n % 3) == 2);
    }

    /**
     * Sort mod0-quints / mod1-quads / mod2-quints and run merge_sa class to
     * merge them together.
     * \param S input string pipe type.
     * \param Mod1 mod1 tuples input pipe type.
     * \param Mod2 mod2 tuples input pipe type.
     */
    template <class S, class Mod1, class Mod2>
    class build_sa
    {
    public:
        using value_type = offset_type;

        static const unsigned int add_rank = 1;  // free first rank to mark ranks beyond end of input

    private:
        // mod1 types
        using mod1_push_type = typename stream::use_push<skew_quad_type>;
        using mod1_runs_type = typename stream::runs_creator<mod1_push_type, less_mod1>;
        using sorted_mod1_runs_type = typename mod1_runs_type::sorted_runs_type;
        using mod1_rm_type = typename stream::runs_merger<sorted_mod1_runs_type, less_mod1>;

        // mod2 types
        using mod2_push_type = typename stream::use_push<skew_quint_type>;
        using mod2_runs_type = typename stream::runs_creator<mod2_push_type, less_mod2>;
        using sorted_mod2_runs_type = typename mod2_runs_type::sorted_runs_type;
        using mod2_rm_type = typename stream::runs_merger<sorted_mod2_runs_type, less_mod2>;

        // mod0 types
        using mod0_push_type = typename stream::use_push<skew_quint_type>;
        using mod0_runs_type = typename stream::runs_creator<mod0_push_type, less_mod0>;
        using sorted_mod0_runs_type = typename mod0_runs_type::sorted_runs_type;
        using mod0_rm_type = typename stream::runs_merger<sorted_mod0_runs_type, less_mod0>;

        // Merge type
        using merge_sa_type = merge_sa<mod0_rm_type, mod1_rm_type, mod2_rm_type>;

        // Functions
        less_mod0 c0;
        less_mod1 c1;
        less_mod2 c2;

        // Runs merger
        mod1_rm_type* mod1_result;
        mod2_rm_type* mod2_result;
        mod0_rm_type* mod0_result;

        // Merger
        merge_sa_type* vmerge_sa;

        // Input
        S& source;
        Mod1& mod_1;
        Mod2& mod_2;

        // Tmp variables
        offset_type t[3];
        offset_type old_t2;
        offset_type old_mod2;
        bool exists[3];
        offset_type mod_one;
        offset_type mod_two;

        offset_type index;

        // Empty_flag
        bool ready;

        // Result
        value_type result;

    public:
        build_sa(S& source_, Mod1& mod_1_, Mod2& mod_2_, size_type a_size, size_t memsize)
            : source(source_), mod_1(mod_1_), mod_2(mod_2_), index(0), ready(false)
        {
            assert(!source_.empty());

            // Runs storage

            // input: ISA_1,2 from previous level
            mod0_runs_type mod0_runs(c0, memsize / 4);
            mod1_runs_type mod1_runs(c1, memsize / 4);
            mod2_runs_type mod2_runs(c2, memsize / 4);

            while (!source.empty())
            {
                exists[0] = false;
                exists[1] = false;
                exists[2] = false;

                if (!source.empty()) {
                    t[0] = *source;
                    ++source;
                    exists[0] = true;
                }

                if (!source.empty()) {
                    assert(!mod_1.empty());
                    t[1] = *source;
                    ++source;
                    mod_one = *mod_1 + add_rank;
                    ++mod_1;
                    exists[1] = true;
                }

                if (!source.empty()) {
                    assert(!mod_2.empty());
                    t[2] = *source;
                    ++source;
                    mod_two = *mod_2 + add_rank;
                    ++mod_2;
                    exists[2] = true;
                }

                // Check special cases in the middle of "source"
                // Cases are cx|xc cxx|cxx and cxxc|xxc

                assert(t[0] != offset_type(0));
                assert(t[1] != offset_type(0));
                assert(t[2] != offset_type(0));

                // Mod 0 : (index0,char0,char1,mod1,mod2)
                // Mod 1 : (index1,mod1,char1,mod2)
                // Mod 2 : (index2,mod2)

                if (exists[2]) { // Nothing is missed
                    mod0_runs.push(skew_quint_type(index, t[0], t[1], mod_one, mod_two));
                    mod1_runs.push(skew_quad_type(index + 1, mod_one, t[1], mod_two));

                    if (index != offset_type(0)) {
                        mod2_runs.push(skew_quint_type((index - 1), old_mod2, old_t2, t[0], mod_one));
                    }
                }
                else if (exists[1]) { // Last element missed
                    mod0_runs.push(skew_quint_type(index, t[0], t[1], mod_one, 0));
                    mod1_runs.push(skew_quad_type(index + 1, mod_one, t[1], 0));

                    if (index != offset_type(0)) {
                        mod2_runs.push(skew_quint_type((index - 1), old_mod2, old_t2, t[0], mod_one));
                    }
                }
                else { // Only one element left
                    assert(exists[0]);
                    mod0_runs.push(skew_quint_type(index, t[0], 0, 0, 0));

                    if (index != offset_type(0)) {
                        mod2_runs.push(skew_quint_type((index - 1), old_mod2, old_t2, t[0], 0));
                    }
                }

                old_mod2 = mod_two;
                old_t2 = t[2];
                index += 3;
            }

            if ((a_size % 3) == 0) { // changed
                if (index != offset_type(0)) {
                    mod2_runs.push(skew_quint_type((index - 1), old_mod2, old_t2, 0, 0));
                }
            }

            mod0_runs.deallocate();
            mod1_runs.deallocate();
            mod2_runs.deallocate();

            std::cout << "merging S0 = " << mod0_runs.size() << ", S1 = " << mod1_runs.size()
                      << ", S2 = " << mod2_runs.size() << " tuples" << std::endl;

            // Prepare for merging

            mod0_result = new mod0_rm_type(mod0_runs.result(), less_mod0(), memsize / 5);
            mod1_result = new mod1_rm_type(mod1_runs.result(), less_mod1(), memsize / 5);
            mod2_result = new mod2_rm_type(mod2_runs.result(), less_mod2(), memsize / 5);

            // output: ISA_1,2 for next level
            vmerge_sa = new merge_sa_type(*mod0_result, *mod1_result, *mod2_result);

            // read first suffix
            result = *(*vmerge_sa);
        }

        const value_type& operator * () const
        {
            return result;
        }

        build_sa& operator ++ ()
        {
            assert(vmerge_sa != 0 && !vmerge_sa->empty());

            ++(*vmerge_sa);
            if (!vmerge_sa->empty()) {
                result = *(*vmerge_sa);
            }
            else {  // cleaning up
                assert(vmerge_sa->empty());
                ready = true;

                assert(vmerge_sa != nullptr);
                delete vmerge_sa, vmerge_sa = nullptr;

                assert(mod0_result != nullptr && mod1_result != nullptr && mod2_result != nullptr);
                delete mod0_result, mod0_result = nullptr;
                delete mod1_result, mod1_result = nullptr;
                delete mod2_result, mod2_result = nullptr;
            }

            return *this;
        }

        ~build_sa()
        {
            if (vmerge_sa) delete vmerge_sa;

            if (mod0_result) delete mod0_result;
            if (mod1_result) delete mod1_result;
            if (mod2_result) delete mod2_result;
        }

        bool empty() const
        {
            return ready;
        }
    };

    /** The skew algorithm.
     *  \param Input type of the input pipe. */
    template <class Input>
    class algorithm
    {
    public:
        using value_type = offset_type;
        using alphabet_type = typename Input::value_type;

    protected:
        // finished reading final suffix array
        bool finished;

        // current recursion depth
        unsigned int rec_depth;

    protected:
        // generate (i) sequence
        using counter_stream_type = stxxl::stream::counter<offset_type>;

        // Sorter
        using mod12cmp = stxxl::comparator<skew_pair_type, stxxl::direction::Less, stxxl::direction::DontCare>;
        using mod12_sorter_type = stxxl::sorter<skew_pair_type, mod12cmp>;

        // Additional streaming items
        using isa_second_type = stream::choose<mod12_sorter_type, 1>;
        using buildSA_type = build_sa<offset_array_it_rg, isa_second_type, isa_second_type>;
        using precompute_isa_type = make_pairs<buildSA_type, counter_stream_type>;

        // Real recursive skew3 implementation
        // This part is the core of the skew algorithm and runs all class objects in their respective order
        template <typename RecInputType>
        buildSA_type * skew3(RecInputType& p_Input)
        {
            // (t_i) -> (i,t_i,t_{i+1},t_{i+2})
            using make_quads_input_type = make_quads<RecInputType, offset_type, 1>;

            // (t_i) -> (i,t_i,t_{i+1},t_{i+2}) with i = 1,2 mod 3
            using mod12_quads_input_type = extract_mod12<make_quads_input_type>;

            // sort (i,t_i,t_{i+1},t_{i+2}) by (t_i,t_{i+1},t_{i+2})
            using less_quad_offset_type = stxxl::comparator<std::tuple<offset_type,offset_type,offset_type,offset_type>, stxxl::direction::DontCare>;
            using sort_mod12_input_type = typename stream::sort<mod12_quads_input_type, less_quad_offset_type>;

            // name (i,t_i,t_{i+1},t_{i+2}) -> (i,n_i)
            using naming_input_type = naming<sort_mod12_input_type>;

            mod12_sorter_type m1_sorter(mod12cmp(), ram_use / 5);
            mod12_sorter_type m2_sorter(mod12cmp(), ram_use / 5);

            // sorted mod1 runs -concatenate- sorted mod2 runs
            using concatenation_type = stxxl::stream::concatenate<mod12_sorter_type, mod12_sorter_type>;

            // (t_i) -> (i,t_i,t_{i+1},t_{i+2})
            offset_array_type text;
            make_quads_input_type quads_input(p_Input, text);

            // (t_i) -> (i,t_i,t_{i+1},t_{i+2}) with i = 1,2 mod 3
            mod12_quads_input_type mod12_quads_input(quads_input);

            // sort (i,t_i,t_{i+1},t_{i+2}) by (t_i,t_i+1},t_{i+2})
            sort_mod12_input_type sort_mod12_input(mod12_quads_input, less_quad_offset_type(), ram_use / 5);

            // name (i,t_i,t_{i+1},t_{i+2}) -> (i,"n_i")
            bool unique = false;         // is the current quad array unique?
            naming_input_type names_input(sort_mod12_input, unique);

            // create (i, s^12[i])
            size_type concat_length = 0; // holds length of current S_12
            while (!names_input.empty()) {
                const skew_pair_type& tmp = *names_input;
                if (std::get<0>(tmp) & 1) {
                    m2_sorter.push(tmp); // sorter #2
                }
                else {
                    m1_sorter.push(tmp); // sorter #1
                }
                ++names_input;
                concat_length++;
            }

            std::cout << "recursion string length = " << concat_length << std::endl;

            m1_sorter.sort();
            m2_sorter.sort();

            if (!unique)
            {
                std::cout << "not unique -> next recursion level = " << ++rec_depth << std::endl;

                // compute s^12 := lexname[S[1 mod 3]] . lexname[S[2 mod 3]], (also known as reduced recursion string 'R')
                concatenation_type concat_mod1mod2(m1_sorter, m2_sorter);

                buildSA_type* recType = skew3(concat_mod1mod2);  // recursion with recursion string T' = concat_mod1mod2 lexnames

                std::cout << "exit recursion level = " << --rec_depth << std::endl;

                counter_stream_type isa_loop_index;
                precompute_isa_type isa_pairs(*recType, isa_loop_index); // add index as component => (SA12, i)

                // store beginning of mod2-tuples of s^12 in mod2_pos
                offset_type special = (concat_length != subp_size(text.size()));
                offset_type mod2_pos = offset_type((subp_size(text.size()) >> 1) + (subp_size(text.size()) & 1) + special);

                mod12_sorter_type isa1_pair(mod12cmp(), ram_use / 5);
                mod12_sorter_type isa2_pair(mod12cmp(), ram_use / 5);

                while (!isa_pairs.empty()) {
                    const skew_pair_type& tmp = *isa_pairs;
                    if (std::get<0>(tmp) < mod2_pos) {
                        if (std::get<0>(tmp) + special < mod2_pos) // else: special sentinel tuple is dropped
                            isa1_pair.push(tmp);            // sorter #1
                    }
                    else {
                        isa2_pair.push(tmp);                // sorter #2
                    }
                    ++isa_pairs;
                }

                delete recType;

                isa1_pair.finish();
                isa2_pair.finish();

                offset_array_it_rg input(text.begin(), text.end());

                // => (i, ISA)
                isa1_pair.sort(ram_use / 8);
                isa2_pair.sort(ram_use / 8);

                // pick ISA of (i, ISA)
                isa_second_type isa1(isa1_pair);
                isa_second_type isa2(isa2_pair);

                // prepare and run merger
                return new buildSA_type(input, isa1, isa2, text.size(), ram_use);
            }
            else // unique
            {
                std::cout << "unique names!" << std::endl;

                isa_second_type isa1(m1_sorter);
                isa_second_type isa2(m2_sorter);

                offset_array_it_rg source(text.begin(), text.end());

                // prepare and run merger
                return new buildSA_type(source, isa1, isa2, text.size(), ram_use);
            }
        } // end of skew3()

    protected:
        // Adapt (t_i) -> (i,t_i) for input to fit to recursive call
        using make_pairs_input_type = make_pairs<counter_stream_type, Input>;

        // points to final constructed suffix array generator
        buildSA_type* out_sa;

    public:
        explicit algorithm(Input& data_in)
            : finished(false), rec_depth(0)
        {
            // (t_i) -> (i,t_i)
            counter_stream_type dummy;
            make_pairs_input_type pairs_input(dummy, data_in);

            out_sa = skew3(pairs_input);
        }

        const value_type& operator * () const
        {
            return *(*out_sa);
        }

        algorithm& operator ++ ()
        {
            assert(out_sa);
            assert(!out_sa->empty());

            ++(*out_sa);

            if (out_sa->empty()) {
                finished = true;
                delete out_sa;
                out_sa = nullptr;
            }
            return *this;
        }

        ~algorithm()
        {
            if (out_sa) delete out_sa;
        }

        bool empty() const
        {
            return finished;
        }
    }; // algorithm class
};     // skew class

//! helper to print out readable characters.
template <typename alphabet_type>
static inline std::string dumpC(alphabet_type c)
{
    std::ostringstream oss;
    if (isalnum(c)) oss << '\'' << static_cast<char>(c) << '\'';
    else oss << static_cast<int>(c);
    return oss.str();
}

//! helper stream to cut input off a specified length.
template <typename InputType>
class cut_stream
{
public:
    //! same value type as input stream
    using value_type = typename InputType::value_type;

protected:
    //! instance of input stream
    InputType& m_input;

    //! counter after which the stream ends
    size_type m_count;

public:
    cut_stream(InputType& input, size_type count)
        : m_input(input), m_count(count)
    { }

    const value_type& operator * () const
    {
        assert(m_count > 0);
        return *m_input;
    }

    cut_stream& operator ++ ()
    {
        assert(!empty());
        --m_count;
        ++m_input;
        return *this;
    }

    bool empty() const
    {
        return (m_count == 0) || m_input.empty();
    }
};

alphabet_type unary_generator()
{
    return 'a';
}

template <typename offset_type>
int process(const std::string& input_filename, const std::string& output_filename,
            size_type sizelimit,
            bool text_output_flag, bool check_flag, bool input_verbatim)
{
    static const size_t block_size = sizeof(offset_type) * 1024 * 1024 / 2;

    using alphabet_vector_type = typename stxxl::vector<alphabet_type, 1, stxxl::lru_pager<2> >;
    using offset_vector_type = typename stxxl::vector<offset_type, 1, stxxl::lru_pager<2>, block_size>;

    // input and output files (if supplied via command line)
    foxxll::file_ptr input_file, output_file;

    // input and output vectors for suffix array construction
    alphabet_vector_type input_vector;
    offset_vector_type output_vector;

    using foxxll::file;

    if (input_verbatim)
    {
        // copy input verbatim into vector
        input_vector.resize(input_filename.size());
        std::copy(input_filename.begin(), input_filename.end(), input_vector.begin());
    }
    else if (input_filename == "random")
    {
        if (sizelimit == std::numeric_limits<size_type>::max()) {
            std::cout << "You must provide -s <size> for generated inputs." << std::endl;
            return 1;
        }

        // fill input vector with random bytes
        input_vector.resize(sizelimit);

        std::mt19937 randgen;
        std::uniform_int_distribution<alphabet_type> distr;
        stxxl::generate(input_vector.begin(), input_vector.end(), std::bind(distr, std::ref(randgen)));
    }
    else if (input_filename == "unary")
    {
        if (sizelimit == std::numeric_limits<size_type>::max()) {
            std::cout << "You must provide -s <size> for generated inputs." << std::endl;
            return 1;
        }

        // fill input vector with random bytes
        input_vector.resize(sizelimit);
        stxxl::generate(input_vector.begin(), input_vector.end(), unary_generator);
    }
    else
    {
        // define input file object and map input_vector to input_file (no copying)
        input_file = tlx::make_counting<foxxll::syscall_file>(
            input_filename, file::RDONLY | file::DIRECT);
        alphabet_vector_type file_input_vector(input_file);
        input_vector.swap(file_input_vector);
    }

    if (output_filename.size())
    {
        // define output file object and map output_vector to output_file
        output_file = tlx::make_counting<foxxll::syscall_file>(
            output_filename, file::RDWR | file::CREAT | file::DIRECT);
        offset_vector_type file_output_vector(output_file);
        output_vector.swap(file_output_vector);
    }

    // I/O measurement
    foxxll::stats* Stats = foxxll::stats::get_instance();
    foxxll::stats_data stats_begin(*Stats);

    // construct skew class with bufreader input type
    using input_type = alphabet_vector_type::bufreader_type;
    using cut_input_type = cut_stream<input_type>;
    using skew_type = typename skew<offset_type>::template algorithm<cut_input_type>;

    size_type size = input_vector.size();
    if (size > sizelimit) size = sizelimit;

    std::cout << "input size = " << size << std::endl;

    if (size + 3 >= std::numeric_limits<offset_type>::max()) {
        std::cout << "error: input is too long for selected word size!" << std::endl;
        return -1;
    }

    input_type input(input_vector);
    cut_input_type cut_input(input, size);
    skew_type skew(cut_input);

    // make sure output vector has the right size
    output_vector.resize(size);

    // write suffix array stream into output vector
    stream::materialize(skew, output_vector.begin(), output_vector.end());

    std::cout << "output size = " << output_vector.size() << std::endl;
    std::cout << (foxxll::stats_data(*Stats) - stats_begin); // print i/o statistics

    if (text_output_flag)
    {
        std::cout << std::endl << "resulting suffix array:" << std::endl;

        for (unsigned int i = 0; i < output_vector.size(); i++) {
            std::cout << i << " : " << output_vector[i] << " : ";

            // We need a const reference because operator[] would write data
            const alphabet_vector_type& cinput = input_vector;

            for (unsigned int j = 0; output_vector[i] + j < cinput.size(); j++) {
                std::cout << dumpC(cinput[output_vector[i] + j]) << " ";
            }

            std::cout << std::endl;
        }
    }

    int ret = 0;

    if (check_flag)
    {
        (std::cout << "checking suffix array... ").flush();

        if (!sacheck_vectors(input_vector, output_vector)) {
            std::cout << "failed!" << std::endl;
            ret = -1;
        }
        else
            std::cout << "ok." << std::endl;
    }

    // close file, but have to deallocate vector first!

    if (input_file) {
        input_vector = alphabet_vector_type();
        input_file.reset();
    }
    if (output_file) {
        output_vector = offset_vector_type();
        output_file.reset();
    }

    return ret;
}

int main(int argc, char* argv[])
{
    stxxl::cmdline_parser cp;

    cp.set_description(
        "DC3 aka skew3 algorithm for external memory suffix array construction.");
    cp.set_author(
        "Jens Mehnert <jmehnert@mpi-sb.mpg.de>, "
        "Timo Bingmann <tb@panthema.net>, "
        "Daniel Feist <daniel.feist@student.kit.edu>");

    std::string input_filename, output_filename;
    size_type sizelimit = std::numeric_limits<size_type>::max();
    bool text_output_flag = false;
    bool check_flag = false;
    bool input_verbatim = false;
    unsigned wordsize = 32;

    cp.add_param_string("input", input_filename,
                        "Path to input file (or verbatim text).\n"
                        "  The special inputs 'random' and 'unary' generate "
                        "such text on-the-fly.");
    cp.add_flag('c', "check", check_flag,
                "Check suffix array for correctness.");
    cp.add_flag('t', "text", text_output_flag,
                "Print out suffix array in readable text.");
    cp.add_string('o', "output", output_filename,
                  "Output suffix array to given path.");
    cp.add_flag('v', "verbatim", input_verbatim,
                "Consider \"input\" as verbatim text to construct "
                "suffix array on.");
    cp.add_bytes('s', "size", sizelimit,
                 "Cut input text to given size, e.g. 2 GiB.");
    cp.add_bytes('M', "memuse", ram_use,
                 "Amount of RAM to use, default: 1 GiB.");
    cp.add_uint('w', "wordsize", wordsize,
                "Set word size of suffix array to 32, 40 or 64 bit, "
                "default: 32-bit.");

    // process command line
    if (!cp.process(argc, argv))
        return -1;

    if (wordsize == 32)
        return process<uint32_t>(
            input_filename, output_filename, sizelimit,
            text_output_flag, check_flag, input_verbatim);
#if 0
    else if (wordsize == 40)
        return process<foxxll::uint40>(
            input_filename, output_filename, sizelimit,
            text_output_flag, check_flag, input_verbatim);
    else if (wordsize == 64)
        return process<uint64_t>(
            input_filename, output_filename, sizelimit,
            text_output_flag, check_flag, input_verbatim);
#endif
    else
        std::cerr << "Invalid wordsize for suffix array: 32, 40 or 64 are allowed." << std::endl;

    return -1;
}
