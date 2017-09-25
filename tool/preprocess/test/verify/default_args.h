// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef default_args_h
#define default_args_h

//
// INTERFACE definition follows 
//


#line 6 "default_args.cpp"
template <typename T> std::vector<T> 
vec(T f1 = T(), T f2 = T(), T f3 = T(),
    T f4 = T(), T f5 = T(), T f6 = T());

//
// IMPLEMENTATION includes follow (for use by inline functions/templates)
//

#line 1 "default_args.cpp"
#include <vector>

//
// IMPLEMENTATION of function templates
//


#line 4 "default_args.cpp"


template <typename T> std::vector<T> 
vec(T f1, T f2, T f3,
    T f4, T f5, T f6)
{
  std::vector<T> v;
  if (f1 != T()) {
    v.push_back (f1);
    if (f2 != T()) {
      v.push_back (f2);
      if (f3 != T()) {
        v.push_back (f3);
        if (f4 != T()) {
          v.push_back (f4);
          if (f5 != T()) {
            v.push_back (f5);
            if (f6 != T()) {
              v.push_back (f6);
            }}}}}}
  return v;
}

#endif // default_args_h
