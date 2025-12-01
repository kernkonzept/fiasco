INTERFACE [arm && 32bit]:

class Cpubits
{
public:
  struct Pt4
  {
    constexpr operator bool() { return false; }
  };
};
