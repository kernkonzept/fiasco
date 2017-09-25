#pragma once

#define zero	$0
#define AT	$1
#define v0	$2
#define v1	$3
#define a0	$4
#define a1	$5
#define a2	$6
#define a3	$7
#define t0	$8
#define t1	$9
#define t2	$10
#define t3	$11
#define t4	$12
#define ta0	$12
#define t5	$13
#define ta1	$13
#define t6	$14
#define ta2	$14
#define t7	$15
#define ta3	$15
#define s0	$16
#define s1	$17
#define s2	$18
#define s3	$19
#define s4	$20
#define s5	$21
#define s6	$22
#define s7	$23
#define t8	$24
#define t9	$25
#define jp	$25
#define k0	$26
#define k1	$27
#define gp	$28
#define sp	$29
#define fp	$30
#define s8	$30
#define ra	$31

#ifdef __mips64
#  define CALL_FRAME_SIZE (0)
#else
#  define CALL_FRAME_SIZE (4 * 4)
#endif

/*
 * LEAF
 *	A leaf routine does
 *	- call no other function,
 *	- never use any register that callee-saved (S0-S8), and
 *	- not use any local stack storage.
 */
#define LEAF(x)				\
	.globl	x;			\
	.ent	x, 0;			\
x: ;					\
	.frame sp, 0, ra;

/*
 * END
 *	Mark end of a procedure.
 */
#define END(x) \
	.end x


