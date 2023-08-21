/*
 * vi:ft=c
 */

#pragma once

#define JDB_TABLE(name) \
  . = ALIGN(32);                               \
  PROVIDE(_jdb_ ## name ## _table = .);        \
  KEEP(*(SORT(.debug.jdb.name ## _table)));    \
  PROVIDE(_jdb_ ## name ## _table_end = .);

/* Constructor and destructor lists, ordered by priority.  The lists
   are traversed from end to start, therefore the *_END__ symbols
   precede the *_LIST__ symbols. */
#define DEFINE_INIT_ARRAYS                      \
                                                \
    __PER_CPU_INIT_ARRAY_START__ = .;           \
    KEEP (*(.init_array.65534))                 \
    KEEP (*(.init_array.65533))                 \
    KEEP (*(.init_array.65532))                 \
    KEEP (*(.init_array.65531))                 \
    KEEP (*(.init_array.65530))                 \
    KEEP (*(.init_array.65529))                 \
    KEEP (*(.init_array.65528))                 \
    KEEP (*(.init_array.65527))                 \
    KEEP (*(.init_array.65526))                 \
    KEEP (*(.init_array.65525))                 \
    __PER_CPU_INIT_ARRAY_END__ = .;             \
                                                \
    __PER_CPU_LATE_INIT_ARRAY_START__ = .;      \
    KEEP (*(.init_array.65524))                 \
    KEEP (*(.init_array.65523))                 \
    KEEP (*(.init_array.65522))                 \
    KEEP (*(.init_array.65521))                 \
    KEEP (*(.init_array.65520))                 \
    KEEP (*(.init_array.65519))                 \
    KEEP (*(.init_array.65518))                 \
    KEEP (*(.init_array.65517))                 \
    KEEP (*(.init_array.65516))                 \
    KEEP (*(.init_array.65515))                 \
    __PER_CPU_LATE_INIT_ARRAY_END__ = .;        \
                                                \
    __INIT_ARRAY_START__ = .;                   \
    KEEP (*(SORT (.init_array.*)))              \
    KEEP (*(.init_array))                       \
    __INIT_ARRAY_END__ = .;                     \
                                                \
    __PER_CPU_CTORS_END__ = .;                  \
    KEEP (*(.ctors.00010))                      \
    KEEP (*(.ctors.00009))                      \
    KEEP (*(.ctors.00008))                      \
    KEEP (*(.ctors.00007))                      \
    KEEP (*(.ctors.00006))                      \
    KEEP (*(.ctors.00005))                      \
    KEEP (*(.ctors.00004))                      \
    KEEP (*(.ctors.00003))                      \
    KEEP (*(.ctors.00002))                      \
    KEEP (*(.ctors.00001))                      \
    __PER_CPU_CTORS_LIST__ = .;                 \
                                                \
    __PER_CPU_LATE_CTORS_END__ = .;             \
    KEEP (*(.ctors.00020))                      \
    KEEP (*(.ctors.00019))                      \
    KEEP (*(.ctors.00018))                      \
    KEEP (*(.ctors.00017))                      \
    KEEP (*(.ctors.00016))                      \
    KEEP (*(.ctors.00015))                      \
    KEEP (*(.ctors.00014))                      \
    KEEP (*(.ctors.00013))                      \
    KEEP (*(.ctors.00012))                      \
    KEEP (*(.ctors.00011))                      \
    __PER_CPU_LATE_CTORS_LIST__ = .;            \
                                                \
    __CTOR_END__ = .;                           \
    CONSTRUCTORS                                \
    KEEP (*(.ctors))                            \
    KEEP (*(SORT (.ctors.*)))                   \
    __CTOR_LIST__ = .;                          \
                                                \
    __DTOR_END__ = .;                           \
    KEEP (*(SORT(.dtors.*)))                    \
    KEEP (*(.dtors))                            \
    __DTOR_LIST__ = .;

#if defined(CONFIG_COV)
EXTERN(cov_print)
ASSERT(DEFINED(cov_print), "cov_print not available")
#endif
