#include <stdio.h>
#include <sys/time.h>
#include <stdint.h>
#include <unistd.h>

#define TIMER_NEAR_SHIFT    8
#define TIMER_NEAR (1 << TIMER_NEAR_SHIFT)
#define TIMER_LEVEL_SHIFT   6
#define TIMER_LEVEL (1 << TIMER_LEVEL_SHIFT)
#define TIMER_NEAR_MASK (TIMER_NEAR - 1)
#define TIMER_LEVEL_MASK (TIMER_LEVEL - 1)

#define Printf(arg) do\
{\
    printf(arg);\
    fflush(stdout);\
}while(0);

typedef void* (*cbfun)(int);

void* print(int arg)
{
    uint64_t param = (uint64_t)arg;
    printf("param=%ld\n", param);
    fflush(stdout);
}

struct TimerNode
{
    struct TimerNode* next;
    unsigned expire;
    cbfun callback;
    int arg;
};

struct LinkList
{
    TimerNode head;
    TimerNode* tail;
};

struct Timer
{
    LinkList    near[TIMER_NEAR];
    LinkList    t[4][TIMER_LEVEL];
    uint32_t    time;
    uint32_t    starttime;
    uint64_t    current;
    uint64_t    current_point;
};

inline TimerNode* link_clear(LinkList* list)
{
    TimerNode* node = list->head.next;
    list->head.next = NULL;
    list->tail = &(list->head);
    return node;
}

void link(LinkList* list, TimerNode* node)
{
    list->tail->next = node;
    list->tail = node;
    node->next = NULL;
}

void add_node(Timer *T, TimerNode* node)
{
    uint32_t time = node->expire;
    uint32_t cur = T->time;
    printf("time=%d  cur=%d\n", time, cur);
    if((time | TIMER_NEAR_MASK) == (cur | TIMER_NEAR_MASK))
    {
        link(&T->near[time&TIMER_NEAR_MASK], node);
        printf("addnode node=%d, near=%d\n",(int)node->arg, time&TIMER_NEAR_MASK);
    }
    else
    {
        int i;
        uint32_t mask = TIMER_NEAR << TIMER_LEVEL_SHIFT;
        for(i = 0; i < 3; i++)
        {
            if((time | mask-1) == (cur | mask -1))
            {
                break;
            }
            mask <<= TIMER_LEVEL_SHIFT;
        }

        int idx = (time >> (TIMER_NEAR_SHIFT + i * TIMER_LEVEL_SHIFT)) & TIMER_LEVEL_MASK;
        link(&(T->t[i][idx]), node);
        printf("addnode node=%d, level=%d index=%d\n",node->arg, i, idx);
    }
}

void timer_add(Timer* T, uint32_t time)
{
    TimerNode *node = new TimerNode;
    node->expire = time + T->time;
    node->callback = print;
    node->arg = time;
    add_node(T, node);
}

void move_list(Timer *T, int level, int idx)
{
    TimerNode* node = link_clear(&(T->t[level][idx]));
    while(node)
    {
        printf("move_list level=%d, idx=%d\n", level, idx);
        TimerNode * temp = node->next;
        add_node(T, node);
        node = temp;
    }
}

void timer_shift(Timer *T)
{
    uint32_t ct = ++T->time;

    if(ct == 0)
    {
        move_list(T, 3, 0);
    }
    else
    {      
        uint32_t mask = TIMER_NEAR;
        uint32_t time = ct >> TIMER_NEAR_SHIFT;
        int i = 0;
        while((ct & mask - 1) == 0)
        {
            int idx = time & TIMER_LEVEL_MASK;
            if(idx != 0)
            {
                move_list(T, i, idx);
                break;
            }
            mask <<= TIMER_LEVEL_SHIFT;
            time >>= TIMER_LEVEL_SHIFT;
            i++;
        }
    }
}

void dispatch_list(TimerNode* node)
{
    do
    {
        //do something
        node->callback(node->arg);
        TimerNode* temp = node->next;
        delete node;
        node = temp;
    }while(node);
}

void timer_execute(Timer *T)
{
    int idx = T->time & TIMER_NEAR_MASK;
    while(T->near[idx].head.next)
    {
        TimerNode *node = link_clear(&(T->near[idx]));
        dispatch_list(node);
    }
}

void timer_update(Timer* T)
{
    timer_execute(T);
    timer_shift(T);
}

Timer* timer_create()
{
    Timer* T = new Timer;
    
    for(int i = 0; i < TIMER_NEAR; i++)
    {
        link_clear(&(T->near[i]));
    }

    for(int i = 0; i < 4; i++)
    {
        for(int j = 0; j < TIMER_LEVEL; j++)
        {
            link_clear((&(T->t[i][j])));
        }
    }
    T->current = 0;
    return T;
}

static uint64_t gettime()
{
    uint64_t t;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    t = (uint64_t)tv.tv_sec * 1000;
    t += tv.tv_usec / 1000;
    return t;
}

void update(Timer *T)
{
    uint64_t cp = gettime();
    if(cp < T->current_point)
    {
        printf("update err cp = %ld, current_point=%ld\n", cp, T->current_point);
    }
    else if(cp != T->current_point)
    {
        uint32_t diff = (uint32_t)(cp - T->current_point);
        T->current_point = cp;
        T->current += diff;
        for(int i = 0; i < diff; i++)
        {
            timer_update(T);
        }
    }
}

int main()
{
    Timer *T = timer_create();
    T->current_point = gettime();
    timer_add(T, 5);
    timer_add(T, 278);
    timer_add(T, 786);
    timer_add(T, 4157);
    timer_add(T, 8157);
    timer_add(T, 1049947);
    timer_add(T, 24157);
    timer_add(T, 134217728);
    timer_add(T, 70078);
    while(true)
    {
        update(T);
        usleep(1000);
        if(T->time == 10000)
        {
            timer_add(T, 6998);
            timer_add(T, 13998);
        }
    }
    return 0;
}
