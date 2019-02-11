INTERFACE [arm && debug]:

void kdb_ke(const char *msg) asm ("kern_kdebug_entry")
FIASCO_LONGCALL;

void kdb_ke_sequence(const char *msg) asm ("kern_kdebug_sequence_entry")
FIASCO_LONGCALL;

