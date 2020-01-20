INTERFACE [arm && debug]:

void kdb_ke(char const *msg) asm ("kern_kdebug_cstr_entry")
FIASCO_LONGCALL;

void kdb_ke_nstr(char const *msg, unsigned len) asm ("kern_kdebug_nstr_entry")
FIASCO_LONGCALL;

void kdb_ke_sequence(char const *msg, unsigned len) asm ("kern_kdebug_sequence_entry")
FIASCO_LONGCALL;

