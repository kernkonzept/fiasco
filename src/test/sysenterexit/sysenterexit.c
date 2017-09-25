
//
// user level code for test_thread-test45
//

int
main(void)
{
  for(;;)
    {
      asm volatile("1:                       \n"
		   "   movl %eax, %edi       \n"
		   "   rdtsc                 \n"
		   "   movl %eax, %esi       \n"
		   "   pushl $0              \n"
		   "   pushl $2f             \n"
		   "   movl  %esp, %ecx      \n"
		   "   rdtsc                 \n"
		   "   sysenter              \n"
		   "2:                       \n"
		   "   jmp 1b                \n");
    }
  return 0;
}
