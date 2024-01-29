


//rdi rsi
int _switch(context* new_ctx, context* cur_ctx);


#ifdef __x86_64__
__asm__(
"   .text                   \n"
"       .p2align 4,,15      \n"
".globl _switch             \n"
".globl __switch            \n"
"_switch:                   \n"
"__switch:                  \n"
"   movq %rsp, 0(%rsi)      \n"
"   movq %rbp, 8(%rsi)      \n"
"   movq (%rsp), %rax       \n"
"   movq %rax, 16(%rsi)     \n"
"   movq %rbx, 24(%rsi)     \n"
"   movq %r12, 32(%rsi)     \n"
"   movq %r13, 40(%rsi)     \n"
"   movq %r14, 48(%rsi)     \n"
"   movq %r15, 56(%rsi)     \n"
"   movq 56(%rdi), %r15     \n"
"   movq 48(%rdi), %r14     \n"
"   movq 40(%rdi), %r13     \n"
"   movq 32(%rdi), %r12     \n"
"   movq 24(%rdi), %rbx     \n"
"   movq 8(%rdi), %rbp      \n"
"   movq 0(%rdi), %rsp      \n"
"   movq 16(%rdi), %rax     \n"
"   movq %rax, %rsp         \n"
"   ret                     \n"
);
#endif