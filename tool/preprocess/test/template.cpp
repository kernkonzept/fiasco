INTERFACE:

template<class T>
class stack_t;

template<class T>
class stack_top_t
{
private:
  friend class stack_t<T>;

  // Dont change the layout !!, comp_and_swap2 expects
  // version and _next  next to each other, in that order
  int _version;
  T   *_next;
};

template<class T>
class stack_t
{
private:
  stack_top_t<T> _head;
};

IMPLEMENTATION:

// 
// atomic-manipulation functions
// 

// typesafe variants
template <class I>
inline bool compare_and_swap(I *ptr, I oldval, I newval)
{
  return compare_and_swap(reinterpret_cast<unsigned*>(ptr),
			  *reinterpret_cast<unsigned*>(&oldval),
			  *reinterpret_cast<unsigned*>(&newval));
}

template <class I>
inline bool test_and_set(I *l)
{
  return test_and_set(reinterpret_cast<unsigned*>(l));
}

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

PUBLIC inline
template<class T>
stack_top_t<T>::stack_top_t (int version, T *next)
  : _version (version),
    _next (next)
{}

PUBLIC inline
template<class T>
stack_top_t<T>::stack_top_t ()
  : _version (0),
    _next (0)
{}

// 
// stack_t
// 

PUBLIC
template<class T>
stack_t<T>::stack_t()
  : _head (0, 0)
{}

PUBLIC
template<class T>
int 
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

PUBLIC
template<class T>
T* 
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

// This version of dequeue only returns a value
// if it is equal to the one passed as top
PUBLIC
template<class T>
T* 
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

PUBLIC
template<class T>
T* 
stack_t<T>::first()
{
  return _head._next;
}

PUBLIC
template<class T>
void 
stack_t<T>::reset()
{
  _head._version = 0;
  _head._next    = 0;
}

template<class T>
stack_t<T>*
create_stack()
{
  return new stack_t<T>();
}

template <>
stack_t<int>*
create_stack<int>()
{
  return new stack<int>();
}

template <>
inline
stack_t<bool>*
create_stack<bool>()
{
  return new stack<bool>();
}

// 
// Member templates
// 

class Foo
{
  template <typename T> T* goo(T* t);
};

template <class Bar>
class TFoo
{
};

PUBLIC
template <typename T>
T*
Foo::bar (T* t)
{
}

IMPLEMENT
template <typename T>
T*
Foo::goo (T* t)
{
}

PUBLIC
template <class Bar>
template <typename T>
T*
TFoo::baz (T* t)
{
}

template<typename FOO,
         typename = typename cxx::enable_if<!cxx::is_same<SPACE, Mem_space>::value>::type>
void
template_with_dfl_arg1()
{}

template<typename FOO,
         typename = typename cxx::enable_if<!cxx::is_same<SPACE, Mem_space>::value>::type,
         typename BAR>
void
template_with_dfl_arg2()
{}
