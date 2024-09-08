#include "globalconfig.h"

.global memcpy
.global __memcpy_fwd
.hidden __memcpy_fwd
.type memcpy,@function
memcpy:
__memcpy_fwd:
	push %esi
	push %edi
#ifdef CONFIG_REGPARM3
	// 1st parameter: EAX = dest
	// 2nd parameter: EDX = src
	// 3nd parameter: ECX = n
	mov %edx,%esi
        mov %eax,%edi
#else
	mov 12(%esp),%edi
	mov 16(%esp),%esi
	mov 20(%esp),%ecx
	mov %edi,%eax
#endif
	cmp $4,%ecx
	jc 1f
	test $3,%edi
	jz 1f
2:	movsb
	dec %ecx
	test $3,%edi
	jnz 2b
1:	mov %ecx,%edx
	shr $2,%ecx
	rep
	movsl
	and $3,%edx
	jz 1f
2:	movsb
	dec %edx
	jnz 2b
1:	pop %edi
	pop %esi
	ret
