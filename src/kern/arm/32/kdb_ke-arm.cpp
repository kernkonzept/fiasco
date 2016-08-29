INTERFACE [arm]:

void kdb_ke(const char *msg) asm ("kern_kdebug_entry")
__attribute__((long_call));

void kdb_ke_sequence(const char *msg) asm ("kern_kdebug_sequence_entry")
__attribute__((long_call));

