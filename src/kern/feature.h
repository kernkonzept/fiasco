#ifndef FEATURE_H__
#define FEATURE_H__

#define KIP_KERNEL_FEATURE(s)					\
	asm(".section .initkip.features, \"a\", %progbits \n"	\
	    ".string \"" s "\"	                          \n"	\
	    ".previous                                    \n")

#define KIP_KERNEL_ABI_VERSION(nr)				\
	asm(".section .initkip.features, \"a\", %progbits \n"	\
	    ".string \"abiver:" nr "\"                    \n"	\
	    ".previous                                    \n")

#endif // FEATURE_H__
