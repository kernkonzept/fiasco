INTERFACE [ppc32 && debug]:

void kdb_ke(char const *msg) asm ("kern_kdebug_entry");
void kdb_ke_nstr(char const *msg, unsigned len) asm ("kern_kdebug_entry");
void kdb_ke_sequence(char const *msg, unsigned len) asm ("kern_kdebug_entry");
