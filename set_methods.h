// Methods that define commonly used operations on stl sets.

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

#include <algorithm>
#include <set>

using namespace std;

class SetMethods {
 public:
  // Computes the set intersection between sets |A| and |B|.
  template <class T> static set<T> SetIntersection(const set<T>& A,
                                                   const set<T>& B) {
    set<T> out;
    set_intersection(A.begin(), A.end(), B.begin(), B.end(),
                     inserter(out, out.begin()));
    return out;
  }

  // Computes the set union between sets |A| and |B|
  template <class T> static set<T> SetUnion(const set<T>& A, const set<T>& B) {
    set<T> out;
    set_union(A.begin(), A.end(), B.begin(), B.end(),
              inserter(out, out.begin()));
    return out;
  }

  // Searches for |needle| in |haystack|.
  template <class T> static bool SetFind(const set<T>& haystack,
                                         const T& needle) {
    return haystack.count(needle) > 0;
  }

  // Computes the set difference between sets |pos| and |neg|.
  // Args:
  //   pos: set from which we subtract elements from neg.
  //   neg: set whose elements we subtract from pos.
  template <class T> static set<T> SetDifference(set<T> pos, set<T> neg) {
    set<T> out;
    set_difference(pos.begin(), pos.end(), neg.begin(), neg.end(),
                   inserter(out, out.begin()));
    return out;
  }
};
