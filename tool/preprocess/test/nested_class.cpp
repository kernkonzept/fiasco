INTERFACE:

class Outer
{
public:
  class Inner
  {
    void *test() const;

    template<typename R>
    R *test_template() const;
  };

  template<typename T>
  class Inner_template
  {
    T *test() const;

    template<typename R>
    R *test_template(T *a) const;
  };
};

IMPLEMENTATION:

IMPLEMENT inline
void *
Outer::Inner::test() const
{ return 0; }

IMPLEMENT template<typename R>
R *
Outer::Inner::test_template() const
{ return 0; }

IMPLEMENT template<typename T>
T *
Outer::Inner_template<T>::test() const
{ return 0; }

IMPLEMENT template<typename T> template<typename R>
R *
Outer::Inner_template<T>::test_template(T *a) const
{ return reinterpret_cast<R *>(a); }
