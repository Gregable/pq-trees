/*
PQ-Tree class based on the paper "Testing for the Consecutive Onces Property,
Interval Graphs, and Graph Planarity Using PQ-Tree Algorithms" by Kellog S. 
Booth and George S. Lueker in the Journal of Computer and System Sciences 13,
335-379 (1976)
*/

#include <list>
#include <set>
#include <vector>
#include <queue>
#include <map>
#include <iostream>
#include "pqnode.h"
#include "set_methods.h"

using namespace std;

#ifndef PQTREE_H
#define PQTREE_H

class PQTree {
  private:
  
  // Root node of the PQTree
  PQNode *root_;
  
  // The number of blocks of blocked nodes during the 1st pass
  int block_count_;

  // The number of blocked nodes during the 1st pass
  int blocked_nodes_;

  // A variable (0 or 1) which is a count of the number of virtual nodes which
  // are imagined to be in the queue during the bubbling up.
  int off_the_top_;

  // Keeps track of all reductions performed on this tree in order
  list<set<int> > reductions_;

  // Keeps a pointer to the leaf containing a particular value this map actually
  // increases the time complexity of the algorithm.  To fix, you can create an
  // array of items so that each item hashes to its leaf address in constant
  // time, but this is a tradeoff to conserve space.
  map<int, PQNode*> leaf_address_;

  // A reference to a pseudonode that cannot be reached through the root
  // of the tree.  The pseudonode is a temporary node designed to handle
  // a special case in the first bubbling up pass it only exists during the
  // scope of the reduce operation
  PQNode* pseudonode_;

  // true if a non-safe reduce has failed, tree is useless.
  bool invalid_;

  // Loops through the consecutive blocked siblings of an unblocked node
  // recursively unblocking the siblings.
  // Args:
  //   sibling: next sibling node to unblock
  //   parent: Node to set as the parent of all unblocked siblings
  //   last: the last node that was unblocked (used in the recursion)
  int UnblockSiblings(PQNode* sibling, PQNode* parent, PQNode* last=NULL);

  // All of the templates for matching a reduce are below.  The template has a
  // letter describing which type of node it refers to and a number indicating
  // the index of the template for that letter.  These are the same indices in
  // the Booth & Lueker paper.  The return value indicates whether or not the
  // pattern accurately matches the template
  bool template_L1(PQNode* candidate_node);
  bool template_Q1(PQNode* candidate_node);
  bool template_Q2(PQNode* candidate_node);
  bool template_Q3(PQNode* candidate_node);
  bool template_P1(PQNode* candidate_node, bool is_reduction_root);
  bool template_P2(PQNode* candidate_node);
  bool template_P3(PQNode* candidate_node);
  bool template_P4(PQNode* candidate_node);
  bool template_P5(PQNode* candidate_node);
  bool template_P6(PQNode* candidate_node);
  
  // This procedure is the first pass of the Booth&Leuker PQTree algorithm
  // It processes the pertinent subtree of the PQ-Tree to determine the mark
  // of every node in that subtree
  // the pseudonode, if created, is returned so that it can be deleted at
  // the end of the reduce step
  bool bubble(set<int> S);

  bool reduceStep(set<int> S);
        
 public:  
  // Default constructor - constructs a tree using a set
  // Only reductions using elements of that set will succeed
  PQTree(set<int> S);
  PQTree(const PQTree& to_copy);
  ~PQTree();
  
  // Mostly for debugging purposes, Prints the tree to standard out
  string Print();
  
  // Cleans up pointer mess caused by having a pseudonode
  void cleanPseudo();

  // Reduces the tree but protects if from becoming invalid if the reduction
  // fails, takes more time.
  bool safeReduce(set<int>);
  bool safeReduceAll(list<set<int> >);

  //reduces the tree - tree can become invalid, making all further
  //reductions fail
  bool reduce(set<int> S);
  bool reduceAll(list<set<int> > L);
  
  //returns 1 possible frontier, or ordering preserving the reductions
  list<int> frontier();
  
  // Assignment operator
  PQTree& operator=(const PQTree& to_copy);  

  // Copies to_copy.  Used for the copy constructor and assignment operator.
  void CopyFrom(const PQTree& to_copy);
  
  
  //returns a frontier not including leaves that were not a part of any
  //reduction
  list<int> reducedFrontier();
  
  //returns the reductions that have been performed on this tree
  list<set<int> > getReductions();
  
  //returns the set of all elements on which a reduction was performed with
  set<int> getContained();
};

#endif
