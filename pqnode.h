// Internal class used by pqtree to represent individual nodes in the pqtree.

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

#ifndef PQNODE_H
#define PQNODE_H

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>
using namespace std;


class PQNode {
 // PQNodes are not exposed by pqtrees, they are internally used only.
 friend class PQTree;
  friend class QNodeChildrenIterator;

 public:
  // Enum types we use throughout.
  enum PQNode_types  {leaf, pnode, qnode};
  enum PQNode_marks  {unmarked, queued, blocked, unblocked};
  enum PQNode_labels {empty, full, partial};

  // Returns the type of the current node, an enum of type PQNode_types.
  PQNode_types Type();

  // Returns the value of the leaf node.  Fails assertion if not leaf node.
  int LeafValue();

  // Returns all of this Node's children if it has any.
  // Return Value is the |children| argument.
  void Children(vector<PQNode*> *children);

 private:
  /***** Used by P Nodes only *****/

  // A doubly-linked of links which form the children of a p-node, the order
  // of the list is arbitrary.
  list<PQNode*> circular_link_;

  // A count of the number of children used by a node.
  int ChildCount();

  // Returns the first |circular_link_| child with a given label or NULL.
  PQNode* CircularChildWithLabel(PQNode_labels label);

  // Moves the full children of this node to children of |new_node|.
  void MoveFullChildren(PQNode* new_node);

  // Replaces the circular_link pointer of |old_child| with |new_child|.
  void ReplaceCircularLink(PQNode* old_child, PQNode* new_child);

  /***** Used by Q Nodes only *****/

  // A set containing the two endmost children of a Q-node
  PQNode *endmost_children_[2];
  PQNode *pseudo_neighbors_[2];

  // Boolean indicating whether or not this is a pseudonode/child.
  bool pseudonode_, pseudochild_;

  // Returns the first endmost child with a given label or NULL.
  PQNode* EndmostChildWithLabel(PQNode_labels label);

  // Returns the first immediate sibling with a given label or NULL.
  PQNode* ImmediateSiblingWithLabel(PQNode_labels label);

  // Returns the first immediate sibling without a given label or NULL.
  PQNode* ImmediateSiblingWithoutLabel(PQNode_labels label);

  // Adds an immediate sibling to this node.
  void AddImmediateSibling(PQNode* sibling);

  // Adds an immediate sibling to this node.
  void RemoveImmediateSibling(PQNode* sibling);

  // Nulls otu the immediate siblings of this node.
  void ClearImmediateSiblings();

  // Returns the number of immediate siblings this node has (0, 1, or 2).
  int ImmediateSiblingCount() const;

  // Replaces the |endmost_children_| pointer to |old_child| with |new_child|.
  void ReplaceEndmostChild(PQNode* old_child, PQNode* new_child);

  // Replaces the immediate sibling of |old_child| with |new_child|.
  void ReplaceImmediateSibling(PQNode* old_child, PQNode* new_child);

  // Replaces the partial child |old_child| with |new_child|.
  void ReplacePartialChild(PQNode* old_child, PQNode* new_child);

  // Forces a Q-Node to "forget" it's pointers to it's endmost children.
  // Useful if you want to delete a Q-Node but not it's children.
  void ForgetChildren();

  // Returns true if all of the full and partial children of this node are
  // consecutive, with the partial children on the outside.
  bool ConsecutiveFullPartialChildren();

  /***** Used by Both Node types *****/

  // A set containing all the children of a node currently known to be full.
  set<PQNode*> full_children_;

  // A set containing all the children of a node currently known to be partial.
  set<PQNode*> partial_children_;

  // Only children of Q nodes have more than 0 immediate siblings.  Stores the
  // siblings to either side of this node in its parent's children.  One or both
  // siblings may be NULL.
  PQNode *immediate_siblings_[2];

  // Label is an indication of whether the node is empty, full, or partial
  enum PQNode_labels label_;

  // Mark is a designation used during the first pass of the reduction
  // algorithm.  Every node is initially unmarked.  It is marked
  // queued when it is placed onto the queue during the bubbling up.
  // It is marked either blocked or unblocked when it is processed.  Blocked
  // nodes can become unblocked if their siblings become unblocked.
  enum PQNode_marks mark_;

  // type is a designation telling whether the node is a leaf, P, or Q.
  enum PQNode_types type_;

  // the immediate ancestor of a node.  This field is always
  // valid for children of P-nodes and for endmost children of Q-nodes
  PQNode* parent_;

  // A count of the number of pertinent children currently possessed by a node
  int pertinent_child_count;

  // A count of the number of pertinent leaves currently possessed by a node
  int pertinent_leaf_count;

  // The value of the PQNode if it is a leaf.
  int leaf_value_;

  // Makes a deep copy of a node, sets this to be it's parent and returns copy.
  PQNode* CopyAsChild(const PQNode& to_copy);

  // Makes a deep copy of copy.
  void Copy(const PQNode& copy);

  // deep assignment operator
  PQNode& operator=(const PQNode& to_copy);

  // Return the next child in the immediate_siblings chain given a last pointer
  // if last pointer is null, will return the first sibling.  Behavior similar
  // to an iterator.
  PQNode* QNextChild(PQNode *last) const;

  void ReplaceChild(PQNode* old_child, PQNode* new_child);

  // removes this node from a q-parent and puts toInsert in it's place
  void SwapQ(PQNode *toInsert);

  // returns this node's parent (same as parent_ unless this node is a non-end
  // child of a Q-Node, in which case returns NULL)
  PQNode* Parent() const;

  // deep copy constructor
  PQNode(const PQNode& to_copy);

  // Constructor for a leaf PQNode.
  PQNode(int value);

  // Constructor for non-leaf PQNode.
  PQNode();

  // Deep destructor.
  ~PQNode();

  // Label's this node as full, updating the parent if needed.
  void LabelAsFull();

  // Walks the tree to build a map from values to leaf pointers.
  void FindLeaves(map<int, PQNode*> &leafAddress);

  // Walks the tree to find it's Frontier, returns one possible ordering.
  void FindFrontier(list<int> &ordering);

  // Resets a bunch of temporary variables after the reduce walks
  void Reset();

  // Walks the tree and prints it's structure to |out|.  P-nodes are
  // represented as ( ), Q-nodes as [ ], and leafs by their integer id. Used
  // primarily for debugging purposes
  void Print(string *out) const;

  void Identify() const;
};

// Q-Nodes have an unusual structure that makes iterating over their children
// slightly tricky.  This class makes the iterating much simpler.
//
// A Q-Node has two children whose ordering is defined strictly, but can be
// reversed, ie: (1,2,3,4) or (4,3,2,1) but not (2,1,4,3).  The Q-Node itself
// has only unordered pointers to the 1 or 2 children nodes on the ends of this
// list.  Each child contains pointers to the 1 or two siblings on either side
// of itself, but knows nothing of the ordering.
//
// Usage:
//   for (QNodeChildrenIterator it(candidate_node); !it.IsDone(); it.Next()) {
//     Process(it.Current());
//   }
class QNodeChildrenIterator {
 public:
  // Creates an iterator of the children of |parent| optionally forcing the
  // iteration to start on the |begin_side|.
  QNodeChildrenIterator(PQNode* parent, PQNode* begin_side=NULL);

  // Returns a pointer to the current PQNode child in the list.
  PQNode* Current();

  // Next advances the current position in the child list.  If |IsDone()|,
  // operation has no effect.
  void Next();

  // Resets the iterator to the beginning of the list of children, optionally
  // forcing the iteration to start on the |begin_side|.
  void Reset(PQNode* begin_side=NULL);

  // Indicate whether or not all children have been looped through.  The initial
  // list order (forward or reverse) is consistent during the life of the
  // object.
  bool IsDone();

 private:
  // Next() helper method to deal with pseudonodes.
  void NextPseudoNodeSibling();
  PQNode* parent_;
  PQNode* current_;
  PQNode* next_;
  PQNode* prev_;
};

#endif
