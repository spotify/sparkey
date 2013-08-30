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

#include "logreader.c"
#include "logwriter.c"

void assert_equals(int64_t expected, int64_t actual) {
  if (expected != actual) {
    printf("Expected %"PRIu64" but got %"PRIu64"\n", expected, actual);
    exit(1);
  }
}

int main() {
  uint8_t buf[20];
  for (int64_t i = 0; i < 60; i++) {
    for (int64_t j = -10; j < 10; j++) {
      int64_t val = (1 << i) + j;
      if (val >= 0) {
        uint64_t written = write_vlq(buf, val);
        uint64_t pos = 0;
        int64_t val2 = read_vlq(buf, &pos);
        assert_equals(written, pos);
        assert_equals(val, val2);
      }
    }
  }

  assert_equals(1, write_vlq(buf, 0));
  assert_equals(1, write_vlq(buf, 127));
  assert_equals(2, write_vlq(buf, 128));
  assert_equals(2, write_vlq(buf, 16383));
  assert_equals(3, write_vlq(buf, 16384));
  assert_equals(3, write_vlq(buf, 2097151));
  assert_equals(4, write_vlq(buf, 2097152));
  printf("Success!\n");
}

