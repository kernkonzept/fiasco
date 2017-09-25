#include <vector>

IMPLEMENTATION:

template <typename T>
std::vector<T> 
vec(T f1 = T(), T f2 = T(), T f3 = T(),
    T f4 = T(), T f5 = T(), T f6 = T())
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

extern "C"
void
disasm_bytes(char *buffer, unsigned len, unsigned va, unsigned task,
             int show_symbols, int show_intel_syntax,
             int (*peek_task)(unsigned addr, unsigned task),
             const char* (*get_symbol)(unsigned addr, unsigned task)) WEAK;
