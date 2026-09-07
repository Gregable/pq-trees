// Randomized test for the PQ-tree library.
//
// Three checks are run, each of which must pass:
//
// 1. Regression cases: fixed reduction sequences that once broke the library.
//
// 2. Satisfiable sequences: a hidden permutation is chosen, and every
//    reduction set is a contiguous slice of it. Every Reduce must succeed, and
//    after each one the tree's Frontier() must be a permutation of all items
//    in which every set reduced so far is contiguous. This catches trees that
//    silently lose or duplicate leaves.
//
// 3. Arbitrary sequences on small trees: reduction sets are random subsets,
//    and Reduce's true/false answer is compared with a brute-force search over
//    all permutations. This catches both false negatives and false positives.
//
// Usage: fuzztest [--iterations N] [--seed S] [--max-leaves L]
// Exits 0 on success and 1 on the first failure, which is printed.

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "permutation_oracle.h"
#include "pqtree.h"

namespace {

struct Options {
  int iterations = 2000;
  unsigned seed = 1;
  int max_leaves = 42;
};

const int kMinLeaves = 3;
const int kMaxBruteForceLeaves = 8;
const int kBruteForceReductions = 8;

std::string SetToString(const std::set<int>& s) {
  std::string out = "{";
  for (std::set<int>::const_iterator i = s.begin(); i != s.end(); ++i) {
    if (i != s.begin()) out += " ";
    out += std::to_string(*i);
  }
  return out + "}";
}

std::string ConstraintsToString(const Constraints& constraints) {
  std::string out;
  for (size_t i = 0; i < constraints.size(); ++i) {
    if (i) out += " ";
    out += SetToString(constraints[i]);
  }
  return out;
}

PQTree MakeTree(int item_count) {
  std::set<int> items;
  for (int i = 0; i < item_count; ++i) items.insert(i);
  return PQTree(items);
}

Permutation FrontierOf(PQTree* tree) {
  std::list<int> frontier = tree->Frontier();
  return Permutation(frontier.begin(), frontier.end());
}

// Applies every constraint in order; all are expected to be satisfiable. If
// |unsatisfiable| is given, it is reduced last and must be rejected.
// Returns an empty string on success, otherwise a description of the failure.
std::string CheckSatisfiableSequence(int item_count,
                                     const Constraints& constraints,
                                     const std::set<int>* unsatisfiable) {
  PQTree tree = MakeTree(item_count);
  Constraints applied;
  for (size_t i = 0; i < constraints.size(); ++i) {
    applied.push_back(constraints[i]);
    if (!tree.Reduce(constraints[i])) {
      return "Reduce returned false for satisfiable set " +
             SetToString(constraints[i]) + " after " +
             ConstraintsToString(applied) + " tree=" + tree.Print();
    }
    if (!SatisfiesAll(FrontierOf(&tree), item_count, applied)) {
      return "Frontier violates constraints after " +
             ConstraintsToString(applied) + " tree=" + tree.Print();
    }
    std::string broken;
    if (!tree.CheckInvariants(&broken)) {
      return "Invariant broken after " + ConstraintsToString(applied) + ": " +
             broken;
    }
  }
  if (unsatisfiable && tree.Reduce(*unsatisfiable)) {
    return "Reduce accepted unsatisfiable set " + SetToString(*unsatisfiable) +
           " after " + ConstraintsToString(applied) + " tree=" + tree.Print();
  }
  return "";
}

// A 39-leaf case where TemplateP6 mistook an interior Q-node child for the
// tree root and replaced the root, dropping 17 leaves while Reduce still
// returned true.
std::string RegressionP6InteriorQChild() {
  const int kItems = 39;
  const int raw[][40] = {
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 17, 18, 19, 20, 21,
       23, 25, 26, 28, 29, 31, 32, 33, 34, 36, 37, 38, -1},
      {5, 7, 10, 11, 25, 26, 31, 37, -1},
      {1, 3, 12, 13, 17, 20, 22, 24, 27, 28, 29, 30, 34, 35, -1},
      {0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 17, 18, 19, 20, 21,
       23, 25, 26, 28, 29, 31, 32, 33, 34, 36, 37, 38, -1},
      {0, 4, 6, 9, 14, 15, 18, 19, 21, 32, 33, 36, 38, -1},
      {0, 1, 3, 4, 5, 6, 7, 9, 10, 11, 14, 15, 16, 18, 19, 21, 25, 26, 28, 29,
       31, 32, 33, 34, 36, 37, 38, -1},
      {5, 7, 10, 11, 16, 21, 25, 31, 37, -1},
  };
  Constraints constraints;
  for (size_t i = 0; i < sizeof(raw) / sizeof(raw[0]); ++i) {
    std::set<int> s;
    for (int j = 0; raw[i][j] >= 0; ++j) s.insert(raw[i][j]);
    constraints.push_back(s);
  }
  return CheckSatisfiableSequence(kItems, constraints, NULL);
}

// A 19-leaf case where TemplateQ2 merged a Q-node into its parent and deleted
// it, leaving a child that became an interior Q-child still pointing at the
// deleted node as its parent. TemplateP6 later read that dangling pointer and
// linked new nodes into freed memory, corrupting the tree (a use-after-free;
// the visible symptom depends on the allocator).
std::string RegressionQ2StaleParentPointer() {
  const int kItems = 19;
  const int raw[][20] = {
      {0, 2, 3, 4, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 17, 18, -1},
      {0, 16, -1},
      {3, 8, 9, 11, 12, 14, -1},
      {10, 13, -1},
      {1, 4, 5, 7, 15, 18, -1},
      {2, 3, 6, 8, 9, 14, 16, 17, -1},
  };
  Constraints constraints;
  for (size_t i = 0; i < sizeof(raw) / sizeof(raw[0]); ++i) {
    std::set<int> s;
    for (int j = 0; raw[i][j] >= 0; ++j) s.insert(raw[i][j]);
    constraints.push_back(s);
  }
  return CheckSatisfiableSequence(kItems, constraints, NULL);
}

// A 33-leaf case where TemplateQ2, applied at the root of the pertinent
// subtree, inserted the root into the partial_children_ set of its stale
// parent pointer, which pointed at a Q-node TemplateQ3 had already deleted.
std::string RegressionQ2AtPertinentRoot() {
  const int kItems = 33;
  const int raw[][34] = {
      {0, 1, 2, 3, 4, 5, 6, 8, 9, 11, 12, 15, 16, 17, 18, 19, 20, 21, 22, 24,
       27, 29, 30, 31, 32, -1},
      {7, 10, 25, 26, 28, -1},
      {26, 28, -1},
      {10, 25, 26, -1},
      {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 19, 21, 22, 23, 24, 25,
       26, 27, 28, 29, 30, -1},
      {3, 5, 6, 8, 9, 10, 11, 13, 14, 15, 16, 17, 19, 21, 22, 23, 24, 25, 26,
       27, 28, 29, 30, -1},
      {26, 28, -1},
  };
  Constraints constraints;
  for (size_t i = 0; i < sizeof(raw) / sizeof(raw[0]); ++i) {
    std::set<int> s;
    for (int j = 0; raw[i][j] >= 0; ++j) s.insert(raw[i][j]);
    constraints.push_back(s);
  }
  return CheckSatisfiableSequence(kItems, constraints, NULL);
}

bool RunRegressions() {
  struct Case {
    const char* name;
    std::string (*run)();
  };
  const Case cases[] = {
      {"P6 interior Q-child", RegressionP6InteriorQChild},
      {"Q2 stale parent pointer", RegressionQ2StaleParentPointer},
      {"Q2 at pertinent root", RegressionQ2AtPertinentRoot},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    std::string failure = cases[i].run();
    if (!failure.empty()) {
      printf("REGRESSION FAILED [%s]: %s\n", cases[i].name, failure.c_str());
      return false;
    }
  }
  printf("regressions: %zu passed\n", sizeof(cases) / sizeof(cases[0]));
  return true;
}

// Check 2: every reduction is a contiguous slice of a hidden permutation.
bool RunSatisfiableSequences(const Options& options, std::mt19937* rng) {
  long reductions = 0;
  for (int it = 0; it < options.iterations; ++it) {
    std::uniform_int_distribution<int> leaf_dist(kMinLeaves, options.max_leaves);
    const int item_count = leaf_dist(*rng);
    Permutation hidden(item_count);
    for (int i = 0; i < item_count; ++i) hidden[i] = i;
    std::shuffle(hidden.begin(), hidden.end(), *rng);

    Constraints constraints;
    std::uniform_int_distribution<int> count_dist(1, 60);
    const int count = count_dist(*rng);
    for (int r = 0; r < count; ++r) {
      std::uniform_int_distribution<int> start_dist(0, item_count - 2);
      const int start = start_dist(*rng);
      std::uniform_int_distribution<int> len_dist(2, item_count - start);
      const int len = len_dist(*rng);
      constraints.push_back(
          std::set<int>(hidden.begin() + start, hidden.begin() + start + len));
    }
    // Finish with a set the tree must reject: after {a b} and {b c} for three
    // consecutive hidden items, b sits between a and c, so {a c} cannot be
    // contiguous.
    std::uniform_int_distribution<int> triple_dist(0, item_count - 3);
    const int t = triple_dist(*rng);
    constraints.push_back(std::set<int>(hidden.begin() + t, hidden.begin() + t + 2));
    constraints.push_back(std::set<int>(hidden.begin() + t + 1, hidden.begin() + t + 3));
    std::set<int> unsatisfiable;
    unsatisfiable.insert(hidden[t]);
    unsatisfiable.insert(hidden[t + 2]);
    reductions += constraints.size() + 1;

    std::string failure =
        CheckSatisfiableSequence(item_count, constraints, &unsatisfiable);
    if (!failure.empty()) {
      printf("SATISFIABLE FAILED seed=%u iteration=%d leaves=%d: %s\n",
             options.seed, it, item_count, failure.c_str());
      return false;
    }
  }
  printf("satisfiable sequences: %d trees, %ld reductions passed\n",
         options.iterations, reductions);
  return true;
}

std::set<int> RandomSubset(int item_count, int min_size, std::mt19937* rng) {
  std::uniform_int_distribution<int> size_dist(min_size, item_count);
  const int size = size_dist(*rng);
  std::uniform_int_distribution<int> item_dist(0, item_count - 1);
  std::set<int> subset;
  while (static_cast<int>(subset.size()) < size) subset.insert(item_dist(*rng));
  return subset;
}

// Check 3: random subsets on small trees, answers compared with brute force.
bool RunBruteForceComparison(const Options& options, std::mt19937* rng) {
  const int max_leaves = std::min(options.max_leaves, kMaxBruteForceLeaves);
  long reductions = 0;
  for (int it = 0; it < options.iterations; ++it) {
    std::uniform_int_distribution<int> leaf_dist(kMinLeaves, max_leaves);
    const int item_count = leaf_dist(*rng);
    PQTree tree = MakeTree(item_count);
    Constraints constraints;
    for (int r = 0; r < kBruteForceReductions; ++r) {
      constraints.push_back(RandomSubset(item_count, 2, rng));
      ++reductions;
      const bool expected = AnyPermutationSatisfies(item_count, constraints);
      const bool actual = tree.Reduce(constraints.back());
      if (actual != expected) {
        printf("BRUTE FORCE FAILED seed=%u iteration=%d leaves=%d: "
               "expected=%d actual=%d after %s tree=%s\n",
               options.seed, it, item_count, expected, actual,
               ConstraintsToString(constraints).c_str(), tree.Print().c_str());
        return false;
      }
      // A failed Reduce leaves the tree invalid by contract, so stop here.
      if (!actual) break;
      if (!SatisfiesAll(FrontierOf(&tree), item_count, constraints)) {
        printf("BRUTE FORCE FAILED seed=%u iteration=%d leaves=%d: frontier "
               "violates %s tree=%s\n",
               options.seed, it, item_count,
               ConstraintsToString(constraints).c_str(), tree.Print().c_str());
        return false;
      }
      std::string broken;
      if (!tree.CheckInvariants(&broken)) {
        printf("BRUTE FORCE FAILED seed=%u iteration=%d leaves=%d: invariant "
               "broken after %s: %s\n",
               options.seed, it, item_count,
               ConstraintsToString(constraints).c_str(), broken.c_str());
        return false;
      }
    }
  }
  printf("brute force comparison: %d trees, %ld reductions passed\n",
         options.iterations, reductions);
  return true;
}

// Parses a whole non-negative decimal integer; rejects anything else.
bool ParseNonNegative(const char* text, long* value) {
  char* end = NULL;
  errno = 0;
  const long parsed = strtol(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' || parsed < 0) return false;
  *value = parsed;
  return true;
}

bool ParseOptions(int argc, char** argv, Options* options) {
  for (int i = 1; i < argc; ++i) {
    const bool has_value = i + 1 < argc;
    long value = 0;
    if (!strcmp(argv[i], "--iterations") && has_value &&
        ParseNonNegative(argv[++i], &value)) {
      options->iterations = static_cast<int>(value);
    } else if (!strcmp(argv[i], "--seed") && has_value &&
               ParseNonNegative(argv[++i], &value)) {
      options->seed = static_cast<unsigned>(value);
    } else if (!strcmp(argv[i], "--max-leaves") && has_value &&
               ParseNonNegative(argv[++i], &value)) {
      options->max_leaves = static_cast<int>(value);
    } else {
      fprintf(stderr,
              "usage: %s [--iterations N] [--seed S] [--max-leaves L]\n",
              argv[0]);
      return false;
    }
  }
  if (options->iterations < 0 || options->max_leaves < kMinLeaves) {
    fprintf(stderr, "iterations must be >= 0 and max-leaves >= %d\n",
            kMinLeaves);
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  if (!ParseOptions(argc, argv, &options)) return 2;
  printf("fuzztest seed=%u iterations=%d max-leaves=%d\n", options.seed,
         options.iterations, options.max_leaves);
  std::mt19937 rng(options.seed);
  if (!RunRegressions()) return 1;
  if (!RunSatisfiableSequences(options, &rng)) return 1;
  if (!RunBruteForceComparison(options, &rng)) return 1;
  printf("all checks passed\n");
  return 0;
}
