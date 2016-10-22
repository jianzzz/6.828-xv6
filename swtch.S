# Context switch
#
#   void swtch(struct context **old, struct context *new);
# 
# Save current register context in old
# and then load register context from new.

.globl swtch
swtch:
  movl 4(%esp), %eax #load *(esp + 4) into eax, that is &cpu->scheduler, pointer's addr
  movl 8(%esp), %edx #load *(esp + 8) into edx, that is p->context, pointer's value

  # Save old callee-save registers
  pushl %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  # Switch stacks
  movl %esp, (%eax)# %esp此时指向上面四个寄存器信息，现在给cpu的context变量赋值
  movl %edx, %esp# %esp指向新的proc的context，上图中的p->context

  # Load new callee-save registers
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
