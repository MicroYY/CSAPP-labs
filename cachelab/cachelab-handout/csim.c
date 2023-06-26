#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "cachelab.h"

void printHelp()
{
    printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n"
            "Options:\n"
            "  -h         Print this help message.\n"
            "  -v         Optional verbose flag.\n"
            "  -s <num>   Number of set index bits.\n"
            "  -E <num>   Number of lines per set.\n"
            "  -b <num>   Number of block offset bits.\n"
            "  -t <file>  Trace file.\n\n"
            "Examples:\n"
            "  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n"
            "  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

int numMiss = 0, numHit = 0, numEvict = 0;

int printUsage = 0;
int printTrace = 0;
char tracefile[256];

typedef struct
{
    int valid;
    int tag;
    unsigned timeStamp;
} CacheLine;

typedef struct
{
    CacheLine* cacheLine;
    int maxStampIdx;
} CacheSet;

typedef struct
{
    CacheSet* cacheSet;
    int S;
    int E;
    int B;
} Cache;

Cache* InitCache(int S, int E, int B)
{
    Cache* cache    = (Cache*)malloc(sizeof(Cache));
    cache->S        = S;
    cache->E        = E;
    cache->B        = B;
    cache->cacheSet = (CacheSet*)malloc(sizeof(CacheSet) * S);
    for (size_t i = 0; i < S; i++)
    {
        cache->cacheSet[i].cacheLine = (CacheLine*)malloc(sizeof(CacheLine) * E);
        cache->cacheSet[i].maxStampIdx       = 0;
        for (size_t j = 0; j < E; j++)
        {
            cache->cacheSet[i].cacheLine[j].tag       = -1;
            cache->cacheSet[i].cacheLine[j].timeStamp = 0;
            cache->cacheSet[i].cacheLine[j].valid     = 0;
        }
        
    }
    return cache;
}

void DestroyCache(Cache* cache)
{
    for (size_t i = 0; i < cache->S; i++)
    {
        free(cache->cacheSet[i].cacheLine);
    }
    free(cache->cacheSet);
    free(cache);
    
}

int GetLineIdx(Cache* cache, unsigned CT, unsigned CI, unsigned CO)
{
    for (int i = 0; i < cache->E; i++)
    {
        if (cache->cacheSet[CI].cacheLine[i].valid && 
            cache->cacheSet[CI].cacheLine[i].tag == CT)
        {
            return i;
        }
    }
    return -1;
}

void UpdateCacheSet(CacheSet* cacheSet, int cacheLineIdx, unsigned CT)
{
    cacheSet->cacheLine[cacheLineIdx].valid = 1;
    cacheSet->cacheLine[cacheLineIdx].tag   = CT;
    cacheSet->cacheLine[cacheLineIdx].timeStamp = cacheSet->cacheLine[cacheSet->maxStampIdx].timeStamp + 1;
    cacheSet->maxStampIdx = cacheLineIdx;
}

int FindEmptyLine(CacheSet* cacheSet, int E)
{
    for (size_t i = 0; i < E; i++)
    {
        if(cacheSet->cacheLine[i].valid == 0)
        {
            return i;
        }
    }
    return -1;
}

int Evict(CacheSet* cacheSet, int E)
{
    unsigned min = cacheSet->cacheLine[0].timeStamp;
    unsigned max = min;
    int minIdx = 0; int maxIdx = 0;
    for (size_t i = 1; i < E; i++)
    {
        if (cacheSet->cacheLine[i].timeStamp < min)
        {
            min = cacheSet->cacheLine[i].timeStamp;
            minIdx = i;
        }
        if (cacheSet->cacheLine[i].timeStamp > max)
        {
            max = cacheSet->cacheLine[i].timeStamp;
            maxIdx = i;
        }
    }
    cacheSet->cacheLine[minIdx].timeStamp = cacheSet->cacheLine[maxIdx].timeStamp;

    return minIdx;
}

void UpdateCache(Cache* cache, unsigned CT, unsigned CI, unsigned CO)
{
    int lineIdx = GetLineIdx(cache, CT, CI, CO);
    if (lineIdx == -1)
    {
        if (printTrace) printf(" miss");
        numMiss++;
        lineIdx = FindEmptyLine(&(cache->cacheSet[CI]), cache->E);
        if(lineIdx == -1)
        {
            if (printTrace) printf(" eviction");
            numEvict++;
            lineIdx = Evict(&(cache->cacheSet[CI]), cache->E);
        }
        UpdateCacheSet(&cache->cacheSet[CI], lineIdx, CT);
    }
    else
    {
        if (printTrace) printf(" hit");
        numHit++;
        UpdateCacheSet(&cache->cacheSet[CI], lineIdx, CT);
    }
}

#define digits 64

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        printHelp();
        return 0;
    }
    
    int s = 0, E = 0, b = 0;
    int S = 0, B = 0;

    int arg;
    while ((arg = getopt(argc, argv, "hvs:E:b:t:")) != -1)
    {
        switch (arg)
        {
        case 'h':
            printUsage = 1;
            printHelp();
            //printf("Print usage info: %d\n", printUsage);
            return 0;
            break;
        case 'v':
            printTrace = 1;
            //printf("Display trace info: %d\n", printTrace);
            break;
        case 's':
            s = atoi(optarg);
            S = pow(2, s);
            //printf("s: %d, S: %d\n", s, S);
            break;
        case 'E':
            E = atoi(optarg);
            //printf("E: %d\n", E);
            break;
        case 'b':
            b = atoi(optarg);
            B = pow(2, b);
            //printf("b: %d, B: %d\n", b, B);
            break;
        case 't':
            strcpy(tracefile, optarg);
            //printf("Name of valgrind trace to replay: %s\n", tracefile);
            break;
        default:
            break;
        }
    }

    int t = digits - s - b;

    Cache* cache = InitCache(S, E, B);

    FILE *file;
    file = fopen(tracefile, "r");
    if(!file) return -1;

    char op;
    unsigned int addr;
    unsigned int size;

    while (fscanf(file, " %c %x,%d", &op, &addr, &size) > 0)
    {
        unsigned CT = addr >> (s + b);
        unsigned CI = (addr << t) >> (digits - s);
        unsigned CO = (addr << (t + s)) >> (digits - b);
        switch (op)
        {
        case 'L':
            if (printTrace) printf("L %x,1", addr);
            UpdateCache(cache, CT, CI, CO);
            break;
        case 'M':
            if (printTrace) printf("M %x,1", addr);
            UpdateCache(cache, CT, CI, CO);
            UpdateCache(cache, CT, CI, CO);
            break;
        case 'S':
            if (printTrace) printf("S %x,1", addr);
            UpdateCache(cache, CT, CI, CO);
            break;
        default:
            break;
        }
        if (printTrace) printf("\n");
    }

    printSummary(numHit, numMiss, numEvict);

    DestroyCache(cache);
    fclose(file);

    return 0;
}
