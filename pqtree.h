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

class PQTree
{
  private:
  
  //the root node of the PQTree
  PQNode *root;
  
  //The number of blocks of blocked nodes during the 1st pass
  int block_count;

  //The number of blocked nodes during the 1st pass
  int blocked_nodes;

  //A variable (0 or 1) which is a count of the number of virtual nodes which are
  //imagined to be in the queue during the bubbling up
  int off_the_top;

  //keeps track of all reductions performed on this tree in order
  list<set<int> > reductions;

  // Keeps a pointer to the leaf containing a particular value this map actually
  // increases the time complexity of the algorithm.  To fix, you can create an
  // array of items so that each item hashes to its leaf address in constant
  // time, but this is a tradeoff to conserve space.
  map<int, PQNode*> leafAddress;

  // A reference to a pseudonode that cannot be reached through the root
  // of the tree.  The pseudonode is a temporary node designed to handle
  // a special case in the first bubbling up pass it only exists during the
  // scope of the reduce operation
  PQNode* pseudonode;

  // true if a non-safe reduce has failed, tree is useless
  bool invalid;

  // Loops through the consecutive blocked siblings of an unblocked 
  // Node recursively unblocking the siblings
  int unblockSiblings(PQNode* X, PQNode* parent, PQNode* last);

  /* 
   * All of the templates for matching a reduce are below.  The template has a
   * letter describing which type of node it refers to and a number indicating
   * the index of the template for that letter.  These are the same indices in
   * the Booth & Lueker paper.  The return value indicates whether or not the
   * pattern accurately matches the template
   */

  bool template_L1(PQNode*);
  bool template_Q1(PQNode*);
  bool template_Q2(PQNode*);
  bool template_Q3(PQNode*);
  bool template_P1(PQNode*, bool);
  bool template_P2(PQNode*);
  bool template_P3(PQNode*);
  bool template_P4(PQNode*);
  bool template_P5(PQNode*);
  bool template_P6(PQNode*);
  
  //This procedure is the first pass of the Booth&Leuker PQTree algorithm
  //It processes the pertinent subtree of the PQ-Tree to determine the mark
  //of every node in that subtree
  //the pseudonode, if created, is returned so that it can be deleted at
  //the end of the reduce step
  bool bubble(set<int> S);

  bool reduceStep(set<int> S);
        
  public:  
  
  //copy constructor
  PQTree(const PQTree& toCopy);
  //default constructor - constructs a tree using a set
  //only reductions using elements of that set will succeed
  PQTree(set<int> S);
  
  //default destructor
  ~PQTree();
 
  //assignment operator
  PQTree& operator=(const PQTree& toCopy);  
  
  // Mostly for debugging purposes, Prints the tree to standard out
  string Print();
  
  //cleans up pointer mess caused by having a pseudonode
  void cleanPseudo();

  //reduces the tree but protects if from becoming invalid
  //if the reduction fails, takes more time
  bool safeReduce(set<int>);
  bool safeReduceAll(list<set<int> >);

  //reduces the tree - tree can become invalid, making all further
  //reductions fail
  bool reduce(set<int> S);
  bool reduceAll(list<set<int> > L);
  
  //returns 1 possible frontier, or ordering preserving the reductions
  list<int> frontier();
  
  //returns a frontier not including leaves that were not a part of any
  //reduction
  list<int> reducedFrontier();
  
  //returns the reductions that have been performed on this tree
  list<set<int> > getReductions();
  
  //returns the set of all elements on which a reduction was performed with
  set<int> getContained();
};

#endif

//copy constructor
PQTree::PQTree(const PQTree& toCopy)
{
  root = new PQNode(*(toCopy.root));
  block_count = toCopy.block_count;
  blocked_nodes = toCopy.blocked_nodes;
  off_the_top = toCopy.off_the_top;
  invalid = toCopy.invalid;
  reductions = toCopy.reductions;
  pseudonode = NULL;
  leafAddress.clear();
  root->FindLeaves(leafAddress);  
}
  
//assignment operator
PQTree& PQTree::operator=(const PQTree& toCopy) {
  //make sure we aren't copying ourself
  if (&toCopy == this)
    return *this;
  
  root = new PQNode(*(toCopy.root));
  block_count = toCopy.block_count;
  blocked_nodes = toCopy.blocked_nodes;
  off_the_top = toCopy.off_the_top;
  reductions = toCopy.reductions;
  leafAddress.clear();
  root->FindLeaves(leafAddress);  
  
  return *this;
}
  
// Loops through the consecutive blocked siblings of an unblocked 
// node recursively unblocking the siblings
int PQTree::unblockSiblings(PQNode* X, PQNode* parent, PQNode* last) {
  int unblocked_count = 0;
  if (X->mark == blocked) {
    // Unblock the current sibling
    unblocked_count++;
    X->mark = unblocked;

    X->parent = parent;

    // Unblock adjacent siblings recursively
    for (set<PQNode *>::iterator i = X->immediate_siblings.begin();
	i != X->immediate_siblings.end(); i++) {
      if (*i != last)
        unblocked_count += unblockSiblings(*i, parent, X);
    }
  }
  return unblocked_count;
}

// All the different templates for matching a reduce are below.  The template
// has a letter describing which type of node it refers to and a number 
// indicating the index of the template for that letter.  These are the same
// indices in the Booth & Lueker paper.

bool PQTree::template_L1(PQNode* X) {
  //check against the pattern
  if (X->type != leaf)
    return false;
  
  X->label = full;
  if (X->parent != NULL)
    X->parent->full_children.insert(X);
  
  return true;
}

bool PQTree::template_Q1(PQNode* X) {  
  //check against pattern
  if (X->type != qnode)
    return false;

  //find an endmost child
  PQNode* cur = X->endmost_children[0];
  PQNode* last = NULL;  
  //walk the pattern to find any empty/partial children
  while(cur != NULL) {
    if (cur->label != full)
      return false;
    PQNode* next = cur->QNextChild(last);
    last = cur;
    cur = next;
  }
  
  X->label = full;    
  if (X->parent != NULL)
    X->parent->full_children.insert(X);
  return true;
}
  
bool PQTree::template_Q2(PQNode* X) {
  //check against the pattern
  if (X->type != qnode)
    return false;
  if (X->pseudonode)
    return false;
  if (X->partial_children.size() > 1)
    return false;
    
  if (X->full_children.size() > 0)
  {
    int numfullside = 0;
    PQNode* Y;
    PQNode* last = NULL;
    // Let Y be the unique element in X's endmost_children that is full
    for (int i = 0; i < 2; i++) {
      if (X->endmost_children[i]->label == full) {
        Y = X->endmost_children[i];
        numfullside++;
      }
    }
    //if there are two full endmost_children, our pattern does not match
    if (numfullside != 1)
      return false;
  
    //check that all the full children are consecutive and next to
    //the partial child
    for (int i=0;i<X->full_children.size();i++) {
      if (Y->label != full)
        return false;
      PQNode* next = Y->QNextChild(last);
      last = Y;
      Y = next;
    }
    if (Y->label != partial && X->partial_children.size() == 1)
      return false;
  } else {
    // Or that the partial child is on the side
    int numpartialside = 0;
    for (int i = 0; i < 2; i++)
      if (X->endmost_children[i]->label == partial)
          numpartialside++;
    if (numpartialside == 0)
      return false;
  }
  
  // If there are no partial children, we are already done, otherwise we need to
  // merge the partial child into X.
  if (X->partial_children.size() > 0) {
    // Let Y be the unique partial child of X
    PQNode* Y = *(X->partial_children.begin());
    // Let FC be the unique endmost full child of Y
    PQNode* FC;
    // Let EC be the unique endmost empty child of Y
    PQNode* EC;
    // Find EC and FC
    for (int i = 0; i < 2; i++) {
      if (Y->endmost_children[i]->label == full) {
        FC = Y->endmost_children[i];
      } else if (Y->endmost_children[i]->label == empty) {
        EC = Y->endmost_children[i];
      }
    }
    
    // Let FS be Y's full immediate sibling if Y has one
    PQNode* FS = NULL;
    // Let ES be Y's empty immediate sibling if Y has one
    PQNode* ES = NULL;
    // Find ES and FS
    for (set<PQNode *>::iterator i = Y->immediate_siblings.begin();
	 i != Y->immediate_siblings.end(); i++) {
      if ((*i)->label == full) {
        FS = *i;
      } else if ((*i)->label == empty) {
        ES = *i;
      }
    }
    
    // If Y has a full immediate sibling
    if (FS != NULL) {
      FS->immediate_siblings.erase(Y);
      FS->immediate_siblings.insert(FC);
      FC->immediate_siblings.insert(FS);
    } else {
      if (X->endmost_children[0] == Y)
        X->endmost_children[0] = FC;
      if (X->endmost_children[1] == Y)
        X->endmost_children[1] = FC;
      FC->parent = X;
    }
    // If Y has an empty immediate sibling
    if (ES != NULL)
    {
      ES->immediate_siblings.erase(Y);
      ES->immediate_siblings.insert(EC);
      EC->immediate_siblings.insert(ES);
    } else {
      if (X->endmost_children[0] == Y)
        X->endmost_children[0] = EC;
      if (X->endmost_children[1] == Y)
        X->endmost_children[1] = EC;
      EC->parent = X;
    }
  
    // We dont need Y anymore, but we dont want the recursive destructor to kill
    // the children either  
    Y->endmost_children[0] = NULL;  
    Y->endmost_children[0] = NULL;  
    delete Y;
  }
  X->label = partial;  
  if (X->parent != NULL)
    X->parent->partial_children.insert(X);
  return true;
}
bool PQTree::template_Q3(PQNode* X) {  
  //check against the pattern
  if (X->type!=qnode)
    return false;
  if (X->partial_children.size()>2)
    return false;

  //handle a special case
  if (X->pseudonode && X->endmost_children[1]==NULL)
    return true;

  bool start=false;
  bool end=false;
  PQNode* cur=X->endmost_children[0];
  PQNode* last=NULL;
  // After this consecutive walk, we know that we have a valid layout.

  bool lastIter = false;
  while(!lastIter) {
    if (cur == X->endmost_children[1])
      lastIter = true;
    if (cur->label == full) {
      if (end)
        return false;
      if (!start)
        start = true;
    } else if (cur->label == empty) {
      if (start)
        end = true;
    } else if (cur->label == partial) {
      if (!start)
        start = true;
      else if (!end)
        end = true;
      else if (end)
        return false;
    }
    if (!lastIter) {
      PQNode* next = cur->QNextChild(last);
      last = cur;
      cur = next;
    }
  }
  
  // Start with the first partial child
  for (set<PQNode *>::iterator j = X->partial_children.begin();
       j != X->partial_children.end(); j++) {
    PQNode* PC = *(j);
    PQNode* EC, *FC;  // Empty and full child of this partial child.
    //Find those children
    if (PC->endmost_children[0]->label == full) {
      FC=PC->endmost_children[0];
      EC=PC->endmost_children[1];
    } else {
      EC=PC->endmost_children[0];
      FC=PC->endmost_children[1];
    }
    
    PQNode* CS;
    // Handle the sides where the child has immediate siblings.
    for (set<PQNode *>::iterator i = PC->immediate_siblings.begin();
	 i != PC->immediate_siblings.end(); i++) {
      if ((*i)->label == empty) {
        (*i)->immediate_siblings.erase(PC);
        (*i)->immediate_siblings.insert(EC);
        EC->immediate_siblings.insert(*i);
        CS = FC;
      } else {  // Either full or partial, we dont care which
        (*i)->immediate_siblings.erase(PC);
        (*i)->immediate_siblings.insert(FC);
        FC->immediate_siblings.insert(*i);
        CS = EC;
      }
    }
    if (X->pseudonode)
      CS = EC;
    // Handle the case where the child has only one immediate sibling
    if (PC->immediate_siblings.size() == 1 || X->pseudonode) {
      CS->parent=X;
      if (X->endmost_children[0] == PC)
        X->endmost_children[0] = CS;
      if (X->endmost_children[1] == PC)
        X->endmost_children[1] = CS;
    }
    
    // We want to delete PC, but not it's children
    PC->endmost_children[0] = NULL;
    PC->endmost_children[1] = NULL;
    delete PC;
    PC = NULL;

  }
  return true;
}
/* A note here.  An error in the Booth and Leuker Algorithm fails to consider
 * the case where a P-node is full, is the pertinent root, and is not an endmost
 * child of a q-node.  In this case, we need to know that the P-node is a
 * pertinent root and not try to update its parent whose pointer is possibly
 * invalid
 */
bool PQTree::template_P1(PQNode* X,bool isRoot) {
    // Check against the pattern
    if (X->type != pnode)
      return false;
    if (X->full_children.size() != X->ChildCount())
      return false;

    X->label = full;
    if (!isRoot)  // Make sure we aren't root first
      X->parent->full_children.insert(X);
    
    return true;
}

bool PQTree::template_P2(PQNode* X) {
  // Check against the pattern
  if (X->type != pnode)
    return false;
  if (X->partial_children.size() != 0)
    return false;
    
  // Move X's full children into their own P-node
  if (X->full_children.size() >= 2) {
    PQNode* Y = new PQNode;
    Y->parent=X;
    Y->type=pnode;
    for (set<PQNode *>::iterator i=X->full_children.begin();i!=X->full_children.end();i++)
    {
      X->circular_link.remove(*i);
      Y->circular_link.push_back(*i);
      (*i)->parent=Y;
    }
    X->circular_link.push_back(Y);
  }
  //mark the root partial
  X->label=partial;
    
  return true;
}
bool PQTree::template_P3(PQNode* X) {
  //check against the pattern
  if (X->type!=pnode)
    return false;
  if (X->partial_children.size()!=0)
    return false;
    
  PQNode* theParent=X->parent;

  //create new Q-node Y
  PQNode* Y = new PQNode;
  Y->parent=theParent;
  Y->type=qnode;

  //switch Y for X as parent's children
  theParent->partial_children.insert(Y);
  theParent->partial_children.erase(X);
  if (theParent->type==pnode)
  {
    theParent->circular_link.remove(X);
    theParent->circular_link.push_back(Y);
  }
  else
    X->SwapQ(Y);

  //set up the p_node on the full child side
  PQNode* FC,*EC;
  if (X->full_children.size()==1)
  {
    FC=*(X->full_children.begin());
    X->circular_link.remove(FC);
  }
  else
  {
    FC = new PQNode;  //FC = full child
    FC->label=full;
    FC->type=pnode;
    for (set<PQNode *>::iterator i=X->full_children.begin();i!=X->full_children.end();i++)
    {
      X->circular_link.remove(*i);
      FC->circular_link.push_back(*i);
      (*i)->parent=FC;
    }
  }
  FC->parent=Y;
  Y->endmost_children[0]=FC;
  Y->full_children.insert(FC);

  //now set up the p-node on the empty child side
  if (X->circular_link.size()==1)
  {
    EC=*(X->circular_link.begin());

    //we want to delete X, but not it's children
    X->circular_link.clear();
    delete X;
  }
  else
    EC=X;
  EC->parent=Y;
  EC->label=empty;
  Y->endmost_children[1]=EC;

  //update the immediate siblings links
  EC->immediate_siblings.clear();
  EC->immediate_siblings.insert(FC);
  FC->immediate_siblings.clear();
  FC->immediate_siblings.insert(EC);
  
  Y->label=partial;
  
  return true;
}
bool PQTree::template_P4(PQNode* X) {
  //check against the pattern
  if (X->type!=pnode)
    return false;
  if (X->partial_children.size()!=1)
    return false;
  
  //Y is the partial Q-node
  PQNode* Y=*X->partial_children.begin();
  PQNode* EC;
  PQNode* FC;
  PQNode* ES;  //empty/full child/sibling of Y

  //find the empty and full endmost child of Y
  if (Y->endmost_children[0]->label==empty) {
    EC=Y->endmost_children[0];
    FC=Y->endmost_children[1];
  } else {
    EC=Y->endmost_children[1];
    FC=Y->endmost_children[0];
  }
  //check that we are indeed matching the pattern
  if (EC->label!=empty || FC->label==empty)
    return false;
  
  //if Y has an empty sibling, set ES to be an empty sibling of Y
  for (list<PQNode *>::iterator i = X->circular_link.begin();
       i != X->circular_link.end(); i++) {
    if ((*i)->label==empty) {
      ES=*i;
      break;
    }
  }
  
  // Move the full children of X to be children of Y
  if (X->full_children.size() > 0) {
    PQNode *ZF;
    //only 1 full child
    if (X->full_children.size() == 1) {
      ZF = *(X->full_children.begin());
      X->circular_link.remove(ZF);
    } else {  // Multiple full children - must be placed in a P-node
      // Create ZF to be a new p-node
      ZF = new PQNode;
      ZF->label = full;
      ZF->type = pnode;

      // For all the full nodes in X, set their parent to be ZF
      for (set<PQNode *>::iterator W = X->full_children.begin();
	   W != X->full_children.end(); W++) {
        (*W)->parent = ZF;
        X->circular_link.remove(*W);
        ZF->circular_link.push_back(*W);
      }
    }
    //more updates
    ZF->parent = Y;
    FC->immediate_siblings.insert(ZF);      
    ZF->immediate_siblings.insert(FC);
    if (Y->endmost_children[0] == FC) {
        Y->endmost_children[0] = ZF;
    } else if (Y->endmost_children[1] == FC) {
      Y->endmost_children[1] = ZF;
    }
    Y->full_children.insert(ZF);
  }

  // If X now only has one child, get rid of X
  if (X->circular_link.size() == 1) {
    PQNode* theParent = X->parent;
    Y->parent = X->parent;
    
    if (theParent != NULL)  // Parent is root of tree
    {

      //update parent to handle the switch
      if (X->immediate_siblings.empty())  //parent is a p-node
      {
        theParent->circular_link.remove(X);  
        theParent->circular_link.push_back(Y);
      }
      else  //parent is a Q-node
      {
        //update the immediate siblings list by removing X and adding Y
        for (set<PQNode *>::iterator i=X->immediate_siblings.begin();i!=X->immediate_siblings.end();i++)
        {
          (*i)->immediate_siblings.erase(X);
          (*i)->immediate_siblings.insert(Y);
          Y->immediate_siblings.insert(*i);
        }
        if (X->immediate_siblings.size()==1)
        {
          if (theParent->endmost_children[0]==X)
            theParent->endmost_children[0]=Y;
          if (theParent->endmost_children[1]==X)
            theParent->endmost_children[1]=Y;
        }
      }
    }
    else
      root=Y;
  }
  
  return true;
}
int runs=0;
bool PQTree::template_P5(PQNode* X)
{  
  //check against the pattern
  if (X->type!=pnode)
    return false;
  if (X->partial_children.size()!=1)
    return false;
    
  
  //Y is the partial Q-node
  PQNode* Y=*X->partial_children.begin();
  PQNode* EC=NULL;
  PQNode* FC=NULL;
  PQNode* ES=NULL;  //empty/full child/sibling of Y

  //find the empty and full endmost child of Y
  if (Y->endmost_children[0]->label==empty)
  {
    EC=Y->endmost_children[0];
    FC=Y->endmost_children[1];
  }
  else
  {
    EC=Y->endmost_children[1];
    FC=Y->endmost_children[0];
  }
  //check that we are indeed matching the pattern
  if (EC->label!=empty || FC->label==empty)
    return false;
    
  //if Y has an empty sibling, set ES to be an empty sibling of Y
  for (list<PQNode *>::iterator i=X->circular_link.begin();i!=X->circular_link.end();i++)
    if ((*i)->label==empty)
    {
      ES=*i;
      break;
    }

  //Y will be the root of the pertinent subtree after the replacement
  PQNode* theParent = X->parent;
  Y->parent = X->parent;
  Y->pertinent_leaf_count = X->pertinent_leaf_count;
  Y->label=partial;
  //add Y to it's parent's list of partial children
  theParent->partial_children.insert(Y);
  //remove Y from X's list of children
  X->circular_link.remove(Y);
  X->partial_children.erase(Y);
  
  //update parent to handle the switch
  if (X->immediate_siblings.size()==0)  //parent is a P-node
  {
    theParent->circular_link.remove(X);
    theParent->circular_link.push_back(Y);
  }
  else  //parent is a Q-node
  {
    //update the immediate siblings list by removing X and adding Y
    for (set<PQNode *>::iterator i=X->immediate_siblings.begin(); i!=X->immediate_siblings.end();i++)
    {
      (*i)->immediate_siblings.erase(X);
      (*i)->immediate_siblings.insert(Y);
      Y->immediate_siblings.insert(*i);
    }
    if (theParent->endmost_children[0]==X)
      theParent->endmost_children[0]=Y;
    if (theParent->endmost_children[1]==X)
      theParent->endmost_children[1]=Y;
  }
  
  //move the full children of X to be children of Y
  if (X->full_children.size()>0)
  {
    PQNode *ZF=NULL;
    //only 1 full child
    if (X->full_children.size()==1)
    {
      ZF=*(X->full_children.begin());
      X->circular_link.remove(ZF);
    }
    //multiple full children - must be placed in a P-node
    else
    {
      //create ZF to be a new p-node
      ZF=new PQNode;
      ZF->label=full;
      ZF->type=pnode;

      //for all the full nodes in X, set their parent to be ZF
      for (set<PQNode *>::iterator W=X->full_children.begin();W!=X->full_children.end();W++)
      {
        (*W)->parent=ZF;
        X->circular_link.remove(*W);
        ZF->circular_link.push_back(*W);
      }
    }
    X->full_children.clear();
    
    //more updates
    ZF->parent=Y;
    FC->immediate_siblings.insert(ZF);      
    ZF->immediate_siblings.insert(FC);
    if (Y->endmost_children[0]==FC)
                  Y->endmost_children[0]=ZF;
                else if (Y->endmost_children[1]==FC)
                        Y->endmost_children[1]=ZF;      
  }

  //if X still has some empty children, insert them  
  if (X->ChildCount()>0)
  {
    PQNode *ZE=NULL;
    if (X->ChildCount()==1)
      ZE=ES;
    else
    {
      ZE=X;
      ZE->label=empty;
      ZE->immediate_siblings.clear();
    }
    ZE->parent=Y;
    EC->immediate_siblings.insert(ZE);
    ZE->immediate_siblings.insert(EC);
    if (Y->endmost_children[0]==EC)
      Y->endmost_children[0]=ZE;
    if (Y->endmost_children[1]==EC)
      Y->endmost_children[1]=ZE;
  }
  if (X->ChildCount()<2)
  {
    //we want to delete X, but not it's children
    X->circular_link.clear();
    delete X;    
  }
  

  return true;
}

bool PQTree::template_P6(PQNode* X)
{
  //check against the pattern
  if (X->type!=pnode)
    return false;
  if (X->partial_children.size()!=2)
    return false;

  
  //Y is the first partial Q-node from which we shall build
  PQNode* Y=*X->partial_children.begin();
  PQNode* Z=*(++(X->partial_children.begin()));
  PQNode* EC=NULL;
  PQNode* FC=NULL;  //empty/full child/sibling of Y

  PQNode *ZF=NULL;  //the child of Y created to hold full X's children

  //find the empty and full endmost child of Y
  if (Y->endmost_children[0]->label==empty)
  {
    EC=Y->endmost_children[0];
    FC=Y->endmost_children[1];
  }
  else
  {
    EC=Y->endmost_children[1];
    FC=Y->endmost_children[0];
  }
  //check that we are indeed matching the pattern
  if (EC->label!=empty || FC->label==empty)
    return false;
    
  //move the full children of X to be children of Y
  if (X->full_children.size()>0)
  {
    //only 1 full child
    if (X->full_children.size()==1)
    {
      ZF=*(X->full_children.begin());
      X->circular_link.remove(ZF);
    }
    //multiple full children - must be placed in a P-node
    else
    {
      //create ZF to be a new p-node
      ZF=new PQNode;
      ZF->label=full;
      ZF->type=pnode;

      //for all the full nodes in X, set their parent to be ZF
      for (set<PQNode *>::iterator W=X->full_children.begin();W!=X->full_children.end();W++)
      {
        (*W)->parent=ZF;
        X->circular_link.remove(*W);
        ZF->circular_link.push_back(*W);
      }
    }
    //more updates
    ZF->parent=Y;
    FC->immediate_siblings.insert(ZF);      
    ZF->immediate_siblings.insert(FC);
    if (Y->endmost_children[0]==FC)
             Y->endmost_children[0]=ZF;
    if (Y->endmost_children[1]==FC)
      Y->endmost_children[1]=ZF;      
    
    //now, incorporate the other partial child

    //find the empty and full endmost child of Z
    if (Z->endmost_children[0]->label==empty)
    {
      EC=Z->endmost_children[0];
      FC=Z->endmost_children[1];
    }
    else
    {
      EC=Z->endmost_children[1];
      FC=Z->endmost_children[0];
    }
    //check that we are indeed matching the pattern
    if (EC->label!=empty || FC->label==empty)
      return false;

    //connect the children of the two partials together
    ZF->immediate_siblings.insert(FC);
    FC->immediate_siblings.insert(ZF);

    //adjust the parent Y
    if (Y->endmost_children[0]==ZF)
      Y->endmost_children[0]=EC;
    if (Y->endmost_children[1]==ZF)
      Y->endmost_children[1]=EC;
    FC->parent=Y;
    EC->parent=Y;
  }
  else  //no full children
  {
    PQNode *ZFC=NULL;  //Z's full child

    //figure out which sides of Y and Z to connect together
    if (Z->endmost_children[0]->label==full)
    {
      ZFC=Z->endmost_children[0];
      if (Y->endmost_children[0]==FC)
        Y->endmost_children[0]=Z->endmost_children[1];
      if (Y->endmost_children[1]==FC)
        Y->endmost_children[1]=Z->endmost_children[1];
    }
    else
    {
      ZFC=Z->endmost_children[1];
      if (Y->endmost_children[0]==FC)
        Y->endmost_children[0]=Z->endmost_children[0];
      if (Y->endmost_children[1]==FC)
        Y->endmost_children[1]=Z->endmost_children[0];
    }
    Y->endmost_children[0]->parent=Y;
    Y->endmost_children[1]->parent=Y;

    FC->immediate_siblings.insert(ZFC);
    ZFC->immediate_siblings.insert(FC);
  }
    
  
  //adjust the root X
  //we dont need Z any more
  X->circular_link.remove(Z);
  Z->endmost_children[0]=NULL;
  Z->endmost_children[1]=NULL;
  delete Z;

  //if X now only has one child, get rid of X
  if (X->circular_link.size()==1)
  {
  
    PQNode* theParent = X->parent;
    Y->parent = X->parent;
    Y->pertinent_leaf_count = X->pertinent_leaf_count;
    Y->label=partial;
  
    if (theParent != NULL)  // Parent is not root of tree
    {  
      //add Y to it's parent's list of partial children
      theParent->partial_children.insert(Y);
  
      // Update parent to handle the switch
      if (theParent->type == pnode)
      {
        theParent->circular_link.remove(X);  
        theParent->circular_link.push_back(Y);
      }
      else { // Parent is a Q-node
        // Update the immediate siblings list by removing X and adding Y
        for (set<PQNode *>::iterator i = X->immediate_siblings.begin();
	    i != X->immediate_siblings.end(); i++) {
          (*i)->immediate_siblings.erase(X);
          (*i)->immediate_siblings.insert(Y);
        }
        if (theParent->endmost_children[0] == X)
          theParent->endmost_children[0] = Y;
        if (theParent->endmost_children[1] == X)
          theParent->endmost_children[1] = Y;
      }
    }
    else {  // This is the root of our tree, act accordingly
      root = Y;
      Y->parent = NULL;
      
      //delete X, but not it's children
      X->circular_link.clear();
      delete X;
    }  
  }
  return true;
}

// This procedure is the first pass of the Booth & Leuker PQTree algorithm.
// It processes the pertinent subtree of the PQ-Tree to determine the mark
// of every node in that subtree.  The pseudonode, if created, is returned so
// that it can be deleted at the end of the reduce step.
bool PQTree::bubble(set<int> S) {
  //initialize variables
  queue<PQNode*> q;
  block_count = 0;
  blocked_nodes = 0;
  off_the_top = 0;
  
  set<PQNode* > blocked_list;  // Stores blocked guys that aren't unblocked

  // Insert the set's leaves into the queue
  for (set<int>::iterator i = S.begin();i != S.end(); i++) {
    PQNode *temp = leafAddress[*i];
    if (temp == NULL)  // Make sure we have this in our leaves already
      return false;
    q.push(temp);
  }

  while(q.size() + block_count + off_the_top > 1) {
    //check to see if there are still guys in the queue
    if (q.empty())
      return false;
    
    // Remove X from the front of the queue
    PQNode* X = q.front();
    q.pop();
    

    // Mark X as blocked
    X->mark = blocked;
    
    // Get the set of blocked and unblocked siblings
    set<PQNode *> US, BS;
    for (set<PQNode *>::iterator i = X->immediate_siblings.begin();
	i != X->immediate_siblings.end(); i++) {
      if ((*i)->mark == blocked) {
        BS.insert(*i);
      } else if ((*i)->mark == unblocked) {
        US.insert(*i);
      }
    }
    // We can assign a parent to X if one of its immediate siblings is unblocked
    // Or it has 0/1 immediate siblings meaning it is a corner child of a q node
    // Or a child of a p node
    if (!US.empty()) {
      X->parent=(*US.begin())->parent;
      X->mark=unblocked;
    }
    else if (X->immediate_siblings.size() < 2) {
      X->mark = unblocked;
    }
    
    // If it is unblocked, we can process it.
    if (X->mark == unblocked) {
      int listSize = 0;
      PQNode* Y = X->parent;
      if (!BS.empty()) {
        X->mark = blocked;  // Will be unblocked by recursive unblocksiblings
        listSize = unblockSiblings(X, Y, NULL);
        Y->pertinent_child_count += listSize-1;
      }
      
      if (Y == NULL) {  // Currently at root node
        off_the_top = 1;
      } else {
        Y->pertinent_child_count++;
        if (Y->mark == unmarked) {
          q.push(Y);
          Y->mark = queued;
        }
      }
      block_count -= BS.size();
      blocked_nodes -= listSize;          
    } else {
      block_count += 1-BS.size();
      blocked_nodes += 1;
      blocked_list.insert(X);
    }
  }

  if (block_count > 1 || (off_the_top == 1 && block_count != 0))
    return false;
  
  int correctblockedcount = 0;
  for (set<PQNode*>::iterator i = blocked_list.begin();
      i != blocked_list.end(); i++) {
      if ((*i)->mark == blocked)
        correctblockedcount++;
  }

  // In this case, we have a block that is contained within a Q-node.  We must
  // assign a psuedonode Z of type Q-node to handle it.
  if (block_count == 1 && correctblockedcount > 1)
  {
    pseudonode = new PQNode;
    pseudonode->type = qnode;
    pseudonode->pseudonode = true;
    int side = 0;
    pseudonode->pertinent_child_count = 0;
    
    //figure out which nodes are still blocked
    //and which of those are the endmost children
    for (set<PQNode*>::iterator i=blocked_list.begin();
	i != blocked_list.end(); i++)
      if ((*i)->mark == blocked) {
        pseudonode->pertinent_child_count++;
        pseudonode->pertinent_leaf_count += (*i)->pertinent_leaf_count;
        int count=0;  //count number of immediate siblings
        int loop=0;
        for (set<PQNode*>::iterator j = (*i)->immediate_siblings.begin();
	    j != (*i)->immediate_siblings.end() && loop < 2;
	    j++) {
          loop++;

          if ((*j)->mark == blocked) {
            count++;
          } else {
            (*i)->immediate_siblings.erase(*j);
            (*j)->immediate_siblings.erase(*i);
            pseudonode->pseudo_neighbors[side] = *j;
          }
        }
        (*i)->parent = pseudonode;
        (*i)->pseudochild = true;
        if (count < 2)
          pseudonode->endmost_children[side++] = *i;
      }
  }
  return true;
}  

bool PQTree::reduceStep(set<int> S) {
  // Build a queue with all the pertinent leaves in it
  queue<PQNode*> q;
  for (set<int>::iterator i=S.begin();i!=S.end();i++) {
    PQNode* X = leafAddress[*i];
    if (X == NULL)
      return false;
    X->pertinent_leaf_count = 1;
    q.push(X);
  }
  
  while(!q.empty()) {
    // Remove X from the front of the queue
    PQNode* X = q.front();
    q.pop();

    if (X->pertinent_leaf_count<S.size()) {  // X is not root
      // Update X's parent Y
      PQNode* Y = X->parent;
      Y->pertinent_leaf_count += X->pertinent_leaf_count;
      Y->pertinent_child_count--;
      // Push Y onto the queue if it has no more pertinent children
      if (Y->pertinent_child_count == 0)
        q.push(Y);
      
      // Test Against various templates
      if (template_L1(X)) {}
      else if (template_P1(X,false)) {}
      else if (template_P3(X)) {}
      else if (template_P5(X)) {}
      else if (template_Q1(X)) {}
      else if (template_Q2(X)) {}
      else {
        cleanPseudo();
        return false;
      }
    } else {  //X is root
      if (template_L1(X)) {}
      else if (template_P1(X,true)) {}
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
  if (pseudonode!=NULL) {
    for (int i = 0; i < 2; i++) {
      pseudonode->endmost_children[i]->immediate_siblings.insert(
	  pseudonode->pseudo_neighbors[i]);
      pseudonode->pseudo_neighbors[i]->immediate_siblings.insert(
	  pseudonode->endmost_children[i]);
    }

    pseudonode->endmost_children[0] = NULL;
    pseudonode->endmost_children[1] = NULL;
    delete pseudonode;
    pseudonode=NULL;
  }
}

//basic constructor from an initial set
PQTree::PQTree(set<int> S) {
  //set up the root node as a P-Node initially
  root = new PQNode;
  root->type = pnode;  
  invalid = false;
  pseudonode = NULL;
  block_count = 0;
  blocked_nodes = 0;
  off_the_top = 0;
  for (set<int>::iterator i = S.begin(); i != S.end(); i++) {
    PQNode *newNode;
    newNode = new PQNode(*i);
    leafAddress[*i] = newNode;
    newNode->parent = root;
    newNode->type = leaf;
    root->circular_link.push_back(newNode);
  }
}

string PQTree::Print() {
  string out;
  root->Print(&out);
  return out;
}

//reduces the tree but protects if from becoming invalid
//if the reduction fails, takes more time
bool PQTree::safeReduce(set<int> S) {
  //using a backup copy to enforce safety
  PQTree toCopy(*this);
  
  if (!reduce(S)) {
    //reduce failed, so perform a copy
    root = new PQNode(*toCopy.root);
    block_count = toCopy.block_count;
    blocked_nodes = toCopy.blocked_nodes;
    off_the_top = toCopy.off_the_top;
    invalid = toCopy.invalid;
    leafAddress.clear();
    root->FindLeaves(leafAddress);  
    return false;
  }
  return true;
}

bool PQTree::safeReduceAll(list<set<int> > L) {
  //using a backup copy to enforce safety
  PQTree toCopy(*this);
  if (!reduceAll(L)) {
    //reduce failed, so perform a copy
    root = new PQNode(*toCopy.root);
    block_count = toCopy.block_count;
    blocked_nodes = toCopy.blocked_nodes;
    off_the_top = toCopy.off_the_top;
    invalid = toCopy.invalid;
    leafAddress.clear();
    root->FindLeaves(leafAddress);  
    return false;
  }
  return true;
}  

  
bool PQTree::reduce(set<int> S) {
  if (S.size() < 2) {
    reductions.push_back(S);
    return true;
  }
  if (invalid)
    return false;
  if (!bubble(S)) {
    invalid = true;
    return false;
  }
  if (!reduceStep(S)) {
    invalid = true;
    return false;
  }
  
  // We dont need any more pseudonodes at least until the next reduce
  if (pseudonode != NULL) {
    pseudonode->endmost_children[0] = NULL;
    pseudonode->endmost_children[1] = NULL;
    delete pseudonode;
    pseudonode = NULL;
  }

  // Reset all the temporary variables for the next round
  root->Reset();
  
  // Store the reduction set for later lookup
  reductions.push_back(S);
  return true;
}

bool PQTree::reduceAll(list<set<int> > L) {
  if (invalid)
    return false;

  for (list<set<int> >::iterator S = L.begin(); S != L.end(); S++) {
    if (S->size() < 2) {
      reductions.push_back(*S);
      continue;
    }
    if (!bubble(*S)) {
      invalid = true;
      return false;
    }
    if (!reduceStep(*S)) {
      invalid = true;
      return false;
    }

    //we dont need any more pseudonodes
    //at least until the next reduce
    if (pseudonode != NULL) {
      pseudonode->endmost_children[0] = NULL;
      pseudonode->endmost_children[1] = NULL;
      delete pseudonode;
      pseudonode=NULL;
    }
    //Reset all the temporary variables for the next round
    root->Reset();
  
    //store the reduction set for later lookup
    reductions.push_back(*S);
  }  
  return true;
}

list<int> PQTree::frontier() {
  list<int> out;
  root->FindFrontier(out);
  return out;
}

list<int> PQTree::reducedFrontier() {
  list<int> out, inter;
  root->FindFrontier(inter);
  set<int> allContained;
  for (list<set<int> >::iterator j = reductions.begin();
      j != reductions.end(); j++) {
      allContained = SetMethods::SetUnion(allContained, *j);
  }
  for (list<int>::iterator j = inter.begin(); j != inter.end(); j++) {
    if (SetMethods::SetFind(allContained, *j))
      out.push_back(*j);
  }
  return out;
}

list<set<int> > PQTree::getReductions() {
  return reductions;
}

set<int> PQTree::getContained() {
  set<int> out;
  for (list<set<int> >::iterator i = reductions.begin();
      i!=reductions.end(); i++) {
    out = SetMethods::SetUnion(out,*i);
  }
  return out;
}

//default destructor, just needs to delete the root
//for safety's sake, we delete the pseudonode too
PQTree::~PQTree() {
  delete root;
  //delete pseudonode;
}
