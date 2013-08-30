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
#include <string.h>

#include "util.h"
#include "sparkey.h"

#include <stdlib.h>
#include <errno.h>

sparkey_returncode sparkey_open_returncode(int e) {
  switch (e) {
  case EPERM:
  case EACCES: return SPARKEY_PERMISSION_DENIED;
  case ENFILE: return SPARKEY_TOO_MANY_OPEN_FILES;
  case ENOENT: return SPARKEY_FILE_NOT_FOUND;
  case EOVERFLOW: return SPARKEY_FILE_TOO_LARGE;
  default:
    printf("_sparkey_open_returncode():%d error: errno = %d\n", __LINE__, e);
    return SPARKEY_INTERNAL_ERROR;
  }
}

sparkey_returncode sparkey_create_returncode(int e) {
  switch (e) {
  case EPERM:
  case EROFS:
  case EACCES: return SPARKEY_PERMISSION_DENIED;
  case EEXIST: return SPARKEY_FILE_ALREADY_EXISTS;
  case EISDIR: return SPARKEY_FILE_IS_DIRECTORY;
  case ENFILE:
  case EMFILE: return SPARKEY_TOO_MANY_OPEN_FILES;
  default:
    printf("_sparkey_create_returncode():%d error: errno = %d\n", __LINE__, e);
    return SPARKEY_INTERNAL_ERROR;
  }
}

sparkey_returncode sparkey_remove_returncode(int e) {
  switch (e) {
  case EPERM:
  case EROFS:
  case EACCES: return SPARKEY_PERMISSION_DENIED;
  case EBUSY: return SPARKEY_FILE_BUSY; // Can't happen on linux
  case EISDIR: return SPARKEY_FILE_IS_DIRECTORY;
  case EOVERFLOW: return SPARKEY_FILE_TOO_LARGE;
  default:
    printf("_sparkey_remove_returncode():%d error: errno = %d\n", __LINE__, e);
    return SPARKEY_INTERNAL_ERROR;
  }
}

char * sparkey_create_log_filename(const char *index_filename) {
  if (index_filename == NULL) return NULL;
  size_t l = strlen(index_filename);

  // Paranoia - avoid ridiculously long filenames.
  if (l > 10000) return NULL;

  // Too short to contain .spi
  if (l < 4) return NULL;

  if (memcmp(&index_filename[l - 4], ".spi", 4)) return NULL;

  char *log_filename = strdup(index_filename);
  if (log_filename == NULL) return NULL;

  log_filename[l - 1] = 'l';
  return log_filename;
}

