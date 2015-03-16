cmake_minimum_required(VERSION 3.1)
project(hotel)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic -std=c99")

find_library(GC gc)
add_subdirectory(libmp)

set(EV_FILES
    ev/ev.c
    ev/ev.h
)
add_library(libev ${EV_FILES})

set(BOOT_FILES
    boot/pregen/compiler.tlb.h
    boot/pregen/hotelparser.c
    boot/pregen/init.tlb.h
    boot/pregen/jsonparser.c
    boot/pregen/tlmeta.c
    boot/pregen/xmlparser.c
)
add_library(libboot ${BOOT_FILES})
target_include_directories(libboot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vm/)

set(LIBTL_FILES
    llib/lhashmap.c
    llib/lhashmap.h
    llib/lqueue.c
    llib/lqueue.h

    vm/object.h vm/map.h vm/set.h vm/tlstring.h vm/buffer.h vm/frame.h vm/sym.h vm/bin.h
    vm/worker.h vm/vm.h vm/lock.h
    vm/error.h vm/native.h vm/eval.h vm/controlflow.h vm/debugger.h vm/list.h vm/evio.h
    vm/queue.h vm/hashmap.h vm/mutable.h vm/number.h vm/tlregex.h vm/serialize.h vm/time.h
    vm/var.h
    vm/a_var.h
    vm/args.c
    vm/args.h
    vm/array.c
    vm/bcode.c
    vm/bcode.h
    vm/bin.c
    vm/buf.h
    vm/buffer.c
    vm/call.c
    vm/controlflow.c
    vm/debug.h
    vm/debugger.c
    vm/env.c
    vm/env.h
    vm/error.c
    vm/eval.c
    vm/evio.c
    vm/frame.c
    vm/hashmap.c
    vm/http.c
    vm/idset.c
    vm/idset.h
    vm/list.c
    vm/lock.c
    vm/map.c
    vm/mutable.c
    vm/number.c
    vm/object.c
    vm/platform.h
    vm/queue.c
    vm/regex.c
    vm/serialize.c
    vm/set.c
    vm/storage.c
    vm/tlstring.c
    vm/sym.c
    vm/tagged.c
    vm/task.c
    vm/task.h
    vm/tests.h
    vm/time.c
    vm/tl.h
    vm/trace-off.h
    vm/trace-on.h
    vm/value.c
    vm/var.c
    vm/vm.c
    vm/weakmap.c
    vm/weakmap.h
    vm/worker.c
    config.h
    )

SET(TEST_FILES
    vm/args_test.c
    vm/env_test.c
    vm/hashmap_test.c
    vm/idset_test.c
    vm/number_test.c
    vm/pmap_test.c
    vm/weakmap_test.c
)

add_library(libtl ${LIBTL_FILES})
target_include_directories(libtl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libmp/)
target_include_directories(libtl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libatomic_ops/src/)


add_executable(tl vm/tl.c)
target_link_libraries(tl GC libmp libev libboot libtl)
