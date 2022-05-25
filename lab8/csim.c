#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

typedef struct line
{
    bool valid_bit;
    int tag;
    clock_t update;
}line_t;

typedef struct set
{
    line_t *line_tab;
}set_t;

typedef struct cache
{
    int block_bit;
    int block_num;
    int line_num;
    int set_bit;
    int set_num;
    set_t *set_tab;
}cache_t;

void print_usage(){ 
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n"); 
    printf("Options:\n"); 
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n"); 
    printf("  -s <num>   Number of set index bits.\n"); 
    printf("  -E <num>   Number of lines per set.\n"); 
    printf("  -b <num>   Number of block offset bits.\n"); 
    printf("  -t <file>  Trace file.\n\n"); 
    printf("Examples:\n"); 
    printf("  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n"); 
    printf("  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n"); 
}


void init_cache(cache_t *cache,int block_bit,int line_num,int set_bit){ 

    (*cache).block_bit=block_bit;
    (*cache).block_num=1<<block_bit;
    (*cache).line_num=line_num;
    (*cache).set_bit=set_bit;
    (*cache).set_num=1<<set_bit;
    (*cache).set_tab = (set_t*) malloc((*cache).set_num * sizeof(set_t));
    for (size_t i = 0; i < (*cache).set_num; i++) 
        (*cache).set_tab[i].line_tab = (line_t*) calloc (line_num, sizeof(line_t)); 
}
bool visit_cache(cache_t *cache,long addr)
{
    int set_index=(addr>>(*cache).block_bit)%(*cache).set_num;
    int tag=addr>>((*cache).block_bit+(*cache).set_bit);
    for(int i=0;i<(*cache).line_num;i++)
    {
        if((*cache).set_tab[set_index].line_tab[i].valid_bit==true&&(*cache).set_tab[set_index].line_tab[i].tag==tag)
        {
            (*cache).set_tab[set_index].line_tab[i].update=clock();
            printf("visit s %d e %d\n",set_index,i);
            return 1;
        }
    }
    return 0;
}

bool replace_cache(cache_t *cache,long addr)
{
    int set_index=(addr>>(*cache).block_bit)%(*cache).set_num;
    int tag=addr>>((*cache).block_bit+(*cache).set_bit);
    line_t lru_line=(*cache).set_tab[set_index].line_tab[0];
    int lru_index=0;

    for(int i=0;i<(*cache).line_num;i++)
    {
        if((*cache).set_tab[set_index].line_tab[i].valid_bit==false)
        {
            (*cache).set_tab[set_index].line_tab[i].valid_bit=true;
            (*cache).set_tab[set_index].line_tab[i].tag=tag;
            (*cache).set_tab[set_index].line_tab[i].update=clock();

            return 0;
        }
        if((*cache).set_tab[set_index].line_tab[i].update<lru_line.update)
            {lru_line=(*cache).set_tab[set_index].line_tab[i];lru_index=i;}
    }
    (*cache).set_tab[set_index].line_tab[lru_index].tag=tag;
    (*cache).set_tab[set_index].line_tab[lru_index].update=clock();

    return 1;
}
int main(int argc,char **argv)
{
    cache_t cache;
    char opt;
    bool disp=false;
    int set_bit,block_bit,line_num;
    char *filename=NULL;
    FILE *file=NULL;
    while ((opt=getopt(argc,argv,"hvs:E:b:t:"))!=-1)
    {
        switch (opt)
        {
        case 'h':
            print_usage();
            return 0;
            break;
        case 'v':
            disp=true;
            break;
        case 's': 
            set_bit = atoi(optarg);
            break; 
        case 'E': 
            line_num = atoi(optarg);
            break; 
        case 'b': 
            block_bit = atoi(optarg);
            break; 
        case 't':
            if((filename=malloc(strlen(optarg)))==NULL)
            {
                print_usage();
                return 0;
            }
            memcpy(filename, optarg, strlen(optarg));
            break;
        default:
            print_usage();
            return 0;
        }
    }

    if(line_num<=0||block_bit<=0||set_bit<=0)
    {
        print_usage();
        return 0;
    }
    
    if(!(file=fopen(filename,"r")))
    {
        printf("%s: No such file or directory",filename);
    }
    
    init_cache(&cache,block_bit,line_num,set_bit);

    char acc_t;
    long addr;
    int byte_num;
    int hits=0,misses=0,evictions=0;

    while(fscanf(file, "%c %lx,%d", &acc_t, &addr, &byte_num) != EOF)
    {
        char hme; 
        if(acc_t!='L' && acc_t!='M' && acc_t!='S') continue;
        if(visit_cache(&cache,addr)==true)
        {
            hits++;
            hme=4;
        }
        else if(replace_cache(&cache,addr)==true)
        {
            evictions++;
            misses++;
            hme=3;
        }
        else
        {
            misses++;
            hme=2;
        }
        if(acc_t=='M')
        {
            hme+=4;
            hits++;
        }
        if(disp)
        {
            if((hme&2)==2)printf(" miss");
            if((hme&1)==1)printf(" eviction");
            if((hme&4)==4)printf(" hit");
            if((hme&8)==8)printf(" hit hit");
            printf("\n");
        }
    }
    printSummary(hits, misses, evictions);
    return 0;
}
