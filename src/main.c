/*
* Copyright (c) 2012-2013 Spotify AB
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "logheader.h"
#include "hashheader.h"
#include "endiantools.h"
#include "sparkey.h"

void usage() {
	printf("Usage: sparkey <command> <options>\n");
	printf("Commands: info [file...]\n");
	printf("Commands: get <index file> <key>\n");
	printf("Commands: writehash <log file>\n");
}

static void assert(sparkey_returncode rc) {
  if (rc != SPARKEY_SUCCESS) {
    fprintf(stderr, "%s\n", sparkey_errstring(rc));
    // skip cleanup - program exit will clean up implicitly.
    exit(1);
  }
}

int info(int argv, const char **args) {
  int retval = 0;
  sparkey_logheader logheader;
  sparkey_hashheader hashheader;
  for (int i = 0; i < argv; i++) {
    const char *filename = args[i];
    sparkey_returncode res = sparkey_load_logheader(&logheader, filename);
    if (res == SPARKEY_SUCCESS) {
      printf("%s\n", filename);
      print_logheader(&logheader);
    } else {
      sparkey_returncode res2 = sparkey_load_hashheader(&hashheader, filename);
      if (res2 == SPARKEY_SUCCESS) {
        printf("%s\n", args[i]);
        print_hashheader(&hashheader);
      } else {
        printf("%s is neither a sparkey log file (%s) nor an index file (%s)\n", filename, sparkey_errstring(res), sparkey_errstring(res2));
        retval = 1;
      }
    }
  }
  return retval;
}

int get(const char *hashfile, const char *logfile, const char *key) {
  sparkey_hashreader *reader;
  sparkey_logreader *logreader;
  sparkey_logiter *iter;
  assert(sparkey_hash_open(&reader, hashfile, logfile));
  logreader = sparkey_hash_getreader(reader);
  assert(sparkey_logiter_create(&iter, logreader));

  uint64_t keylen = strlen(key);
  assert(sparkey_hash_get(reader, (uint8_t*) key, keylen, iter));

  int exitcode = 2;
  if (sparkey_logiter_state(iter) == SPARKEY_ITER_ACTIVE) {
    exitcode = 0;
    uint8_t * res;
    uint64_t len;
    do {
      assert(sparkey_logiter_valuechunk(iter, logreader, 1 << 31, &res, &len));
      assert(write_full(STDOUT_FILENO, res, len));
    } while (len > 0);
  }
  sparkey_logiter_close(&iter);
  sparkey_hash_close(&reader);
  return exitcode;
}

int writehash(const char *indexfile, const char *logfile) {
  assert(sparkey_hash_write(indexfile, logfile, 0));
  return 0;
}

int main(int argv, const char **args) {
  if (argv < 2) {
    usage();
    return 1;
  }
  const char *command = args[1];
  if (strcmp(command, "info") == 0) {
    if (argv < 3) {
      usage();
      return 1;
    }
    return info(argv - 2, args + 2);
  } else if (strcmp(command, "get") == 0) {
    if (argv < 4) {
      usage();
      return 1;
    }
    const char *index_filename = args[2];
    char *log_filename = sparkey_create_log_filename(index_filename);
    if (log_filename == NULL) {
      printf("index filename must end with .spi\n");
      return 1;
    }
    int retval = get(args[2], log_filename, args[3]);
    free(log_filename);
    return retval;
  } else if (strcmp(command, "writehash") == 0) {
    if (argv < 3) {
      usage();
      return 1;
    }
    const char *log_filename = args[2];
    char *index_filename = sparkey_create_index_filename(log_filename);
    if (index_filename == NULL) {
      printf("log filename must end with .spl\n");
      return 1;
    }
    int retval = writehash(index_filename, log_filename);
    free(index_filename);
    return retval;
  } else {
    printf("Unknown command: %s\n", command);
    usage();
    return 1;
  }
}
