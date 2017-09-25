// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef template_inline_h
#define template_inline_h

//
// INTERFACE definition follows 
//

#line 2 "template.cpp"

template<class T>
class stack_t;
#line 5 "template.cpp"

template<class T>
class stack_top_t
{
private:
  friend class stack_t<T>;

  // Dont change the layout !!, comp_and_swap2 expects
  // version and _next  next to each other, in that order
  int _version;
  T   *_next;

public:  
#line 46 "template.cpp"
  // 
  // stack_top_t
  // 
  
  // template<class T>
  // stack_top_t<T> &
  // stack_top_t<T>::operator=(const stack_top_t& _copy){
  //   _version = _copy._version;
  //   _next    = _copy._next;
  //   return *this;
  // }
  
  
  
  inline stack_top_t (int version, T *next);
  
#line 67 "template.cpp"
  inline stack_top_t ();
};
#line 17 "template.cpp"

template<class T>
class stack_t
{
private:
  stack_top_t<T> _head;

public:  
#line 72 "template.cpp"
  // 
  // stack_t
  // 
  
  
  
  stack_t();
  
#line 84 "template.cpp"
  int 
  insert(T *e);
  
#line 103 "template.cpp"
  T* 
  dequeue();
  
#line 132 "template.cpp"
  // This version of dequeue only returns a value
  // if it is equal to the one passed as top
  
  
  T* 
  dequeue(T *top);
  
#line 164 "template.cpp"
  T* 
  first();
  
#line 172 "template.cpp"
  void 
  reset();
};

#line 27 "template.cpp"
// 
// atomic-manipulation functions
// 

// typesafe variants

template <class I> inline bool compare_and_swap(I *ptr, I oldval, I newval);

#line 41 "template.cpp"
template <class I> inline bool test_and_set(I *l);

#line 180 "template.cpp"
template<class T> stack_t<T>*
create_stack();

#line 187 "template.cpp"
template <> stack_t<int>*
create_stack();

#line 195 "template.cpp"
template <> inline stack_t<bool>*
create_stack();

#line 238 "template.cpp"
template<typename FOO,
         typename = typename cxx::enable_if<!cxx::is_same<SPACE, Mem_space>::value>::type> void
template_with_dfl_arg1();

#line 244 "template.cpp"
template<typename FOO,
         typename = typename cxx::enable_if<!cxx::is_same<SPACE, Mem_space>::value>::type,
         typename BAR> void
template_with_dfl_arg2();

//
// IMPLEMENTATION of inline functions (and needed classes)
//


#line 45 "template.cpp"

// 
// stack_top_t
// 

// template<class T>
// stack_top_t<T> &
// stack_top_t<T>::operator=(const stack_top_t& _copy){
//   _version = _copy._version;
//   _next    = _copy._next;
//   return *this;
// }



template<class T> inline stack_top_t<T>::stack_top_t (int version, T *next)
  : _version (version),
    _next (next)
{}

#line 64 "template.cpp"



template<class T> inline stack_top_t<T>::stack_top_t ()
  : _version (0),
    _next (0)
{}

#line 26 "template.cpp"

// 
// atomic-manipulation functions
// 

// typesafe variants

template <class I> inline bool compare_and_swap(I *ptr, I oldval, I newval)
{
  return compare_and_swap(reinterpret_cast<unsigned*>(ptr),
			  *reinterpret_cast<unsigned*>(&oldval),
			  *reinterpret_cast<unsigned*>(&newval));
}

#line 39 "template.cpp"


template <class I> inline bool test_and_set(I *l)
{
  return test_and_set(reinterpret_cast<unsigned*>(l));
}

#line 192 "template.cpp"



template <> inline stack_t<bool>*
create_stack<bool>()
{
  return new stack<bool>();
}

//
// IMPLEMENTATION of function templates
//


#line 71 "template.cpp"

// 
// stack_t
// 



template<class T> stack_t<T>::stack_t()
  : _head (0, 0)
{}

#line 81 "template.cpp"



template<class T> int 
stack_t<T>::insert(T *e)
{
  stack_top_t<T>  old_head,
                  new_head;

  do {
      e->set_next(_head._next);
      old_head             = _head;
      new_head._version    = _head._version+1;
      new_head._next       = e;
  }  while (! compare_and_swap2((int *) &_head, 
				(int *) &old_head,
				(int *) &new_head));
  return new_head._version;
}

#line 100 "template.cpp"



template<class T> T* 
stack_t<T>::dequeue()
{
  stack_top_t<T> old_head,
                 new_head;

  T *first;

  do {
    old_head          = _head;

    first = _head._next;
    if(! first){
      break;
    }

    new_head._next    = first->get_next();
    new_head._version = _head._version + 1;
  }  while (! compare_and_swap2((int *) &_head,
				(int *) &old_head,
				(int *) &new_head));
  //  XXX Why did the old implementation test on e ?
  //  while (e && ! compare_and_swap(&_first, e, e->list_property.next));
  //  This is necessary to handle the case of a empty stack.

  //  return old_head._next;
  return first;
}

#line 131 "template.cpp"

// This version of dequeue only returns a value
// if it is equal to the one passed as top


template<class T> T* 
stack_t<T>::dequeue(T *top)
{
  stack_top_t<T> old_head,
                 new_head;
  //  stack_elem_t  *first;

  old_head._version  = _head._version;   // version doesnt matter
  old_head._next     = top;              // cas will fail, if top aint at top
  if(!_head._next){                      // empty stack
    return 0;
  }
  new_head._version = _head._version + 1;
  new_head._next    = top->get_next();
  

  if(! compare_and_swap2((int *) &_head,
			 (int *) &old_head,
			 (int *) &new_head))
    // we didnt succeed
    return 0;
  else 
    // top was on top , so we dequeued it
    return top;
}

#line 161 "template.cpp"



template<class T> T* 
stack_t<T>::first()
{
  return _head._next;
}

#line 169 "template.cpp"



template<class T> void 
stack_t<T>::reset()
{
  _head._version = 0;
  _head._next    = 0;
}

#line 178 "template.cpp"


template<class T> stack_t<T>*
create_stack()
{
  return new stack_t<T>();
}

#line 236 "template.cpp"


template<typename FOO,
         typename> void
template_with_dfl_arg1()
{}

#line 242 "template.cpp"


template<typename FOO,
         typename,
         typename BAR> void
template_with_dfl_arg2()
{}

#endif // template_inline_h
