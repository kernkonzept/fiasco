INTERFACE:

template<typename T>
struct Base {};

template<typename X>
struct Derived : Base<int>
{};

IMPLEMENTATION:

PUBLIC template<typename T> inline
Derived<T>::Derived() : Base<int>()
{}

