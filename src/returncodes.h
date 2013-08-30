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
#ifndef SPARKEY_RETURNCODES_H
#define SPARKEY_RETURNCODES_H

typedef enum {
  SPARKEY_SUCCESS = 0,
  SPARKEY_INTERNAL_ERROR = -1,

  SPARKEY_FILE_NOT_FOUND = -100,
  SPARKEY_PERMISSION_DENIED = -101,
  SPARKEY_TOO_MANY_OPEN_FILES = -102,
  SPARKEY_FILE_TOO_LARGE = -103,
  SPARKEY_FILE_ALREADY_EXISTS = -104,
  SPARKEY_FILE_BUSY = -105,
  SPARKEY_FILE_IS_DIRECTORY = -106,
  SPARKEY_FILE_SIZE_EXCEEDED = -107,
  SPARKEY_FILE_CLOSED = -108,
  SPARKEY_OUT_OF_DISK = -109,
  SPARKEY_UNEXPECTED_EOF = -110,
  SPARKEY_MMAP_FAILED = -111,

  SPARKEY_WRONG_LOG_MAGIC_NUMBER = -200,
  SPARKEY_WRONG_LOG_MAJOR_VERSION = -201,
  SPARKEY_UNSUPPORTED_LOG_MINOR_VERSION = -202,
  SPARKEY_LOG_TOO_SMALL = -203,
  SPARKEY_LOG_CLOSED = -204,
  SPARKEY_LOG_ITERATOR_INACTIVE = -205,
  SPARKEY_LOG_ITERATOR_MISMATCH = -206,
  SPARKEY_LOG_ITERATOR_CLOSED = -207,
  SPARKEY_LOG_HEADER_CORRUPT = -208,
  SPARKEY_INVALID_COMPRESSION_BLOCK_SIZE = -209,
  SPARKEY_INVALID_COMPRESSION_TYPE = -210,

  SPARKEY_WRONG_HASH_MAGIC_NUMBER = -300,
  SPARKEY_WRONG_HASH_MAJOR_VERSION = -301,
  SPARKEY_UNSUPPORTED_HASH_MINOR_VERSION = -302,
  SPARKEY_HASH_TOO_SMALL = -303,
  SPARKEY_HASH_CLOSED = -304,
  SPARKEY_FILE_IDENTIFIER_MISMATCH = -305,
  SPARKEY_HASH_HEADER_CORRUPT = -306,
  SPARKEY_HASH_SIZE_INVALID = -307,

} sparkey_returncode;

/**
 * Get a human readable string from a return code.
 * @param code a return code
 * @returns a string representing the return code.
 */
const char * sparkey_errstring(sparkey_returncode code);

#endif


