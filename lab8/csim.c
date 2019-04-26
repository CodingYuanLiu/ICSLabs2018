/* Liu Qingyuan 516072910016
 */


#include "cachelab.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <math.h>

#define IS_BLANK(s) (*(s)==' ' || *(s)=='\t' || *(s)=='\n')
#define IS_DELIM(s) (*(s)==',')
#define IS_DIGIT(s) (*(s)>='0' || *(s) <= '9')
#define SKIP_BLANK(s) while(IS_BLANK(s)) (s)++;
#define MAX_LINE_SIZE 64


/* Data structure of a single cache line*/
typedef struct{
    int valid;
    long long int tag;
    long long int setNum;
    int lineNum;
    int accessSeq;
}Cache;

/* Mode of the memory access */
typedef enum {Inst_Load,Load,Store,Modify} Mode;

/* Hit status of a memory access */
typedef enum {Cache_HIT,Cold_Miss,Conflict_Miss} Line_status;

void parseArg(char **fileloc,int argc,char *argv[]);
void parseline(char* line);
void analyse_line(Cache* cache);
void print_cache(Cache* cache);//For debug
void init_Cache(Cache *cache);
int replace_line(Cache *cache,long long int tag,long long int setBit);//Fine the line to fulfill the memory access.


static Mode mode;
static Line_status lst;
static long long int addr;
static int size;
static int hit,miss,eviction;
static int s,E,b;
static int S,B;
static int accessNum;

int main(int argc,char *argv[])
{
    s=0;E=0;b=0;
    S=0;B=0;
    char* line=(char *)malloc(MAX_LINE_SIZE);
    
    hit=0;miss=0;eviction=0;

    char* fileloc=NULL;
    FILE* trace;

    //Get the arguments.
    parseArg(&fileloc,argc,argv);
    trace = fopen(fileloc,"r");
    if(!trace) printf("Wrong File location.\n");

    //Create a cache.
    Cache *cache = (Cache *)malloc(E*S*sizeof(Cache));
    init_Cache(cache);
    
    //Init the access number.
    accessNum = 0;
    while(fgets(line,MAX_LINE_SIZE,trace))
    {
        //printf("%s",line); //For debugging.
        accessNum++;
        // Get a memory access.
        parseline(line);
        // Do the memory access.
        analyse_line(cache);
    
    }
    printSummary(hit, miss, eviction);

    return 0;
}

/* 
 * parseArg: get the arguments 
*/
void parseArg(char **fileloc,int argc,char *argv[])
{
    int ch;
    while((ch=getopt(argc,argv,"s:E:b:t:")) != -1)
    {
        switch(ch)
        {
            case 's':
                s = atoi(optarg);
                S=pow(2,s);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                B=pow(2,b);
                break;
            case 't':
                *fileloc = (char *)malloc(strlen(optarg)+1);
                strcpy(*fileloc,optarg);   
                break;
            default:
                printf("Wrong args");
                break;
        }
    }
}

/*
 * parseline: Get a single memory access and read the mode, address and size.  
 */

void parseline(char* line)
{
    SKIP_BLANK(line);
    switch(*line)
    {
        case 'I':
            mode = Inst_Load;break;
        case 'L':
            mode = Load;break;
        case 'S':
            mode = Store;break;
        case 'M':
            mode = Modify;break;
    }
    line++;
    SKIP_BLANK(line);
    addr = strtoll(line,&line,16);
    //printf("mode:%d, addr:%llx, ",mode,addr);
    SKIP_BLANK(line);
    line++;
    SKIP_BLANK(line);
    size = (int)strtoll(line,&line,10);
    //printf("size:%d \n",size);
}

/*
 * init-Cache: Initialize the cache lines at the beginning,
 */
void init_Cache(Cache *cache)
{
    for(int i = 0;i<E*S;i++)
    {
        cache[i].tag=0;
        cache[i].valid=0;
        cache[i].setNum=i/E;
        cache[i].lineNum=i%E;
        cache[i].accessSeq=0;
    }
}

/*
 * print_cache: Print out every single cache line for debugging.
 */
void print_cache(Cache* cache)
{
    for(int i =0; i<E*S;i++) {
        printf("valid:%d,tag:%lld,setNum:%lld,lineNum:%d,accessSeq:%d\n",
        cache[i].valid,cache[i].tag,cache[i].setNum,cache[i].lineNum,cache[i].accessSeq);
    }
}

/*
 * analyse_line: Analyse a memory access and add the count of hits, misses, evictions.
 */
void analyse_line(Cache* cache)
{
    if(mode == Inst_Load)
        return;
    long long int index = (addr & (unsigned long)-1>>(64-b));
    long long int setBit = ((addr & (unsigned long)-1>>(64-b-s)) & (~index))>>b;
    long long int tag = (addr & ~((unsigned long)-1>>(64-b-s)))>>(s+b);
    //printf("tag:%llx,setBit:%llx,index:%llx\n",tag,setBit,index); //For debugging
    int lineindex = replace_line(cache,tag,setBit);

    cache[E*setBit + lineindex].accessSeq= accessNum;
    switch(lst){
        case Cache_HIT:{
            hit++;
            break;
        }
        case Cold_Miss:{
            cache[E*setBit + lineindex].valid = 1;
            cache[E*setBit + lineindex].tag = tag;
            miss++; 
            break;
        }
        case Conflict_Miss:{
            cache[E*setBit + lineindex].tag = tag;
            miss++;
            eviction++;
            break;
        }
        default:
            printf("Error lst\n");
    }

    if(mode == Modify){
        hit++;
    }

}



/*
 * replace_line: Find the location which is required by the memory access.
 * Set the status of the access by the way.
 */
int replace_line(Cache* cache,long long int tag,long long int setBit)
{
    Cache line;

    /* First iteration: check the valid bit.*/
    for(int i = 0;i<E;i++){
        line = cache[E*setBit + i];
        if(!line.valid) //The set is not full and cache miss.
        {
            lst = Cold_Miss;
            return i; //The first empty line in the set.
        }
        if(line.tag == tag && line.valid) //cache hit occurs.
        {
            lst = Cache_HIT;
            return i;
        }
    }
    /* Second iteration: a conflict miss occurs, LRU to find the replaced line.*/
    /* Find the line whose accessSeq is minimum*/
    int min = accessNum;
    int minline = 0;
    for(int i =0;i<E;i++) {
        line = cache[E*setBit + i];
        if(line.accessSeq < min)
        {
            min = line.accessSeq;
            minline = i;
        }
    }
    lst = Conflict_Miss;

    return minline;
}