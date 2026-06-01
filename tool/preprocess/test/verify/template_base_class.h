// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#pragma once

//
// INTERFACE definition follows
//

#line 2 "template_base_class.cpp"

template<typename T>
struct Base {};
#line 5 "template_base_class.cpp"

template<typename X>
struct Derived : Base<int>
{
public:  
#line 13 "template_base_class.cpp"
  Derived();
};

//
// IMPLEMENTATION of function templates
//


#line 11 "template_base_class.cpp"

 
template<typename T> Derived<T>::Derived() : Base<int>()
{}
