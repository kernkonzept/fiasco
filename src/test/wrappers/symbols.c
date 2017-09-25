/* char _start, _edata, _end; */
/* char _physmem_1; */
/* char _tcbs_1; */
/* char _iobitmap_1; */

asm (".global _unused1_1; _unused1_1 = 0xea000000");
asm (".global _unused2_1; _unused2_1 = 0xea400000");
/* _unused3_1; */
asm (".global _unused4_io_1; _unused4_io_1 = 0xefc80000");

asm (".global _ipc_window0_1; _ipc_window0_1 = 0xee000000");
asm (".global _ipc_window1_1; _ipc_window1_1 = 0xee800000");
