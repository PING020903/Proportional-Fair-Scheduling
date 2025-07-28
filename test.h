#ifndef _TEST_H_
#define _TEST_H_

// 任务状态定义
typedef enum {
    READY,
    RUNNING,
    COMPLETED
} testTaskStatus;

// 任务运行状态结构体
typedef struct
{
    short status;
    unsigned long Counter;
    unsigned short executeCount;
    unsigned long totalExecuteCount;
    unsigned short period; // 该任务是否持续运行
} testTaskRunStatus;

// 任务节点结构体
typedef void (*Func_RetVoid)(void* arg);
typedef struct testTaskNode
{
    Func_RetVoid functionCallback;
    void* functionArgment;
    short SetPriority;
    testTaskRunStatus status;
    struct testTaskNode* next; // 链表指针
} testTaskNode;

testTaskNode* create_task(Func_RetVoid func, void* arg, short priority, short isPeriod);

void scheduler_run();

void scheduler_shutdown();

void cleanup_tasks();

#endif // !_TEST_H_


