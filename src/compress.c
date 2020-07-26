/*
* Copyright (c) 2012-2020 Spotify AB
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

#include "sparkey-internal.h"
#include "sparkey.h"

#include <snappy-c.h>
#include <zstd.h>


static uint32_t sparkey_snappy_max_compressed_size(uint32_t block_size) {
  return snappy_max_compressed_length(block_size);
}

static sparkey_returncode sparkey_snappy_decompress(uint8_t *input, uint32_t compressed_size, uint8_t *output, uint32_t *uncompressed_size) {
  size_t rsize = *uncompressed_size;
  snappy_status status = snappy_uncompress((char *) input, compressed_size, (char *) output, &rsize);
  *uncompressed_size = rsize;
  if (status == SNAPPY_OK) {
    return SPARKEY_SUCCESS;
  }
  return SPARKEY_INTERNAL_ERROR;
}

static sparkey_returncode sparkey_snappy_compress(uint8_t *input, uint32_t uncompressed_size, uint8_t *output, uint32_t *compressed_size) {
  size_t rsize = *compressed_size;
  snappy_status status = snappy_compress((char *) input, uncompressed_size, (char *) output, &rsize);
  *compressed_size = rsize;
  if (status == SNAPPY_OK) {
    return SPARKEY_SUCCESS;
  }
  return SPARKEY_INTERNAL_ERROR;
}

static uint32_t sparkey_zstd_max_compressed_size(uint32_t block_size) {
  return ZSTD_compressBound(block_size);
}

static sparkey_returncode sparkey_zstd_decompress(uint8_t *input, uint32_t compressed_size, uint8_t *output, uint32_t *uncompressed_size) {
  size_t ret = ZSTD_decompress(output, *uncompressed_size, input, compressed_size);
  if (ZSTD_isError(ret)) {
    return SPARKEY_INTERNAL_ERROR;
  }
  *uncompressed_size = ret;
  return SPARKEY_SUCCESS;
}

static sparkey_returncode sparkey_zstd_compress(uint8_t *input, uint32_t uncompressed_size, uint8_t *output, uint32_t *compressed_size) {
  size_t ret = ZSTD_compress(output, *compressed_size, input, uncompressed_size, 3);
  if (ZSTD_isError(ret)) {
    return SPARKEY_INTERNAL_ERROR;
  }
  *compressed_size = ret;
  return SPARKEY_SUCCESS;
}

struct sparkey_compressor sparkey_compressors[] = {
  {
    .max_compressed_size = NULL,
  },
  {
    .max_compressed_size = sparkey_snappy_max_compressed_size,
    .decompress = sparkey_snappy_decompress,
    .compress = sparkey_snappy_compress,
  },
  {
    .max_compressed_size = sparkey_zstd_max_compressed_size,
    .decompress = sparkey_zstd_decompress,
    .compress = sparkey_zstd_compress,
  },
};

int sparkey_uses_compressor(sparkey_compression_type t) {
  switch (t) {
    case SPARKEY_COMPRESSION_SNAPPY:
    case SPARKEY_COMPRESSION_ZSTD:
      return 1;
    default:
      return 0;
  }
}
