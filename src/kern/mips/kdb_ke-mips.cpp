/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
 */

INTERFACE [mips]:

void kdb_ke(const char *msg) asm ("kern_kdebug_entry");
void kdb_ke_sequence(const char *msg) asm ("kern_kdebug_sequence_entry");
void kdb_ke_ipi() asm("kern_kdebug_ipi_entry");
