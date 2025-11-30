#ifndef _FAIRSCHEDUL_H_
#define _FAIRSCHEDUL_H_



#define TASKS_DYNAMIC 0
#define TASKS_STATIC 1

#define IDLE_TASK_HOOK 0

#define TASK_CFG(pFunc, pArg, _priority) {\
.functionCallback = pFunc,\
.functionArgment = pArg,\
.SetPriority = _priority,\
}


// 任务状态定义
typedef enum {
    INIT = 0,
    READY = 1, // 就绪
    RUNNING, // 运行中
    COMPLETED // 已完成
} FairSchedul_TaskStatus;

typedef enum {
    CYCLE_ONCE = 0,
    CYCLE_PERIOD
}FairSchedul_taskCycle;

// 任务运行状态结构体
typedef struct
{
    short status; // 任务运行状态
    unsigned int numberID; // 唯一ID
    unsigned short executeCount; // 临时记录的运行次数
    unsigned int totalExecuteCount; // 实际运行总次数
    unsigned short period; // 该任务是否持续运行
} FairSchedul_TaskRunStatus;

// 任务节点结构体
typedef void (*FairSchedul_pTaskFunc)(void* arg);
typedef struct FairSchedul_TaskNode
{
    FairSchedul_pTaskFunc functionCallback;
    void* functionArgment;
    short SetPriority;
    FairSchedul_TaskRunStatus status;
    struct FairSchedul_TaskNode* next; // 链表指针
} FairSchedul_TaskNode;

FairSchedul_TaskNode* create_task(const FairSchedul_TaskNode* taskCfg,
    const FairSchedul_taskCycle isPeriod);

#if IDLE_TASK_HOOK
void idleHookReg(FairSchedul_pTaskFunc pFunc, void* arg);
#endif

void scheduler_run();

void scheduler_shutdown();

void cleanup_tasks();

#endif // !_FAIRSCHEDUL_H_


