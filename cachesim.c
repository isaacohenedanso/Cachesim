#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
unsigned associativity = 2;
int blocksize_bytes = 32;
int cachesize_KB = 64;
int miss_penalty = 30;
int j = 1;
int hit;
int filled_ways = 1; // when empty ways found in set of cache
int miss = 0;        // when address not found in cache
char marker;
int loadstore;
int icount;
unsigned long long address;
int addr_len;
unsigned tag;
unsigned set;
unsigned block_offset;
unsigned block_bits;
unsigned set_bits;
unsigned sets;
int exec_time = 0;
int instructions = 0;
int memory_accesses = 0;
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
void print_usage() {
  printf("Usage: gunzip2 -c <tracefile> | ./cache -a assoc -l blocksz -s size "
         "-mp mispen\n");
  printf("  tracefile : The memory trace file\n");
  printf("  -a assoc : The associativity of the cache\n");
  printf("  -l blocksz : The blocksize (in bytes) of the cache\n");
  printf("  -s size : The size (in KB) of the cache\n");
  printf("  -mp mispen: The miss penalty (in cycles) of a miss\n");
  exit(0);
}
void accept_command_line_arguments(int argc, char *argv[]) {
  while (j < argc) {
    if (strcmp("-a", argv[j]) == 0) {
      j++;
      if (j >= argc)
        print_usage();
      associativity = atoi(argv[j]);
      j++;
    } else if (strcmp("-l", argv[j]) == 0) {
      j++;
      if (j >= argc)
        print_usage();
      blocksize_bytes = atoi(argv[j]);
      j++;
    } else if (strcmp("-s", argv[j]) == 0) {
      j++;
      if (j >= argc)
        print_usage();
      cachesize_KB = atoi(argv[j]);
      j++;
    } else if (strcmp("-mp", argv[j]) == 0) {
      j++;
      if (j >= argc)
        print_usage();
      miss_penalty = atoi(argv[j]);
      j++;
    } else {
      print_usage();
    }
  }
}
void print_cache_configuration() {
  if (1) {
    if (sets == 1) { // fully associative cache
      printf("\t\tFULLY ASSOCIATIVE CACHE\n");
    } else {
      if (associativity == 1)
        printf("\t\tDIRECT MAPPED CACHE\n");
      else
        printf("\t\t%d-WAY SET ASSOCIATIVE CACHE\n", associativity);
    }
    printf("-----------------------------------------------\n");
    printf("Cache parameters:\n");
    printf("Cache Size (KB)\t\t\t%d\n", cachesize_KB);
    printf("Cache Associativity\t\t%d\n", associativity);
    printf("Cache Block Size (bytes)\t%d\n", blocksize_bytes);
    printf("Miss penalty (cyc)\t\t%d\n", miss_penalty);
    printf("Total Sets\t\t\t%d\n", sets);
    printf("\n");
  }
}
void print_parameters() {
  overall_miss_rate = (float)(load_misses + store_misses) / memory_accesses;
  read_miss_rate = (float)load_misses / (load_misses + load_hits);
  memory_cpi = (float)(exec_time - instructions) / instructions;
  total_cpi = (float)exec_time / instructions;
  amat = (float)(exec_time - instructions) / memory_accesses;
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
}
void access_fields_from_address() {
  addr_len = sizeof(address) * 8;
  block_bits = floor(log2(blocksize_bytes));
  block_offset =
      (address << (addr_len - block_bits)) >> (addr_len - block_bits);
  if (sets == 1) {
    set_bits = 0;
    set = 0;
    tag = address >> (block_bits);
  } else {
    addr_len = sizeof(address) * 8;
    set_bits = floor(log2(sets)); // no of bits needed for block_offset
    set = (address << (addr_len - (block_bits + set_bits))) >>
          (addr_len - set_bits);
    tag = address >> (block_bits + set_bits);
  }

  instructions += icount;
  exec_time += icount;
  memory_accesses += 1;
}
void update_used_bits(unsigned cache[][associativity][4], int i) {
  for (int j = 0; j < associativity; j++) {
    if (cache[set][i][2] > cache[set][j][2]) {
      cache[set][j][2] += 1;
    }
  }
  cache[set][i][2] = 0;
}
int main(int argc, char *argv[]) {
  // read command line arguments
  accept_command_line_arguments(argc, argv);

  // if arguments are negative
  if (associativity < 1 || blocksize_bytes < 1 || cachesize_KB < 1) {
    printf("Error, arguments must not be negative\n");
    EXIT_FAILURE;
  }

  // calculate number of sets
  sets = (cachesize_KB * 1024) / (blocksize_bytes * associativity);

  // LRU_bit = associativity - 1;
  // MRU = 0
  int use_bit = associativity - 1;

  // initialize cache
  unsigned cache[sets][associativity][4];
  for (unsigned i = 0; i < sets; i++) {
    use_bit = associativity - 1;
    for (int j = 0; j < associativity; j++) {
      cache[i][j][0] = 0;       // valid
      cache[i][j][1] = 0;       // tag
      cache[i][j][2] = use_bit; // used
      cache[i][j][3] = 0;       // dirty
      --use_bit;
    }
  }

  // read memory access trace
  while (scanf("%c %d %llx %d\n", &marker, &loadstore, &address, &icount) !=
         EOF) {
    access_fields_from_address();
    filled_ways = 1;
    miss = 0;
    if (loadstore == 0) // load instruction
    {
      for (int i = 0; i < associativity;
           i++) // looping through the specified set
      {
        if (cache[set][i][0] == 1 && tag == cache[set][i][1]) // load hit
        {
          load_hits += 1;
          miss = 0;
          update_used_bits(cache, i);
          break;
        } else {
          if (cache[set][i][0] == 0) {
            filled_ways = 0;
          }
          miss = 1;
          continue;
        }
      }
      if (miss == 1) {
        load_misses += 1;
        if (filled_ways == 0) {
          for (int i = 0; i < associativity; i++) {
            if (cache[set][i][0] == 0) // way use_bit
            {
              exec_time += miss_penalty;
              cache[set][i][0] = 1;
              cache[set][i][1] = tag;
              update_used_bits(cache, i);
              cache[set][i][3] = 0;
              break;
            }
          }
        } else {
          for (int i = 0; i < associativity; i++) {
            if (cache[set][i][2] ==
                associativity - 1) // least recently used block
            {
              cache[set][i][1] = tag;
              if (cache[set][i][3] == 1) {
                dirty_evictions += 1;
                exec_time += miss_penalty + 2;
              } else {
                exec_time += miss_penalty;
              }
              update_used_bits(cache, i);
              cache[set][i][3] = 0;
              break;
            }
          }
        }
      }
    }

    if (loadstore == 1) // store instruction
    {
      for (int i = 0; i < associativity;
           i++) // looping through the specified set
      {
        if (cache[set][i][0] == 1 && tag == cache[set][i][1]) // store hit
        {
          store_hits += 1;
          miss = 0;
          update_used_bits(cache, i);
          cache[set][i][3] = 1;
          break;
        } else {
          if (cache[set][i][0] == 0) {
            filled_ways = 0;
          }
          miss = 1;
          continue;
        }
      }
      if (miss == 1) {
        store_misses += 1;
        if (filled_ways == 0) {
          for (int i = 0; i < associativity; i++) {
            if (cache[set][i][1] == 0) // way use_bit
            {
              exec_time += miss_penalty;
              cache[set][i][0] = 1;
              cache[set][i][1] = tag;
              update_used_bits(cache, i);
              cache[set][i][3] = 1;
              break;
            }
          }
        } else {
          for (int i = 0; i < associativity; i++) {
            if (cache[set][i][2] ==
                associativity - 1) // least recently used block
            {
              cache[set][i][1] = tag;
              if (cache[set][i][3] == 1) {
                dirty_evictions += 1;
                exec_time += miss_penalty + 2;
              } else {
                exec_time += miss_penalty;
              }
              update_used_bits(cache, i);
              cache[set][i][3] = 1;
              break;
            }
          }
        }
      }
    }
  }
  print_cache_configuration();
  print_parameters();
  EXIT_SUCCESS;
}
