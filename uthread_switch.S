	.text

/* Switch from current_thread to next_thread. Make next_thread
 * the current_thread, and set next_thread to 0.
 * Use eax as a temporary register, which should be caller saved.
 */
	.globl thread_switch
thread_switch:
	/* YOUR CODE HERE */

	# We are running on current_thread's stack, push all registers to save
	# attention: first time we call thread_switch, we actually do not run on current_thread's stack
	# because current_thread point to all_thread[0], and we just fake it is running, but it does not matter
	pushal

	# Save current thread's stack pointer
	movl current_thread, %eax
	movl %esp, (%eax)

	# Switch stacks to next thread
	movl next_thread, %eax
	movl (%eax), %esp #point to sp'value, if is every thread's first-time-running, it already leaves the space to save registers

	# Set current_thread to next_thread 
	movl %eax, current_thread 
	# Set next_thread to 0
	movl $0, next_thread

	# Pop next thread's registers from the stack into the appropriate registers
	popal

	// exec current_thread
	ret				/* pop return address from stack */
