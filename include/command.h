#ifndef SYSPROG24_1_COMMAND_H
#define SYSPROG24_1_COMMAND_H

enum command_types
{
	cmd_invalid,
	cmd_empty,
	cmd_issueJob,
	cmd_setConcurrency,
	cmd_stop,
	cmd_pollrunning,
	cmd_pollqueued,
	cmd_exit
};

enum command_length
{
	cmd_const,
	cmd_var
};

int command_recognize(struct array *arr);
void command_strip(struct array *arr, struct array **ret);

#endif /*SYSPROG24_1_COMMAND_H*/