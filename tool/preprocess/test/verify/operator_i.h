// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#ifndef operator_i_h
#define operator_i_h
#line 40 "operator.cpp"

// Some systematic tests (contributed by Matthias Daum)

struct X { 
public:  
#line 45 "operator.cpp"
  void* operator new(unsigned int);
  
#line 46 "operator.cpp"
  void operator delete(void*);
  
#line 47 "operator.cpp"
  void* operator new[] (unsigned int, int);
  
#line 48 "operator.cpp"
  void operator delete[] (void*);
  
#line 49 "operator.cpp"
  X operator+ (const X&);
  
#line 50 "operator.cpp"
  X operator- (const X&);
  
#line 51 "operator.cpp"
  X operator* (const X&);
  
#line 52 "operator.cpp"
  X operator/ (const X&);
  
#line 53 "operator.cpp"
  X operator% (const X&);
  
#line 54 "operator.cpp"
  X operator^ (const X&);
  
#line 55 "operator.cpp"
  X operator& (const X&);
  
#line 56 "operator.cpp"
  X operator| (const X&);
  
#line 57 "operator.cpp"
  X operator~ ();
  
#line 58 "operator.cpp"
  X operator! ();
  
#line 59 "operator.cpp"
  X& operator= (const X&);
  
#line 60 "operator.cpp"
  bool operator< (const X&);
  
#line 61 "operator.cpp"
  bool operator> (const X&);
  
#line 62 "operator.cpp"
  X& operator+= (const X&);
  
#line 63 "operator.cpp"
  X& operator-= (const X&);
  
#line 64 "operator.cpp"
  X& operator*= (const X&);
  
#line 65 "operator.cpp"
  X& operator/= (const X&);
  
#line 66 "operator.cpp"
  X& operator%= (const X&);
  
#line 67 "operator.cpp"
  X& operator^= (const X&);
  
#line 68 "operator.cpp"
  X& operator&= (const X&);
  
#line 69 "operator.cpp"
  X& operator|= (const X&);
  
#line 70 "operator.cpp"
  X operator<< (const X&);
  
#line 71 "operator.cpp"
  X operator>> (const X&);
  
#line 72 "operator.cpp"
  X& operator>>= (const X&);
  
#line 73 "operator.cpp"
  X& operator<<= (const X&);
  
#line 74 "operator.cpp"
  bool operator== (const X&);
  
#line 75 "operator.cpp"
  bool operator!= (const X&);
  
#line 76 "operator.cpp"
  bool operator<= (const X&);
  
#line 77 "operator.cpp"
  bool operator>= (const X&);
  
#line 78 "operator.cpp"
  bool operator&& (const X&);
  
#line 79 "operator.cpp"
  bool operator|| (const X&);
  
#line 80 "operator.cpp"
  X& operator++ ();
  
#line 81 "operator.cpp"
  X operator++ (int);
  
#line 82 "operator.cpp"
  X& operator-- ();
  
#line 83 "operator.cpp"
  X operator-- (int);
  
#line 84 "operator.cpp"
  X& operator, (const X&);
  
#line 85 "operator.cpp"
  X* operator->* (const X&);
  
#line 86 "operator.cpp"
  X* operator-> ();
  
#line 87 "operator.cpp"
  int operator() (const X&);
  
#line 88 "operator.cpp"
  int operator[] (const X&);
};

#endif // operator_i_h
