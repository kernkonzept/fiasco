
#define SYMBOL_NAME(x) x
#define SYMBOL_NAME_LABEL(x) x##:
#define ALIGN .align 4
#define ENTRY(x) \
  .globl SYMBOL_NAME(x); \
  ALIGN;                 \
  SYMBOL_NAME_LABEL(x)

