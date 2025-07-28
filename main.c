#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "Windows.h"
#include "test.h"

#if 0
#define TASKS_MAX 8
#define TASK_MAX_PRIORITY TASKS_MAX
#define TASK_MIN_PRIORITY 0

typedef void* (*Func_RetPtr)(void* arg);
typedef void (*Func_RetVoid)(void* arg);

typedef enum
{
    TASK_UNREADY,
    TASK_READY,
} TaskStatus;

#if 0
typedef enum
{
    FUNC_RET_PTR,
    FUNC_RET_VOID
} FuncType;


typedef union
{
    // Func_RetPtr funcA;
    Func_RetVoid funcB;
} UserFunc;
#endif

typedef struct
{
    short status;
    unsigned long Counter;
    // unsigned long period;
    unsigned long executeCount;
} TaskRunStatus;

typedef struct
{
    Func_RetVoid functionCallback;
    void* functionArgment;
    short SetPriority;
    TaskRunStatus status;
} TaskNode;

/// @brief 降序比较
/// @param a
/// @param b
/// @return
/// @note 传递参数实际类型为(void**), 比较函数需注意
static int CompareTaskNodeBySetPriorityDesc(const void* a, const void* b)
{
    const TaskNode* taskA = *(const void**)a;
    const TaskNode* taskB = *(const void**)b;
    int numA = taskA->SetPriority, numB = taskB->SetPriority;
    if ( numB == numA )
        return 0;
    return (int)((numB - numA) < 0) ? -1 : 1;

    /*
    如果 a->SetPriority > b->SetPriority，返回负值（表示 a 应该排在 b 前面）；
    如果 a->SetPriority == b->SetPriority，返回 0（表示两者相等）；
    如果 a->SetPriority < b->SetPriority，返回正值（表示 a 应该排在 b 后面）。
    */
}

int TaskNode_check(TaskNode* tasks, const size_t len)
{
    size_t count = 0;
    if ( tasks == NULL || len == 0U )
        goto _funcEnd;
    for ( ; count < len; count++ )
    {
        printf("[%llu]:{set:[%u]}\n", count, tasks[count].SetPriority);
        if ( tasks[count].SetPriority > TASK_MAX_PRIORITY )
            return -1;
    }
_funcEnd:
    return 0;
}

int TaskNodeMap_check(void** map, size_t len)
{
    size_t count = 0;
    if ( map == NULL || len == 0U )
        goto _funcEnd;
    for ( ; count < len; count++ )
    {

        printf("[%llu]:{set:[%u]}\n", count,
               ((TaskNode*)(*(map + count)))->SetPriority);

        if ( ((TaskNode*)map[count])->SetPriority < TASK_MIN_PRIORITY &&
            ((TaskNode*)map[count])->SetPriority > TASK_MAX_PRIORITY )
            return 1;
    }
_funcEnd:
    return 0;
}

static void test_checkRunCntMap(unsigned char* map, size_t len)
{
    size_t loop = 0;
    if ( map == NULL || len == 0 )
        return;

    for ( ; loop < len; loop++ )
        printf("[%zu](%u) ", loop, map[loop]);

    putchar('\n');
    return;
}

int TaskNode_statusInit(TaskNode* tasks, size_t len)
{
    size_t loop = 0;
    if ( tasks == NULL || len == 0 )
        return 1;

    for ( loop = 0; loop < len; loop++ )
    {
        tasks[loop].status.Counter = 0;
        tasks[loop].status.executeCount = 0;
        tasks[loop].status.status = TASK_UNREADY;
    }
    return 0;
}

int TaskNode_proportionCalculator(void** tasks, size_t taskCnt, int* proportionMap)
{
    size_t loop;
    short minCnt, maxCnt;
    TaskNode* CurrentTask = NULL, * NextTask = NULL;
    if ( tasks == NULL || proportionMap == NULL || taskCnt == 0U )
        return -1;

    for ( loop = 0; loop < (taskCnt - 1); loop++ )
    {
        CurrentTask = tasks[loop];
        NextTask = tasks[loop + 1];
        minCnt = (CurrentTask->SetPriority > NextTask->SetPriority)
            ? NextTask->SetPriority
            : CurrentTask->SetPriority;

        maxCnt = (CurrentTask->SetPriority < NextTask->SetPriority)
            ? NextTask->SetPriority
            : CurrentTask->SetPriority;

        proportionMap[loop] = maxCnt / minCnt;
        printf("[%zu, pri:%d]max:%d, min:%d, end:%d, mod:%d\n",
               loop, CurrentTask->SetPriority, maxCnt, minCnt, proportionMap[loop], maxCnt % minCnt);
        if ( maxCnt % minCnt > 5 )
            proportionMap[loop]++;
    }

    for ( loop = 0; loop < (taskCnt - 1); loop++ )
        printf("[%zu]{%d}  ", loop, proportionMap[loop]);

    return (int)loop;
}

void TaskNode_runCurrentTask(TaskNode* current, size_t runCnt)
{
    if ( current == NULL || current->status.status == TASK_UNREADY )
        return;

    for ( ; runCnt > 0; runCnt-- )
    {
        current->functionCallback(current->functionArgment);
        current->status.executeCount++;
    }
    current->status.status = TASK_UNREADY;
}


int TaskNode_urgentTask(void** tasks, int taskCnt)
{
    size_t urgent = 0;
    int loop;
    TaskNode* current = NULL;
    if ( tasks == NULL || taskCnt == 0)
        return 0;

    for ( loop = 0; loop < taskCnt; loop++ )
    {
        current = tasks[loop];
        if ( (current->SetPriority - current->status.executeCount) > urgent )
            urgent = loop;
    }
    return loop;
}


void tasksLoop(TaskNode* tasks, const size_t taskCnt)
{
    int CurrentTaskNum = 0, maxIndex = -1, CurrentTaskRunCounter = 0, loop;
    size_t count = 0, loop1;
    TaskNode* CurrentTask = NULL;

    void* ret = NULL;
    void** taskmap = NULL;
    int* taskProportionMap = NULL;

    if ( tasks == NULL || taskCnt == 0U )
        goto _funcEnd;

    if ( TaskNode_check(tasks, taskCnt) )
        goto _funcEnd;

    Sleep(2000);
    taskmap = (void**)calloc(taskCnt, sizeof(void*));
    if ( taskmap == NULL )
    {
        perror("task map alloc fail...\n");
        return;
    }
    printf("taskmap len:[%llu], element size:[%llu]\n", taskCnt, sizeof(void*));
    memset(taskmap, 0, sizeof(void*) * taskCnt);
    for ( ; count < taskCnt; count++ )
    {
        *(taskmap + count) = (void*)&tasks[count];
    }

    // 传递taskmap类型为(void**), 比较函数需注意
    qsort(taskmap, taskCnt, sizeof(void*), CompareTaskNodeBySetPriorityDesc);
    if ( TaskNodeMap_check(taskmap, taskCnt) )                                   // 优先级初始化有误
        goto _funcEnd;

    taskProportionMap = calloc(taskCnt - 1, sizeof(int));
    if ( taskProportionMap == NULL )
        goto _funcEnd;

    // 根据任务列表中, 逐个前一个任务与后一个任务的优先级相比较, 得到任务执行比例图
    if ( TaskNode_proportionCalculator(taskmap, taskCnt, taskProportionMap) != (taskCnt - 1) )
        goto _funcEnd;
    //goto _funcEnd;
    

    Sleep(2000);
//    system("cls");
    TaskNode_statusInit(tasks, taskCnt);
    putchar('\n');

    for ( ;;)
    {
#if 0
        for ( loop = 0; loop < taskCnt; loop++ )
        {
            CurrentTask = (TaskNode*)taskmap[loop];
            if ( CurrentTask->status.status == TASK_READY )
            {
                CurrentTask->status.Counter += CurrentTask->SetPriority;
            }
        }

        for ( loop = 0; loop < taskCnt; loop++ )
        {
            CurrentTask = (TaskNode*)taskmap[loop];
            if ( CurrentTask->status.status == TASK_READY )
            {
                if ( CurrentTask->status.Counter > max_taskRunCounter )
                {
                    max_taskRunCounter = CurrentTask->status.Counter;
                    maxIndex = (int)loop;
                }
            }
        }


        if ( maxIndex > -1 )
        {
            CurrentTask = (TaskNode*)taskmap[maxIndex];
            if ( CurrentTask->status.status == TASK_UNREADY )
                goto _nextTask;
            (CurrentTask->functionCallback)(CurrentTask->functionArgment);
            CurrentTask->status.status = TASK_UNREADY;
            CurrentTask->status.Counter -= CurrentTask->SetPriority;
            (CurrentTask->status.executeCount) += 1;
        }
_nextTask:
        for ( loop = 0; loop < taskCnt; loop++ )
        {
            CurrentTask = (TaskNode*)taskmap[maxIndex];

        }
#else
        //  8:4:2:1
        loop = 0;
        while ( loop < taskCnt )
        {
            CurrentTask = (TaskNode*)taskmap[loop];
            CurrentTask->status.status = TASK_READY;
            printf("LOOP:[%d]--------\n", loop);
            TaskNode_runCurrentTask(CurrentTask, *(taskProportionMap + loop));

            // 执行了比例图的预定次数, 切换任务
            if ( CurrentTask->status.executeCount == *(taskProportionMap + loop) )
            {
                CurrentTask->status.Counter += CurrentTask->status.executeCount;
                CurrentTask->status.executeCount = 0;
                loop++;
            }
            else
            {
                loop = TaskNode_urgentTask(taskmap, taskCnt);
            }
        }
#if 0
        for ( loop = 0; loop < taskCnt; loop++ )
        {
            

        }
#endif
        goto _funcEnd;
#endif
    }
_funcEnd:
    if ( taskmap )
        free(taskmap);
    if ( taskProportionMap )
        free(taskProportionMap);
    return;
}
#endif

#define test_func_addr(name) test_##name

#define create_test_func(name)                                     \
    static int name##_conut = 0;                                   \
    static void test_##name(void *arg)                             \
    {                                                              \
        printf("func:[%s], count:[%d]\n", __func__, name##_conut); \
        name##_conut++;                                            \
        Sleep(500);                                               \
    }

create_test_func(task1)
create_test_func(task2)
create_test_func(task3)
create_test_func(task4)

static void MyShutdown(void* arg)
{
    scheduler_shutdown();
}

#define SIZE_ARRARY(ARR) sizeof(ARR) / sizeof(ARR[0])
int main(void)
{
    Sleep(2000);

#if 0
    tasksLoop(taskTable, SIZE_ARRARY(taskTable));
#else
    printf("this is cmake study project.\n");
    // 创建任务参数
    int task1_arg = 1;
    int task2_arg = 2;
    int task3_arg = 3;
    int task4_arg = 4;

    // 创建任务节点
    testTaskNode* t1 = create_task(test_func_addr(task1), &task1_arg, 8, 0);
    testTaskNode* t2 = create_task(test_func_addr(task2), &task2_arg, 4, 1);
    testTaskNode* t3 = create_task(test_func_addr(task3), &task3_arg, 2, 1);
    testTaskNode* t4 = create_task(test_func_addr(task4), &task4_arg, 10, 1);
    create_task(MyShutdown, NULL, 1, 0);


    // 运行调度器
    scheduler_run();

    // 清理资源
    cleanup_tasks();
#endif
    system("pause");
    return 0;
}
