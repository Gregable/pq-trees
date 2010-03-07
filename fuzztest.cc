// PQ-Tree fuzz test.  We basically repeatedly create a random sequence of
// integers, choose random consecutive subseries out of the original series as
// a reduction, then apply the reductions to a PQ Tree.  For now, we are just
// looking to see that the library doesn't crash or return false.

// This file is part of the PQ Tree library.
//
// The PQ Tree library is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by the 
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.
//
// The PQ Tree Library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License along 
// with the PQ Tree Library.  If not, see <http://www.gnu.org/licenses/>.


#include <cstdlib>
#include <iostream>
#include <set>
#include <vector>
#include "pqtree.h"

int ITERATIONS = 10000;  // Number of fuzztest iterations to run.
int REDUCTIONS = 20;     // Number of reductions to apply on each run
int TREE_SIZE = 10;      // Size of the PQ-Tree in each fuzztest.

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
      cout << endl;
      if (!tree.Reduce(reduction)) {
	return false;
      }
      cout << tree.Print() << endl;
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
