cmake_minimum_required(VERSION 3.6)
project(flow VERSION "0.11.0" LANGUAGES CXX)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(FLOW_DIRECT_THREADED_VM "Flow VM using direct threaded mode [default: OFF]" OFF)
option(ENABLE_TESTS "Enable building tests [default: ON]" ON)
option(ENABLE_EXAMPLES "Enable building examples [default: ON]" ON)
option(ENABLE_TIDY "Enable clang-tidy [default: OFF]" OFF)

set(PACKAGE_URL "https://github.com/christianparpart/flow/")

find_package(Git)
include(ExternalProject)

if(GIT_FOUND) # {{{ fmtlib
  ExternalProject_Add(
    fmt
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG master
    CMAKE_ARGS -DMASTER_PROJECT=OFF
                -DFMT_DOC=OFF
                -DFMT_INSTALL=ON
                -DFMT_TEST=OFF
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    LOG_DOWNLOAD 1
    UPDATE_DISCONNECTED 1
    BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config ${configuration}
    INSTALL_COMMAND ""
  )
  add_definitions(-DFMT_HEADER_ONLY=1 -DFMT_USE_WINDOWS_H=0)
  if(UNIX)
		set(FMT_LIBRARIES "${CMAKE_CURRENT_BINARY_DIR}/3rdparty/fmt/src/fmt-build/libfmt.a")
	else()
		set(FMT_LIBRARIES "${CMAKE_CURRENT_BINARY_DIR}/3rdparty/fmt/src/fmt-build/fmt.lib")
	endif()
else()
  # assume catch is installed in a system directory
  #add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt")
endif() # }}}

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
"
CACHE STRING "clang-tidy checks to enable/disable")
message("Clang Tidy Checks: ${CLANG_TIDY_CHECKS}")

if(ENABLE_TIDY)
  find_program(CLANG_TIDY_EXE
    NAMES clang-tidy-7 clang-tidy-6.0 clang-tidy
    DOC "Path to clang-tidy executable")
  if(NOT CLANG_TIDY_EXE)
    message(STATUS "clang-tidy not found.")
  else()
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}" "-checks=${CLANG_TIDY_CHECKS}")
  endif()
endif()

include(FindDoxygen)
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(CheckFunctionExists)
include(CheckVariableExists)
include(CheckTypeSize)
include(CheckLibraryExists)
include(CheckCSourceCompiles)
include(CMakeDetermineCCompiler)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/src
                    ${CMAKE_CURRENT_SOURCE_DIR}/src
                    ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/fmt/src/fmt/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt/include)

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
            src/flow/util/RegExp.cc
            src/flow/vm/ConstantPool.cc
            src/flow/vm/Handler.cc
            src/flow/vm/Instruction.cc
            src/flow/vm/Match.cc
            src/flow/vm/Program.cc
            src/flow/vm/Runner.cc
            src/flow/vm/Runtime.cc
            )

add_dependencies(FlowCore fmt)
set_target_properties(FlowCore PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(CLANG_TIDY_EXE)
  set_target_properties(FlowCore PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
endif()
install(TARGETS FlowCore DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/flow/"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/include/flow"
    FILES_MATCHING PATTERN "*.h")
# }}}
# {{{ FlowLang
add_library(FlowLang STATIC
            src/flow/lang/AST.cc
            src/flow/lang/ASTPrinter.cc
            src/flow/lang/CallVisitor.cc
            src/flow/lang/IRGenerator.cc
            src/flow/lang/Lexer.cc
            src/flow/lang/Parser.cc
            src/flow/lang/Token.cc
            )

set_target_properties(FlowLang PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_dependencies(FlowLang fmt)
if(CLANG_TIDY_EXE)
  set_target_properties(FlowLang PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
endif()
install(TARGETS FlowLang DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/flow/lang"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/include/flow/lang"
    FILES_MATCHING PATTERN "*.h")
# }}}
# {{{ fltest
if(ENABLE_TESTS)
  add_executable(fltest src/fltest/main.cc
                        src/fltest/flowtest.cc)

  set_target_properties(fltest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  if(CLANG_TIDY_EXE)
    set_target_properties(fltest PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
  endif()
  target_link_libraries(fltest FlowLang FlowCore)

  if(UNIX)
    target_link_libraries(fltest stdc++fs)
  endif()
endif()
# }}}
