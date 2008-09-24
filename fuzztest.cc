// PQ-Tree fuzz test.  We basically repeatedly create a random sequence of
// integers, choose random consecutive subseries out of the original series as
// a reduction, then apply the reductions to a PQ Tree.  For now, we are just
// looking to see that the library doesn't crash or return false.

#include <cstdlib>
#include <iostream>
#include <set>
#include <vector>
#include "pqtree.h"

int ITERATIONS = 100;  // Number of fuzztest iterations to run.
int REDUCTIONS = 20;  // Number of reductions to apply on each run
int TREE_SIZE = 10;   // Size of the PQ-Tree in each fuzztest.

bool fuzztest() {
  for (int i = 0; i < ITERATIONS; ++i) {
    // Generate a tree:
    set<int> items;
    vector<int> frontier;
    cout << "new tree: ";
    for(int j = 0; j < TREE_SIZE; ++j) {
      items.insert(j);
      frontier.push_back(j);
    }
    PQTree tree(items);

    // We pick a random ordering of the items.
    random_shuffle(frontier.begin(), frontier.end());
    for (int k = 0; k < TREE_SIZE; k++) {
      cout << frontier[k] << " ";
    }
    cout << "\n";
    for (int j = 0; j < REDUCTIONS; ++j) {

      // Then we choose a random starting point in the list and a random length
      int start = rand() % (TREE_SIZE - 2);
      int size = min(rand() % 10 + 2, TREE_SIZE - start);

      set<int> reduction;
      for (int k = start; k < start + size; ++k) {
	reduction.insert(frontier[k]);
	cout << frontier[k] << " ";
      }
      cout << "\n";
      if (!tree.reduce(reduction)) {
	return false;
      }
      cout << tree.Print() << "\n";
    }
  }
  return true;
}
  
// Returns 0 if fuzztest succeeded, 1 if a failure was detected.
int main(int argc, char **argv)
{
  if (!fuzztest()) {
    cout << "failure\n";
    return false;
  }
  return true;
}


