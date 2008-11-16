// See PQNode.h

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
  ClearImmediateSiblings();
  ForgetChildren();

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
      current->AddImmediateSibling(last);
      last->AddImmediateSibling(current);
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
  } else {
    if (!last && 2 == ImmediateSiblingCount()) // occurs at edge of pseudonode.
        return immediate_siblings_[1];
    return immediate_siblings_[0];
  }
}

void PQNode::ReplaceChild(PQNode* old_child, PQNode* new_child) {
  if (type_ == pnode) {
    ReplaceCircularLink(old_child, new_child);
  } else { // qnode
    for (int i = 0; i < 2 && old_child->immediate_siblings_[i]; ++i) {
      PQNode *sibling = old_child->immediate_siblings_[i];
      sibling->ReplaceImmediateSibling(old_child, new_child);
    }
    ReplaceEndmostChild(old_child, new_child);
  }
  new_child->parent_ = old_child->parent_;
  if (new_child->label_ == partial)
    new_child->parent_->partial_children_.insert(new_child);
  if (new_child->label_ == full)
    new_child->parent_->full_children_.insert(new_child);
}
  
// Removes this node from a q-parent and puts toInsert in it's place
void PQNode::SwapQ(PQNode *toInsert) {
  toInsert->pseudochild_ = pseudochild_;
  toInsert->ClearImmediateSiblings();
  for (int i = 0; i < 2; ++i) {
    if (parent_->endmost_children_[i] == this)
      parent_->endmost_children_[i] = toInsert;
    if (immediate_siblings_[i])
      immediate_siblings_[i]->ReplaceImmediateSibling(this, toInsert);
  }
  ClearImmediateSiblings();
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
  ClearImmediateSiblings();
  ForgetChildren();
}
  
PQNode::PQNode() {
  pseudonode_ = false;
  parent_ = NULL;
  label_ = empty;
  mark_ = unmarked;
  pertinent_child_count = 0;
  pertinent_leaf_count = 0;
  ClearImmediateSiblings();
  ForgetChildren();
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
  for (int i = 0; i < 2; ++i)
    if (endmost_children_[i] && endmost_children_[i]->label_ == label)
      return endmost_children_[i];
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
  int null_idx = ImmediateSiblingCount();
  assert(null_idx < 2);
  immediate_siblings_[null_idx] = sibling;
}

void PQNode::RemoveImmediateSibling(PQNode *sibling) {
  if (immediate_siblings_[0] == sibling) {
    immediate_siblings_[0] = immediate_siblings_[1];
    immediate_siblings_[1] = NULL;
  } else if (immediate_siblings_[1] == sibling) {
    immediate_siblings_[1] = NULL;
  } else {
    assert(false);
  }
}

void PQNode::ClearImmediateSiblings() {
  for (int i = 0; i < 2; ++i)
    immediate_siblings_[i] = NULL;
}

int PQNode::ImmediateSiblingCount() {
  int count = 0;
  for (int i = 0; i < 2 && immediate_siblings_[i]; ++i)
    count ++;
  return count;
}

void PQNode::ReplaceEndmostChild(PQNode* old_child, PQNode* new_child) {
  for (int i = 0; i < 2; ++i) {
    if (endmost_children_[i] == old_child) {
      endmost_children_[i] = new_child;
      return;
    }
  }
}

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

bool PQNode::ConsecutiveFullPartialChildren() {
  // Trivial Case:
  if (full_children_.size() + partial_children_.size() <= 1)
    return true;
  // The strategy here is to count the number of each label of the siblings of
  // all of the full and partial children and see if the counts are correct.
  map<PQNode_labels, int> counts;
  for(set<PQNode*>::iterator it = full_children_.begin();
      it != full_children_.end(); ++it) {
    for (int i = 0; i < 2 && (*it)->immediate_siblings_[i]; ++i)
      counts[(*it)->immediate_siblings_[i]->label_]++;
  }
  for(set<PQNode*>::iterator it = partial_children_.begin();
      it != partial_children_.end(); ++it) {
    for (int i = 0; i < 2 && (*it)->immediate_siblings_[i]; ++i)
      counts[(*it)->immediate_siblings_[i]->label_]++;
  }
  if (counts[partial] != partial_children_.size())
    return false;
  // Depending on how many partials there are, most full children will get
  // counted twice.
  if (counts[full] != (full_children_.size() * 2) - (2 - counts[partial]))
    return false;
  return true;
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
    next_ = current_->immediate_siblings_[0];
    if (next_ == prev_)
      next_ = current_->immediate_siblings_[1];
  }
}

bool QNodeChildrenIterator::IsDone() {
  return current_ == NULL;
}
