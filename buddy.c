#include "buddy.h"
#include<math.h>
#include<assert.h>
#include<stdlib.h>
#include<stdio.h>
#define NULL ((void *)0)
#define PAGESIZE 4096

struct ListHead{
    struct Node * head;
    int *condition_map;//位图

};
struct Node{
    struct Node *nxt;
    unsigned long address;
};
struct FreeArea{
    struct ListHead head[17];//1-16

};
unsigned long *alloca_area;
struct FreeArea free_area;
unsigned long p_start;
int pg_count;

void add_page(int rank,unsigned long addr){
    struct Node * new_node = malloc(sizeof(struct Node));
    new_node->nxt = NULL;
    new_node->address = addr;
    struct Node * ptr = free_area.head[rank].head;
    if(ptr==NULL)free_area.head[rank].head = new_node;
    else {//加在链表头即可，乱序
        new_node->nxt = ptr;
        free_area.head[rank].head = new_node;
    }
}

void remove_page(int rank, unsigned long addr){
    struct Node * p = free_area.head[rank].head;
    assert(p!=NULL);
    if(p->address == addr){
        free_area.head[rank].head = p->nxt;
        free(p);
    }else{
        // printf("%s\n","qwq");
        // printf("%d\n",query_page_counts(1));
        int cnt = 0;
        while(p->nxt != NULL && p->nxt->address != addr){
            p = p->nxt;
            ++cnt;
        }
        // printf("%d\n",cnt);
        // printf("%s\n","QWQQQ");
        assert(p->nxt!=NULL);
        // printf("%s\n","QWQQQ");
        struct Node * q = p->nxt;
        p->nxt = p->nxt->nxt;
        // printf("%s\n","QWQQQ");
        // printf("%d\n",query_page_counts(1));
        free(q);
    }
}

unsigned long find_page_this_rank(int rank){
    if(free_area.head[rank].head!=NULL){
        struct Node * p = free_area.head[rank].head;
        free_area.head[rank].head = p->nxt;
        unsigned long addr = p->address;
        int order = (addr - p_start)/PAGESIZE;
        int index = order >> (rank);
        free_area.head[rank].condition_map[index] = free_area.head[rank].condition_map[index]^1;
        free(p);
        return addr;
    }
    else return 0;
}

unsigned long find_page_up_rank(int cur, int tar){
   if(free_area.head[cur].head!=NULL){
        //remove page
        struct Node * p = free_area.head[cur].head;
        free_area.head[cur].head = p->nxt;
        unsigned long addr = p->address;
        unsigned long ans = p->address;
        //change condition map
        int order = (addr - p_start)/PAGESIZE;
        int index = order >> (cur);
        free_area.head[cur].condition_map[index] = free_area.head[cur].condition_map[index]^1;
        free(p);
        for(int i = cur-1; i >= tar; --i){ 
            //add page
            addr = ans + pow(2,i-1)*PAGESIZE;
            add_page(i,addr);
            //change condition map
            order = (addr-p_start)/PAGESIZE;
            index = order >> i;
            free_area.head[i].condition_map[index] = free_area.head[i].condition_map[index]^1; 
        }
        
        return ans;
   }else return 0;
}

int init_page(void *p, int pgcount){
    pg_count = pgcount;
    p_start = (unsigned long) p;
    assert(realloc(p,pgcount * 1024 * 4)!=NULL);
    alloca_area = calloc(pgcount,sizeof(unsigned long));
    for(int i = 0; i < pgcount; ++i)alloca_area[i]=-1;
    int rank = log(pgcount)/log(2) + 1;
    // printf("%d\n",rank);
    add_page(rank,(unsigned long)p);
    for(int i = 0; i < 17; ++i){//最多pgcount/2个伙伴
        free_area.head[i].condition_map = calloc(pgcount/2,sizeof(int));
        if(i != rank)free_area.head[i].head = NULL;
    }
    return OK;
}

void *alloc_pages(int rank){//分配指定rank的page
    if(rank < 1 || rank > 16)return -EINVAL;
    unsigned long addr = find_page_this_rank(rank);
    if(!addr){//需要分割
        for(int i = rank+1; i <= 16; ++i){
            // printf("%s\n","qwq");
            unsigned long addr = find_page_up_rank(i,rank);
            // printf("%d\n",i);
            // printf(addr);
            if(addr){
                int order = (addr-p_start)/PAGESIZE;
                alloca_area[order] = rank;
                // printf("%s\n","##");
                // printf("%d\n",p_start);
                return addr;
            }else continue;
        }
        return -ENOSPC;
    }
    else{
        int order = (addr-p_start)/PAGESIZE;
        alloca_area[order]=rank;
        return addr;
    } 
}
int flag = 0;
int return_pages(void *p){
    // if(!flag){
    //     printf("%d\n",pg_count);
    //     for(int i = 0; i <= pg_count; ++i){
    //         if(alloca_area[i]!=1){
    //             printf("%d\n",alloca_area[i]);
    //             printf("%d\n",i);
    //             exit(0);
    //             }
    //     }
    //     flag =1;
    // }
    
     if(p==NULL)return -EINVAL;
     else if((unsigned long)p < p_start ||(unsigned long)p > p_start + PAGESIZE * pg_count)return -EINVAL;
     int order = ((unsigned long)p - p_start)/PAGESIZE;
    //  printf("%s\n","!!!");
    //  printf("%d\n",order);
     int rank = alloca_area[order];
     if(rank == -1) return -EINVAL;
    //  printf("%s\n","&&&&");
     alloca_area[order] = -1;
     int index = order >> rank;
     free_area.head[rank].condition_map[index] = free_area.head[rank].condition_map[index]^1; 
     int cur_rank = rank;
     if(free_area.head[rank].condition_map[index]){//不需要合并
        // printf("%s\n","NO");
        add_page(rank,(unsigned long)p);
     }else{//需要向上合并
        // printf("%s\n","COMBINE");
        do{
            //remove from low rank
            if(!(order & (int)pow(2,cur_rank-1))){//return 的是buddy前者
                // printf("%s\n","JL");
                remove_page(cur_rank,p_start + order * PAGESIZE + pow(2,cur_rank-1) * PAGESIZE);//删除buddy后者
            }
            else{
                // printf("%s\n","LAI");
                // printf("%d\n",cur_rank);
                remove_page(cur_rank,p_start + order * PAGESIZE - pow(2,cur_rank-1) * PAGESIZE);//删除buddy前者
                order -= pow(2,cur_rank-1);
            } 
            //add it to the high rank
            cur_rank++;
            index = order >> cur_rank;
            free_area.head[cur_rank].condition_map[index] = free_area.head[cur_rank].condition_map[index] ^1; 
            if(cur_rank == 16)break;
        }while(!free_area.head[cur_rank].condition_map[index]);
        if(cur_rank == 16){//special case: rank = 16
            assert(order == 0);
            add_page(cur_rank,p_start + order * PAGESIZE);
        }else{
             add_page(cur_rank,p_start + order * PAGESIZE);
        }
     }

    return OK;
}

int query_ranks(void *p){
    if((unsigned long)p < p_start || (unsigned long)p > p_start + pg_count * PAGESIZE)return -EINVAL;
    int order = ((unsigned long)p - p_start) / PAGESIZE;
    if(alloca_area[order]!=-1)return alloca_area[order];//query page has been allocated
    else{//not allocated
        int order = ((unsigned long)p-p_start) / PAGESIZE;
        for(int i = 16; i >= 1; --i){
            if(order % (int)pow(2,i) == 0)return i==16 ? 16 : i+1;
        }
    }
    return OK;
}

int query_page_counts(int rank){
    if(rank > 16 || rank < 1)return -EINVAL;
    struct Node * p = free_area.head[rank].head;
    int cnt = 0;
    while(p != NULL){
        cnt++;
        p = p->nxt;
    };
    return cnt;
}
