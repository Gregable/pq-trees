// Deterministic tests of the PQTree API beyond plain Reduce: the copy
// constructor, assignment, SafeReduce, ReduceAll, SafeReduceAll, the
// accessors, and how failed reductions are handled.
//
// Exits 0 on success and 1 after printing every failed expectation. Run it
// under AddressSanitizer with leak detection on to check that copies and
// failure paths free everything.

#include <cstdio>
#include <list>
#include <set>
#include <string>
#include <vector>

#include "permutation_oracle.h"
#include "pqtree.h"

namespace {

int failures = 0;

void Expect(bool condition, const char* test, const char* what) {
  if (condition) return;
  ++failures;
  printf("FAILED [%s]: %s\n", test, what);
}

PQTree MakeTree(int item_count) {
  std::set<int> items;
  for (int i = 0; i < item_count; ++i) items.insert(i);
  return PQTree(items);
}

std::set<int> Set(int a, int b, int c = -1) {
  std::set<int> s;
  s.insert(a);
  s.insert(b);
  if (c >= 0) s.insert(c);
  return s;
}

Permutation FrontierOf(PQTree* tree) {
  std::list<int> frontier = tree->Frontier();
  return Permutation(frontier.begin(), frontier.end());
}

Constraints ConstraintsOf(PQTree* tree) {
  std::list<std::set<int> > reductions = tree->GetReductions();
  return Constraints(reductions.begin(), reductions.end());
}

// A tree is consistent if its structure passes CheckInvariants and its
// frontier satisfies every reduction it has recorded.
bool Consistent(PQTree* tree, int item_count) {
  std::string broken;
  if (!tree->CheckInvariants(&broken)) {
    printf("  invariant broken: %s\n", broken.c_str());
    return false;
  }
  return SatisfiesAll(FrontierOf(tree), item_count, ConstraintsOf(tree));
}

void TestCopyConstructorIsIndependent() {
  const char* name = "copy constructor";
  PQTree original = MakeTree(8);
  Expect(original.Reduce(Set(0, 1)), name, "setup reduce {0 1}");
  Expect(original.Reduce(Set(2, 3, 4)), name, "setup reduce {2 3 4}");

  PQTree copy(original);
  Expect(copy.Print() == original.Print(), name, "copy prints the same tree");
  Expect(copy.GetReductions() == original.GetReductions(), name,
         "copy carries the reductions");
  Expect(Consistent(&copy, 8), name, "copy is consistent");

  Expect(copy.Reduce(Set(4, 5)), name, "reduce copy {4 5}");
  Expect(original.Reduce(Set(6, 7)), name, "reduce original {6 7}");
  Expect(copy.Print() != original.Print(), name,
         "reductions on one tree do not affect the other");
  Expect(Consistent(&copy, 8), name, "copy consistent after diverging");
  Expect(Consistent(&original, 8), name, "original consistent after diverging");
}

void TestAssignmentReplacesExistingTree() {
  const char* name = "assignment";
  PQTree target = MakeTree(6);
  Expect(target.Reduce(Set(0, 1)), name, "setup reduce target {0 1}");
  PQTree source = MakeTree(6);
  Expect(source.Reduce(Set(2, 3)), name, "setup reduce source {2 3}");
  Expect(source.Reduce(Set(3, 4)), name, "setup reduce source {3 4}");

  target = source;
  Expect(target.Print() == source.Print(), name, "target prints like source");
  Expect(target.GetReductions() == source.GetReductions(), name,
         "target carries the source reductions");
  Expect(Consistent(&target, 6), name, "target is consistent");

  const std::string before_self = target.Print();
  target = target;
  Expect(target.Print() == before_self, name, "self assignment is a no-op");
  Expect(Consistent(&target, 6), name, "consistent after self assignment");

  Expect(source.Reduce(Set(0, 5)), name, "reduce source {0 5}");
  Expect(target.Print() == before_self, name,
         "target unaffected by later reductions on source");
}

void TestSafeReducePreservesTreeOnFailure() {
  const char* name = "SafeReduce";
  PQTree tree = MakeTree(6);
  Expect(tree.Reduce(Set(0, 1)), name, "setup reduce {0 1}");
  Expect(tree.Reduce(Set(1, 2)), name, "setup reduce {1 2}");
  const std::string before = tree.Print();

  Expect(!tree.SafeReduce(Set(0, 2)), name, "{0 2} is unsatisfiable");
  Expect(tree.Print() == before, name, "tree unchanged after failure");
  Expect(tree.GetReductions().size() == 2, name,
         "failed reduction is not recorded");
  Expect(Consistent(&tree, 6), name, "consistent after failure");

  Expect(tree.SafeReduce(Set(3, 4)), name, "SafeReduce still works after failure");
  Expect(tree.Reduce(Set(4, 5)), name, "Reduce still works after failure");
  Expect(tree.GetReductions().size() == 4, name, "four reductions recorded");
  Expect(Consistent(&tree, 6), name, "consistent at the end");
}

void TestReduceFailureInvalidatesTree() {
  const char* name = "Reduce failure";
  PQTree tree = MakeTree(6);
  Expect(tree.Reduce(Set(0, 1)), name, "setup reduce {0 1}");
  Expect(tree.Reduce(Set(1, 2)), name, "setup reduce {1 2}");
  Expect(!tree.Reduce(Set(0, 2)), name, "{0 2} is unsatisfiable");
  Expect(!tree.Reduce(Set(3, 4)), name,
         "a satisfiable reduction fails once the tree is invalid");
  Expect(tree.GetReductions().size() == 2, name,
         "failed reductions are not recorded");
}

void TestReduceAllStopsAtFirstFailure() {
  const char* name = "ReduceAll";
  PQTree tree = MakeTree(6);
  std::list<std::set<int> > all_succeed;
  all_succeed.push_back(Set(0, 1));
  all_succeed.push_back(Set(1, 2));
  Expect(tree.ReduceAll(all_succeed), name, "satisfiable list succeeds");
  Expect(Consistent(&tree, 6), name, "consistent after list");

  std::list<std::set<int> > with_failure;
  with_failure.push_back(Set(3, 4));
  with_failure.push_back(Set(0, 2));
  with_failure.push_back(Set(4, 5));
  Expect(!tree.ReduceAll(with_failure), name, "list with unsatisfiable set fails");
  Expect(tree.GetReductions().size() == 3, name,
         "reductions before the failure are recorded, later ones are not");
}

void TestSafeReduceAllRestoresOnFailure() {
  const char* name = "SafeReduceAll";
  PQTree tree = MakeTree(6);
  Expect(tree.Reduce(Set(4, 5)), name, "setup reduce {4 5}");
  const std::string before = tree.Print();

  std::list<std::set<int> > with_failure;
  with_failure.push_back(Set(0, 1));
  with_failure.push_back(Set(1, 2));
  with_failure.push_back(Set(0, 2));
  Expect(!tree.SafeReduceAll(with_failure), name, "list with failure fails");
  Expect(tree.Print() == before, name, "tree restored after failure");
  Expect(tree.GetReductions().size() == 1, name,
         "no reduction from the failed list is recorded");
  Expect(Consistent(&tree, 6), name, "consistent after failure");

  std::list<std::set<int> > all_succeed;
  all_succeed.push_back(Set(0, 1));
  all_succeed.push_back(Set(1, 2));
  Expect(tree.SafeReduceAll(all_succeed), name, "satisfiable list succeeds");
  Expect(tree.GetReductions().size() == 3, name, "three reductions recorded");
  Expect(Consistent(&tree, 6), name, "consistent at the end");
}

void TestAccessors() {
  const char* name = "accessors";
  PQTree tree = MakeTree(5);
  Expect(tree.Root() != NULL, name, "root exists");
  Expect(tree.Root()->Type() == PQNode::pnode, name, "fresh root is a P-node");
  Expect(tree.Reduce(Set(1, 2)), name, "reduce {1 2}");
  Expect(tree.Reduce(Set(3, 4)), name, "reduce {3 4}");

  std::set<int> contained = tree.GetContained();
  Expect(contained.size() == 4 && contained.count(0) == 0, name,
         "GetContained holds exactly the reduced items");
  Expect(FrontierOf(&tree).size() == 5, name, "Frontier lists every item");

  std::list<int> reduced = tree.ReducedFrontier();
  Permutation reduced_order(reduced.begin(), reduced.end());
  Expect(reduced_order.size() == 4, name,
         "ReducedFrontier omits items never reduced");
  Expect(IsContiguous(reduced_order, Set(1, 2)) &&
             IsContiguous(reduced_order, Set(3, 4)),
         name, "ReducedFrontier satisfies the reductions");

  std::set<int> singleton;
  singleton.insert(0);
  Expect(tree.Reduce(singleton), name, "a singleton reduction succeeds");
  Expect(tree.GetReductions().size() == 3, name,
         "the singleton reduction is recorded");
  Expect(Consistent(&tree, 5), name, "consistent at the end");
}

}  // namespace

int main() {
  TestCopyConstructorIsIndependent();
  TestAssignmentReplacesExistingTree();
  TestSafeReducePreservesTreeOnFailure();
  TestReduceFailureInvalidatesTree();
  TestReduceAllStopsAtFirstFailure();
  TestSafeReduceAllRestoresOnFailure();
  TestAccessors();
  if (failures) {
    printf("%d expectation(s) failed\n", failures);
    return 1;
  }
  printf("all API tests passed\n");
  return 0;
}
