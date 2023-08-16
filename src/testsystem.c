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
#include <inttypes.h>

#include "sparkey.h"

static int max(int a, int b) {
  return a > b ? a : b;
}

static void _assert_equals(const char *file, int line, int expected, int actual) {
  if (expected != actual) {
    printf("%s:%d: Expected %d but got %d\n", file, line, expected, actual);
    exit(1);
  }
}

#define assert_equals(expected, actual) _assert_equals(__FILE__, __LINE__, expected, actual)

static void _assert_str_equals(const char *file, int line, const char *expected, const char *actual) {
  if (expected == NULL && actual != NULL) {
    printf("%s:%d: Expected NULL but got '%s'\n", file, line, actual);
    exit(1);
  }
  if (expected != NULL && actual == NULL) {
    printf("%s:%d: Expected '%s' but got NULL\n", file, line, expected);
    exit(1);
  }
  if (expected != NULL && actual != NULL && strcmp(expected, actual)) {
    printf("%s:%d: Expected '%s' but got '%s'\n", file, line, expected, actual);
    exit(1);
  }
}

#define assert_str_equals(expected, actual) _assert_str_equals(__FILE__, __LINE__, expected, actual)

void verify(sparkey_compression_type compression, int blocksize, int hashsize, int num_puts, int num_deletes, int num_puts2) {
  int expected_puts = max(0, num_puts - max(num_deletes, num_puts2));
  int expected_total = expected_puts + num_puts2;

  // write some data to the log
  sparkey_logwriter *mywriter;
  assert_equals(SPARKEY_SUCCESS, sparkey_logwriter_create(&mywriter, "test.spl", compression, blocksize));

  for (int i = 0; i < num_puts; i++) {
    char key[100];
    char value[100];
    sprintf(key, "key_%d", i);
    sprintf(value, "value_%d", i);
    assert_equals(SPARKEY_SUCCESS, sparkey_logwriter_put(mywriter, strlen(key), (uint8_t*) key, strlen(value), (uint8_t*) value));
  }

  for (int i = 0; i < num_deletes; i++) {
    char key[100];
    sprintf(key, "key_%d", i);
    assert_equals(SPARKEY_SUCCESS, sparkey_logwriter_delete(mywriter, strlen(key), (uint8_t*) key));
  }

  for (int i = 0; i < num_puts2; i++) {
    char key[100];
    char value[100];
    sprintf(key, "key_%d", i);
    sprintf(value, "newvalue_%d", i);
    assert_equals(SPARKEY_SUCCESS, sparkey_logwriter_put(mywriter, strlen(key), (uint8_t*) key, strlen(value), (uint8_t*) value));
  }

  assert_equals(SPARKEY_SUCCESS, sparkey_logwriter_close(&mywriter));

  // verify correct log iteration
  sparkey_logreader *myreader;
  assert_equals(SPARKEY_SUCCESS, sparkey_logreader_open(&myreader, "test.spl"));
  sparkey_logiter *myiter;
  assert_equals(SPARKEY_SUCCESS, sparkey_logiter_create(&myiter, myreader));

  int visited = 0;
  while (1) {
    assert_equals(SPARKEY_SUCCESS, sparkey_logiter_next(myiter, myreader));
    if (sparkey_logiter_state(myiter) != SPARKEY_ITER_ACTIVE) {
      break;
    }
    visited++;
    uint64_t wanted_keylen = sparkey_logiter_keylen(myiter);

    // one extra byte to account for the extra \0 at the end, as we're going to compare it as a string.
    // By using calloc we also ensure that it initializes to 0 directly.
    uint8_t *keybuf = calloc(1 + wanted_keylen, 1);

    uint64_t actual_keylen;
    assert_equals(SPARKEY_SUCCESS, sparkey_logiter_fill_key(myiter, myreader, wanted_keylen, keybuf, &actual_keylen));
    assert_equals(wanted_keylen, actual_keylen);

    uint64_t wanted_valuelen = sparkey_logiter_valuelen(myiter);
    uint8_t *valuebuf = calloc(1 + wanted_valuelen, 1);
    uint64_t actual_valuelen;
    assert_equals(SPARKEY_SUCCESS, sparkey_logiter_fill_value(myiter, myreader, wanted_valuelen, valuebuf, &actual_valuelen));
    assert_equals(wanted_valuelen, actual_valuelen);

    sparkey_entry_type expected_type;
    int expected_id;
    const char *expected_value_prefix;
    if (visited <= num_puts) {
      expected_type = SPARKEY_ENTRY_PUT;
      expected_id = visited - 1;
      expected_value_prefix = "value";
    } else if (visited <= num_puts + num_deletes) {
      expected_type = SPARKEY_ENTRY_DELETE;
      expected_id = visited - num_puts - 1;
      expected_value_prefix = "UNUSED";
    } else {
      expected_type = SPARKEY_ENTRY_PUT;
      expected_id = visited - num_puts - num_deletes - 1;
      expected_value_prefix = "newvalue";
    }
    assert_equals(expected_type, sparkey_logiter_type(myiter));
    char expected_key[100];
    char expected_value[100];
    sprintf(expected_key, "key_%d", expected_id);
    sprintf(expected_value, "%s_%d", expected_value_prefix, expected_id);

    assert_str_equals(expected_key, (char*) keybuf);
    if (expected_type == SPARKEY_ENTRY_PUT) {
      assert_str_equals(expected_value, (char*) valuebuf);
    }

    free(keybuf);
    free(valuebuf);
  }
  assert_equals(num_puts + num_deletes + num_puts2, visited);
  sparkey_logreader_close(&myreader);
  sparkey_logiter_close(&myiter);

  // create the hash
  assert_equals(SPARKEY_SUCCESS, sparkey_hash_write("test.spi", "test.spl", hashsize));

  // verify hash iteration
  sparkey_hashreader *myhashreader;
  assert_equals(SPARKEY_SUCCESS, sparkey_hash_open(&myhashreader, "test.spi", "test.spl"));
  myreader = sparkey_hash_getreader(myhashreader);
  assert_equals(SPARKEY_SUCCESS, sparkey_logiter_create(&myiter, myreader));

  visited = 0;
  while (1) {
    assert_equals(SPARKEY_SUCCESS, sparkey_logiter_hashnext(myiter, myhashreader));
    if (sparkey_logiter_state(myiter) != SPARKEY_ITER_ACTIVE) {
      break;
    }
    visited++;
    uint64_t wanted_keylen = sparkey_logiter_keylen(myiter);
    uint8_t *keybuf = calloc(1 + wanted_keylen, 1);
    uint64_t actual_keylen;
    assert_equals(SPARKEY_SUCCESS, sparkey_logiter_fill_key(myiter, myreader, wanted_keylen, keybuf, &actual_keylen));
    assert_equals(wanted_keylen, actual_keylen);

    uint64_t wanted_valuelen = sparkey_logiter_valuelen(myiter);
    uint8_t *valuebuf = calloc(1 + wanted_valuelen, 1);
    uint64_t actual_valuelen;
    assert_equals(SPARKEY_SUCCESS, sparkey_logiter_fill_value(myiter, myreader, wanted_valuelen, valuebuf, &actual_valuelen));
    assert_equals(wanted_valuelen, actual_valuelen);

    assert_equals(SPARKEY_ENTRY_PUT, sparkey_logiter_type(myiter));

    int expected_id;
    const char *expected_value_prefix;
    if (visited <= expected_puts) {
      expected_id = max(num_deletes, num_puts2) + visited - 1;
      expected_value_prefix = "value";
    } else {
      expected_id = visited - expected_puts - 1;
      expected_value_prefix = "newvalue";

    }
    char expected_key[100];
    char expected_value[100];
    sprintf(expected_key, "key_%d", expected_id);
    sprintf(expected_value, "%s_%d", expected_value_prefix, expected_id);

    assert_str_equals(expected_key, (char*) keybuf);
    assert_str_equals(expected_value, (char*) valuebuf);

    free(keybuf);
    free(valuebuf);
  }
  assert_equals(expected_total, visited);

  // verify random access
  for (int i = 0; i < max(num_puts, num_puts2) + 100; i++) {
    char key[100];
    char expected_value[100];
    sprintf(key, "key_%d", i);
    assert_equals(SPARKEY_SUCCESS, sparkey_hash_get(myhashreader, (uint8_t*) key, strlen(key), myiter));
    if (i < num_puts2) {
      assert_equals(SPARKEY_ITER_ACTIVE, sparkey_logiter_state(myiter));
      sprintf(expected_value, "newvalue_%d", i);
    } else if (i >= num_deletes && i < num_puts) {
      assert_equals(SPARKEY_ITER_ACTIVE, sparkey_logiter_state(myiter));
      sprintf(expected_value, "value_%d", i);
    } else {
      assert_equals(SPARKEY_ITER_INVALID, sparkey_logiter_state(myiter));
    }

    if (sparkey_logiter_state(myiter) == SPARKEY_ITER_ACTIVE) {
      uint64_t wanted_valuelen = sparkey_logiter_valuelen(myiter);
      uint8_t *valuebuf = calloc(1 + wanted_valuelen, 1);
      uint64_t actual_valuelen;
      assert_equals(SPARKEY_SUCCESS, sparkey_logiter_fill_value(myiter, myreader, wanted_valuelen, valuebuf, &actual_valuelen));
      assert_equals(wanted_valuelen, actual_valuelen);

      assert_str_equals(expected_value, (char*) valuebuf);
      free(valuebuf);
    }
  }
  sparkey_hash_close(&myhashreader);
  sparkey_logiter_close(&myiter);
}

void verify_files_closed() {
  // Verify that SPARKEY_FILE_IDENTIFIER_MISMATCH is returned appropriately.
  sparkey_logwriter *writer;
  assert_equals(SPARKEY_SUCCESS, sparkey_logwriter_create(&writer, "test1.spl",
    SPARKEY_COMPRESSION_NONE, 4096));
  assert_equals(SPARKEY_SUCCESS, sparkey_logwriter_close(&writer));
  assert_equals(1, writer == NULL);

  assert_equals(SPARKEY_SUCCESS, sparkey_logwriter_create(&writer, "test2.spl",
    SPARKEY_COMPRESSION_NONE, 4096));
  assert_equals(SPARKEY_SUCCESS, sparkey_logwriter_close(&writer));
  assert_equals(1, writer == NULL);

  // Now create a hash for test1.
  assert_equals(SPARKEY_SUCCESS, sparkey_hash_write("test1.spi", "test1.spl", 0));

  // and try to open the wrong files:
  sparkey_hashreader* reader;
  sparkey_returncode rc = sparkey_hash_open(
    &reader,
    "test1.spi",
    "test2.spl"
  );

  assert_equals(SPARKEY_FILE_IDENTIFIER_MISMATCH, rc);
}

int main() {
  verify(SPARKEY_COMPRESSION_NONE, 0, 0, 0, 0, 0);
  verify(SPARKEY_COMPRESSION_NONE, 0, 0, 1, 0, 0);
  verify(SPARKEY_COMPRESSION_NONE, 0, 0, 100, 0, 0);
  verify(SPARKEY_COMPRESSION_NONE, 0, 0, 0, 100, 0);
  verify(SPARKEY_COMPRESSION_NONE, 0, 0, 0, 0, 100);
  verify(SPARKEY_COMPRESSION_NONE, 0, 0, 100, 10, 5);

  for (sparkey_compression_type t = SPARKEY_COMPRESSION_SNAPPY; t <= SPARKEY_COMPRESSION_ZSTD; t++) {
    verify(t, 10, 0, 100, 0, 0);
    verify(t, 20, 0, 100, 0, 0);
    verify(t, 100, 0, 100, 0, 0);
    verify(t, 100, 0, 1000, 0, 0);
    verify(t, 1000, 0, 1000, 0, 0);

    verify(t, 100, 0, 1000, 100, 0);
    verify(t, 100, 0, 1000, 100, 50);

    verify(t, 100, 4, 1000, 0, 0);
    verify(t, 100, 8, 1000, 0, 0);
  }

  verify_files_closed();

  printf("Success!\n");
}

