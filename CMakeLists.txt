cmake_minimum_required(VERSION 3.6)
project(flow VERSION "0.11.0" LANGUAGES CXX)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(MASTER_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(MASTER_PROJECT ON)
endif ()

option(FLOW_DIRECT_THREADED_VM "Flow VM using direct threaded mode [default: OFF]" OFF)
option(FLOW_EMBEDDED_FMTLIB "Build against embedded fmtlib [default: ${MASTER_PROJECT}]" ${MASTER_PROJECT})
option(FLOW_ENABLE_TESTS "Enable building tests [default: ${MASTER_PROJECT}]" ${MASTER_PROJECT})
option(FLOW_ENABLE_EXAMPLES "Enable building examples [default: ${MASTER_PROJECT}]" ${MASTER_PROJECT})
option(FLOW_ENABLE_TIDY "Enable clang-tidy [default: OFF]" OFF)

set(PACKAGE_URL "https://github.com/christianparpart/flow/")

if(FLOW_ENABLE_TIDY) # {{{ clang-tidy
  set(CLANG_TIDY_CHECKS "\
,bugprone-*\
,clang-analyzer-*\
,cppcoreguidelines-*\
,modernize-*\
,performance-*\
,-bugprone-macro-parentheses\
,-cppcoreguidelines-pro-bounds-array-to-pointer-decay\
,-cppcoreguidelines-pro-bounds-constant-array-index\
,-cppcoreguidelines-pro-bounds-pointer-arithmetic\
,-cppcoreguidelines-pro-type-cstyle-cast\
,-cppcoreguidelines-pro-type-reinterpret-cast\
,-cppcoreguidelines-pro-type-static-cast-downcast\
,-cppcoreguidelines-pro-type-union-access\
,-cppcoreguidelines-pro-type-vararg\
,-modernize-use-auto\
")

  find_program(CLANG_TIDY_EXE
    NAMES clang-tidy-7 clang-tidy-6.0 clang-tidy
    DOC "Path to clang-tidy executable")
  if(NOT CLANG_TIDY_EXE)
    message(STATUS "clang-tidy not found.")
  else()
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}" "-checks=${CLANG_TIDY_CHECKS}")
  endif()
endif() # }}}

include(FindDoxygen)
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(CheckFunctionExists)
include(CheckVariableExists)
include(CheckTypeSize)
include(CheckLibraryExists)
include(CheckCSourceCompiles)
include(CMakeDetermineCCompiler)

# fmtlib
if(FLOW_EMBEDDED_FMTLIB)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt" EXCLUDE_FROM_ALL)
  add_definitions(-DFMT_USE_WINDOWS_H=0)
else()
  # master project must provide its own fmtlib
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR}/src
                    ${CMAKE_CURRENT_SOURCE_DIR}/src)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/flow/sysconfig.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/src/flow/sysconfig.h)

# {{{ FlowCore
add_library(FlowCore STATIC
            src/flow/Diagnostics.cc
            src/flow/LiteralType.cc
            src/flow/MatchClass.cc
            src/flow/NativeCallback.cc
            src/flow/SourceLocation.cc
            src/flow/Signature.cc
            src/flow/TargetCodeGenerator.cc
            src/flow/ir/BasicBlock.cc
            src/flow/ir/Constant.cc
            src/flow/ir/ConstantArray.cc
            src/flow/ir/ConstantValue.cc
            src/flow/ir/IRBuilder.cc
            src/flow/ir/IRHandler.cc
            src/flow/ir/IRProgram.cc
            src/flow/ir/Instr.cc
            src/flow/ir/InstructionVisitor.cc
            src/flow/ir/Instructions.cc
            src/flow/ir/PassManager.cc
            src/flow/ir/Value.cc
            src/flow/transform/EmptyBlockElimination.cc
            src/flow/transform/InstructionElimination.cc
            src/flow/transform/MergeBlockPass.cc
            src/flow/transform/UnusedBlockPass.cc
            src/flow/util/Cidr.cc
            src/flow/util/Flags.cc
            src/flow/util/RegExp.cc
            src/flow/vm/ConstantPool.cc
            src/flow/vm/Handler.cc
            src/flow/vm/Instruction.cc
            src/flow/vm/Match.cc
            src/flow/vm/Program.cc
            src/flow/vm/Runner.cc
            src/flow/vm/Runtime.cc
            )

target_link_libraries(FlowCore PUBLIC fmt::fmt-header-only)
set_target_properties(FlowCore PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(CLANG_TIDY_EXE)
  set_target_properties(FlowCore PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
endif()

install(TARGETS FlowCore DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/flow/"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/include/flow"
    FILES_MATCHING PATTERN "*.h")

target_include_directories(FlowCore PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
  $<INSTALL_INTERFACE:include>)
# }}}
# {{{ FlowLang
add_library(FlowLang STATIC
            src/flow/lang/AST.cc
            src/flow/lang/ASTPrinter.cc
            src/flow/lang/CallVisitor.cc
            src/flow/lang/Interpreter.cc
            src/flow/lang/IRGenerator.cc
            src/flow/lang/Lexer.cc
            src/flow/lang/Parser.cc
            src/flow/lang/Token.cc
            )

set_target_properties(FlowLang PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(FlowLang PRIVATE fmt::fmt-header-only)
if(CLANG_TIDY_EXE)
  set_target_properties(FlowLang PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
endif()

install(TARGETS FlowLang DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/flow/lang"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/include/flow/lang"
    FILES_MATCHING PATTERN "*.h")

target_include_directories(FlowLang PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
  $<INSTALL_INTERFACE:include>)
# }}}
# {{{ fltest
if(FLOW_ENABLE_TESTS)
  add_executable(fltest src/fltest/main.cc
                        src/fltest/flowtest.cc)

  set_target_properties(fltest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  if(CLANG_TIDY_EXE)
    set_target_properties(fltest PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
  endif()

  target_link_libraries(fltest PUBLIC fmt::fmt-header-only)
  target_link_libraries(fltest PRIVATE FlowLang FlowCore)
  if(UNIX)
    target_link_libraries(fltest PRIVATE stdc++fs)
  endif()
endif()
# }}}
# {{{ examples
if(FLOW_ENABLE_EXAMPLES)
  add_executable(example_hello examples/hello.cc)

  set_target_properties(example_hello PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  if(CLANG_TIDY_EXE)
    set_target_properties(example_hello PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
  endif()

  target_link_libraries(example_hello PRIVATE FlowLang FlowCore)
endif()
# }}}
