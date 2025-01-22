#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

int associativity = 2;
int blocksize_bytes = 32;
int cachesize_kb = 64;
int miss_penalty = 30;
int j = 1;
int hit;
int filled_ways = 0;   // when empty ways found in set of cache
int missing_block = 0; // when address not found in cache

char marker;
int loadstore;
int icount;
unsigned int address;
unsigned int tag;
unsigned int set;
unsigned int block_offset;
unsigned int block_bits;
unsigned int set_bits;
unsigned int sets;

int exec_time = 0;
int instructions = 0;
int memory_accesses;
float overall_miss_rate = 0;
float read_miss_rate = 0;
float memory_cpi = 0;
float total_cpi = 0;
float amat = 0;
int dirty_evictions = 0;
int load_misses = 0;
int store_misses = 0;
int store_hits = 0;
int load_hits = 0;
int use_bit;

void print_usage()
{
  printf("Usage: gunzip2 -c <tracefile> | ./cache -a assoc -l blocksz -s size -mp mispen\n");
  printf("  tracefile : The memory trace file\n");
  printf("  -a assoc : The associativity of the cache\n");
  printf("  -l blocksz : The blocksize (in bytes) of the cache\n");
  printf("  -s size : The size (in KB) of the cache\n");
  printf("  -mp mispen: The miss penalty (in cycles) of a miss\n");
  exit(0);
}
void accept_command_line_arguments(int argc, char *argv[])
{
  while (j < argc)
  {
    if (strcmp("-a", argv[j]) == 0)
    {
      j++;
      if (j >= argc)
        print_usage();
      associativity = atoi(argv[j]);
      j++;
    }
    else if (strcmp("-l", argv[j]) == 0)
    {
      j++;
      if (j >= argc)
        print_usage();
      blocksize_bytes = atoi(argv[j]);
      j++;
    }
    else if (strcmp("-s", argv[j]) == 0)
    {
      j++;
      if (j >= argc)
        print_usage();
      cachesize_kb = atoi(argv[j]);
      j++;
    }
    else if (strcmp("-mp", argv[j]) == 0)
    {
      j++;
      if (j >= argc)
        print_usage();
      miss_penalty = atoi(argv[j]);
      j++;
    }
    else
    {
      print_usage();
    }
  }
}
void print_cache_configuration()
{
  if (1)
  {
    printf("Cache parameters:\n");
    printf("Cache Size (KB)\t\t\t%d\n", cachesize_kb);
    printf("Cache Associativity\t\t%d\n", associativity);
    printf("Cache Block Size (bytes)\t%d\n", blocksize_bytes);
    printf("Miss penalty (cyc)\t\t%d\n", miss_penalty);
    printf("Total Sets\t\t\t%d\n", sets);
    printf("\n");
  }
}

int main(int argc, char *argv[])
{
  time_t start_time = time(NULL);
  clock_t clock();
  // read command line arguments
  accept_command_line_arguments(argc, argv);

  // if arguments are negative
  if ((associativity < 1) || (blocksize_bytes < 1) || (cachesize_kb < 1) || (miss_penalty < 1))
  {
    printf("Error, arguments must not be negative\n");
    EXIT_FAILURE;
  }

  // calculate number of sets
  sets = (cachesize_kb * 1024) / (blocksize_bytes * associativity);
  set_bits = floor(log2(sets));

  // no of bits needed for block_offset
  block_bits = floor(log2(blocksize_bytes));

  // LRU_bit = associativity - 1;
  // MRU = 0
  int use_bit = associativity - 1;

  if (sets == 1)
  { // fully associative cache
    printf("FULLY ASSOCIATIVE CACHE\n");
    unsigned int cache[associativity][4];
    for (int i = 0; i < associativity; i++)
    {
      cache[i][0] = 0;
      cache[i][1] = 0;
      cache[i][2] = use_bit--;
      cache[i][3] = 0;
    }

    while (scanf("%c %d %x %d\n", &marker, &loadstore, &address, &icount) != EOF)
    {
      // accessing tag and index field from address
      block_offset = (address << (32 - block_bits)) >> (32 - block_bits);
      set = (address << (32 - (block_bits + set_bits))) >> (32 - set_bits);
      tag = address >> (block_bits + set_bits);
      instructions += icount;
      exec_time += icount;
      memory_accesses += 1;

      if (loadstore == 0) // load instruction
      {
        for (int i = 0; i < associativity; i++) // looping through the specified set
        {
          if ((tag == cache[i][0]) && (cache[i][1] == 1)) // load hit
          {
            load_hits += 1;
            missing_block = 0;
            for (int j = 0; j < associativity; j++)
            {
              if (cache[i][2] > cache[j][2])
              {
                cache[j][2] += 1;
              }
            }
            cache[i][2] = 0;
            break;
          }
          else if (tag != cache[i][0])
          {
            missing_block = 1;
            continue;
          }
        }
        if (missing_block == 1)
        {
          load_misses += 1;
          for (int i = 0; i < associativity; i++) // check for empty ways
          {
            if (cache[i][1] == 0) // empty way found
            {
              filled_ways = 0;
              break;
            }
            else
            { // empty way not found
              filled_ways = 1;
              continue;
            }
          }
          if (filled_ways == 0) // some ways empty
          {
            for (int i = 0; i < associativity; i++)
            {
              if (cache[i][1] == 0) // way use_bit
              {
                exec_time += miss_penalty;
                cache[i][0] = tag;
                cache[i][1] = 1;
                for (int j = 0; j < associativity; j++)
                {
                  if (cache[i][2] > cache[j][2])
                  {
                    cache[j][2] += 1;
                  }
                }
                cache[i][2] = 0;
                cache[i][3] = 0;
                break;
              }
            }
          }
          if (filled_ways == 1) // empty way not found
          {
            for (int i = 0; i < associativity; i++)
            {
              if (cache[i][2] == (unsigned int)(associativity - 1)) // least recently used block
              {
                cache[i][0] = tag;
                if (cache[i][3] == 1)
                {
                  dirty_evictions += 1;
                  exec_time += (miss_penalty + 2);
                  cache[i][3] = 0;
                }
                else
                {
                  exec_time += miss_penalty;
                  cache[i][3] = 0;
                }
                for (int j = 0; j < associativity; j++)
                {
                  if (cache[i][2] > cache[j][2])
                  {
                    cache[j][2] += 1;
                  }
                }
                cache[i][2] = 0;
                break;
              }
            }
          }
        }
      }

      if (loadstore == 1) // store instruction
      {
        for (int i = 0; i < associativity; i++) // looping through the specified set
        {
          if ((tag == cache[i][0]) && (cache[i][1] == 1)) // store hit
          {
            store_hits += 1;
            missing_block = 0;
            for (int j = 0; j < associativity; j++)
            {
              if (cache[i][2] > cache[j][2])
              {
                cache[j][2] += 1;
              }
            }
            cache[i][2] = 0;
            cache[i][3] = 1;
            break;
          }
          else if (tag != cache[i][0])
          {
            missing_block = 1;
            continue;
          }
        }
        if (missing_block == 1)
        {
          store_misses += 1;
          for (int i = 0; i < associativity; i++) // check for empty ways
          {
            if (cache[i][1] == 0) // empty way found
            {
              filled_ways = 0;
              break;
            }
            else
            { // empty way not found
              filled_ways = 1;
              continue;
            }
          }
          if (filled_ways == 0) // some ways empty
          {
            for (int i = 0; i < associativity; i++)
            {
              if (cache[i][1] == 0) // way use_bit
              {
                exec_time += miss_penalty;
                cache[i][0] = tag;
                cache[i][1] = 1;
                for (int j = 0; j < associativity; j++)
                {
                  if (cache[i][2] > cache[j][2])
                  {
                    cache[j][2] += 1;
                  }
                }
                cache[i][2] = 0;
                cache[i][3] = 1;
                break;
              }
            }
          }
          if (filled_ways == 1) // empty way not found
          {
            for (int i = 0; i < associativity; i++)
            {
              if (cache[i][2] == (unsigned int)(associativity - 1)) // least recently used block
              {
                cache[i][0] = tag;
                if (cache[i][3] == 1)
                {
                  dirty_evictions += 1;
                  exec_time += (miss_penalty + 2);
                  cache[i][3] = 1;
                }
                else
                {
                  exec_time += miss_penalty;
                  cache[i][3] = 1;
                }
                for (int j = 0; j < associativity; j++)
                {
                  if (cache[i][2] > cache[j][2])
                  {
                    cache[j][2] += 1;
                  }
                }
                cache[i][2] = 0;
                break;
              }
            }
          }
        }
      }
    }
    overall_miss_rate = (float)(load_misses + store_misses) / memory_accesses;
    read_miss_rate = (float)load_misses / (load_misses + load_hits);
    memory_cpi = (float)(exec_time - instructions) / instructions;
    total_cpi = (float)exec_time / instructions;
    amat = (float)(exec_time - instructions) / memory_accesses;
    // show cache configuration
    print_cache_configuration();

    printf("Simulation results:\n\a");

    printf("\texecution time %u cycles\n", exec_time);
    printf("\tinstructions %u\n", instructions);
    printf("\tmemory accesses %u\n", memory_accesses);
    printf("\toverall miss rate %.2f\n", overall_miss_rate);
    printf("\tread miss rate %.2f\n", read_miss_rate);
    printf("\tmemory CPI %.2f\n", memory_cpi);
    printf("\ttotal CPI %.2f\n", total_cpi);
    printf("\taverage memory access time %.2f cycles\n", amat);
    printf("dirty evictions %d\n", dirty_evictions);
    printf("load_misses %u\n", load_misses);
    printf("store_misses %u\n", store_misses);
    printf("load_hits %u\n", load_hits);
    printf("store_hits %u\n", store_hits);
    time_t end_time = time(NULL);
    printf("Total time taken: %f\n", difftime(end_time, start_time));
    printf("Clock cycle: %f", difftime(end_time, start_time) / CLOCKS_PER_SEC);
    EXIT_SUCCESS;
  }
  else
  {

    // initialize cache
    unsigned int cache[sets][associativity][4];
    for (unsigned int i = 0; i < sets; i++)
    {
      use_bit = associativity - 1;
      for (int j = 0; j < associativity; j++)
      {
        cache[i][j][0] = 0;       // tag
        cache[i][j][1] = 0;       // valid
        cache[i][j][2] = use_bit; // used
        cache[i][j][3] = 0;       // dirty
        --use_bit;
      }
    }

    // read memory access trace
    while (scanf("%c %d %x %d\n", &marker, &loadstore, &address, &icount) != EOF)
    {
      // accessing tag and index field from address
      block_offset = (address << (32 - block_bits)) >> (32 - block_bits);
      set = (address << (32 - (block_bits + set_bits))) >> (32 - set_bits);
      tag = address >> (block_bits + set_bits);
      instructions += icount;
      exec_time += icount;
      memory_accesses += 1;

      if (loadstore == 0) // load instruction
      {
        for (int i = 0; i < associativity; i++) // looping through the specified set
        {
          if ((tag == cache[set][i][0]) && (cache[set][i][1] == 1)) // load hit
          {
            load_hits += 1;
            missing_block = 0;
            for (int j = 0; j < associativity; j++)
            {
              if (cache[set][i][2] > cache[set][j][2])
              {
                cache[set][j][2] += 1;
              }
            }
            cache[set][i][2] = 0;
            break;
          }
          else if (tag != cache[set][i][0])
          {
            missing_block = 1;
            continue;
          }
        }
        if (missing_block == 1)
        {
          load_misses += 1;
          for (int i = 0; i < associativity; i++) // check for empty ways
          {
            if (cache[set][i][1] == 0) // empty way found
            {
              filled_ways = 0;
              break;
            }
            else
            { // empty way not found
              filled_ways = 1;
              continue;
            }
          }
          if (filled_ways == 0) // some ways empty
          {
            for (int i = 0; i < associativity; i++)
            {
              if (cache[set][i][1] == 0) // way use_bit
              {
                exec_time += miss_penalty;
                cache[set][i][0] = tag;
                cache[set][i][1] = 1;
                for (int j = 0; j < associativity; j++)
                {
                  if (cache[set][i][2] > cache[set][j][2])
                  {
                    cache[set][j][2] += 1;
                  }
                }
                cache[set][i][2] = 0;
                cache[set][i][3] = 0;
                break;
              }
            }
          }
          if (filled_ways == 1) // empty way not found
          {
            for (int i = 0; i < associativity; i++)
            {
              if (cache[set][i][2] == (unsigned int)(associativity - 1)) // least recently used block
              {
                cache[set][i][0] = tag;
                if (cache[set][i][3] == 1)
                {
                  dirty_evictions += 1;
                  exec_time += (miss_penalty + 2);
                  cache[set][i][3] = 0;
                }
                else
                {
                  exec_time += miss_penalty;
                  cache[set][i][3] = 0;
                }
                for (int j = 0; j < associativity; j++)
                {
                  if (cache[set][i][2] > cache[set][j][2])
                  {
                    cache[set][j][2] += 1;
                  }
                }
                cache[set][i][2] = 0;
                break;
              }
            }
          }
        }
      }

      if (loadstore == 1) // store instruction
      {
        for (int i = 0; i < associativity; i++) // looping through the specified set
        {
          if ((tag == cache[set][i][0]) && (cache[set][i][1] == 1)) // store hit
          {
            store_hits += 1;
            missing_block = 0;
            for (int j = 0; j < associativity; j++)
            {
              if (cache[set][i][2] > cache[set][j][2])
              {
                cache[set][j][2] += 1;
              }
            }
            cache[set][i][2] = 0;
            cache[set][i][3] = 1;
            break;
          }
          else if (tag != cache[set][i][0])
          {
            missing_block = 1;
            continue;
          }
        }
        if (missing_block == 1)
        {
          store_misses += 1;
          for (int i = 0; i < associativity; i++) // check for empty ways
          {
            if (cache[set][i][1] == 0) // empty way found
            {
              filled_ways = 0;
              break;
            }
            else
            { // empty way not found
              filled_ways = 1;
              continue;
            }
          }
          if (filled_ways == 0) // some ways empty
          {
            for (int i = 0; i < associativity; i++)
            {
              if (cache[set][i][1] == 0) // way use_bit
              {
                exec_time += miss_penalty;
                cache[set][i][0] = tag;
                cache[set][i][1] = 1;
                for (int j = 0; j < associativity; j++)
                {
                  if (cache[set][i][2] > cache[set][j][2])
                  {
                    cache[set][j][2] += 1;
                  }
                }
                cache[set][i][2] = 0;
                cache[set][i][3] = 1;
                break;
              }
            }
          }
          if (filled_ways == 1) // empty way not found
          {
            for (int i = 0; i < associativity; i++)
            {
              if (cache[set][i][2] == (unsigned int)(associativity - 1)) // least recently used block
              {
                cache[set][i][0] = tag;
                if (cache[set][i][3] == 1)
                {
                  dirty_evictions += 1;
                  exec_time += (miss_penalty + 2);
                  cache[set][i][3] = 1;
                }
                else
                {
                  exec_time += miss_penalty;
                  cache[set][i][3] = 1;
                }
                for (int j = 0; j < associativity; j++)
                {
                  if (cache[set][i][2] > cache[set][j][2])
                  {
                    cache[set][j][2] += 1;
                  }
                }
                cache[set][i][2] = 0;
                break;
              }
            }
          }
        }
      }
    }

    overall_miss_rate = (float)(load_misses + store_misses) / memory_accesses;
    read_miss_rate = (float)load_misses / (load_misses + load_hits);
    memory_cpi = (float)(exec_time - instructions) / instructions;
    total_cpi = (float)exec_time / instructions;
    amat = (float)(exec_time - instructions) / memory_accesses;
    // show cache configuration

    if (associativity == 1)
      printf("DIRECT MAPPED CACHE\n");
    else
      printf("SET ASSOCIATIVE CACHE\n");
    print_cache_configuration();
    printf("Simulation results:\n\a");
    printf("\texecution time %u cycles\n", exec_time);
    printf("\tinstructions %u\n", instructions);
    printf("\tmemory accesses %u\n", memory_accesses);
    printf("\toverall miss rate %.2f\n", overall_miss_rate);
    printf("\tread miss rate %.2f\n", read_miss_rate);
    printf("\tmemory CPI %.2f\n", memory_cpi);
    printf("\ttotal CPI %.2f\n", total_cpi);
    printf("\taverage memory access time %.2f cycles\n", amat);
    printf("dirty evictions %d\n", dirty_evictions);
    printf("load_misses %u\n", load_misses);
    printf("store_misses %u\n", store_misses);
    printf("load_hits %u\n", load_hits);
    printf("store_hits %u\n", store_hits);
    time_t end_time = time(NULL);
    printf("Total time taken: %f\n", difftime(end_time, start_time));
    printf("Clock cycle: %f", difftime(end_time, start_time) / CLOCKS_PER_SEC);
  }
}