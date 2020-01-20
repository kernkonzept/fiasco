/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
 */

INTERFACE [mips && debug]:

void kdb_ke(char const *msg) asm ("kern_kdebug_cstr_entry");
void kdb_ke_nstr(char const *msg, unsigned len) asm ("kern_kdebug_nstr_entry");
void kdb_ke_sequence(char const *msg, unsigned len) asm ("kern_kdebug_sequence_entry");
void kdb_ke_ipi() asm("kern_kdebug_ipi_entry");
