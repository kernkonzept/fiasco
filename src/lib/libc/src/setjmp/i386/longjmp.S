#include "globalconfig.h"

.global _longjmp
.global longjmp
.type _longjmp,@function
.type longjmp,@function
_longjmp:
longjmp:
#ifdef CONFIG_REGPARM3
	cmp       $1,%edx
	adc       $0, %dl
	mov   (%eax),%ebx
	mov  4(%eax),%esi
	mov  8(%eax),%edi
	mov 12(%eax),%ebp
	mov 16(%eax),%esp
	jmp *20(%eax)
#else
	mov  4(%esp),%edx
	mov  8(%esp),%eax
	cmp       $1,%eax
	adc       $0, %al
	mov   (%edx),%ebx
	mov  4(%edx),%esi
	mov  8(%edx),%edi
	mov 12(%edx),%ebp
	mov 16(%edx),%esp
	jmp *20(%edx)
#endif
