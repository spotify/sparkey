set (SRC
	buf.c hashalgorithms.c hashiter.c hashwriter.c
	logreader.c returncodes.c util.c endiantools.c
	hashheader.c hashreader.c logheader.c logwriter.c MurmurHash3.c)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Werror -pedantic -std=c99")

add_library (sparkey ${SRC})
add_executable (sparkey-cli ${SRC} main.c)
add_executable (sparkey-bench ${SRC} bench.c)

set_target_properties (sparkey-cli PROPERTIES OUTPUT_NAME sparkey)

target_link_libraries (sparkey-cli snappy)
target_link_libraries (sparkey-bench snappy)
