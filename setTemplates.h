/*
 * A group of templates that define commonly used operations on stl sets.
 */
 
#include <algorithm>
#include <set>

using namespace std;

template <class T> set<T> setintersection(set<T> A, set<T> B)
{
  set<T> out;
  set_intersection(A.begin(),A.end(),B.begin(),B.end(),
		   inserter(out,out.begin()));
  return out;
}
template <class T> set<T> setunion(set<T> A, set<T> B)
{
  set<T> out;
  set_union(A.begin(),A.end(),B.begin(),B.end(),
            inserter(out,out.begin()) );
  return out;
}
template <class T> bool setfind(set<T> A, T B)
{
  set<T> temp;
  temp.insert(B);
  return setintersection(A,temp).size() > 0;
}
template <class T> set<T> setdifference(set<T> A, set<T> B)
{
  set<T> out;
  set_difference(A.begin(),A.end(),B.begin(),B.end(),
          inserter(out,out.begin()) );
  return out;
}

