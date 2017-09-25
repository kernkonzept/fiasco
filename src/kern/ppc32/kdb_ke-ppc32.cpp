INTERFACE [ppc32]:

void kdb_ke(const char *msg) asm ("kern_kdebug_entry");
void kdb_ke_sequence(const char *msg) asm ("kern_kdebug_entry");
