INTERFACE:

template< typename T >
class Test
{
public:
  void test_func();
};


IMPLEMENTATION:

IMPLEMENT
template< typename T >
// comment
void __attribute__((deprecated))
Test<T>::test_func()
{
}

PUBLIC
template< typename T > // comment 1
template<
  typename X, // comment within template args list
  typename X2 // another comment in tl args
>
// comment 2
void __attribute__((deprecated))
Test<T>::test_func2<X, X2>()
{
}
