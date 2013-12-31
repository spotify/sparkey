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
#include <unistd.h>
#include <ctype.h>

#include "logheader.h"
#include "hashheader.h"
#include "endiantools.h"
#include "util.h"
#include "sparkey.h"

#define MINIMUM_CAPACITY (1<<8)
#define MAXIMUM_CAPACITY (1<<28)
#define SNAPPY_DEFAULT_BLOCKSIZE (1<<12)
#define SNAPPY_MAX_BLOCKSIZE (1<<30)
#define SNAPPY_MIN_BLOCKSIZE (1<<4)

void usage() {
  fprintf(stderr, "Usage: sparkey <command> <options>\n");
  fprintf(stderr, "  sparkey info [file...]\n");
  fprintf(stderr, "      Show information about files. Files can be either index or log files.\n");
  fprintf(stderr, "  sparkey get <index file> <key>\n");
  fprintf(stderr, "      Get the value for a specific key.\n");
  fprintf(stderr, "      Returns 0 on found, 1 on error and 2 on not-found.\n");
  fprintf(stderr, "  sparkey writehash <log file>\n");
  fprintf(stderr, "      Write a new index file for a log file\n");
  fprintf(stderr, "  sparkey createlog [-c <none|snappy> | -b <n>] <log file>\n");
  fprintf(stderr, "      Create a new empty log file with specified settings:\n");
  fprintf(stderr, "        -c <none|snappy>  Compression algorithm [default: none]\n");
  fprintf(stderr, "        -b <n>            Compression blocksize [default: %d]\n",
    SNAPPY_DEFAULT_BLOCKSIZE);
  fprintf(stderr, "                          [min: %d, max: %d]\n",
    SNAPPY_MIN_BLOCKSIZE, SNAPPY_MAX_BLOCKSIZE);
  fprintf(stderr, "  sparkey appendlog [-d <char>] <log file>\n");
  fprintf(stderr, "      Append data from STDIN to a log file with settings.\n");
  fprintf(stderr, "      data must be formatted as a sequence of\n");
  fprintf(stderr, "        <key> <delimiter> <value> <newline>\n");
  fprintf(stderr, "      Options:\n");
  fprintf(stderr, "        -d <char>  Delimiter char to split input records on [default: TAB]\n");
}

static void assert(sparkey_returncode rc) {
  if (rc != SPARKEY_SUCCESS) {
    fprintf(stderr, "%s\n", sparkey_errstring(rc));
    // skip cleanup - program exit will clean up implicitly.
    exit(1);
  }
}

int info(int argv, char * const *args) {
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
        fprintf(stderr, "%s is neither a sparkey log file (%s) nor an index file (%s)\n", filename, sparkey_errstring(res), sparkey_errstring(res2));
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

static size_t read_line(char **buffer, size_t *capacity, FILE *input) {
  char *buf = *buffer;
  size_t cap = *capacity, pos = 0;

  if (cap < MINIMUM_CAPACITY) {
    cap = MINIMUM_CAPACITY;
  } else if (cap > MAXIMUM_CAPACITY) {
    return pos;
  }

  while (1) {
    buf = realloc(buf, cap);
    if (buf == NULL) {
      return pos;
    }
    *buffer = buf;
    *capacity = cap;

    if (fgets(buf + pos, cap - pos, input) == NULL) {
      break;
    }

    pos += strcspn(buf + pos, "\n");
    if (buf[pos] == '\n') {
      break;
    }

    cap *= 2;
  }

  return pos;
}

static int append(sparkey_logwriter *writer, char delimiter, FILE *input) {
  char *line = NULL;
  char *key = NULL;
  char *value = NULL;
  size_t size = 0;
  sparkey_returncode returncode;
  char delim[2];
  delim[0] = delimiter;
  delim[1] = '\0';

  for (size_t end = read_line(&line, &size, input); line[end] == '\n'; end = read_line(&line, &size, input)) {
    line[end] = '\0'; // trim '\n' off the end
    // Split on the first delimiter
    key = strtok(line, delim);
    value = strtok(NULL, delim);
    if (value != NULL) {
      // Write to log
      TRY(sparkey_logwriter_put(writer, strlen(key), (uint8_t*)key, strlen(value), (uint8_t*)value), put_fail);
    } else {
      goto split_fail;
    }
  }

  free(line);
  return 0;

split_fail:
  free(line);
  fprintf(stderr, "Cannot split input line, exiting with status %d\n", 1);
  return 1;
put_fail:
  free(line);
  fprintf(stderr, "Cannot put line to log file, exiting with status %d\n", returncode);
  return returncode;
}

int main(int argv, char * const *args) {
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
      fprintf(stderr, "index filename must end with .spi\n");
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
      fprintf(stderr, "log filename must end with .spl\n");
      return 1;
    }
    int retval = writehash(index_filename, log_filename);
    free(index_filename);
    return retval;
  } else if (strcmp(args[1], "createlog") == 0) {
    opterr = 0;
    optind = 2;
    int opt_char;
    int block_size = SNAPPY_DEFAULT_BLOCKSIZE;
    sparkey_compression_type compression_type = SPARKEY_COMPRESSION_NONE;
    while ((opt_char = getopt (argv, args, "b:c:")) != -1) {
      switch (opt_char) {
      case 'b':
        if (sscanf(optarg, "%d", &block_size) != 1) {
          fprintf(stderr, "Block size must be an integer, but was '%s'\n", optarg);
          return 1;
        }
        if (block_size > SNAPPY_MAX_BLOCKSIZE || block_size < SNAPPY_MIN_BLOCKSIZE) {
          fprintf(stderr, "Block size %d, not in range. Max is %d, min is %d\n",
          block_size, SNAPPY_MAX_BLOCKSIZE, SNAPPY_MIN_BLOCKSIZE);
          return 1;
        }
        break;
      case 'c':
        if (strcmp(optarg, "none") == 0) {
          compression_type = SPARKEY_COMPRESSION_NONE;
        } else if (strcmp(optarg, "snappy") == 0) {
          compression_type = SPARKEY_COMPRESSION_SNAPPY;
        } else {
          fprintf(stderr, "Invalid compression type: '%s'\n", optarg);
          return 1;
        }
        break;
      case '?':
        if (optopt == 'b' || optopt == 'c') {
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
        } else if (isprint(optopt)) {
          fprintf(stderr, "Unknown option '-%c'.\n", optopt);
        } else {
          fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
        }
        return 1;
      default:
        fprintf(stderr, "Unknown option parsing failure\n");
        return 1;
      }
    }        
        
    if (optind >= argv) {
      fprintf(stderr, "Expected <logfile> after options\n");
      return 1;
    }

    const char *log_filename = args[optind];
    sparkey_logwriter *writer;
  assert(sparkey_logwriter_create(&writer, log_filename,
      compression_type, block_size));
    assert(sparkey_logwriter_close(&writer));
    return 0;
  } else if (strcmp(args[1], "appendlog") == 0) {
    opterr = 0;
    optind = 2;
    int opt_char;
    char delimiter = '\t';
    while ((opt_char = getopt (argv, args, "d:")) != -1) {
      switch (opt_char) {
      case 'd':
        if (strlen(optarg) != 1) {
          fprintf(stderr, "delimiter must be one character, but was '%s'\n", optarg);
          return 1;
        }
        delimiter = optarg[0];
        break;
      case '?':
        if (optopt == 'd') {
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
        } else if (isprint(optopt)) {
          fprintf(stderr, "Unknown option '-%c'.\n", optopt);
        } else {
          fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
        }
        return 1;
      default:
        fprintf(stderr, "Unknown option parsing failure\n");
        return 1;
      }
    }        
        
    if (optind >= argv) {
      fprintf(stderr, "Expected <logfile> after options\n");
      return 1;
    }

    const char *log_filename = args[optind];
    sparkey_logwriter *writer;
    assert(sparkey_logwriter_append(&writer, log_filename));
    append(writer, delimiter, stdin);
    assert(sparkey_logwriter_close(&writer));
    return 0;
  } else {
    fprintf(stderr, "Unknown command: %s\n", command);
    return 1;
  }
}
