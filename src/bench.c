/*
* Copyright (c) 2013 Spotify AB
*
* Licensed under the Apache License, Version 2.0 (the "License"); you may not
* use this file except in compliance with the License. You may obtain a copy of
* the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
* License for the specific language governing permissions and limitations under
* the License.
*/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

static void _errno_assert(const char *file, int line, int i) {
  if (i != 0) {
    printf("%s:%d: assertion failed: %s\n", file, line, strerror(errno));
    exit(i);
  }
}

#define errno_assert(i) _errno_assert(__FILE__, __LINE__, i)

static void rm_rec(const char *dir) {
  DIR *tmpd = opendir(dir);
  if (tmpd != NULL) {
    struct dirent *d;
    while ((d = readdir(tmpd))) {
      char subdir[100];
      sprintf(subdir, "%s/%s", dir, d->d_name);
      if(strcmp(d->d_name, ".") &&
         strcmp(d->d_name, "..")) {
        if (d->d_type == DT_DIR) {
          rm_rec(subdir);
        } else if (d->d_type == DT_REG) {
          remove(subdir);
        }
      }
    }
    closedir(tmpd);
  }
  remove(dir);
}

static void rm_all_rec(const char** files) {
  int i = 0;
  while (1) {
    const char *filename = files[i];
    if (filename == NULL) {
      return;
    }
    rm_rec(filename);
    i++;
  }
}


static size_t file_size_rec(const char *dir) {
  struct stat buf;
  errno_assert(stat(dir, &buf));
  if (S_ISREG(buf.st_mode)) {
    return buf.st_size;
  } else if (S_ISDIR(buf.st_mode)) {
    size_t sum = 0;
    DIR *tmpd = opendir(dir);
    if (tmpd != NULL) {
      struct dirent *d;
      while ((d = readdir(tmpd))) {
        char subdir[100];
        sprintf(subdir, "%s/%s", dir, d->d_name);
        if(strcmp(d->d_name, ".") &&
           strcmp(d->d_name, "..")) {
          sum += file_size_rec(subdir);
        }
      }
      closedir(tmpd);
    }
    return sum;
  } else {
    return 0;
  }
}

static size_t total_file_size(const char** files) {
  size_t sum = 0;
  int i = 0;
  while (1) {
    const char *filename = files[i];
    if (filename == NULL) {
      return sum;
    }
    sum += file_size_rec(filename);
    i++;
  }
}

#ifdef __APPLE__
#include <mach/mach_time.h>
static float wall() {
  static double multiplier = 0;
  if (multiplier <= 0) {
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    multiplier = (double) info.numer / (double) info.denom / 1000000000.0;
  }
  return (float) (multiplier * mach_absolute_time());
}
static float cpu() {
  return wall();
}

#else

#include <time.h>
#ifdef CLOCK_MONOTONIC_RAW
#define CLOCK_SUITABLE CLOCK_MONOTONIC_RAW
#else
#define CLOCK_SUITABLE CLOCK_MONOTONIC
#endif
static float wall() {
  struct timespec tp;
  clock_gettime(CLOCK_SUITABLE, &tp);
  return tp.tv_sec + 1e-9 * tp.tv_nsec;
}

static float cpu() {
  struct timespec tp;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp);
  return tp.tv_sec + 1e-9 * tp.tv_nsec;
}
#endif

typedef struct {
  char *name;
  void (*create)(int n);
  void (*randomaccess)(int n, int lookups);
  const char** (*files)();
} candidate;

/* Sparkey stuff */

#include "sparkey.h"

static void _sparkey_assert(const char *file, int line, sparkey_returncode i) {
  if (i != SPARKEY_SUCCESS) {
    printf("%s:%d: assertion failed: %s\n", file, line, sparkey_errstring(i));
    exit(i);
  }
}

#define sparkey_assert(i) _sparkey_assert(__FILE__, __LINE__, i)


static void sparkey_create(int n, sparkey_compression_type compression_type, int block_size) {
  sparkey_logwriter *mywriter;
  sparkey_assert(sparkey_logwriter_create(&mywriter, "test.spl", compression_type, block_size));
  for (int i = 0; i < n; i++) {
    char mykey[100];
    char myvalue[100];
    sprintf(mykey, "key_%d", i);
    sprintf(myvalue, "value_%d", i);
    sparkey_assert(sparkey_logwriter_put(mywriter, strlen(mykey), (uint8_t*)mykey, strlen(myvalue), (uint8_t*)myvalue));
  }
  sparkey_assert(sparkey_logwriter_close(&mywriter));
  sparkey_assert(sparkey_hash_write("test.spi", "test.spl", 0));
}

static void sparkey_randomaccess(int n, int lookups) {
  sparkey_hashreader *myreader;
  sparkey_logiter *myiter;
  sparkey_assert(sparkey_hash_open(&myreader, "test.spi", "test.spl"));
  sparkey_logreader *logreader = sparkey_hash_getreader(myreader);
  sparkey_assert(sparkey_logiter_create(&myiter, logreader));

  uint8_t *valuebuf = malloc(sparkey_logreader_maxvaluelen(logreader));

  for (int i = 0; i < lookups; i++) {
    char mykey[100];
    char myvalue[100];
    int r = rand() % n;
    sprintf(mykey, "key_%d", r);
    sprintf(myvalue, "value_%d", r);
    sparkey_assert(sparkey_hash_get(myreader, (uint8_t*)mykey, strlen(mykey), myiter));
    if (sparkey_logiter_state(myiter) != SPARKEY_ITER_ACTIVE) {
      printf("Failed to lookup key: %s\n", mykey);
      exit(1);
    }

    uint64_t wanted_valuelen = sparkey_logiter_valuelen(myiter);
    uint64_t actual_valuelen;
    sparkey_assert(sparkey_logiter_fill_value(myiter, logreader, wanted_valuelen, valuebuf, &actual_valuelen));
    if (actual_valuelen != strlen(myvalue) || memcmp(myvalue, valuebuf, actual_valuelen)) {
      printf("Did not get the expected value for key: %s\n", mykey);
      exit(1);
    }
  }
  sparkey_logiter_close(&myiter);
  sparkey_hash_close(&myreader);
}

static void sparkey_create_uncompressed(int n) {
  sparkey_create(n, SPARKEY_COMPRESSION_NONE, 0);
}

static void sparkey_create_compressed(int n) {
  sparkey_create(n, SPARKEY_COMPRESSION_SNAPPY, 1024);
}

static const char* sparkey_list[] = {"test.spi", "test.spl", NULL};

static const char** sparkey_files() {
  return sparkey_list;
}

static candidate sparkey_candidate_uncompressed = {
  "Sparkey uncompressed", &sparkey_create_uncompressed, &sparkey_randomaccess, &sparkey_files
};

static candidate sparkey_candidate_compressed = {
  "Sparkey compressed(1024)", &sparkey_create_compressed, &sparkey_randomaccess, &sparkey_files
};

/* main */

void test(candidate *c, int n, int lookups) {
  printf("Testing bulk insert of %d elements and %d random lookups\n", n, lookups);

  printf("  Candidate: %s\n", c->name);
  rm_all_rec(c->files());

  float t1_wall = wall();
  float t1_cpu = cpu();

  c->create(n);

  float t2_wall = wall();
  float t2_cpu = cpu();
  printf("    creation time (wall):     %2.2f\n", t2_wall - t1_wall);
  printf("    creation time (cpu):      %2.2f\n", t2_cpu - t1_cpu);
  printf("    throughput (puts/cpusec): %2.2f\n", (float) n / (t2_cpu - t1_cpu));
  printf("    file size:                %ld\n", total_file_size(c->files()));

  c->randomaccess(n, lookups);

  float t3_wall = wall();
  float t3_cpu = cpu();
  printf("    lookup time (wall):          %2.2f\n", t3_wall - t2_wall);
  printf("    lookup time (cpu):           %2.2f\n", t3_cpu - t2_cpu);
  printf("    throughput (lookups/cpusec): %2.2f\n", (float) lookups / (t3_cpu - t2_cpu));
  rm_all_rec(c->files());

  printf("\n");
}

int main() {
  test(&sparkey_candidate_uncompressed, 1000, 1*1000*1000);
  test(&sparkey_candidate_uncompressed, 1000*1000, 1*1000*1000);
  test(&sparkey_candidate_uncompressed, 10*1000*1000, 1*1000*1000);
  test(&sparkey_candidate_uncompressed, 100*1000*1000, 1*1000*1000);

  test(&sparkey_candidate_compressed, 1000, 1*1000*1000);
  test(&sparkey_candidate_compressed, 1000*1000, 1*1000*1000);
  test(&sparkey_candidate_compressed, 10*1000*1000, 1*1000*1000);
  test(&sparkey_candidate_compressed, 100*1000*1000, 1*1000*1000);

  return 0;
}


