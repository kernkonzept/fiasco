INTERFACE:

class Foo
{
};

IMPLEMENTATION:

PUBLIC 
void * 
Foo::operator new (size_t)	// funny comment
{
}

PUBLIC 
Foo&
Foo::operator + (const Foo&)	// funny comment
{
}

PUBLIC 
Foo&
Foo::operator = (const Foo&)	// funny comment
{
}

PUBLIC 
Foo&
Foo::operator * (const Foo&)	// funny comment
{
}

template <typename T, typename A>
std::vector<T, A >& 
operator << (std::vector<T, A>& in, const T& new_elem)
{
  in.push_back (new_elem);
  return in;
}

// Some systematic tests (contributed by Matthias Daum)

struct X { };

PUBLIC inline void* X::operator new(unsigned int) { return (void*)0; }
PUBLIC inline void X::operator delete(void*) { }
PUBLIC inline void* X::operator new  [] (unsigned int, int) { return (void*)0; }
PUBLIC inline void X::operator delete  [] (void*) { }
PUBLIC inline X X::operator + (const X&) { return *this; }
PUBLIC inline X X::operator - (const X&) { return *this; }
PUBLIC inline X X::operator * (const X&) { return *this; }
PUBLIC inline X X::operator / (const X&) { return *this; }
PUBLIC inline X X::operator % (const X&) { return *this; }
PUBLIC inline X X::operator ^ (const X&) { return *this; }
PUBLIC inline X X::operator & (const X&) { return *this; }
PUBLIC inline X X::operator | (const X&) { return *this; }
PUBLIC inline X X::operator ~ () { return *this; }
PUBLIC inline X X::operator ! () { return *this; }
PUBLIC inline X& X::operator = (const X&) {return *this; }
PUBLIC inline bool X::operator < (const X&) { return false; }
PUBLIC inline bool X::operator > (const X&) { return false; }
PUBLIC inline X& X::operator += (const X&) { return *this; }
PUBLIC inline X& X::operator -= (const X&) { return *this; }
PUBLIC inline X& X::operator *= (const X&) { return *this; }
PUBLIC inline X& X::operator /= (const X&) { return *this; }
PUBLIC inline X& X::operator %= (const X&) { return *this; }
PUBLIC inline X& X::operator ^= (const X&) { return *this; }
PUBLIC inline X& X::operator &= (const X&) { return *this; }
PUBLIC inline X& X::operator |= (const X&) { return *this; }
PUBLIC inline X X::operator << (const X&) { return *this; }
PUBLIC inline X X::operator >> (const X&) { return *this; }
PUBLIC inline X& X::operator >>= (const X&) { return *this; }
PUBLIC inline X& X::operator <<= (const X&) { return *this; }
PUBLIC inline bool X::operator == (const X&) { return true; }
PUBLIC inline bool X::operator != (const X&) { return false; }
PUBLIC inline bool X::operator <= (const X&) { return true; }
PUBLIC inline bool X::operator >= (const X&) { return true; }
PUBLIC inline bool X::operator && (const X&) { return false; }
PUBLIC inline bool X::operator || (const X&) { return true; }
PUBLIC inline X& X::operator ++ () { return *this; }
PUBLIC inline X X::operator ++ (int) { return *this; }
PUBLIC inline X& X::operator -- () { return *this; }
PUBLIC inline X X::operator -- (int) { return *this; }
PUBLIC inline X& X::operator , (const X&) { return *this; }
PUBLIC inline X* X::operator ->* (const X&) { return this; }
PUBLIC inline X* X::operator -> () { return this; }
PUBLIC inline int X::operator () (const X&) { return 0; }
PUBLIC inline int X::operator [] (const X&) { return 0; }
