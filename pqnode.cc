// See PQNode.h

#include <list>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <cstdio>

#include "pqnode.h"

using namespace std;

int PQNode::ChildCount() {
  return circular_link.size();
}

PQNode* PQNode::CopyAsChild(const PQNode& toCopy) {
  PQNode* temp = new PQNode(toCopy);
  temp->parent=this;
  return temp;
}

PQNode::PQNode(const PQNode& toCopy) {
  Copy(toCopy);
}

void PQNode::Copy(const PQNode& toCopy) {
  // Copy the easy stuff
  leaf_value             = toCopy.leaf_value;
  pertinent_leaf_count  = toCopy.pertinent_leaf_count;
  pertinent_child_count = toCopy.pertinent_child_count;
  type                  = toCopy.type;
  mark                  = toCopy.mark;
  label                 = toCopy.label;
  pseudonode            = toCopy.pseudonode;
  pseudochild           = toCopy.pseudochild;

  // Make sure that these are unset initially
  immediate_siblings.clear();
  partial_children.clear();
  full_children.clear();
  circular_link.clear();
  parent	      = NULL;
  endmost_children[0] = NULL;
  endmost_children[1] = NULL;

  // Copy the nodes in circular link for pnodes.
  // If it is not a pnode, it will be empty, so this will be a no-op.
  for(list<PQNode*>::const_iterator i = toCopy.circular_link.begin();
      i != toCopy.circular_link.end(); i++)
    circular_link.push_back(CopyAsChild(**i));

  // Copy the sibling chain for qnodes
  if(type == qnode) {
    PQNode *current, *last;
    // Pointers to nodes we are going to copy
    PQNode *curCopy, *lastCopy, *nextCopy;  
    endmost_children[0] = CopyAsChild(*toCopy.endmost_children[0]);
    curCopy = toCopy.endmost_children[0];
    lastCopy = NULL;
    last = endmost_children[0];
    
    // Get all the intermediate children
    nextCopy = curCopy->QNextChild(lastCopy);
    while(nextCopy != NULL)
    {
      lastCopy = curCopy;
      curCopy  = nextCopy;
      current  = CopyAsChild(*curCopy);
      current->immediate_siblings.insert(last);
      last->immediate_siblings.insert(current);
      last = current;

      nextCopy = curCopy->QNextChild(lastCopy);
    }

    // Now set our last endmost_children pointer to our last child
    endmost_children[1] = current;
  }
}

PQNode& PQNode::operator=(const PQNode& toCopy) {
  // Check for self copy
  if (&toCopy == this)
    return *this;

  Copy(toCopy);
    
  // Return ourself for chaining.
  return *this;
}

// Return the next child in the immediate_siblings chain given a last pointer.
// If last pointer is null, will return the first sibling.
PQNode* PQNode::QNextChild(PQNode *last) {
  if (*(immediate_siblings.begin()) == last) {
    if (1 == immediate_siblings.size()) {
      return NULL;
    } else {
      return *(++immediate_siblings.begin());
    }
  } else {
    // Last is not either of our siblings.
    // This occurs when we are on the edge of a pseudonode
    if (last == NULL && 2 == immediate_siblings.size())
    {
      if ((*(immediate_siblings.begin()))->label != empty)
        return *(immediate_siblings.begin());
      return *(++immediate_siblings.begin());
    }
    return *(immediate_siblings.begin());
  }
}
  
// Removes this node from a q-parent and puts toInsert in it's place
void PQNode::SwapQ(PQNode *toInsert) {
  toInsert->pseudochild = pseudochild;
  if (parent->endmost_children[0] == this) {
    parent->endmost_children[0] = toInsert;
  } else if (parent->endmost_children[1] == this) {
    parent->endmost_children[1] = toInsert;
  }
  
  toInsert->immediate_siblings.clear();
  for(set<PQNode *>::iterator i = immediate_siblings.begin();
      i != immediate_siblings.end(); i++) {
    toInsert->immediate_siblings.insert(*i);
    (*i)->immediate_siblings.erase(this);
    (*i)->immediate_siblings.insert(toInsert);
  }
  immediate_siblings.clear();
  parent = NULL;
}
  
PQNode::PQNode(int value) {
  leaf_value             = value;
  label                 = empty;
  mark                  = unmarked;
  type                  = leaf;
  PQNode *parent        = NULL;
  pertinent_child_count = 0;
  pertinent_leaf_count  = 0;
  parent                = NULL;
  endmost_children[0]   = NULL;
  endmost_children[1]   = NULL;
}
  
PQNode::PQNode() {
  parent=NULL;
  pseudonode=false;
  label=empty;
  mark=unmarked;
  pertinent_child_count=0;
  pertinent_leaf_count=0;
  endmost_children[0]=NULL;
  endmost_children[1]=NULL;
}

PQNode::~PQNode() {
  if (type == qnode) {
    PQNode *last     = NULL;
    PQNode *current  = endmost_children[0];
    while(current) {
      PQNode *next = current->QNextChild(last);
      delete last;
      last    = current;
      current = next;
    }
    delete last;
  } else if (type == pnode) {  
    for (list<PQNode*>::iterator i = circular_link.begin();
         i != circular_link.end(); i++)
      delete *i;
    circular_link.clear();
  }
}

// FindLeaves, FindFrontier, Reset, and Print are very similar recursive
// functions.  Each is a depth-first walk of the entire tree looking for data
// at the leaves.  
// TODO: Could probably be implemented better using function pointers.
void PQNode::FindLeaves(map<int, PQNode*> &leafAddress) {
  if (type == leaf) {
    leafAddress[leaf_value] = this;
  } else if (type == pnode) {
    // Recurse by asking each child in circular_link to find it's leaves.
    for (list<PQNode*>::iterator i = circular_link.begin();
         i!=circular_link.end(); i++)
      (*i)->FindLeaves(leafAddress);
  } else if (type == qnode) {
    // Recurse by asking each child in my child list to find it's leaves.
    PQNode *last    = NULL;
    PQNode *current = endmost_children[0];
    while (current) {
      current->FindLeaves(leafAddress);
      PQNode *next = current->QNextChild(last);
      last    = current;
      current = next;
    }
  }
}
  
void PQNode::FindFrontier(list<int> &ordering) {
  if (type == leaf) {
    ordering.push_back(leaf_value);
  } else if (type == pnode) {
    for(list<PQNode*>::iterator i = circular_link.begin();
        i != circular_link.end();i++)
      (*i)->FindFrontier(ordering);
  } else if (type == qnode) {
    PQNode *last    = NULL;
    PQNode *current = endmost_children[0];
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
  if (type == pnode) {
    for (list<PQNode*>::iterator i = circular_link.begin();
         i != circular_link.end(); i++)
      (*i)->Reset();
  } else if(type == qnode) {
    PQNode *last    = NULL;
    PQNode *current = endmost_children[0];
    while (current) {
      current->Reset();
      PQNode *next = current->QNextChild(last);
      last    = current;
      current = next;
    }
  }
  
  full_children.clear();
  partial_children.clear();
  label                 = empty;
  mark                  = unmarked;
  pertinent_child_count = 0;
  pertinent_leaf_count  = 0;
  pseudochild           = false;
  pseudonode            = false;
}

// Walks the tree from the top and prints the tree structure to the string out.
// Used primarily for debugging purposes.
void PQNode::Print(string *out) {
  if (type == leaf) {
    char value_str[10];
    sprintf(value_str, "%d", leaf_value);
    *out += value_str;
  } else if (type == pnode) {
    *out += "(";
    for (list<PQNode*>::iterator i = circular_link.begin();
         i != circular_link.end(); i++) {
      (*i)->Print(out);
      // Add a space if there are more elements remaining.
      if (++i != circular_link.end())
        *out += " ";
      --i;
    }
    *out += ")";
  } else if (type==qnode) {
    *out += "[";
    PQNode *last     = NULL;
    PQNode *current  = endmost_children[0];
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
