// See PQNode.h

#include <assert.h>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <cstdio>

#include "pqnode.h"

using namespace std;

int PQNode::ChildCount() {
  return circular_link_.size();
}

PQNode* PQNode::CopyAsChild(const PQNode& to_copy) {
  PQNode* temp = new PQNode(to_copy);
  temp->parent_ = this;
  return temp;
}

PQNode::PQNode(const PQNode& to_copy) {
  Copy(to_copy);
}

void PQNode::Copy(const PQNode& to_copy) {
  // Copy the easy stuff
  leaf_value            = to_copy.leaf_value;
  pertinent_leaf_count  = to_copy.pertinent_leaf_count;
  pertinent_child_count = to_copy.pertinent_child_count;
  type_                  = to_copy.type_;
  mark_                  = to_copy.mark_;
  label_                 = to_copy.label_;
  pseudonode_            = to_copy.pseudonode_;
  pseudochild_           = to_copy.pseudochild_;

  // Make sure that these are unset initially
  parent_ = NULL;
  partial_children_.clear();
  full_children_.clear();
  circular_link_.clear();
  for (int i = 0; i < 2; ++i) {
    endmost_children_[i] = NULL;
    immediate_siblings_[i] = NULL;
  }

  // Copy the nodes in circular link for pnodes.
  // If it is not a pnode, it will be empty, so this will be a no-op.
  for (list<PQNode*>::const_iterator i = to_copy.circular_link_.begin();
      i != to_copy.circular_link_.end(); i++)
    circular_link_.push_back(CopyAsChild(**i));

  // Copy the sibling chain for qnodes
  if (type_ == qnode) {
    PQNode *current, *last;
    // Pointers to nodes we are going to copy
    PQNode *curCopy, *lastCopy, *nextCopy;  
    endmost_children_[0] = CopyAsChild(*to_copy.endmost_children_[0]);
    curCopy = to_copy.endmost_children_[0];
    lastCopy = NULL;
    last = endmost_children_[0];
    
    // Get all the intermediate children
    nextCopy = curCopy->QNextChild(lastCopy);
    while (nextCopy != NULL) {
      lastCopy = curCopy;
      curCopy  = nextCopy;
      current  = CopyAsChild(*curCopy);
      assert(current->ImmediateSiblingCount() == 0);
      assert(last->ImmediateSiblingCount() == 0);
      current->immediate_siblings_[0] = last;
      last->immediate_siblings_[0] = current;
      last = current;
      nextCopy = curCopy->QNextChild(lastCopy);
    }

    // Now set our last endmost_children_ pointer to our last child
    endmost_children_[1] = current;
  }
}

PQNode& PQNode::operator=(const PQNode& to_copy) {
  // Check for self copy
  if (&to_copy == this)
    return *this;
  Copy(to_copy);
  // Return ourself for chaining.
  return *this;
}

void PQNode::LabelAsFull() {
  label_ = full;
  if (parent_)
    parent_->full_children_.insert(this);
}

// Return the next child in the immediate_siblings_ chain given a last pointer.
// If last pointer is null, will return the first sibling.
PQNode* PQNode::QNextChild(PQNode *last) {
  if (immediate_siblings_[0] == last) {
    return immediate_siblings_[1];
  } else if (last == NULL && ImmediateSiblingCount() == 2) {
    // If |last| is not either sibling, we are on the edge of a pseudonode.
    //TODO: Just return this
	PQNode* sibling = ImmediateSiblingWithoutLabel(empty);
	assert(sibling);
	return sibling;
  } else {
    return immediate_siblings_[0];
  }
}
  
// Removes this node from a q-parent and puts toInsert in it's place
void PQNode::SwapQ(PQNode *toInsert) {
  toInsert->pseudochild_ = pseudochild_;
  for (int i = 0; i < 2; ++i) {
    if (parent_->endmost_children_[i] == this)
      parent_->endmost_children_[i] = toInsert;

    if (immediate_siblings_[i]) {
      toInsert->immediate_siblings_[i] = immediate_siblings_[i];
      immediate_siblings_[i]->ReplaceImmediateSibling(this, toInsert);
      immediate_siblings_[i] = NULL;
    }
  }
  parent_ = NULL;
}
  
PQNode::PQNode(int value) {
  leaf_value             = value;
  type_                  = leaf;
  parent_                = NULL;
  label_                 = empty;
  mark_                  = unmarked;
  pertinent_child_count  = 0;
  pertinent_leaf_count   = 0;
  for (int i = 0; i < 2; ++i) {
    endmost_children_[i]   = NULL;
    immediate_siblings_[i] = NULL;
  }
}
  
PQNode::PQNode() {
  pseudonode_ = false;
  parent_ = NULL;
  label_ = empty;
  mark_ = unmarked;
  pertinent_child_count = 0;
  pertinent_leaf_count = 0;
  for (int i = 0; i < 2; ++i) {
    endmost_children_[i]   = NULL;
    immediate_siblings_[i] = NULL;
  }
}

PQNode::~PQNode() {
  if (type_ == qnode) {
    PQNode *last     = NULL;
    PQNode *current  = endmost_children_[0];
    while(current) {
      PQNode *next = current->QNextChild(last);
      delete last;
      last    = current;
      current = next;
    }
    delete last;
  } else if (type_ == pnode) {  
    for (list<PQNode*>::iterator i = circular_link_.begin();
         i != circular_link_.end(); i++)
      delete *i;
    circular_link_.clear();
  }
}

PQNode* PQNode::CircularChildWithLabel(PQNode_labels label) {
  for (list<PQNode*>::iterator i = circular_link_.begin();
       i != circular_link_.end(); i++) {
    if ((*i)->label_ == label)
      return *i;
  }
  return NULL;
}


PQNode* PQNode::EndmostChildWithLabel(PQNode_labels label) {
  for (int i = 0; i < 2; ++i) {
    if (endmost_children_[i] && endmost_children_[i]->label_ == label)
      return endmost_children_[i];
  }
  return NULL;
}

PQNode* PQNode::ImmediateSiblingWithLabel(PQNode_labels label) {
  for (int i = 0; i < 2 && immediate_siblings_[i]; ++i)
    if (immediate_siblings_[i]->label_ == label)
      return immediate_siblings_[i];
  return NULL;
}

PQNode* PQNode::ImmediateSiblingWithoutLabel(PQNode_labels label) {
  for (int i = 0; i < 2 && immediate_siblings_[i]; ++i)
    if (immediate_siblings_[i]->label_ != label)
      return immediate_siblings_[i];
  return NULL;
}

void PQNode::AddImmediateSibling(PQNode *sibling) {
  for (int i = 0; i < 2; ++i) {
    if (!immediate_siblings_[i]) {
      immediate_siblings_[i] = sibling;
      return;
    }
  }
  assert(false);
}

int PQNode::ImmediateSiblingCount() {
  for (int i = 0; i < 2; ++i) {
    if (!immediate_siblings_[i])
      return i;
  }
  return 2;
}

void PQNode::ReplaceEndmostChild(PQNode* old_child, PQNode* new_child) {
  for (int i = 0; i < 2; ++i) {
    if (endmost_children_[i] == old_child) {
      endmost_children_[i] = new_child;
      return;
    }
  }
}

// TODO: Can this method be removed if the immediate siblings are ordered?
void PQNode::ReplaceImmediateSibling(PQNode* old_child, PQNode* new_child) {
  for (int i = 0; i < 2 && immediate_siblings_[i]; ++i)
    if (immediate_siblings_[i] == old_child)
      immediate_siblings_[i] = new_child;
  new_child->immediate_siblings_[new_child->ImmediateSiblingCount()] = this;
}

void PQNode::ReplacePartialChild(PQNode* old_child, PQNode* new_child) {
  new_child->parent_ = this;
  partial_children_.insert(new_child);
  partial_children_.erase(old_child);
  if (type_ == pnode) {
    circular_link_.remove(old_child);
    circular_link_.push_back(new_child);
  } else {
    old_child->SwapQ(new_child);
  }
}

void PQNode::ForgetChildren() {
  for (int i = 0; i < 2; ++i)
    endmost_children_[i] = NULL;
}  

void PQNode::MoveFullChildren(PQNode* new_node) {
  for (set<PQNode*>::iterator i = full_children_.begin();
       i != full_children_.end(); ++i) {
    circular_link_.remove(*i);
    new_node->circular_link_.push_back(*i);
    (*i)->parent_ = new_node;
  }
}

void PQNode::ReplaceCircularLink(PQNode* old_child, PQNode* new_child) {
  circular_link_.remove(old_child);
  circular_link_.push_back(new_child);
}


// FindLeaves, FindFrontier, Reset, and Print are very similar recursive
// functions.  Each is a depth-first walk of the entire tree looking for data
// at the leaves.  
// TODO: Could probably be implemented better using function pointers.
void PQNode::FindLeaves(map<int, PQNode*> &leafAddress) {
  if (type_ == leaf) {
    leafAddress[leaf_value] = this;
  } else if (type_ == pnode) {
    // Recurse by asking each child in circular_link_ to find it's leaves.
    for (list<PQNode*>::iterator i = circular_link_.begin();
         i!=circular_link_.end(); i++)
      (*i)->FindLeaves(leafAddress);
  } else if (type_ == qnode) {
    // Recurse by asking each child in my child list to find it's leaves.
    PQNode *last    = NULL;
    PQNode *current = endmost_children_[0];
    while (current) {
      current->FindLeaves(leafAddress);
      PQNode *next = current->QNextChild(last);
      last    = current;
      current = next;
    }
  }
}
  
void PQNode::FindFrontier(list<int> &ordering) {
  if (type_ == leaf) {
    ordering.push_back(leaf_value);
  } else if (type_ == pnode) {
    for(list<PQNode*>::iterator i = circular_link_.begin();
        i != circular_link_.end();i++)
      (*i)->FindFrontier(ordering);
  } else if (type_ == qnode) {
    PQNode *last    = NULL;
    PQNode *current = endmost_children_[0];
    while (current) {
      current->FindFrontier(ordering);
      PQNode *next = current->QNextChild(last);
      last    = current;
      current = next;
    }
  }
}
    
// Resets a bunch of temporary variables after the reduce walks
void PQNode::Reset() {
  if (type_ == pnode) {
    for (list<PQNode*>::iterator i = circular_link_.begin();
         i != circular_link_.end(); i++)
      (*i)->Reset();
  } else if(type_ == qnode) {
    PQNode *last    = NULL;
    PQNode *current = endmost_children_[0];
    while (current) {
      current->Reset();
      PQNode *next = current->QNextChild(last);
      last    = current;
      current = next;
    }
  }
  
  full_children_.clear();
  partial_children_.clear();
  label_                 = empty;
  mark_                  = unmarked;
  pertinent_child_count = 0;
  pertinent_leaf_count  = 0;
  pseudochild_           = false;
  pseudonode_            = false;
}

// Walks the tree from the top and prints the tree structure to the string out.
// Used primarily for debugging purposes.
void PQNode::Print(string *out) {
  if (type_ == leaf) {
    char value_str[10];
    sprintf(value_str, "%d", leaf_value);
    *out += value_str;
  } else if (type_ == pnode) {
    *out += "(";
    for (list<PQNode*>::iterator i = circular_link_.begin();
         i != circular_link_.end(); i++) {
      (*i)->Print(out);
      // Add a space if there are more elements remaining.
      if (++i != circular_link_.end())
        *out += " ";
      --i;
    }
    *out += ")";
  } else if (type_ == qnode) {
    *out += "[";
    PQNode *last     = NULL;
    PQNode *current  = endmost_children_[0];
    while (current) {
      current->Print(out);
      PQNode *next = current->QNextChild(last);
      last     = current;
      current  = next;
      if (current)
        *out += " ";
    }
    *out += "]";
  }
}

/***** QNodeChildrenIterator class *****/

QNodeChildrenIterator::QNodeChildrenIterator(PQNode* parent,
					     PQNode* begin_side) {
  parent_ = parent;
  Reset(begin_side);
}

void QNodeChildrenIterator::Reset(PQNode* begin_side) {
  current_ = parent_->endmost_children_[0];
  if (begin_side)
    current_ = begin_side;
  prev_ = NULL;
  next_ = current_->immediate_siblings_[0];
}

PQNode* QNodeChildrenIterator::Current() {
  return current_;
}

void QNodeChildrenIterator::NextPseudoNodeSibling() {
  // This should only be called from the first Next() call after Reset() and
  // only if the first subnode has two immediate siblings.  We want to advance
  // our iterator to the non-empty sibling of |current_|
  prev_ = current_;
  current_ = current_->ImmediateSiblingWithLabel(PQNode::full);
  if (!current_)
    current_ = current_->ImmediateSiblingWithLabel(PQNode::partial);
}

void QNodeChildrenIterator::Next() {
  // If the first child has 2 immediate siblings, then we are on
  // the edge of a pseudonode.
  if (IsDone())
    return;
  if (prev_ == NULL && current_->ImmediateSiblingCount() == 2) {
    NextPseudoNodeSibling();
  } else {
    prev_ = current_;
    current_ = next_;
  }

  if (current_) {
    if (1 == current_->ImmediateSiblingCount()) {
      next_ = NULL;
    } if (current_->immediate_siblings_[0] != prev_) {
      next_ = current_->immediate_siblings_[0];
    } else {
      next_ = current_->immediate_siblings_[1];
    }
  }
}

bool QNodeChildrenIterator::IsDone() {
  return current_ == NULL;
}
