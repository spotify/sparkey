/*
* Copyright (c) 2012-2014 Spotify AB
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

static void usage() {
  fprintf(stderr, "Usage: sparkey <command> [<args>]\n");
  fprintf(stderr, "Commands:\n");
  fprintf(stderr, "  info      - Show information about sparkey files.\n");
  fprintf(stderr, "  get       - Get the value associated with a key.\n");
  fprintf(stderr, "  writehash - Generate a hash file from a log file.\n");
  fprintf(stderr, "  createlog - Create an empty log file.\n");
  fprintf(stderr, "  appendlog - Append key-value pairs to an existing log file.\n");
  fprintf(stderr, "  rewrite   - Rewrite an existing log/index file pair, "
                                "trimming away all replaced entries and "
                                "possibly changing the compression format.\n");
  fprintf(stderr, "  help      - Show this help text.\n");
}

static void usage_info() {
  fprintf(stderr, "Usage: sparkey info file1 [file2, ...]\n");
  fprintf(stderr, "  Show information about files. Files can be either index or log files.\n");
}

static void usage_get() {
  fprintf(stderr, "Usage: sparkey get <index file> <key>\n");
  fprintf(stderr, "  Get the value for a specific key.\n");
  fprintf(stderr, "  Returns 0 if found,\n");
  fprintf(stderr, "          1 on error,\n");
  fprintf(stderr, "          2 on not-found.\n");
}

static void usage_writehash() {
  fprintf(stderr, "Usage: sparkey writehash <file.spl>\n");
  fprintf(stderr, "  Write a new index file for a log file.\n");
  fprintf(stderr, "  Creates and possibly overwrites a new file with file ending .spi\n");
}

static void usage_createlog() {
  fprintf(stderr, "Usage: sparkey createlog [-c <none|snappy> | -b <n>] <file.spl>\n");
  fprintf(stderr, "  Create a new empty log file.\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -c <none|snappy>  Compression algorithm [default: none]\n");
  fprintf(stderr, "  -b <n>            Compression blocksize [default: %d]\n",
    SNAPPY_DEFAULT_BLOCKSIZE);
  fprintf(stderr, "                    [min: %d, max: %d]\n",
    SNAPPY_MIN_BLOCKSIZE, SNAPPY_MAX_BLOCKSIZE);
}

static void usage_appendlog() {
  fprintf(stderr, "Usage: sparkey appendlog [-d <char>] <file.spl>\n");
  fprintf(stderr, "  Append data from STDIN to a log file with settings.\n");
  fprintf(stderr, "  data must be formatted as a sequence of\n");
  fprintf(stderr, "  <key> <delimiter> <value> <newline>\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -d <char>  Delimiter char to split input records on [default: TAB]\n");
}

static void usage_rewrite() {
  fprintf(stderr, "Usage: sparkey rewrite [-c <none|snappy> | -b <n>] <input.spi> <output.spi>\n");
  fprintf(stderr, "  Iterate over all entries in <file.spi> and create a new index and log pair\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -c <none|snappy>  Compression algorithm [default: same as before]\n");
  fprintf(stderr, "  -b <n>            Compression blocksize [default: same as before]\n");
  fprintf(stderr, "                    [min: %d, max: %d]\n",
    SNAPPY_MIN_BLOCKSIZE, SNAPPY_MAX_BLOCKSIZE);
}

static void assert(sparkey_returncode rc) {
  if (rc != SPARKEY_SUCCESS) {
    fprintf(stderr, "%s\n", sparkey_errstring(rc));
    // skip cleanup - program exit will clean up implicitly.
    exit(1);
  }
}

static int info_file(const char *filename) {
  sparkey_logheader logheader;
  sparkey_hashheader hashheader;
  sparkey_returncode res = sparkey_load_logheader(&logheader, filename);
  if (res == SPARKEY_SUCCESS) {
    printf("Filename: %s\n", filename);
    print_logheader(&logheader);
    printf("\n");
    return 0;
  }

  if (res != SPARKEY_WRONG_LOG_MAGIC_NUMBER) {
    fprintf(stderr, "%s: %s\n", filename, sparkey_errstring(res));
    return 1;
  }

  res = sparkey_load_hashheader(&hashheader, filename);
  if (res == SPARKEY_SUCCESS) {
    printf("Filename: %s\n", filename);
    print_hashheader(&hashheader);
    printf("\n");
    return 0;
  }

  if (res != SPARKEY_WRONG_HASH_MAGIC_NUMBER) {
    fprintf(stderr, "%s: %s\n", filename, sparkey_errstring(res));
    return 1;
  }

  fprintf(stderr, "%s: Not a sparkey file.\n", filename);
  return 1;
}

int info(int argc, char * const *argv) {
  int retval = 0;
  for (int i = 0; i < argc; i++) {
    retval |= info_file(argv[i]);
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
    // Split on the first delimiter
    key = strtok(line, delim);
    value = strtok(NULL, "\n");
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
  fprintf(stderr, "Cannot split input line, aborting early.\n");
  return 1;
put_fail:
  free(line);
  fprintf(stderr, "Cannot append line to log file, aborting early: %s\n", sparkey_errstring(returncode));
  return 1;
}

int main(int argc, char * const *argv) {
  if (argc < 2) {
    usage();
    return 0;
  }
  const char *command = argv[1];
  if (strcmp(command, "info") == 0) {
    if (argc < 3) {
      usage_info();
      return 1;
    }
    return info(argc - 2, argv + 2);
  } else if (strcmp(command, "get") == 0) {
    if (argc < 4) {
      usage_get();
      return 1;
    }
    const char *index_filename = argv[2];
    char *log_filename = sparkey_create_log_filename(index_filename);
    if (log_filename == NULL) {
      fprintf(stderr, "index filename must end with .spi\n");
      return 1;
    }
    int retval = get(argv[2], log_filename, argv[3]);
    free(log_filename);
    return retval;
  } else if (strcmp(command, "writehash") == 0) {
    if (argc < 3) {
      usage_writehash();
      return 1;
    }
    const char *log_filename = argv[2];
    char *index_filename = sparkey_create_index_filename(log_filename);
    if (index_filename == NULL) {
      fprintf(stderr, "log filename must end with .spl\n");
      return 1;
    }
    int retval = writehash(index_filename, log_filename);
    free(index_filename);
    return retval;
  } else if (strcmp(command, "createlog") == 0) {
    opterr = 0;
    optind = 2;
    int opt_char;
    int block_size = SNAPPY_DEFAULT_BLOCKSIZE;
    sparkey_compression_type compression_type = SPARKEY_COMPRESSION_NONE;
    while ((opt_char = getopt (argc, argv, "b:c:")) != -1) {
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

    if (optind >= argc) {
      usage_createlog();
      return 1;
    }

    const char *log_filename = argv[optind];
    sparkey_logwriter *writer;
    assert(sparkey_logwriter_create(&writer, log_filename,
      compression_type, block_size));
    assert(sparkey_logwriter_close(&writer));
    return 0;
  } else if (strcmp(command, "appendlog") == 0) {
    opterr = 0;
    optind = 2;
    int opt_char;
    char delimiter = '\t';
    while ((opt_char = getopt (argc, argv, "d:")) != -1) {
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

    if (optind >= argc) {
      usage_appendlog();
      return 1;
    }

    const char *log_filename = argv[optind];
    sparkey_logwriter *writer;
    assert(sparkey_logwriter_append(&writer, log_filename));
    int rc = append(writer, delimiter, stdin);
    assert(sparkey_logwriter_close(&writer));
    return rc;
  } else if (strcmp(command, "rewrite") == 0) {
    opterr = 0;
    optind = 2;
    int opt_char;
    int block_size = -1;
    sparkey_compression_type compression_type = SPARKEY_COMPRESSION_NONE;
    int compression_set = 0;
    while ((opt_char = getopt (argc, argv, "b:c:")) != -1) {
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
        compression_set = 1;
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

    if (optind + 1 >= argc) {
      usage_rewrite();
      return 1;
    }

    const char *input_index_filename = argv[optind];
    const char *output_index_filename = argv[optind + 1];

    if (strcmp(input_index_filename, output_index_filename) == 0) {
      fprintf(stderr, "input and output must be different.\n");
      return 1;
    }

    char *input_log_filename = sparkey_create_log_filename(input_index_filename);
    if (input_log_filename == NULL) {
      fprintf(stderr, "input filename must end with .spi but was '%s'\n", input_index_filename);
      return 1;
    }

    char *output_log_filename = sparkey_create_log_filename(output_index_filename);
    if (output_log_filename == NULL) {
      fprintf(stderr, "output filename must end with .spi but was '%s'\n", output_index_filename);
      return 1;
    }

    sparkey_hashreader *reader;
    assert(sparkey_hash_open(&reader, input_index_filename, input_log_filename));
    sparkey_logreader *logreader = sparkey_hash_getreader(reader);
    if (!compression_set) {
      compression_type = sparkey_logreader_get_compression_type(logreader);
    }
    if (block_size == -1) {
      block_size = sparkey_logreader_get_compression_blocksize(logreader);
    }

    // TODO: skip rewrite if compression type and block size are unchanged, and there is no garbage in the log

    sparkey_logwriter *writer;
    assert(sparkey_logwriter_create(&writer, output_log_filename, compression_type, block_size));

    sparkey_logiter *iter;
    assert(sparkey_logiter_create(&iter, logreader));

    uint8_t *keybuf = malloc(sparkey_logreader_maxkeylen(logreader));
    uint8_t *valuebuf = malloc(sparkey_logreader_maxvaluelen(logreader));

    while (1) {
      assert(sparkey_logiter_next(iter, logreader));
      if (sparkey_logiter_state(iter) != SPARKEY_ITER_ACTIVE) {
        break;
      }
      uint64_t wanted_keylen = sparkey_logiter_keylen(iter);
      uint64_t actual_keylen;
      assert(sparkey_logiter_fill_key(iter, logreader, wanted_keylen, keybuf, &actual_keylen));
      uint64_t wanted_valuelen = sparkey_logiter_valuelen(iter);
      uint64_t actual_valuelen;
      assert(sparkey_logiter_fill_value(iter, logreader, wanted_valuelen, valuebuf, &actual_valuelen));

      assert(sparkey_logwriter_put(writer, wanted_keylen, keybuf, wanted_valuelen, valuebuf));
    }
    free(keybuf);
    free(valuebuf);
    sparkey_logiter_close(&iter);
    assert(sparkey_logwriter_close(&writer));
    sparkey_hash_close(&reader);

    writehash(output_index_filename, output_log_filename);

    return 0;
  } else if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
    usage();
    return 0;
  } else {
    fprintf(stderr, "Unknown command: %s\n", command);
    return 1;
  }
}
