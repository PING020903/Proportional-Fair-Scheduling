#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <Windows.h>
#include "test.h"

#define DEBUG_PRINT_FLAG 0
#define MIN_UTILITY (0.01F)


// 全局任务链表
static testTaskNode* g_taskList = NULL;
static int g_shutdown = 0; // 新增关闭标志

// 效用计算函数
static float calculate_utility(testTaskNode* task)
{
    if ( task->status.executeCount == 0 )
    {
        return task->SetPriority;
    }

    return (float)task->SetPriority / (task->status.executeCount + 1);
}

// 选择下一个任务
static testTaskNode* select_next_task()
{
    testTaskNode* current = g_taskList;
    testTaskNode* selected = NULL;
    float max_utility = -FLT_MAX;
    float utility;

    while ( current != NULL )
    {
        if ( current->status.status == READY )
        {
            utility = calculate_utility(current);
            if ( utility > max_utility )
            {
                max_utility = utility;
                selected = current;
            }
        }
        current = current->next;
    }
#if DEBUG_PRINT_FLAG
    printf("max_utility:[%f]", max_utility);
#endif
    return selected;
}


// 创建新任务
testTaskNode* create_task(Func_RetVoid func, void* arg, short priority, short isPeriod)
{
    if ( func == NULL )
        return NULL;

    testTaskNode* current;
    testTaskNode* task = (testTaskNode*)malloc(sizeof(testTaskNode));
    task->functionCallback = func;
    task->functionArgment = arg;
    task->SetPriority = priority;
    task->status.status = READY;
    task->status.Counter = 0;
    task->status.executeCount = 0;
    task->status.period = isPeriod;
    task->next = NULL;

    if ( g_taskList == NULL )
    {
        g_taskList = task;
    }
    else
    {
        current = g_taskList;
        while ( current->next != NULL )
        {
            current = current->next;
        }
        current->next = task;
    }
    return task;
}


// 调度主循环
void scheduler_run()
{
    testTaskNode* next_task = NULL;
    testTaskNode* current = NULL;
    float total_utility = 0.0f, thisUtility = 0.0f;
    int taskCnt = 0;

    while ( !g_shutdown )
    {
        taskCnt = 0;
        total_utility = 0.0f;
        for ( current = g_taskList; current != NULL; current = current->next )
        {
            if ( current->status.status == READY )
            {
                thisUtility = calculate_utility(current);
                total_utility += thisUtility;
                taskCnt++;
#if DEBUG_PRINT_FLAG
                printf("[pri:%d]{%f}\n", current->SetPriority, thisUtility);
#endif
            }
        }
#if DEBUG_PRINT_FLAG
        Sleep(500);
        printf("{%f}{%d}[%f]", total_utility, taskCnt, (total_utility / taskCnt));
#endif
        
        if ( taskCnt > 0 && (total_utility / taskCnt) < MIN_UTILITY )
        {
            for ( current = g_taskList; current != NULL; current = current->next )
            {
#if DEBUG_PRINT_FLAG
                printf("[%s][taskCnt:%d]%p, next:%p\n", __func__, taskCnt, current, current->next);
#endif
                current->status.totalExecuteCount += current->status.executeCount;
                current->status.executeCount = 0;
                if ( !(current->status.period) )
                    continue;
                current->status.status = READY;
            }
            
        }


        next_task = select_next_task();
        if ( next_task == NULL )
        {
            printf("No ready tasks. Waiting...\n");
            //Sleep(500); // 等待新任务
            continue;
        }

        next_task->status.status = RUNNING;
        next_task->functionCallback(next_task->functionArgment);
        next_task->status.status = COMPLETED;
        next_task->status.executeCount++;

        // 自动重置周期性任务
        if ( next_task->status.period > 0 )
        {
#if 0
            next_task->status.Counter++;
            if ( next_task->status.Counter >= next_task->status.period )
            {
                next_task->status.Counter = 0;
                next_task->status.status = READY;
            }
#else
            next_task->status.Counter = 0;
            next_task->status.status = READY;
#endif
        }      
    }

    printf("Scheduler shutdown initiated.\n");
}

// 请求关闭调度器
void scheduler_shutdown()
{
    g_shutdown = 1;
}

// 清理任务资源
void cleanup_tasks()
{
    testTaskNode* current = g_taskList;
    testTaskNode* next = NULL;
    while ( current != NULL )
    {
        next = current->next;
        free(current);
        current = next;
    }
}