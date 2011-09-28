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
#include <stdlib.h>
#include <inttypes.h>

#include "util.c"

void assert_equals(const char *expected, const char *actual) {
  if (expected == NULL && actual != NULL) {
    printf("Expected NULL but got '%s'\n", actual);
    exit(1);
  }
  if (expected != NULL && actual == NULL) {
    printf("Expected '%s' but got NULL\n", expected);
    exit(1);
  }
  if (expected != NULL && actual != NULL && strcmp(expected, actual)) {
    printf("Expected '%s' but got '%s'\n", expected, actual);
    exit(1);
  }
}

int main() {
  assert_equals(NULL, sparkey_create_log_filename(NULL));
  assert_equals(NULL, sparkey_create_log_filename(""));
  assert_equals(NULL, sparkey_create_log_filename("spi"));
  assert_equals(".spl", sparkey_create_log_filename(".spi"));
  assert_equals(NULL, sparkey_create_log_filename(".spx"));
  assert_equals("foo.spl", sparkey_create_log_filename("foo.spi"));

  printf("Success!\n");
}

