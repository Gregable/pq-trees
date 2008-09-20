#include <iostream>
#include <set>
#include "pqtree.h"

void testbed()
{
  set<int> S;
  for(int i=1;i<9;i++)
    S.insert(i);
  PQTree P(S);

  cout << "PQ Tree with 8 elements and no reductions" << endl;  
  cout << P.Print() << endl;
  
  cout << "Reducing by set {4, 3}" << endl;
  S.clear();
  S.insert(4);
  S.insert(3);
  cout << P.reduce(S) << endl;
  cout << P.Print() << endl;
  
  cout << "Reducing by set {4, 3, 6}" << endl;
  S.clear();
  S.insert(6);
  S.insert(4);
  S.insert(3);
  cout << P.reduce(S) << endl;
  cout << P.Print() << endl;
  
  cout << "Reducing by set {4, 3, 5}" << endl;
  S.clear();
  S.insert(4);
  S.insert(3);
  S.insert(5);
  cout << P.reduce(S) << endl;
  cout << P.Print() << endl;
    
  cout << "Reducing by set {4, 5}" << endl;
  S.clear();
  S.insert(4);
  S.insert(5);
  cout << P.reduce(S) << endl;
  cout << P.Print() << endl;
  
  cout << "Reducing by set {2, 6}" << endl;
  S.clear();
  S.insert(2);
  S.insert(6);
  cout << P.reduce(S) << endl;
  cout << P.Print() << endl;
  
  cout << "Reducing by set {1, 2}" << endl;
  S.clear();
  S.insert(1);
  S.insert(2);
  cout << P.reduce(S) << endl;
  cout << P.Print() << endl;
  
  cout << "Reducing by set {4, 5}" << endl;
  S.clear();
  S.insert(4);
  S.insert(5);
  cout << P.reduce(S) << endl;
  cout << P.Print() << endl;
  
  cout << "Reducing by set {5, 3} - will fail" << endl;
  S.clear();
  S.insert(5);
  S.insert(3);
  cout << P.reduce(S) << endl;
  cout << P.Print() << endl;
}
  

int main(int argc, char **argv)
{
  testbed();
}


