#ifndef _TASK_H
#define _TASK_H

#include "tasks.h"

void t_init();
void t_deinit();
void t_show(int, int, TS_Item);
int t_current_listId();
int t_current_taskId();

#endif
