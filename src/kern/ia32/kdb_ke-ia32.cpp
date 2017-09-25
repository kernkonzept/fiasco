INTERFACE [ia32,ux,amd64]:

#define kdb_ke(msg)			\
  asm ("int3           		\n\t"	\
       "jmp 1f			\n\t"	\
       ".ascii " #msg  "	\n\t"	\
       "1:			\n\t")

#define kdb_ke_sequence(msg)		\
  asm ("int3			\n\t"	\
       "jmp 1f			\n\t"	\
       ".ascii \"*##\"		\n\t"	\
       "1:			\n\t"	\
       : : "a"(msg))

