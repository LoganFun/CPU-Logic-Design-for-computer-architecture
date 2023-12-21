///////////////////////////////////////////////////////////////////////////////
// You will need to modify this file to implement part A and, for extra      //
// credit, parts E and F.                                                    //
///////////////////////////////////////////////////////////////////////////////

// cache.cpp
// Defines the functions used to implement the cache.

#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream> // for cout
#include <stdlib.h> // for calloc
#include <assert.h> // for assert

// You may add any other #include directives you need here, but make sure they
// compile on the reference machine!

///////////////////////////////////////////////////////////////////////////////
//                    EXTERNALLY DEFINED GLOBAL VARIABLES                    //
///////////////////////////////////////////////////////////////////////////////

/**
 * The current clock cycle number.
 * 
 * This can be used as a timestamp for implementing the LRU replacement policy.
 */
extern uint64_t current_cycle;

/**
 * For static way partitioning, the quota of ways in each set that can be
 * assigned to core 0.
 * 
 * The remaining number of ways is the quota for core 1.
 * 
 * This is used to implement extra credit part E.
 */
extern unsigned int SWP_CORE0_WAYS;

uint64_t num_Hit_core0 = 0;
uint64_t num_Hit_core1 = 0;


///////////////////////////////////////////////////////////////////////////////
//                           FUNCTION DEFINITIONS                            //
///////////////////////////////////////////////////////////////////////////////

// As described in cache.h, you are free to deviate from the suggested
// implementation as you see fit.

// The only restriction is that you must not remove cache_print_stats() or
// modify its output format, since its output will be used for grading.

/**
 * Allocate and initialize a cache.
 * 
 * This is intended to be implemented in part A.
 *
 * @param size The size of the cache in bytes.
 * @param associativity The associativity of the cache.
 * @param line_size The size of a cache line in bytes.
 * @param replacement_policy The replacement policy of the cache.
 * @return A pointer to the cache.
 */
Cache *cache_new(uint64_t size, uint64_t associativity, uint64_t line_size,
                 ReplacementPolicy replacement_policy)
{
    // TODO: Allocate memory to the data structures and initialize the required
    //       fields. (You might want to use calloc() for this.)

    // Cache memory allocation 
    Cache *c = (Cache *) calloc(1,sizeof(Cache));

    //Allocate related values
    c->num_ways=associativity;
        //Error Test
        if(c->num_ways > MAX_WAYS_PER_CACHE_SET)
        {
            std::cout<<"Num of ways exceed max numeber"<<MAX_WAYS_PER_CACHE_SET<<std::endl; 
        }

    c->replacementPolicy = replacement_policy;

    c->num_sets = size/(line_size*c->num_ways);

    c->sets = (CacheSet *) calloc (c->num_sets, sizeof(CacheSet));
    
    c->stat_write_miss = 0;
    c->stat_write_access = 0;
    c->stat_read_miss = 0;
    c->stat_read_access = 0;
    c->stat_dirty_evicts = 0; 

    return c;
}

/**
 * Access the cache at the given address.
 * 
 * Also update the cache statistics accordingly.
 * 
 * This is intended to be implemented in part A.
 * 
 * @param c The cache to access.
 * @param line_addr The address of the cache line to access (in units of the
 *                  cache line size, i.e., excluding the line offset bits).
 * @
 * param is_write Whether this access is a write.
 * @param core_id The CPU core ID that requested this access.
 * @return Whether the cache access was a hit or a miss.
 */
CacheResult cache_access(Cache *c, uint64_t line_addr, bool is_write,
                         unsigned int core_id)
{
    // TODO: Return HIT if the access hits in the cache, and MISS otherwise.
    // TODO: If is_write is true, mark the resident line as dirty.
    // TODO: Update the appropriate cache statistics.
    uint64_t set_num;
    uint64_t tag;

    //For tag, take the left part of addr
    tag = line_addr/c->num_sets;

    //For the set index, take the right part of addr
    set_num = line_addr % c->num_sets;
        // Faster
        // set_index = lineaddr & (c->num_sets-1); 

    //For test use
    //std::cout<<"Line_addr"<<line_addr<<"set_num"<<set_num<<"tag"<<tag<<std::endl;
    for ( uint64_t way_num = 0; way_num < c->num_ways; way_num++)
    {   
        // For Hit 
        // Valid true + core id match + tag match
        if((c->sets[set_num].line[way_num].valid == true)
            && (c->sets[set_num].line[way_num].core_id == core_id)
            && (c->sets[set_num].line[way_num].tag == tag)  
          )
        {   
            // If Match
            // Update LRU Time  
            c->sets[set_num].line[way_num].lastAccessTime = current_cycle;
            
            // Check read or write
            // IF it is write
            if (is_write == true)
            {   
                // Update write access
                c->stat_write_access++;
            
                // update dirty
                c->sets[set_num].line[way_num].dirty=true;

                // Install the line
                //cache_install(c,line_addr,is_write,core_id);                
            }
            // IF it is read
            else
            {
                // Update read access num
                c->stat_read_access++;  
            }               
            return HIT;
        }
    }
    
    // For Miss
    // Total access number update
    if (is_write == true)  
    {
        // Update write access
        c->stat_write_access++;
        c->stat_write_miss++;
    } 
    else
    {
        // Update read access
        c->stat_read_access++;
        c->stat_read_miss++;        
    } 

    return MISS;
}

/**
 * Install the cache line with the given address.
 * 
 * Also update the cache statistics accordingly.
 * 
 * This is intended to be implemented in part A.
 * 
 * @param c The cache to install the line into.
 * @param line_addr The address of the cache line to install (in units of the
 *                  cache line size, i.e., excluding the line offset bits).
 * @param is_write Whether this install is triggered by a write.
 * @param core_id The CPU core ID that requested this access.
 */
void cache_install(Cache *c, uint64_t line_addr, bool is_write,
                   unsigned int core_id)
{
    // TODO: Use cache_find_victim() to determine the victim line to evict.
    // TODO: Copy it into a last_evicted_line field in the cache in order to
    //       track writebacks.
    // TODO: Initialize the victim entry with the line to install.
    // TODO: Update the appropriate cache statistics.
    
    // Find the cache line
    // Calculate set index and tag for find() function
    uint64_t set_num = line_addr % c->num_sets;
    uint64_t tag = line_addr / c->num_sets;
    // Find the cache line id
    unsigned int line_id = cache_find_victim(c, set_num, core_id);

    // Then move it to evicted line
    c->lastEvictedLine = c->sets[set_num].line[line_id];
    // Empty the original one
    c->sets[set_num].line[line_id].valid = false;
    c->sets[set_num].line[line_id].dirty = false;

    // Update the cache statistics
    if ((c->lastEvictedLine.valid==true) && (c->lastEvictedLine.dirty==true))
    {
        c->stat_dirty_evicts++;
    }

    // Initialize the victim line with the line to install
    c->sets[set_num].line[line_id].valid = true;
    c->sets[set_num].line[line_id].core_id = core_id;
    c->sets[set_num].line[line_id].lastAccessTime = current_cycle;
    c->sets[set_num].line[line_id].tag = tag;

    // For write or not
    if (is_write == true)
    {
        c->sets[set_num].line[line_id].dirty = true;        
    }
    else
    {
        c->sets[set_num].line[line_id].dirty = false;
    }
    
    // Update Cache access
    // Not sure
}

/**
 * Find which way in a given cache set to replace when a new cache line needs
 * to be installed. This should be chosen according to the cache's replacement
 * policy.
 * 
 * The returned victim can be valid (non-empty), in which case the calling
 * function is responsible for evicting the cache line from that victim way.
 * 
 * This is intended to be initially implemented in part A and, for extra
 * credit, extended in parts E and F.
 * 
 * @param c The cache to search.
 * @param set_index The index of the cache set to search.
 * @param core_id The CPU core ID that requested this access.
 * @return The index of the victim way.
 */
unsigned int cache_find_victim(Cache *c, unsigned int set_index,
                               unsigned int core_id)
{
    // TODO: Find a victim way in the given cache set according to the cache's
    //       replacement policy.
    // TODO: In part A, implement the LRU and random replacement policies.
    // TODO: In part E, for extra credit, implement static way partitioning.
    // TODO: In part F, for extra credit, implement dynamic way partitioning.

    unsigned int return_id = c->num_ways;
    uint64_t least_cycle = 0xFFFFFFFF;


    //Select which policy
    switch (c->replacementPolicy)
    {
    /* LRU Policy */
    case 0:{
        for (uint64_t way_num = 0; way_num < c->num_ways; way_num++)
        {
            /* code */
            if (c->sets[set_index].line[way_num].valid==true)
            {
                if (least_cycle > c->sets[set_index].line[way_num].lastAccessTime)
                {
                    least_cycle = c->sets[set_index].line[way_num].lastAccessTime;
                    return_id = way_num;
                }
            }
            else
            {
                return way_num;
            }
        }       
        
        break;
    }
    // Random
    case 1: 
    {
        for (uint64_t way_num = 0; way_num < c->num_ways; way_num++)
        {
            if (c->sets[set_index].line[way_num].valid==true)
            {
                return_id = (rand() % c->num_ways);
            }
            else
            {
                return way_num;
            }
        }   

        break;
    }
    // SWU
    case 2:
    {
        // Invalid first
        for (uint64_t way_num = 0; way_num < c->num_ways; way_num++)
        {
            /* code */
            if (c->sets[set_index].line[way_num].valid==false)
            {
                return way_num;
            }        
        }

        // Check for Quota
        uint64_t num_core0 = 0;
        uint64_t num_core1 = 0;
        
        for(uint64_t i=0; i<c->num_ways; i++) 
        {
            if (c->sets[set_index].line[i].core_id == 0) 
            {
                num_core0++;
            }
            else 
            //if (c->sets[set_index].line[i].core_id == 1 )
            {
                num_core1++;
            }
        }

        bool core0_over_quota = false;
        bool core1_over_quota = false;
        
        if (num_core0 > SWP_CORE0_WAYS)
        {
            core0_over_quota = true;            
        }

        if (num_core1 > (c->num_ways - SWP_CORE0_WAYS))
        {
            core1_over_quota = true;     
        }
                 
        // Different Core
        if (core0_over_quota || ((core_id == 0) && (!core1_over_quota)))
        {
            for (uint64_t way_num = 0; way_num < c->num_ways; way_num++)
            {
                if(c->sets[set_index].line[way_num].core_id == 0)
                {
                    if (least_cycle > c->sets[set_index].line[way_num].lastAccessTime)
                    {
                        least_cycle = c->sets[set_index].line[way_num].lastAccessTime;
                        return_id = way_num;
                    }
                }
            }
        }
        // Core 1 
        else if(core1_over_quota || ((core_id == 1) && (!core0_over_quota)))
        {
            for (uint64_t way_num = 0; way_num < c->num_ways; way_num++)
            {   
                if(c->sets[set_index].line[way_num].core_id == 1)
                {
                    if (least_cycle > c->sets[set_index].line[way_num].lastAccessTime)
                    {
                        least_cycle = c->sets[set_index].line[way_num].lastAccessTime;
                        return_id = way_num;
                    }
                }
            }
        }

        break;
    }
    // DWU
    case 3:
    {

    }
    
    default:
        break;
    }

    return return_id;
}

/**
 * Print the statistics of the given cache.
 * 
 * This is implemented for you. You must not modify its output format.
 * 
 * @param c The cache to print the statistics of.
 * @param label A label for the cache, which is used as a prefix for each
 *              statistic.
 */
void cache_print_stats(Cache *c, const char *header)
{
    double read_miss_percent = 0.0;
    double write_miss_percent = 0.0;

    if (c->stat_read_access)
    {
        read_miss_percent = 100.0 * (double)(c->stat_read_miss) /
                            (double)(c->stat_read_access);
    }

    if (c->stat_write_access)
    {
        write_miss_percent = 100.0 * (double)(c->stat_write_miss) /
                             (double)(c->stat_write_access);
    }

    printf("\n");
    printf("%s_READ_ACCESS     \t\t : %10llu\n", header, c->stat_read_access);
    printf("%s_WRITE_ACCESS    \t\t : %10llu\n", header, c->stat_write_access);
    printf("%s_READ_MISS       \t\t : %10llu\n", header, c->stat_read_miss);
    printf("%s_WRITE_MISS      \t\t : %10llu\n", header, c->stat_write_miss);
    printf("%s_READ_MISS_PERC  \t\t : %10.3f\n", header, read_miss_percent);
    printf("%s_WRITE_MISS_PERC \t\t : %10.3f\n", header, write_miss_percent);
    printf("%s_DIRTY_EVICTS    \t\t : %10llu\n", header, c->stat_dirty_evicts);
}
