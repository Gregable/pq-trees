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
  root_		 = new PQNode(*to_copy.root_);
  block_count_   = to_copy.block_count_;
  blocked_nodes_ = to_copy.blocked_nodes_;
  invalid_	 = to_copy.invalid_;
  off_the_top_   = to_copy.off_the_top_;
  pseudonode_	 = NULL;
  reductions_	 = to_copy.reductions_;

  leaf_address_.clear();
  root_->FindLeaves(leaf_address_);  
}
  
int PQTree::UnblockSiblings(PQNode* X, PQNode* parent, PQNode* last) {
  int unblocked_count = 0;
  if (X->mark_ == PQNode::blocked) {
    // Unblock the current sibling and set it's parent to |parent|.
    unblocked_count++;
    X->mark_ = PQNode::unblocked;
    X->parent_ = parent;

    // Unblock adjacent siblings recursively
    for (set<PQNode *>::iterator i = X->immediate_siblings_.begin();
	i != X->immediate_siblings_.end(); i++) {
      if (*i != last)
        unblocked_count += UnblockSiblings(*i, parent, X);
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

bool PQTree::template_L1(PQNode* candidate_node) {
  // Check against the pattern: must be a leaf
  if (candidate_node->type_ != PQNode::leaf)
    return false;
 
  candidate_node->LabelAsFull(); 
  return true;
}

bool PQTree::template_Q1(PQNode* candidate_node) {  
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
  
bool PQTree::template_Q2(PQNode* candidate_node) {
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
  
    // We no longer need |to_merge|, but we dont want the recursive destructor
    // to kill the children either, so we first null out it's child pointers.
    to_merge->endmost_children_[0] = NULL;  
    to_merge->endmost_children_[0] = NULL;  
    delete to_merge;
  }
  candidate_node->label_ = PQNode::partial;
  if (candidate_node->parent_)
    candidate_node->parent_->partial_children_.insert(candidate_node);
  return true;
}

bool PQTree::template_Q3(PQNode* candidate_node) {  
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
    
    // We no longer need |to_merge|, but we dont want the recursive destructor
    // to kill the children either, so we first null out it's child pointers.
    to_merge->endmost_children_[0] = NULL;  
    to_merge->endmost_children_[0] = NULL;  
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
bool PQTree::template_P1(PQNode* X, bool is_reduction_root) {
    // Check against the pattern
    if (X->type_ != PQNode::pnode)
      return false;
    if (X->full_children_.size() != X->ChildCount())
      return false;

    X->label_ = PQNode::full;
    if (!is_reduction_root)  // Make sure we aren't root first
      X->parent_->full_children_.insert(X);
    
    return true;
}

bool PQTree::template_P2(PQNode* X) {
  // Check against the pattern
  if (X->type_ != PQNode::pnode)
    return false;
  if (X->partial_children_.size() != 0)
    return false;
    
  // Move X's full children into their own P-node
  if (X->full_children_.size() >= 2) {
    PQNode* Y = new PQNode;
    Y->parent_ = X;
    Y->type_ = PQNode::pnode;
    for (set<PQNode *>::iterator i=X->full_children_.begin();i!=X->full_children_.end(); ++i)
    {
      X->circular_link_.remove(*i);
      Y->circular_link_.push_back(*i);
      (*i)->parent_ = Y;
    }
    X->circular_link_.push_back(Y);
  }
  // Mark the root partial
  X->label_ = PQNode::partial;
    
  return true;
}
bool PQTree::template_P3(PQNode* X) {
  //check against the pattern
  if (X->type_ != PQNode::pnode)
    return false;
  if (X->partial_children_.size() != 0)
    return false;
    
  PQNode* theParent=X->parent_;

  // Create new Q-node Y
  PQNode* Y = new PQNode;
  Y->parent_ = theParent;
  Y->type_ = PQNode::qnode;

  // Switch Y for X as s children
  theParent->partial_children_.insert(Y);
  theParent->partial_children_.erase(X);
  if (theParent->type_ == PQNode::pnode)
  {
    theParent->circular_link_.remove(X);
    theParent->circular_link_.push_back(Y);
  }
  else
    X->SwapQ(Y);

  //set up the p_node on the full child side
  PQNode* FC,*EC;
  if (X->full_children_.size()==1)
  {
    FC=*(X->full_children_.begin());
    X->circular_link_.remove(FC);
  } else {
    FC = new PQNode;  //FC = full child
    FC->label_ = PQNode::full;
    FC->type_ = PQNode::pnode;
    for (set<PQNode *>::iterator i = X->full_children_.begin();
	 i != X->full_children_.end(); i++) {
      X->circular_link_.remove(*i);
      FC->circular_link_.push_back(*i);
      (*i)->parent_ = FC;
    }
  }
  FC->parent_ = Y;
  Y->endmost_children_[0] = FC;
  Y->full_children_.insert(FC);

  //now set up the p-node on the empty child side
  if (X->circular_link_.size() == 1) {
    EC=*(X->circular_link_.begin());

    //we want to delete X, but not it's children
    X->circular_link_.clear();
    delete X;
  } else {
    EC = X;
  }
  EC->parent_ = Y;
  EC->label_ = PQNode::empty;
  Y->endmost_children_[1] = EC;

  //update the immediate siblings links
  EC->immediate_siblings_.clear();
  EC->immediate_siblings_.insert(FC);
  FC->immediate_siblings_.clear();
  FC->immediate_siblings_.insert(EC);
  
  Y->label_ = PQNode::partial;
  
  return true;
}
bool PQTree::template_P4(PQNode* X) {
  //check against the pattern
  if (X->type_ != PQNode::pnode)
    return false;
  if (X->partial_children_.size()!=1)
    return false;
  
  //Y is the partial Q-node
  PQNode* Y=*X->partial_children_.begin();
  PQNode* EC;
  PQNode* FC;
  PQNode* ES;  //empty/full child/sibling of Y

  //find the empty and full endmost child of Y
  if (Y->endmost_children_[0]->label_ == PQNode::empty) {
    EC=Y->endmost_children_[0];
    FC=Y->endmost_children_[1];
  } else {
    EC=Y->endmost_children_[1];
    FC=Y->endmost_children_[0];
  }
  //check that we are indeed matching the pattern
  if (EC->label_ != PQNode::empty || FC->label_ == PQNode::empty)
    return false;
  
  //if Y has an empty sibling, set ES to be an empty sibling of Y
  for (list<PQNode *>::iterator i = X->circular_link_.begin();
       i != X->circular_link_.end(); i++) {
    if ((*i)->label_ == PQNode::empty) {
      ES=*i;
      break;
    }
  }
  
  // Move the full children of X to be children of Y
  if (X->full_children_.size() > 0) {
    PQNode *ZF;
    //only 1 full child
    if (X->full_children_.size() == 1) {
      ZF = *(X->full_children_.begin());
      X->circular_link_.remove(ZF);
    } else {  // Multiple full children - must be placed in a P-node
      // Create ZF to be a new p-node
      ZF = new PQNode;
      ZF->label_ = PQNode::full;
      ZF->type_ = PQNode::pnode;

      // For all the full nodes in X, set their to be ZF
      for (set<PQNode *>::iterator W = X->full_children_.begin();
	   W != X->full_children_.end(); W++) {
        (*W)->parent_ = ZF;
        X->circular_link_.remove(*W);
        ZF->circular_link_.push_back(*W);
      }
    }
    //more updates
    ZF->parent_ = Y;
    FC->immediate_siblings_.insert(ZF);      
    ZF->immediate_siblings_.insert(FC);
    if (Y->endmost_children_[0] == FC) {
        Y->endmost_children_[0] = ZF;
    } else if (Y->endmost_children_[1] == FC) {
      Y->endmost_children_[1] = ZF;
    }
    Y->full_children_.insert(ZF);
  }

  // If X now only has one child, get rid of X
  if (X->circular_link_.size() == 1) {
    PQNode* theParent = X->parent_;
    Y->parent_ = X->parent_;
    
    if (theParent != NULL)  // Parent is root of tree
    {

      //update to handle the switch
      if (X->immediate_siblings_.empty())  // is a p-node
      {
        theParent->circular_link_.remove(X);  
        theParent->circular_link_.push_back(Y);
      }
      else  //is a Q-node
      {
        //update the immediate siblings list by removing X and adding Y
        for (set<PQNode *>::iterator i=X->immediate_siblings_.begin();i!=X->immediate_siblings_.end(); ++i)
        {
          (*i)->immediate_siblings_.erase(X);
          (*i)->immediate_siblings_.insert(Y);
          Y->immediate_siblings_.insert(*i);
        }
        if (X->immediate_siblings_.size()==1)
        {
          if (theParent->endmost_children_[0]==X)
            theParent->endmost_children_[0]=Y;
          if (theParent->endmost_children_[1]==X)
            theParent->endmost_children_[1]=Y;
        }
      }
    }
    else {
      root_ = Y;
    }
  }
  
  return true;
}
int runs=0;
bool PQTree::template_P5(PQNode* X) {  
  //check against the pattern
  if (X->type_ != PQNode::pnode)
    return false;
  if (X->partial_children_.size() != 1)
    return false;
  
  //Y is the partial Q-node
  PQNode* Y = *X->partial_children_.begin();
  PQNode* EC = NULL;
  PQNode* FC = NULL;
  PQNode* ES = NULL;  //empty/full child/sibling of Y

  //find the empty and full endmost child of Y
  if (Y->endmost_children_[0]->label_ == PQNode::empty) {
    EC=Y->endmost_children_[0];
    FC=Y->endmost_children_[1];
  } else {
    EC=Y->endmost_children_[1];
    FC=Y->endmost_children_[0];
  }

  //check that we are indeed matching the pattern
  if (EC->label_ != PQNode::empty || FC->label_  == PQNode::empty)
    return false;
    
  //if Y has an empty sibling, set ES to be an empty sibling of Y
  for (list<PQNode *>::iterator i = X->circular_link_.begin();
       i != X->circular_link_.end(); i++) {
    if ((*i)->label_ == PQNode::empty) {
      ES = *i;
      break;
    }
  }

  //Y will be the root of the pertinent subtree after the replacement
  PQNode* theParent = X->parent_;
  Y->parent_ = X->parent_;
  Y->pertinent_leaf_count = X->pertinent_leaf_count;
  Y->label_ = PQNode::partial;
  //add Y to it's s list of partial children
  theParent->partial_children_.insert(Y);
  //remove Y from X's list of children
  X->circular_link_.remove(Y);
  X->partial_children_.erase(Y);
  
  // Update to handle the switch.
  if (X->immediate_siblings_.size() == 0) {  // Parent is a P-node
    theParent->circular_link_.remove(X);
    theParent->circular_link_.push_back(Y);
  } else {   // Parent is a Q-node
    //update the immediate siblings list by removing X and adding Y
    for (set<PQNode *>::iterator i=X->immediate_siblings_.begin();
	 i!=X->immediate_siblings_.end(); i++) {
      (*i)->immediate_siblings_.erase(X);
      (*i)->immediate_siblings_.insert(Y);
      Y->immediate_siblings_.insert(*i);
    }
    if (theParent->endmost_children_[0]==X)
      theParent->endmost_children_[0]=Y;
    if (theParent->endmost_children_[1]==X)
      theParent->endmost_children_[1]=Y;
  }
  
  //move the full children of X to be children of Y
  if (X->full_children_.size()>0)
  {
    PQNode *ZF=NULL;
    //only 1 full child
    if (X->full_children_.size()==1)
    {
      ZF=*(X->full_children_.begin());
      X->circular_link_.remove(ZF);
    }
    //multiple full children - must be placed in a P-node
    else
    {
      //create ZF to be a new p-node
      ZF = new PQNode;
      ZF->label_ = PQNode::full;
      ZF->type_ = PQNode::pnode;

      // For all the full nodes in X, set their to be ZF.
      for (set<PQNode *>::iterator W=X->full_children_.begin();W!=X->full_children_.end();W++)
      {
        (*W)->parent_ = ZF;
        X->circular_link_.remove(*W);
        ZF->circular_link_.push_back(*W);
      }
    }
    X->full_children_.clear();
    
    //more updates
    ZF->parent_ = Y;
    FC->immediate_siblings_.insert(ZF);      
    ZF->immediate_siblings_.insert(FC);
    if (Y->endmost_children_[0]==FC)
                  Y->endmost_children_[0]=ZF;
                else if (Y->endmost_children_[1]==FC)
                        Y->endmost_children_[1]=ZF;      
  }

  //if X still has some empty children, insert them  
  if (X->ChildCount()>0)
  {
    PQNode *ZE = NULL;
    if (X->ChildCount() == 1)
      ZE = ES;
    else
    {
      ZE = X;
      ZE->label_ = PQNode::empty;
      ZE->immediate_siblings_.clear();
    }
    ZE->parent_ = Y;
    EC->immediate_siblings_.insert(ZE);
    ZE->immediate_siblings_.insert(EC);
    if (Y->endmost_children_[0] == EC)
      Y->endmost_children_[0] = ZE;
    if (Y->endmost_children_[1] == EC)
      Y->endmost_children_[1] = ZE;
  }
  if (X->ChildCount()<2)
  {
    //we want to delete X, but not it's children
    X->circular_link_.clear();
    delete X;    
  }
  

  return true;
}

bool PQTree::template_P6(PQNode* X)
{
  //check against the pattern
  if (X->type_ != PQNode::pnode)
    return false;
  if (X->partial_children_.size()!=2)
    return false;

  
  //Y is the first partial Q-node from which we shall build
  PQNode* Y=*X->partial_children_.begin();
  PQNode* Z=*(++(X->partial_children_.begin()));
  PQNode* EC=NULL;
  PQNode* FC=NULL;  //empty/full child/sibling of Y

  PQNode *ZF=NULL;  //the child of Y created to hold full X's children

  //find the empty and full endmost child of Y
  if (Y->endmost_children_[0]->label_ == PQNode::empty)
  {
    EC = Y->endmost_children_[0];
    FC = Y->endmost_children_[1];
  }
  else
  {
    EC = Y->endmost_children_[1];
    FC = Y->endmost_children_[0];
  }
  // check that we are indeed matching the pattern
  if (EC->label_ != PQNode::empty || FC->label_ == PQNode::empty)
    return false;
    
  // move the full children of X to be children of Y
  if (X->full_children_.size() > 0) {
    // only 1 full child
    if (X->full_children_.size() == 1) {
      ZF = *(X->full_children_.begin());
      X->circular_link_.remove(ZF);
    }
    // multiple full children - must be placed in a P-node
    else
    {
      // create ZF to be a new p-node
      ZF = new PQNode;
      ZF->label_ = PQNode::full;
      ZF->type_ = PQNode::pnode;

      // for all the full nodes in X, set their to be ZF
      for (set<PQNode *>::iterator W = X->full_children_.begin();
	   W != X->full_children_.end(); W++)
      {
        (*W)->parent_ = ZF;
        X->circular_link_.remove(*W);
        ZF->circular_link_.push_back(*W);
      }
    }
    // more updates
    ZF->parent_ = Y;
    FC->immediate_siblings_.insert(ZF);      
    ZF->immediate_siblings_.insert(FC);
    if (Y->endmost_children_[0]==FC)
             Y->endmost_children_[0]=ZF;
    if (Y->endmost_children_[1]==FC)
      Y->endmost_children_[1]=ZF;      
    
    //now, incorporate the other partial child

    //find the empty and full endmost child of Z
    if (Z->endmost_children_[0]->label_ == PQNode::empty)
    {
      EC=Z->endmost_children_[0];
      FC=Z->endmost_children_[1];
    }
    else
    {
      EC=Z->endmost_children_[1];
      FC=Z->endmost_children_[0];
    }
    //check that we are indeed matching the pattern
    if (EC->label_ != PQNode::empty || FC->label_ == PQNode::empty)
      return false;

    //connect the children of the two partials together
    ZF->immediate_siblings_.insert(FC);
    FC->immediate_siblings_.insert(ZF);

    // Adjust the Y
    if (Y->endmost_children_[0] == ZF)
      Y->endmost_children_[0] = EC;
    if (Y->endmost_children_[1] == ZF)
      Y->endmost_children_[1] = EC;
    FC->parent_ = Y;
    EC->parent_ = Y;
  }
  else  //no full children
  {
    PQNode *ZFC=NULL;  //Z's full child

    //figure out which sides of Y and Z to connect together
    if (Z->endmost_children_[0]->label_ == PQNode::full)
    {
      ZFC=Z->endmost_children_[0];
      if (Y->endmost_children_[0]==FC)
        Y->endmost_children_[0]=Z->endmost_children_[1];
      if (Y->endmost_children_[1]==FC)
        Y->endmost_children_[1]=Z->endmost_children_[1];
    }
    else
    {
      ZFC=Z->endmost_children_[1];
      if (Y->endmost_children_[0]==FC)
        Y->endmost_children_[0]=Z->endmost_children_[0];
      if (Y->endmost_children_[1]==FC)
        Y->endmost_children_[1]=Z->endmost_children_[0];
    }
    Y->endmost_children_[0]->parent_ = Y;
    Y->endmost_children_[1]->parent_ = Y;

    FC->immediate_siblings_.insert(ZFC);
    ZFC->immediate_siblings_.insert(FC);
  }
    
  
  //adjust the root X
  //we dont need Z any more
  X->circular_link_.remove(Z);
  Z->endmost_children_[0]=NULL;
  Z->endmost_children_[1]=NULL;
  delete Z;

  //if X now only has one child, get rid of X
  if (X->circular_link_.size()==1)
  {
  
    PQNode* theParent = X->parent_;
    Y->parent_ = X->parent_;
    Y->pertinent_leaf_count = X->pertinent_leaf_count;
    Y->label_ = PQNode::partial;
  
    if (theParent != NULL) {  // Parent is not root of tree
      // Add Y to it's s list of partial children
      theParent->partial_children_.insert(Y);
  
      // Update to handle the switch
      if (theParent->type_ == PQNode::pnode)
      {
        theParent->circular_link_.remove(X);  
        theParent->circular_link_.push_back(Y);
      }
      else { // Parent is a Q-node
        // Update the immediate siblings list by removing X and adding Y
        for (set<PQNode *>::iterator i = X->immediate_siblings_.begin();
	    i != X->immediate_siblings_.end(); i++) {
          (*i)->immediate_siblings_.erase(X);
          (*i)->immediate_siblings_.insert(Y);
        }
        if (theParent->endmost_children_[0] == X)
          theParent->endmost_children_[0] = Y;
        if (theParent->endmost_children_[1] == X)
          theParent->endmost_children_[1] = Y;
      }
    }
    else {  // This is the root of our tree, act accordingly
      root_ = Y;
      Y->parent_ = NULL;
      
      //delete X, but not it's children
      X->circular_link_.clear();
      delete X;
    }  
  }
  return true;
}

// This procedure is the first pass of the Booth & Leuker PQTree algorithm.
// It processes the pertinent subtree of the PQ-Tree to determine the mark
// of every node in that subtree.  The pseudonode, if created, is returned so
// that it can be deleted at the end of the reduce step.
bool PQTree::bubble(set<int> reduction_set) {
  //initialize variables
  queue<PQNode*> q;
  block_count_ = 0;
  blocked_nodes_ = 0;
  off_the_top_ = 0;
  
  // Stores nodes that aren't PQNode::unblocked
  set<PQNode*> blocked_list;  

  // Insert the set's leaves into the queue
  for (set<int>::iterator i = reduction_set.begin();
       i != reduction_set.end(); ++i) {
    PQNode *temp = leaf_address_[*i];
    if (temp == NULL)  // Make sure we have this in our leaves already
      return false;
    q.push(temp);
  }

  while(q.size() + block_count_ + off_the_top_ > 1) {
    // Check to see if there are still nodes in the queue
    if (q.empty())
      return false;
    
    // Remove X from the front of the queue
    PQNode* X = q.front();
    q.pop();
    
    // Mark X as blocked
    X->mark_ = PQNode::blocked;
    
    // Get the set of blocked and PQNode::unblocked siblings
    set<PQNode *> US, BS;
    for (set<PQNode *>::iterator i = X->immediate_siblings_.begin();
	i != X->immediate_siblings_.end(); i++) {
      if ((*i)->mark_ == PQNode::blocked) {
        BS.insert(*i);
      } else if ((*i)->mark_ == PQNode::unblocked) {
        US.insert(*i);
      }
    }
    // We can assign a to X if one of its immediate siblings is PQNode::unblocked
    // Or it has 0/1 immediate siblings meaning it is a corner child of a q node
    // Or a child of a p node
    if (!US.empty()) {
      X->parent_ = (*US.begin())->parent_;
      X->mark_ = PQNode::unblocked;
    }
    else if (X->immediate_siblings_.size() < 2) {
      X->mark_ = PQNode::unblocked;
    }
    
    // If it is PQNode::unblocked, we can process it.
    if (X->mark_ == PQNode::unblocked) {
      int listSize = 0;
      PQNode* Y = X->parent_;
      if (!BS.empty()) {
        X->mark_ = PQNode::blocked;  // Will be PQNode::unblocked by recursive unblocksiblings
        listSize = UnblockSiblings(X, Y);
        Y->pertinent_child_count += listSize-1;
      }
      
      if (Y == NULL) {  // Currently at root node
        off_the_top_ = 1;
      } else {
        Y->pertinent_child_count++;
        if (Y->mark_ == PQNode::unmarked) {
          q.push(Y);
          Y->mark_ = PQNode::queued;
        }
      }
      block_count_ -= BS.size();
      blocked_nodes_ -= listSize;          
    } else {
      block_count_ += 1-BS.size();
      blocked_nodes_ += 1;
      blocked_list.insert(X);
    }
  }

  if (block_count_ > 1 || (off_the_top_ == 1 && block_count_ != 0))
    return false;
  
  int correctblockedcount = 0;
  for (set<PQNode*>::iterator i = blocked_list.begin();
      i != blocked_list.end(); i++) {
      if ((*i)->mark_ == PQNode::blocked)
        correctblockedcount++;
  }

  // In this case, we have a block that is contained within a Q-node.  We must
  // assign a psuedonode Z of Q-node to handle it.
  if (block_count_ == 1 && correctblockedcount > 1) {
    pseudonode_ = new PQNode;
    pseudonode_->type_ = PQNode::qnode;
    pseudonode_->pseudonode_ = true;
    int side = 0;
    pseudonode_->pertinent_child_count = 0;
    
    // Figure out which nodes are still blocked and which of those are the
    // endmost children
    for (set<PQNode*>::iterator i = blocked_list.begin();
	i != blocked_list.end(); ++i)
      if ((*i)->mark_ == PQNode::blocked) {
        pseudonode_->pertinent_child_count++;
        pseudonode_->pertinent_leaf_count += (*i)->pertinent_leaf_count;
        int count = 0;  //count number of immediate siblings
        int loop = 0;
        for (set<PQNode*>::iterator j = (*i)->immediate_siblings_.begin();
	    j != (*i)->immediate_siblings_.end() && loop < 2;
	    j++) {
          loop++;

          if ((*j)->mark_ == PQNode::blocked) {
            count++;
          } else {
            (*i)->immediate_siblings_.erase(*j);
            (*j)->immediate_siblings_.erase(*i);
            pseudonode_->pseudo_neighbors_[side] = *j;
          }
        }
        (*i)->parent_ = pseudonode_;
        (*i)->pseudochild_ = true;
        if (count < 2)
          pseudonode_->endmost_children_[side++] = *i;
      }
  }
  return true;
}  

bool PQTree::reduceStep(set<int> reduction_set) {
  // Build a queue with all the pertinent leaves in it
  queue<PQNode*> q;
  for (set<int>::iterator i = reduction_set.begin();
       i != reduction_set.end(); i++) {
    PQNode* X = leaf_address_[*i];
    if (X == NULL)
      return false;
    X->pertinent_leaf_count = 1;
    q.push(X);
  }

  while (!q.empty()) {
    // Remove X from the front of the queue
    PQNode* X = q.front();
    q.pop();

    // if X is not the root of the reduction subtree
    if (X->pertinent_leaf_count < reduction_set.size()) {
      // Update X's parent Y
      PQNode* Y = X->parent_;
      Y->pertinent_leaf_count += X->pertinent_leaf_count;
      Y->pertinent_child_count--;
      // Push Y onto the queue if it has no more pertinent children
      if (Y->pertinent_child_count == 0)
        q.push(Y);

      // Test against each template in turn until one of them returns true.
      if      (template_L1(X)) {}
      else if (template_P1(X, /*is_reduction_root=*/ false)) {}
      else if (template_P3(X)) {}
      else if (template_P5(X)) {}
      else if (template_Q1(X)) {}
      else if (template_Q2(X)) {}
      else {
        cleanPseudo();
        return false;
      }
    } else {  // X is the root of the reduction subtree
      // Test against each template in turn until one of them returns true.
      if      (template_L1(X)) {}
      else if (template_P1(X, /*is_reduction_root=*/ true)) {}
      else if (template_P2(X)) {}
      else if (template_P4(X)) {}
      else if (template_P6(X)) {}
      else if (template_Q1(X)) {}
      else if (template_Q2(X)) {}
      else if (template_Q3(X)) {}
      else {
        cleanPseudo();
        return false;
      }
    }
  }
  cleanPseudo();
  return true;
}

void PQTree::cleanPseudo() {
  if (pseudonode_ != NULL) {
    for (int i = 0; i < 2; i++) {
      pseudonode_->endmost_children_[i]->immediate_siblings_.insert(
	  pseudonode_->pseudo_neighbors_[i]);
      pseudonode_->pseudo_neighbors_[i]->immediate_siblings_.insert(
	  pseudonode_->endmost_children_[i]);
    }

    pseudonode_->endmost_children_[0] = NULL;
    pseudonode_->endmost_children_[1] = NULL;
    delete pseudonode_;
    pseudonode_ = NULL;
  }
}

//basic constructor from an initial set
PQTree::PQTree(set<int> S) {
  //set up the root node as a P-Node initially
  root_ = new PQNode;
  root_->type_ = PQNode::pnode;  
  invalid_ = false;
  pseudonode_ = NULL;
  block_count_ = 0;
  blocked_nodes_ = 0;
  off_the_top_ = 0;
  for (set<int>::iterator i = S.begin(); i != S.end(); i++) {
    PQNode *newNode;
    newNode = new PQNode(*i);
    leaf_address_[*i] = newNode;
    newNode->parent_ = root_;
    newNode->type_ = PQNode::leaf;
    root_->circular_link_.push_back(newNode);
  }
}

string PQTree::Print() {
  string out;
  root_->Print(&out);
  return out;
}

//reduces the tree but protects if from becoming invalid
//if the reduction fails, takes more time
bool PQTree::safeReduce(set<int> S) {
  //using a backup copy to enforce safety
  PQTree toCopy(*this);
  
  if (!reduce(S)) {
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

bool PQTree::safeReduceAll(list<set<int> > L) {
  //using a backup copy to enforce safety
  PQTree toCopy(*this);
  if (!reduceAll(L)) {
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

  
bool PQTree::reduce(set<int> reduction_set) {
  if (reduction_set.size() < 2) {
    reductions_.push_back(reduction_set);
    return true;
  }
  if (invalid_)
    return false;
  if (!bubble(reduction_set)) {
    invalid_ = true;
    return false;
  }
  if (!reduceStep(reduction_set)) {
    invalid_ = true;
    return false;
  }
  
  // We dont need any more pseudonodes at least until the next reduce
  if (pseudonode_ != NULL) {
    pseudonode_->endmost_children_[0] = NULL;
    pseudonode_->endmost_children_[1] = NULL;
    delete pseudonode_;
    pseudonode_ = NULL;
  }

  // Reset all the temporary variables for the next round
  root_->Reset();
  
  // Store the reduction set for later lookup
  reductions_.push_back(reduction_set);
  return true;
}

bool PQTree::reduceAll(list<set<int> > L) {
  if (invalid_)
    return false;

  for (list<set<int> >::iterator S = L.begin(); S != L.end(); S++) {
    if (S->size() < 2) {
      reductions_.push_back(*S);
      continue;
    }
    if (!bubble(*S)) {
      invalid_ = true;
      return false;
    }
    if (!reduceStep(*S)) {
      invalid_ = true;
      return false;
    }

    //we dont need any more pseudonodes
    //at least until the next reduce
    if (pseudonode_ != NULL) {
      pseudonode_->endmost_children_[0] = NULL;
      pseudonode_->endmost_children_[1] = NULL;
      delete pseudonode_;
      pseudonode_ = NULL;
    }
    //Reset all the temporary variables for the next round
    root_->Reset();
  
    //store the reduction set for later lookup
    reductions_.push_back(*S);
  }  
  return true;
}

list<int> PQTree::frontier() {
  list<int> out;
  root_->FindFrontier(out);
  return out;
}

list<int> PQTree::reducedFrontier() {
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

list<set<int> > PQTree::getReductions() {
  return reductions_;
}

set<int> PQTree::getContained() {
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
