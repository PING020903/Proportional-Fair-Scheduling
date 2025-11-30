#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <stdint.h>
#include <Windows.h> // 在WINDOWS中调用延时函数, 用于观察调度
#include "FairSchedul.h"
#include "DBG_macro.h"

#define OS_TASK_MODE TASKS_DYNAMIC

#define TASKS_MAX 16

#define DEBUG_PRINT_FLAG 0
#define DEBUG_SCHEDULER 0
#define DEBUG_DYNAMIC_CREATE_TASK 0
#define MIN_UTILITY (0.01F)


// 全局任务链表
#if (OS_TASK_MODE == TASKS_DYNAMIC)
static FairSchedul_TaskNode* g_taskList = NULL;
#endif

#if (OS_TASK_MODE == TASKS_STATIC)
static FairSchedul_TaskNode g_taskList[TASKS_MAX] = { 0 };
#endif

static int g_shutdown = 0; // 新增关闭标志
static unsigned int taskId = 0;

#if IDLE_TASK_HOOK
static FairSchedul_pTaskFunc pTask_idle = NULL;
static void* pIdleArg = NULL;

void idleHookReg(FairSchedul_pTaskFunc pFunc, void* arg) {
    if (!pFunc)
        return;

    pTask_idle = pFunc;
    pIdleArg = arg;
}
#endif

static void idleTask(void* arg) {
#if IDLE_TASK_HOOK
    if (pTask_idle == NULL)
        return;
    pTask_idle(arg);
#endif
    DEBUG_PRINT("");
}

// 效用计算函数
static inline float calculate_utility(FairSchedul_TaskNode* task)
{
    if (task->status.executeCount == 0)
    {
        return task->SetPriority;
    }

    //printf("[%s]:{%f}\n", __func__, (float)task->SetPriority / (task->status.executeCount + 1));
    return (float)task->SetPriority / (task->status.executeCount + 1);
}

// 选择下一个任务
static inline FairSchedul_TaskNode* SelectReadyTask()
{
    FairSchedul_TaskNode* current = g_taskList;
    FairSchedul_TaskNode* selected = NULL;
    float max_utility = -FLT_MAX;
    float utility;

    while (current != NULL)
    {
        if (current->status.status == READY)
        {
            utility = calculate_utility(current);

            if (utility > max_utility) // 选中先遇到最大效用值的任务
            {
                max_utility = utility;
                selected = current;
            }
        }
        current = current->next;
    }
#if DEBUG_SCHEDULER
    printf("max_utility:[%f]", max_utility);
#endif
    return selected;
}

static FairSchedul_TaskNode* FindOrCreateFreeTaskNode(void) {
    FairSchedul_TaskNode* pTask = NULL;
#if (OS_TASK_MODE == TASKS_DYNAMIC)
    FairSchedul_TaskNode* current = NULL;
#endif
#if (OS_TASK_MODE == TASKS_STATIC)
    for (int i = 0; i < SIZE_ARRARY(g_taskList); i++) {
        if (pTask)
            break;

        switch (g_taskList[i].status.status) {
        case INIT:
        case COMPLETED:
            pTask = &g_taskList[i];
            break;
        }
    }
#endif

#if (OS_TASK_MODE == TASKS_DYNAMIC)
    if (g_taskList == NULL)
        goto _NewNode;

    int cnt = 0;
    for (current = g_taskList; current != NULL; current = current->next) {
#if DEBUG_DYNAMIC_CREATE_TASK
        DEBUG_PRINT("cnt[%d], status[%u]", cnt++, current->status.status);
#endif
        switch (current->status.status) {
        case INIT:
        case COMPLETED:
            pTask = current;
            break;
        }

        // 使用已经执行结束的任务, 就无需再申请堆
        if (pTask)
            goto _end;
    }

_NewNode:
    pTask = (FairSchedul_TaskNode*)malloc(sizeof(FairSchedul_TaskNode));
#if DEBUG_DYNAMIC_CREATE_TASK
    VAR_PRINT_POS(pTask);
#endif
    if (pTask == NULL)
        return NULL;
_end:
#endif
    return pTask;
}

// 创建新任务  tips: 0优先级为idle任务, 需另外开启idle_hook, 任何优先级0都会被设为1
FairSchedul_TaskNode* create_task(const FairSchedul_TaskNode* taskCfg,
    const FairSchedul_taskCycle isPeriod)
{
#if (OS_TASK_MODE == TASKS_DYNAMIC)
    FairSchedul_TaskNode* current;
#endif
    FairSchedul_TaskNode* task = NULL;
    if (taskCfg == NULL && taskCfg->functionCallback == NULL)
        return NULL;

    task = FindOrCreateFreeTaskNode();
    if (task == NULL)
        return NULL;


    task->functionCallback = taskCfg->functionCallback;
    task->functionArgment = taskCfg->functionArgment;
    task->SetPriority = (task->SetPriority == 0) ? 1 : taskCfg->SetPriority;
    task->status.status = READY;
    task->status.numberID = ++taskId; // ID为非0值
    task->status.executeCount = 0;
    task->status.period = isPeriod;
    task->next = NULL;

    // 判断是否为 idleTask
    if(taskCfg->functionCallback == idleTask)
        task->SetPriority = 0; // 重置优先级

#if (OS_TASK_MODE == TASKS_DYNAMIC)
    if (g_taskList == NULL)
    {
        g_taskList = task;
#if DEBUG_DYNAMIC_CREATE_TASK
        DEBUG_PRINT("new head");
#endif
    }
    else
    {
        current = g_taskList;
        while (current->next != NULL)
        {
            current = current->next;
        }
        current->next = task;
#if DEBUG_DYNAMIC_CREATE_TASK
        DEBUG_PRINT("add node");
#endif
    }
#endif
#if (OS_TASK_MODE == TASKS_STATIC)
    for (int i = 0; i < TASKS_MAX; i++) {
        g_taskList[i].next = &g_taskList[i + 1];
    }
    g_taskList[TASKS_MAX - 1].next = NULL;
#endif
    
    return task;
}

static void idleReg(FairSchedul_pTaskFunc pFunc, void* arg) {
    if (!pFunc)
        return;

    FairSchedul_TaskNode cfg = TASK_CFG(pFunc, arg, 0);
    create_task(&cfg, CYCLE_PERIOD);
}


// 调度主循环
void scheduler_run()
{
    FairSchedul_TaskNode* next_task = NULL;
    FairSchedul_TaskNode* current = NULL;
    float total_utility = 0.0f, // 总效用(所有任务的效用相加)
        thisUtility = 0.0f; // 当前任务的效用
    int taskCnt = 0;

    idleReg(idleTask, NULL);

    while (!g_shutdown)
    {
        taskCnt = 0;
        total_utility = 0.0f;
#if (OS_TASK_MODE == TASKS_DYNAMIC)
        current = g_taskList;
#endif
#if (OS_TASK_MODE == TASKS_STATIC)
        current = &g_taskList[0];
#endif
        // 计算效用
        // 效用 = 优先级 / (已执行次数 + 1)
        for (; current != NULL; current = current->next)
        {
            if (current->status.status == READY)
            {
                thisUtility = calculate_utility(current);
                total_utility += thisUtility;
                taskCnt++;
#if DEBUG_SCHEDULER
                printf("[pri:%d]{%f}\n", current->SetPriority, thisUtility);
#endif
            }
        }
#if DEBUG_SCHEDULER
        Sleep(500);
        printf("{%f}{%d}[%f]", total_utility, taskCnt, (total_utility / taskCnt));
#endif


        // 由于该任务调度算法设计的原因, 会导致效用越来越低, 将会耗费更多的float精度
        // 导致优先级的作用近乎失效, 从而导致系统调度的无效化
        // 因此, 低于最小效用时, 强制将所有效用提高到系统开始运行时的效用
        if (taskCnt > 0 && (total_utility / taskCnt) < MIN_UTILITY)
        {
#if (OS_TASK_MODE == TASKS_DYNAMIC)
            current = g_taskList;
#endif
#if (OS_TASK_MODE == TASKS_STATIC)
            current = &g_taskList[0];
#endif
            for (; current != NULL; current = current->next)
            {
#if DEBUG_SCHEDULER
                printf("[%s][taskCnt:%d]%p, next:%p\n",
                    __func__, taskCnt, current, current->next);
#endif
                current->status.totalExecuteCount += current->status.executeCount;
                current->status.executeCount = 0;
                if (!(current->status.period))
                    continue;
                current->status.status = READY;
            }

        }

        // 选中任务
        next_task = SelectReadyTask();
        if (next_task == NULL)
        {
#if DEBUG_SCHEDULER
            printf("No ready tasks. Waiting...\n");
#endif
            Sleep(1); // 等待新任务
            continue;
        }

        // 执行任务
        next_task->status.status = RUNNING;
#if DEBUG_SCHEDULER
        VAR_PRINT_POS(next_task->functionArgment);
#endif
        if (next_task->functionCallback != NULL) // 防止调用NULL
            next_task->functionCallback(next_task->functionArgment);
        next_task->status.status = COMPLETED;
        next_task->status.executeCount++;
#if DEBUG_SCHEDULER
        DEBUG_PRINT("exeCount[%u], id[%u]", next_task->status.executeCount,
            next_task->status.numberID);
#endif

        // 自动重置周期性任务
        if (next_task->status.period == CYCLE_PERIOD)
        {
            next_task->status.status = READY;
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
    FairSchedul_TaskNode* current = g_taskList;
    FairSchedul_TaskNode* next = NULL;
#if (OS_TASK_MODE == TASKS_DYNAMIC)
    while (current != NULL)
    {
        next = current->next;
        free(current);
        current = next;
    }
#endif
#if (OS_TASK_MODE == TASKS_STATIC)
    memset(current, 0, sizeof(g_taskList));
#endif
}