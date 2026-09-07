// Structural invariant checks for PQTree, used by tests. See
// PQTree::CheckInvariants in pqtree.h.
//
// The invariants checked, beyond basic tree shape, are the ones the reduction
// templates depend on:
//
//  - Every child of a P-node has parent_ set to that P-node and no siblings.
//  - Both endmost children of a Q-node have parent_ set to that Q-node.
//  - Interior children of a Q-node have parent_ either NULL or set to that
//    Q-node. Bubble sets it during a reduction and nothing needs to clear it,
//    but it must never point anywhere else, which is what a dangling pointer
//    to a deleted node would look like.
//  - Sibling links are symmetric and the chain from one endmost child ends
//    at the other.
//  - No node is reachable twice, and the leaf index matches the leaves found.

#include <map>
#include <set>
#include <sstream>
#include <string>

#include "pqnode.h"
#include "pqtree.h"

struct PQTree::InvariantWalker {
  std::set<const PQNode*> seen;
  std::map<int, const PQNode*> leaves;
  std::string failure;

  bool Fail(const std::string& what) {
    if (failure.empty()) failure = what;
    return false;
  }

  bool CheckNode(const PQNode* node) {
    if (!seen.insert(node).second) return Fail("node reachable twice");
    switch (node->type_) {
      case PQNode::leaf:
        return CheckLeaf(node);
      case PQNode::pnode:
        return CheckPNode(node);
      case PQNode::qnode:
        return CheckQNode(node);
    }
    return Fail("node with unknown type");
  }

  bool CheckLeaf(const PQNode* node) {
    if (!node->circular_link_.empty() || node->endmost_children_[0] ||
        node->endmost_children_[1])
      return Fail("leaf has children");
    if (!leaves.insert(std::make_pair(node->leaf_value_, node)).second)
      return Fail("duplicate leaf value");
    return true;
  }

  bool CheckPNode(const PQNode* node) {
    if (node->circular_link_.size() < 2)
      return Fail("P-node with fewer than two children");
    for (std::list<PQNode*>::const_iterator i = node->circular_link_.begin();
         i != node->circular_link_.end(); ++i) {
      const PQNode* child = *i;
      if (child->parent_ != node) return Fail("P-node child has wrong parent");
      if (child->immediate_siblings_[0] || child->immediate_siblings_[1])
        return Fail("P-node child has siblings");
      if (!CheckNode(child)) return false;
    }
    return true;
  }

  bool CheckQNode(const PQNode* node) {
    const PQNode* first = node->endmost_children_[0];
    const PQNode* end = node->endmost_children_[1];
    if (!first || !end) return Fail("Q-node missing an endmost child");
    if (first->parent_ != node || end->parent_ != node)
      return Fail("Q-node endmost child has wrong parent");

    int count = 0;
    const PQNode* last = NULL;
    const PQNode* current = first;
    while (current) {
      ++count;
      const bool endmost = (current == first || current == end);
      if (endmost) {
        if (current->ImmediateSiblingCount() > 1)
          return Fail("Q-node endmost child has two siblings");
      } else {
        if (current->ImmediateSiblingCount() != 2)
          return Fail("Q-node interior child does not have two siblings");
        if (current->parent_ && current->parent_ != node)
          return Fail("Q-node interior child points at a foreign parent");
      }
      if (last && !HasSibling(current, last))
        return Fail("Q-node sibling links are not symmetric");
      if (!CheckNode(current)) return false;
      if (current == end) break;
      const PQNode* next = current->QNextChild(const_cast<PQNode*>(last));
      last = current;
      current = next;
    }
    if (current != end) return Fail("Q-node chain does not reach the far end");
    if (count < 2) return Fail("Q-node with fewer than two children");
    return true;
  }

  static bool HasSibling(const PQNode* node, const PQNode* sibling) {
    return node->immediate_siblings_[0] == sibling ||
           node->immediate_siblings_[1] == sibling;
  }
};

bool PQTree::CheckInvariants(string* failure) const {
  InvariantWalker walk;
  bool ok = true;
  if (!root_) {
    ok = walk.Fail("tree has no root");
  } else if (root_->parent_ || root_->ImmediateSiblingCount() != 0) {
    ok = walk.Fail("root has a parent or siblings");
  } else {
    ok = walk.CheckNode(root_);
  }
  if (ok && walk.leaves.size() != leaf_address_.size()) {
    ok = walk.Fail("leaf index size does not match leaves in tree");
  }
  if (ok) {
    for (std::map<int, PQNode*>::const_iterator i = leaf_address_.begin();
         i != leaf_address_.end(); ++i) {
      std::map<int, const PQNode*>::const_iterator found =
          walk.leaves.find(i->first);
      if (found == walk.leaves.end() || found->second != i->second) {
        ok = walk.Fail("leaf index entry does not match the tree");
        break;
      }
    }
  }
  if (!ok && failure) {
    std::ostringstream out;
    out << walk.failure << " in tree " << Print();
    *failure = out.str();
  }
  return ok;
}
