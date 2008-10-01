#include "pqtree.h"

#include <assert.h>

PQTree::PQTree(const PQTree& to_copy) {
  CopyFrom(to_copy);
}

PQTree& PQTree::operator=(const PQTree& to_copy) {
  if (&to_copy != this)
    CopyFrom(to_copy);
  return *this;
}

void PQTree::CopyFrom(const PQTree& to_copy) {
  root_                 = new PQNode(*to_copy.root_);
  block_count_   = to_copy.block_count_;
  blocked_nodes_ = to_copy.blocked_nodes_;
  invalid_         = to_copy.invalid_;
  off_the_top_   = to_copy.off_the_top_;
  pseudonode_         = NULL;
  reductions_         = to_copy.reductions_;

  leaf_address_.clear();
  root_->FindLeaves(leaf_address_);
}

int PQTree::UnblockSiblings(PQNode* candidate_node, PQNode* parent, PQNode* last) {
  int unblocked_count = 0;
  if (candidate_node->mark_ == PQNode::blocked) {
    // Unblock the current sibling and set it's parent to |parent|.
    unblocked_count++;
    candidate_node->mark_ = PQNode::unblocked;
    candidate_node->parent_ = parent;

    // Unblock adjacent siblings recursively
    for (set<PQNode *>::iterator i = candidate_node->immediate_siblings_.begin();
        i != candidate_node->immediate_siblings_.end(); i++) {
      if (*i != last)
        unblocked_count += UnblockSiblings(*i, parent, candidate_node);
    }
  }
  return unblocked_count;
}

// All the different templates for matching a reduce are below.  The template
// has a letter describing which type of node it refers to and a number
// indicating the index of the template for that letter.  These are the same
// indices found in the Booth & Lueker paper.
//
// Each template method is called in order within the reduce phase.  The
// template method determines if the Node passed to it is of the form that the
// template method can reduce.  If so, the method processes the Node and returns
// true.  If not, the method makes no changes and returns false.  Methods
// should be attempted in order until one of them returns true and then the
// remainder of the methods should be skipped.  The template ordering is:
// L1, P1, P2, P3, P4, P5, P6, Q1, Q2, Q3

bool PQTree::TemplateL1(PQNode* candidate_node) {
  // Check against the pattern: must be a leaf
  if (candidate_node->type_ != PQNode::leaf)
    return false;

  candidate_node->LabelAsFull();
  return true;
}

bool PQTree::TemplateQ1(PQNode* candidate_node) {
  if (candidate_node->type_ != PQNode::qnode)
    return false;

  // Loop over all of our children.
  // If all of them are full, so are we, mark as full return true.
  // If any of them are empty or partial, return false.
  for (QNodeChildrenIterator it(candidate_node); !it.IsDone(); it.Next()) {
    if (it.Current()->label_ != PQNode::full)
      return false;
  }

  candidate_node->LabelAsFull();
  return true;
}

bool PQTree::TemplateQ2(PQNode* candidate_node) {
  // Check against the pattern
  if (candidate_node->type_ != PQNode::qnode ||
      candidate_node->pseudonode_ ||
      candidate_node->partial_children_.size() > 1)
    return false;

  // Q2 must either have exactly 1 full end child or >0 partial end child.
  if (!candidate_node->full_children_.empty()) {
    int num_full_end_children = 0;
    // full_end_child is the unique full element in our endmost_children_.
    PQNode* full_end_child;
    for (int i = 0; i < 2; ++i) {
      if (candidate_node->endmost_children_[i]->label_ == PQNode::full) {
        full_end_child = candidate_node->endmost_children_[i];
        num_full_end_children++;
      }
    }
    // If not exactly 1 full endmost children, our pattern does not match.
    if (num_full_end_children != 1)
      return false;

    // All the full children must be consecutive.
    PQNode* full_child = full_end_child;
    QNodeChildrenIterator iter(candidate_node, /*begin_side=*/full_end_child);
    for (int i = 0; i < candidate_node->full_children_.size(); ++i) {
      if (iter.Current()->label_ != PQNode::full)
        return false;
      iter.Next();
    }

    // If there is a partial child, it must also be consecutive with the full
    // children.
    if (iter.Current()->label_ != PQNode::partial &&
        candidate_node->partial_children_.size() == 1)
      return false;
  } else {
    // Or that the partial child is on the side
    if (!candidate_node->EndmostChildWithLabel(PQNode::partial))
      return false;
  }

  // Pattern matches.  If exists, merge partial child into the candidate_node.
  if (candidate_node->partial_children_.size() > 0) {
    PQNode* to_merge = *candidate_node->partial_children_.begin();

    PQNode* full_child = to_merge->EndmostChildWithLabel(PQNode::full);
    PQNode* empty_child = to_merge->EndmostChildWithLabel(PQNode::empty);

    PQNode* full_sibling = to_merge->ImmediateSiblingWithLabel(PQNode::full);
    PQNode* empty_sibling = to_merge->ImmediateSiblingWithLabel(PQNode::empty);

    // If |partial_child| has a full immediate sibling
    if (full_sibling) {
      full_sibling->ReplaceImmediateSibling(to_merge, full_child);
    } else {
      candidate_node->ReplaceEndmostChild(to_merge, full_child);
      full_child->parent_ = candidate_node;
    }

    // If node |to_merge| has an empty immediate sibling
    if (empty_sibling) {
      empty_sibling->ReplaceImmediateSibling(to_merge, empty_child);
    } else {
      candidate_node->ReplaceEndmostChild(to_merge, empty_child);
      empty_child->parent_ = candidate_node;
    }

    to_merge->ForgetChildren();
    delete to_merge;
  }
  candidate_node->label_ = PQNode::partial;
  if (candidate_node->parent_)
    candidate_node->parent_->partial_children_.insert(candidate_node);
  return true;
}

bool PQTree::TemplateQ3(PQNode* candidate_node) {
  // Check against the pattern
  if (candidate_node->type_ != PQNode::qnode ||
      candidate_node->partial_children_.size() > 2)
    return false;

  // Handle a special case
  if (candidate_node->pseudonode_ &&
      candidate_node->endmost_children_[1] == NULL)
    return true;

  // For Q3 to match, it must have at most one run of consecutive full or
  // partial children, with any partial children being on the ends of the run.
  bool run_started = false;
  bool run_finished = false;
  for (QNodeChildrenIterator it(candidate_node); !it.IsDone(); it.Next()) {
    if (it.Current()->label_ == PQNode::full) {
      if (run_finished)
        return false;
      run_started = true;
    } else if (it.Current()->label_ == PQNode::empty) {
      if (run_started)
        run_finished = true;
    } else if (it.Current()->label_ == PQNode::partial) {
      if (run_finished)
        return false;
      if (run_started)
        run_finished = true;
      run_started = true;
    }
  }
  cout << "Q3" << endl;

  for (set<PQNode*>::iterator j = candidate_node->partial_children_.begin();
       j != candidate_node->partial_children_.end(); j++) {
    PQNode* to_merge = *j;
    PQNode* empty_child = to_merge->EndmostChildWithLabel(PQNode::empty);
    PQNode* full_child = to_merge->EndmostChildWithLabel(PQNode::full);

    // TODO: What exactly is CS's role?  How can it be better named?
    PQNode* CS;
    // Handle the sides where the child has immediate siblings.
    for (set<PQNode *>::iterator i = to_merge->immediate_siblings_.begin();
         i != to_merge->immediate_siblings_.end(); ++i) {
      PQNode* sibling = *i;
      if (sibling->label_ == PQNode::empty) {
        sibling->ReplaceImmediateSibling(to_merge, empty_child);
        CS = full_child;
      } else {  // |sibling| is either full or partial, we dont care which.
        sibling->ReplaceImmediateSibling(to_merge, full_child);
        CS = empty_child;
      }
    }
    if (candidate_node->pseudonode_)
      CS = empty_child;
    // Handle the case where |to_merge| had only one immediate sibling.
    bool has_only_one_sibling = (to_merge->immediate_siblings_.size() == 1);
    if (has_only_one_sibling || candidate_node->pseudonode_) {
      CS->parent_ = candidate_node;
      candidate_node->ReplaceEndmostChild(to_merge, CS);
    }

    to_merge->ForgetChildren();
    delete to_merge;
  }
  return true;
}
/* A note here.  An error in the Booth and Leuker Algorithm fails to consider
 * the case where a P-node is full, is the pertinent root, and is not an endmost
 * child of a q-node.  In this case, we need to know that the P-node is a
 * pertinent root and not try to update its whose pointer is possibly
 * invalid
 */
bool PQTree::TemplateP1(PQNode* candidate_node, bool is_reduction_root) {
    // Check against the pattern
    if (candidate_node->type_ != PQNode::pnode ||
        candidate_node->full_children_.size() != candidate_node->ChildCount())
      return false;

    candidate_node->label_ = PQNode::full;
    if (!is_reduction_root)
      candidate_node->parent_->full_children_.insert(candidate_node);
    return true;
}

bool PQTree::TemplateP2(PQNode* candidate_node) {
  // Check against the pattern
  if (candidate_node->type_ != PQNode::pnode ||
      candidate_node->partial_children_.size() != 0)
    return false;

  // Move candidate_node's full children into their own P-node
  if (candidate_node->full_children_.size() >= 2) {
    PQNode* new_pnode = new PQNode;
    new_pnode->type_ = PQNode::pnode;
    new_pnode->parent_ = candidate_node;
    candidate_node->MoveFullChildren(new_pnode);
    candidate_node->circular_link_.push_back(new_pnode);
  }
  // Mark the root partial
  candidate_node->label_ = PQNode::partial;

  return true;
}
bool PQTree::TemplateP3(PQNode* candidate_node) {
  //check against the pattern
  if (candidate_node->type_ != PQNode::pnode ||
      !candidate_node->partial_children_.empty())
    return false;

  // Set up the p_node on the full child side.
  PQNode* full_child;
  if (candidate_node->full_children_.size() == 1) {
    full_child = *candidate_node->full_children_.begin();
    candidate_node->circular_link_.remove(full_child);
  } else {
    full_child = new PQNode;
    full_child->type_ = PQNode::pnode;
    full_child->label_ = PQNode::full;
    candidate_node->MoveFullChildren(full_child);
  }

  PQNode* new_qnode = new PQNode;
  new_qnode->type_ = PQNode::qnode;
  candidate_node->parent_->ReplacePartialChild(candidate_node, new_qnode);

  full_child->parent_ = new_qnode;
  new_qnode->endmost_children_[0] = full_child;
  new_qnode->full_children_.insert(full_child);

  // Now set up the p-node on the empty child side.
  PQNode* empty_child;
  if (candidate_node->circular_link_.size() == 1) {
    empty_child = *candidate_node->circular_link_.begin();

    // We want to delete |candidate_node|, but not it's children.
    candidate_node->circular_link_.clear();
    delete candidate_node;
  } else {
    empty_child = candidate_node;
  }
  empty_child->parent_ = new_qnode;
  empty_child->label_ = PQNode::empty;
  new_qnode->endmost_children_[1] = empty_child;

  //update the immediate siblings links
  empty_child->immediate_siblings_.clear();
  empty_child->immediate_siblings_.insert(full_child);
  full_child->immediate_siblings_.clear();
  full_child->immediate_siblings_.insert(empty_child);

  new_qnode->label_ = PQNode::partial;

  return true;
}

bool PQTree::TemplateP4(PQNode* candidate_node) {
  if (candidate_node->type_ != PQNode::pnode ||
      candidate_node->partial_children_.size() != 1)
    return false;

  PQNode* partial_qnode = *candidate_node->partial_children_.begin();
  assert(partial_qnode->type_ == PQNode::qnode);
  PQNode* empty_child = partial_qnode->EndmostChildWithLabel(PQNode::empty);
  PQNode* full_child = partial_qnode->EndmostChildWithLabel(PQNode::full);
  PQNode* empty_sibling = candidate_node->CircularChildWithLabel(PQNode::empty);

  if (!empty_child || !full_child)
    return false;

  // Move the full children of |candidate_node| to children of |partial_qnode|.
  if (!candidate_node->full_children_.empty()) {
    PQNode *full_children_root;
    if (candidate_node->full_children_.size() == 1) {
      full_children_root = *candidate_node->full_children_.begin();
      candidate_node->circular_link_.remove(full_children_root);
    } else {
      full_children_root = new PQNode;
      full_children_root->type_ = PQNode::pnode;
      full_children_root->label_ = PQNode::full;
      candidate_node->MoveFullChildren(full_children_root);
    }
    full_children_root->parent_ = partial_qnode;
    partial_qnode->ReplaceEndmostChild(full_child, full_children_root);
    partial_qnode->full_children_.insert(full_children_root);
    full_child->immediate_siblings_.insert(full_children_root);
    full_children_root->immediate_siblings_.insert(full_child);
  }

  // If |candidate_node| now only has one child, get rid of |candidate_node|.
  if (candidate_node->circular_link_.size() == 1) {
    PQNode* the_parent = candidate_node->parent_;
    partial_qnode->parent_ = candidate_node->parent_;
    if (the_parent != NULL) {  // Parent is root of tree
      if (candidate_node->immediate_siblings_.empty()) {  // Is p-node?
        the_parent->circular_link_.remove(candidate_node);
        the_parent->circular_link_.push_back(partial_qnode);
      } else { // Is Q-node?
        // Update the immediate siblings list by removing candidate_node and adding partial_qnode
        for (set<PQNode*>::iterator i =
                 candidate_node->immediate_siblings_.begin();
             i != candidate_node->immediate_siblings_.end(); ++i) {
          (*i)->ReplaceImmediateSibling(candidate_node, partial_qnode);
        }
        if (candidate_node->immediate_siblings_.size() == 1)
          the_parent->ReplaceEndmostChild(candidate_node, partial_qnode);
      }
    } else {
      root_ = partial_qnode;
    }
  }
  return true;
}

bool PQTree::TemplateP5(PQNode* candidate_node) {
  //check against the pattern
  if (candidate_node->type_ != PQNode::pnode ||
      candidate_node->partial_children_.size() != 1)
    return false;

  PQNode* partial_qnode = *candidate_node->partial_children_.begin();
  assert(partial_qnode->type_ == PQNode::qnode);
  PQNode* empty_child = partial_qnode->EndmostChildWithLabel(PQNode::empty);
  PQNode* full_child = partial_qnode->EndmostChildWithLabel(PQNode::full);
  PQNode* empty_sibling = candidate_node->CircularChildWithLabel(PQNode::empty);

  if (!empty_child || !full_child)
    return false;

  // |partial_qnode| will become the root of the pertinent subtree after the
  // replacement.
  PQNode* the_parent = candidate_node->parent_;
  partial_qnode->parent_ = candidate_node->parent_;
  partial_qnode->pertinent_leaf_count = candidate_node->pertinent_leaf_count;
  partial_qnode->label_ = PQNode::partial;
  the_parent->partial_children_.insert(partial_qnode);
  // remove partial_qnode from candidate_node's list of children
  candidate_node->circular_link_.remove(partial_qnode);
  candidate_node->partial_children_.erase(partial_qnode);

  if (candidate_node->immediate_siblings_.size() == 0) {
    the_parent->ReplaceCircularLink(candidate_node, partial_qnode);
  } else {
    for (set<PQNode *>::iterator i=candidate_node->immediate_siblings_.begin();
         i!=candidate_node->immediate_siblings_.end(); i++)
      (*i)->ReplaceImmediateSibling(candidate_node, partial_qnode);
    the_parent->ReplaceEndmostChild(candidate_node, partial_qnode);
  }

  // Move the full children of |candidate_node| to children of |partial_qnode|.
  if (candidate_node->full_children_.size() > 0) {
    PQNode *full_children_root=NULL;
    if (candidate_node->full_children_.size() == 1) {
      full_children_root = *candidate_node->full_children_.begin();
      candidate_node->circular_link_.remove(full_children_root);
    } else {
      full_children_root = new PQNode;
      full_children_root->type_ = PQNode::pnode;
      full_children_root->label_ = PQNode::full;
      candidate_node->MoveFullChildren(full_children_root);
    }
    candidate_node->full_children_.clear();

    full_children_root->parent_ = partial_qnode;
    full_child->immediate_siblings_.insert(full_children_root);
    full_children_root->immediate_siblings_.insert(full_child);
    partial_qnode->ReplaceEndmostChild(full_child, full_children_root);
  }

  // If candidate_node still has some empty children, insert them
  if (candidate_node->ChildCount()) {
    PQNode *empty_children_root = NULL;
    if (candidate_node->ChildCount() == 1) {
      empty_children_root = empty_sibling;
    } else {
      empty_children_root = candidate_node;
      empty_children_root->label_ = PQNode::empty;
      empty_children_root->immediate_siblings_.clear();
    }
    empty_children_root->parent_ = partial_qnode;
    empty_child->immediate_siblings_.insert(empty_children_root);
    empty_children_root->immediate_siblings_.insert(empty_child);
    partial_qnode->ReplaceEndmostChild(empty_child, empty_children_root);
  }
  if (candidate_node->ChildCount() < 2) {
    // We want to delete candidate_node, but not it's children.
    candidate_node->circular_link_.clear();
    delete candidate_node;
  }

  return true;
}

bool PQTree::TemplateP6(PQNode* candidate_node)
{
  if (candidate_node->type_ != PQNode::pnode ||
      candidate_node->partial_children_.size() != 2)
    return false;

  // TODO: Convert these to an array so we don't have 2 of everything.
  PQNode* partial_qnode1 = *candidate_node->partial_children_.begin();
  PQNode* partial_qnode2 = *(++(candidate_node->partial_children_.begin()));

  PQNode* empty_child1 = partial_qnode1->EndmostChildWithLabel(PQNode::empty);
  PQNode* full_child1 = partial_qnode1->EndmostChildWithLabel(PQNode::full);
  if (!empty_child1 || !full_child1)
    return false;

  PQNode* empty_child2 = partial_qnode2->EndmostChildWithLabel(PQNode::empty);
  PQNode* full_child2 = partial_qnode2->EndmostChildWithLabel(PQNode::full);
  if (!empty_child2 || !full_child2)
    return false;

  // Move the full children of candidate_node to be children of partial_qnode1
  if (!candidate_node->full_children_.empty()) {
    PQNode *full_children_root = NULL;
    if (candidate_node->full_children_.size() == 1) {
      full_children_root = *candidate_node->full_children_.begin();
      candidate_node->circular_link_.remove(full_children_root);
    } else {
      // create full_children_root to be a new p-node
      full_children_root = new PQNode;
      full_children_root->type_ = PQNode::pnode;
      full_children_root->label_ = PQNode::full;
      candidate_node->MoveFullChildren(full_children_root); 
    }
    full_children_root->parent_ = partial_qnode1;
    full_child2->parent_ = partial_qnode1;

    full_child1->immediate_siblings_.insert(full_children_root);
    full_child2->immediate_siblings_.insert(full_children_root);
    full_children_root->immediate_siblings_.insert(full_child1);
    full_children_root->immediate_siblings_.insert(full_child2);
  } else  {
    full_child1->immediate_siblings_.insert(full_child2);
    full_child2->immediate_siblings_.insert(full_child1);
  }
  partial_qnode1->ReplaceEndmostChild(full_child1, empty_child2);
  empty_child2->parent_ = partial_qnode1;

  // We dont need |partial_qnode2| any more
  candidate_node->circular_link_.remove(partial_qnode2);
  partial_qnode2->ForgetChildren();
  delete partial_qnode2;

  // If |candidate_node| now only has one child, get rid of |candidate_node|.
  if (candidate_node->circular_link_.size() == 1) {
    partial_qnode1->parent_ = candidate_node->parent_;
    partial_qnode1->pertinent_leaf_count = candidate_node->pertinent_leaf_count;
    partial_qnode1->label_ = PQNode::partial;

    if (candidate_node->parent_) {
      candidate_node->parent_->partial_children_.insert(partial_qnode1);
      if (candidate_node->parent_->type_ == PQNode::pnode) {
        candidate_node->parent_->ReplaceCircularLink(candidate_node,
                                                     partial_qnode1);
      } else {
        for (set<PQNode *>::iterator i = 
                 candidate_node->immediate_siblings_.begin();
             i != candidate_node->immediate_siblings_.end(); i++) {
          (*i)->ReplaceImmediateSibling(candidate_node, partial_qnode1);
        }
        candidate_node->parent_->ReplaceEndmostChild(candidate_node,
                                                     partial_qnode1);
      }
    }
    else {
      root_ = partial_qnode1;
      partial_qnode1->parent_ = NULL;

      // Delete candidate_node, but not it's children.
      candidate_node->circular_link_.clear();
      delete candidate_node;
    }
  }
  return true;
}

// This procedure is the first pass of the Booth & Leuker PQTree algorithm.
// It processes the pertinent subtree of the PQ-Tree to determine the mark
// of every node in that subtree.  The pseudonode, if created, is returned so
// that it can be deleted at the end of the reduce step.
bool PQTree::Bubble(set<int> reduction_set) {
  queue<PQNode*> q;
  block_count_ = 0;
  blocked_nodes_ = 0;
  off_the_top_ = 0;

  // Stores nodes that aren't unblocked
  set<PQNode*> blocked_list;

  // Insert the set's leaves into the queue
  for (set<int>::iterator i = reduction_set.begin();
       i != reduction_set.end(); ++i) {
    PQNode *temp = leaf_address_[*i];
    assert (temp);
    q.push(temp);
  }

  while (q.size() + block_count_ + off_the_top_ > 1) {
    if (q.empty())
      return false;

    PQNode* candidate_node = q.front();
    q.pop();
    candidate_node->mark_ = PQNode::blocked;

    // Get the set of blocked and PQNode::unblocked siblings
    set<PQNode*> unblocked_siblings;
    set<PQNode*> blocked_siblings;
    for (set<PQNode *>::iterator i = 
	  candidate_node->immediate_siblings_.begin();
        i != candidate_node->immediate_siblings_.end(); ++i) {
      if ((*i)->mark_ == PQNode::blocked) {
        blocked_siblings.insert(*i);
      } else if ((*i)->mark_ == PQNode::unblocked) {
        unblocked_siblings.insert(*i);
      }
    }

    // We can unblock |candidate_node| if any of there conditions is met:
    //  - 1 or more of its immediate siblings is unblocked.
    //  - It has 1 immediate sibling meaning it is a corner child of a q node.
    //  - It has 0 immediate siblings meaning it is a p node.
    if (!unblocked_siblings.empty()) {
      candidate_node->parent_ = (*unblocked_siblings.begin())->parent_;
      candidate_node->mark_ = PQNode::unblocked;
    } else if (candidate_node->immediate_siblings_.size() < 2) {
      candidate_node->mark_ = PQNode::unblocked;
    }

    // If |candidate_node| is unblocked, we can process it.
    if (candidate_node->mark_ == PQNode::unblocked) {
      int list_size = 0;
      if (!blocked_siblings.empty()) {
        candidate_node->mark_ = PQNode::blocked;
        list_size = UnblockSiblings(candidate_node, candidate_node->parent_);
        candidate_node->parent_->pertinent_child_count += list_size - 1;
      }

      if (!candidate_node->parent_) {
        off_the_top_ = 1;
      } else {
        candidate_node->parent_->pertinent_child_count++;
        if (candidate_node->parent_->mark_ == PQNode::unmarked) {
          q.push(candidate_node->parent_);
          candidate_node->parent_->mark_ = PQNode::queued;
        }
      }
      block_count_ -= blocked_siblings.size();
      blocked_nodes_ -= list_size; 
    } else {
      block_count_ += 1 - blocked_siblings.size();
      blocked_nodes_ += 1;
      blocked_list.insert(candidate_node);
    }
  }

  if (block_count_ > 1 || (off_the_top_ == 1 && block_count_ != 0))
    return false;

  int correct_blocked_count = 0;
  for (set<PQNode*>::iterator blocked_node = blocked_list.begin();
      blocked_node != blocked_list.end(); ++blocked_node) {
      if ((*blocked_node)->mark_ == PQNode::blocked)
        correct_blocked_count++;
  }

  // In this case, we have a block that is contained within a Q-node.  We must
  // assign a psuedonode to handle it.
  if (block_count_ == 1 && correct_blocked_count > 1) {
    pseudonode_ = new PQNode;
    pseudonode_->type_ = PQNode::qnode;
    pseudonode_->pseudonode_ = true;
    pseudonode_->pertinent_child_count = 0;

    // Find the blocked nodes and which of those are endmost children.
    int side = 0;
    for (set<PQNode*>::iterator i = blocked_list.begin();
        i != blocked_list.end(); ++i) {
      PQNode* blocked = *i;
      if (blocked->mark_ == PQNode::blocked) {
        pseudonode_->pertinent_child_count++;
        pseudonode_->pertinent_leaf_count += blocked->pertinent_leaf_count;
        int count = 0;  // count number of immediate siblings.
        int loop = 0;
        for (set<PQNode*>::iterator j = blocked->immediate_siblings_.begin();
            j != blocked->immediate_siblings_.end() && loop < 2;
            j++) {
	  PQNode* sibling = *j;
          ++loop;

          if (sibling->mark_ == PQNode::blocked) {
            ++count;
          } else {
            blocked->immediate_siblings_.erase(sibling);
            (sibling)->immediate_siblings_.erase(blocked);
            pseudonode_->pseudo_neighbors_[side] = sibling;
          }
        }
        blocked->parent_ = pseudonode_;
        blocked->pseudochild_ = true;
        if (count < 2)
          pseudonode_->endmost_children_[side++] = blocked;
      }
    }
  }
  return true;
}

bool PQTree::ReduceStep(set<int> reduction_set) {
  // Build a queue with all the pertinent leaves in it
  queue<PQNode*> q;
  for (set<int>::iterator i = reduction_set.begin();
       i != reduction_set.end(); i++) {
    PQNode* candidate_node = leaf_address_[*i];
    if (candidate_node == NULL)
      return false;
    candidate_node->pertinent_leaf_count = 1;
    q.push(candidate_node);
  }

  while (!q.empty()) {
    // Remove candidate_node from the front of the queue
    PQNode* candidate_node = q.front();
    q.pop();

    // We test against different templates depending on whether |candidate_node|
    // is the root of the pertinent subtree.
    if (candidate_node->pertinent_leaf_count < reduction_set.size()) {
      PQNode* candidate_parent = candidate_node->parent_;
      candidate_parent->pertinent_leaf_count += 
	  candidate_node->pertinent_leaf_count;
      candidate_parent->pertinent_child_count--;
      // Push |candidate_parent| onto the queue if it no longer has any
      // pertinent children.
      if (candidate_parent->pertinent_child_count == 0)
        q.push(candidate_parent);

      // Test against each template in turn until one of them returns true.
      if      (TemplateL1(candidate_node)) {}
      else if (TemplateP1(candidate_node, /*is_reduction_root=*/ false)) {}
      else if (TemplateP3(candidate_node)) {}
      else if (TemplateP5(candidate_node)) {}
      else if (TemplateQ1(candidate_node)) {}
      else if (TemplateQ2(candidate_node)) {}
      else {
        CleanPseudo();
        return false;
      }
    } else {  // candidate_node is the root of the reduction subtree
      // Test against each template in turn until one of them returns true.
      if      (TemplateL1(candidate_node)) {}
      else if (TemplateP1(candidate_node, /*is_reduction_root=*/ true)) {}
      else if (TemplateP2(candidate_node)) {}
      else if (TemplateP4(candidate_node)) {}
      else if (TemplateP6(candidate_node)) {}
      else if (TemplateQ1(candidate_node)) {}
      else if (TemplateQ2(candidate_node)) {}
      else if (TemplateQ3(candidate_node)) {}
      else {
        CleanPseudo();
        return false;
      }
    }
  }
  CleanPseudo();
  return true;
}

void PQTree::CleanPseudo() {
  if (pseudonode_) {
    for (int i = 0; i < 2; i++) {
      pseudonode_->endmost_children_[i]->immediate_siblings_.insert(
          pseudonode_->pseudo_neighbors_[i]);
      pseudonode_->pseudo_neighbors_[i]->immediate_siblings_.insert(
          pseudonode_->endmost_children_[i]);
    }

    pseudonode_->ForgetChildren();
    delete pseudonode_;
    pseudonode_ = NULL;
  }
}

// Basic constructor from an initial set.
PQTree::PQTree(set<int> reduction_set) {
  // Set up the root node as a P-Node initially.
  root_ = new PQNode;
  root_->type_ = PQNode::pnode;
  invalid_ = false;
  pseudonode_ = NULL;
  block_count_ = 0;
  blocked_nodes_ = 0;
  off_the_top_ = 0;
  for (set<int>::iterator i = reduction_set.begin();
       i != reduction_set.end(); i++) {
    PQNode *new_node;
    new_node = new PQNode(*i);
    leaf_address_[*i] = new_node;
    new_node->parent_ = root_;
    new_node->type_ = PQNode::leaf;
    root_->circular_link_.push_back(new_node);
  }
}

string PQTree::Print() {
  string out;
  root_->Print(&out);
  return out;
}

//reduces the tree but protects if from becoming invalid
//if the reduction fails, takes more time
bool PQTree::SafeReduce(set<int> S) {
  //using a backup copy to enforce safety
  PQTree toCopy(*this);

  if (!Reduce(S)) {
    //reduce failed, so perform a copy
    root_ = new PQNode(*toCopy.root_);
    block_count_ = toCopy.block_count_;
    blocked_nodes_ = toCopy.blocked_nodes_;
    off_the_top_ = toCopy.off_the_top_;
    invalid_ = toCopy.invalid_;
    leaf_address_.clear();
    root_->FindLeaves(leaf_address_);
    return false;
  }
  return true;
}

bool PQTree::SafeReduceAll(list<set<int> > L) {
  //using a backup copy to enforce safety
  PQTree toCopy(*this);
  if (!ReduceAll(L)) {
    //reduce failed, so perform a copy
    root_ = new PQNode(*toCopy.root_);
    block_count_ = toCopy.block_count_;
    blocked_nodes_ = toCopy.blocked_nodes_;
    off_the_top_ = toCopy.off_the_top_;
    invalid_ = toCopy.invalid_;
    leaf_address_.clear();
    root_->FindLeaves(leaf_address_);
    return false;
  }
  return true;
}


bool PQTree::Reduce(set<int> reduction_set) {
  if (reduction_set.size() < 2) {
    reductions_.push_back(reduction_set);
    return true;
  }
  if (invalid_)
    return false;
  if (!Bubble(reduction_set)) {
    invalid_ = true;
    return false;
  }
  if (!ReduceStep(reduction_set)) {
    invalid_ = true;
    return false;
  }

  if (pseudonode_) {
    pseudonode_->ForgetChildren();
    delete pseudonode_;
  }

  // Reset all the temporary variables for the next round.
  root_->Reset();

  // Store the reduction set for later lookup.
  reductions_.push_back(reduction_set);
  return true;
}

bool PQTree::ReduceAll(list<set<int> > L) {
  for (list<set<int> >::iterator S = L.begin(); S != L.end(); S++) {
    if (!Reduce(*S))
      return false;
  }
  return true;
}

list<int> PQTree::Frontier() {
  list<int> out;
  root_->FindFrontier(out);
  return out;
}

list<int> PQTree::ReducedFrontier() {
  list<int> out, inter;
  root_->FindFrontier(inter);
  set<int> allContained;
  for (list<set<int> >::iterator j = reductions_.begin();
      j != reductions_.end(); j++) {
      allContained = SetMethods::SetUnion(allContained, *j);
  }
  for (list<int>::iterator j = inter.begin(); j != inter.end(); j++) {
    if (SetMethods::SetFind(allContained, *j))
      out.push_back(*j);
  }
  return out;
}

list<set<int> > PQTree::GetReductions() {
  return reductions_;
}

set<int> PQTree::GetContained() {
  set<int> out;
  for (list<set<int> >::iterator i = reductions_.begin();
      i!=reductions_.end(); i++) {
    out = SetMethods::SetUnion(out,*i);
  }
  return out;
}

// Default destructor, Needs to delete the root.
PQTree::~PQTree() {
  delete root_;
}
