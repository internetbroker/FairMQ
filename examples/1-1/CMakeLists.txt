################################################################################
#    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

add_library(Example11Lib STATIC
    "Sampler.cxx"
    "Sampler.h"
    "Sink.cxx"
    "Sink.h"
)

target_link_libraries(Example11Lib PUBLIC FairMQ)

add_executable(fairmq-ex-1-1-sampler runSampler.cxx)
target_link_libraries(fairmq-ex-1-1-sampler PRIVATE Example11Lib)

add_executable(fairmq-ex-1-1-sink runSink.cxx)
target_link_libraries(fairmq-ex-1-1-sink PRIVATE Example11Lib)

add_custom_target(Example11 DEPENDS fairmq-ex-1-1-sampler fairmq-ex-1-1-sink)

set(EX_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(FAIRMQ_BIN_DIR ${CMAKE_BINARY_DIR}/fairmq)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/fairmq-start-ex-1-1.sh.in ${CMAKE_CURRENT_BINARY_DIR}/fairmq-start-ex-1-1.sh)

# test

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test-ex-1-1.sh.in ${CMAKE_CURRENT_BINARY_DIR}/test-ex-1-1.sh)

add_test(NAME Example-1-1-zeromq COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test-ex-1-1.sh zeromq)
set_tests_properties(Example-1-1-zeromq PROPERTIES TIMEOUT "30" RUN_SERIAL true PASS_REGULAR_EXPRESSION "Received: ")

if(BUILD_NANOMSG_TRANSPORT)
  add_test(NAME Example-1-1-nanomsg COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test-ex-1-1.sh nanomsg)
  set_tests_properties(Example-1-1-nanomsg PROPERTIES TIMEOUT "30" RUN_SERIAL true PASS_REGULAR_EXPRESSION "Received: ")
endif()

add_test(NAME Example-1-1-shmem COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test-ex-1-1.sh shmem)
set_tests_properties(Example-1-1-shmem PROPERTIES TIMEOUT "30" RUN_SERIAL true PASS_REGULAR_EXPRESSION "Received: ")

# install

install(
    TARGETS
    fairmq-ex-1-1-sampler
    fairmq-ex-1-1-sink

    LIBRARY DESTINATION ${PROJECT_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${PROJECT_INSTALL_BINDIR}
)

# configure run script with different executable paths for build and for install directories
set(EX_BIN_DIR ${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_BINDIR})
set(FAIRMQ_BIN_DIR ${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_BINDIR}/fairmq)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/fairmq-start-ex-1-1.sh.in ${CMAKE_CURRENT_BINARY_DIR}/fairmq-start-ex-1-1.sh_install)

install(
    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/fairmq-start-ex-1-1.sh_install
    DESTINATION ${PROJECT_INSTALL_BINDIR}
    RENAME fairmq-start-ex-1-1.sh
)
