#ifndef PQNODE_H
#define PQNODE_H

/**
Internal class used by pqtree to represent individual nodes in the pqtree.
*/

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>
using namespace std;

// Enum types we use throughout.
enum PQNode_types  {leaf, pnode, qnode};
enum PQNode_marks  {unmarked, queued, blocked, unblocked};
enum PQNode_labels {empty, full, partial};

class PQNode
{
  // PQNodes are not exposed by pqtrees, they are internally used only,
  // so everything here is private.
  friend class PQTree;
  private:

  ////////////////////////////////////////////////////
  // Used by P Nodes only
  ///////////////////////////////////////////////////
  
  // A doubly-linked of links which form the children of a p-node, the order
  // of the list is arbitrary.
  list<PQNode*> circular_link;
  
  // A count of the number of children used by a node.
  int ChildCount();
  
  ////////////////////////////////////////////////////
  // Used by Q Nodes only
  ///////////////////////////////////////////////////
  
  // A set containing the two endmost children of a Q-node
  PQNode *endmost_children[2];
  PQNode *pseudo_neighbors[2];

  // Boolean indicating whether or not this is a pseudonode/child.
  bool pseudonode, pseudochild;
  
  ////////////////////////////////////////////////////
  // Used by Both Node Types
  ///////////////////////////////////////////////////
  
  // A set containing all the children of a node currently known to be full.
  set<PQNode*> full_children;
  
  // A set containing all the children of a node currently
  // known to be partial
  set<PQNode*> partial_children;
  
  // A set containing exactly 0, 1, or 2 nodes
  // Only children of Q nodes have more than 0 immediate siblings
  set<PQNode*> immediate_siblings;
  
  // Label is an indication of whether the node is empty, full, or partial
  enum PQNode_labels label;
  
  // Mark is a designation used during the first pass of the reduction
  // algorithm.  Every node is initially unmarked.  It is marked
  // queued when it is placed onto the queue during the bubbling up.
  // It is marked either blocked or unblocked when it is processed.  Blocked
  // nodes can become unblocked if their siblings become unblocked.
  enum PQNode_marks mark;

  // type is a designation telling whether the node is a leaf, P, or Q.
  enum PQNode_types type;

  // the immediate ancestor of a node.  This field is always
  // valid for children of P-nodes and for endmost children of Q-nodes
  PQNode* parent;

  // A count of the number of pertinent children currently possessed by a node
  int pertinent_child_count;

  // A count of the number of pertinent leaves currently possessed by a node
  int pertinent_leaf_count;

  // The value of the PQNode if it is a leaf.
  int leaf_value;

  // Makes a deep copy of a node, sets this to be it's parent and returns copy.
  PQNode* CopyAsChild(const PQNode& toCopy);

  // Makes a deep copy of copy.
  void Copy(const PQNode& copy);
  
  // deep assignment operator
  PQNode& operator=(const PQNode& toCopy);
  
  // Return the next child in the immediate_siblings chain given a last pointer
  // if last pointer is null, will return the first sibling.  Behavior similar
  // to an iterator.
  PQNode* QNextChild(PQNode *last);
  
  // removes this node from a q-parent and puts toInsert in it's place
  void SwapQ(PQNode *toInsert);
  
  // deep copy constructor
  PQNode(const PQNode& toCopy);
    
  // Constructor for a leaf PQNode.
  PQNode(int value);
  
  // Constructor for non-leaf PQNode.
  PQNode();
  
  // Deep destructor.
  ~PQNode();
  
  // Walks the tree to build a map from values to leaf pointers.
  void FindLeaves(map<int, PQNode*> &leafAddress);
  
  // Walks the tree to find it's Frontier, returns one possible ordering.
  void FindFrontier(list<int> &ordering);
  
  // Resets a bunch of temporary variables after the reduce walks
  void Reset();
  
  // Walks the tree and prints it's structure to |out|.  P-nodes are
  // represented as ( ), Q-nodes as [ ], and leafs by their integer id. Used
  // primarily for debugging purposes
  void Print(string *out);
};

#endif
