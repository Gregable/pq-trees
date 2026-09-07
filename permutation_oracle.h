// Brute-force oracles used by fuzztest to check PQTree answers.
//
// A PQ-tree over a set of items represents exactly the permutations in which
// every reduced set appears contiguously. These helpers decide the same
// question by direct enumeration, so they can be trusted independently of the
// library under test.
#ifndef PERMUTATION_ORACLE_H
#define PERMUTATION_ORACLE_H

#include <algorithm>
#include <set>
#include <vector>

typedef std::vector<int> Permutation;
typedef std::vector<std::set<int> > Constraints;

// True if every member of |subset| appears in one contiguous run of |order|.
inline bool IsContiguous(const Permutation& order, const std::set<int>& subset) {
  int first = -1;
  int last = -1;
  for (size_t i = 0; i < order.size(); ++i) {
    if (!subset.count(order[i])) continue;
    if (first < 0) first = i;
    last = i;
  }
  return first >= 0 && (last - first + 1) == static_cast<int>(subset.size());
}

// True if |order| is a permutation of 0..item_count-1 that satisfies every
// constraint.
inline bool SatisfiesAll(const Permutation& order, int item_count,
                         const Constraints& constraints) {
  if (static_cast<int>(order.size()) != item_count) return false;
  std::set<int> seen(order.begin(), order.end());
  if (static_cast<int>(seen.size()) != item_count) return false;
  for (size_t i = 0; i < constraints.size(); ++i) {
    if (!IsContiguous(order, constraints[i])) return false;
  }
  return true;
}

// True if some permutation of 0..item_count-1 satisfies every constraint.
// Enumerates item_count! permutations, so keep item_count small.
inline bool AnyPermutationSatisfies(int item_count,
                                    const Constraints& constraints) {
  Permutation order(item_count);
  for (int i = 0; i < item_count; ++i) order[i] = i;
  do {
    if (SatisfiesAll(order, item_count, constraints)) return true;
  } while (std::next_permutation(order.begin(), order.end()));
  return false;
}

#endif  // PERMUTATION_ORACLE_H
