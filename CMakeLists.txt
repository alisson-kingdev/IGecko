cmake_minimum_required(VERSION 3.20)

project(
    IGecko
    VERSION 0.1.0
    DESCRIPTION "IGecko Programming Language"
    LANGUAGES CXX
)

# =========================================================
# Project Settings
# =========================================================

include(cmake/StandardProjectSettings.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/StaticAnalyzers.cmake)
include(cmake/LTO.cmake)

# =========================================================
# Dependencies
# =========================================================

include(FetchContent)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
    httplib
    URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.15.3.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0
)

FetchContent_MakeAvailable(json httplib Catch2 ftxui)

# =========================================================
# Sources
# =========================================================

set(INTERPRETER_SOURCES
    src/cli/main.cpp
    src/lexer/Lexer.cpp
    src/parser/Parser.cpp
    src/ast/Nodes.cpp
    src/interpreter/Interpreter.cpp
    src/interpreter/impl/Builtins.cpp
    src/interpreter/impl/ExprVisitor.cpp
    src/interpreter/impl/StmtVisitor.cpp
    src/interpreter/utils/InterpreterUtils.cpp
    src/interpreter/utils/Operations.cpp
    src/interpreter/module/FsModule.cpp
    src/interpreter/module/ModuleLoader.cpp
    src/interpreter/module/HttpModule.cpp
    src/interpreter/module/JsonModule.cpp
    src/interpreter/environment/Environment.cpp
    src/interpreter/runtime/Runtime.cpp
    src/utils/Logger.cpp
    src/utils/ProcessUtils.cpp
    src/vm/Value.cpp
    src/vm/Object.cpp
    src/vm/Chunk.cpp
    src/vm/Disassembler.cpp
    src/vm/Vm.cpp
    src/compiler/Compiler.cpp
)

set(GPM_SOURCES
    src/cli/gpm/main.cpp
    src/cli/gpm/Config.cpp
    src/cli/gpm/Installer.cpp
    src/utils/ProcessUtils.cpp
)

# =========================================================
# Executables
# =========================================================

add_executable(gecko ${INTERPRETER_SOURCES})
add_executable(gpm ${GPM_SOURCES})

# =========================================================
# Include Directories
# =========================================================

target_include_directories(gecko PRIVATE src)
target_include_directories(gpm PRIVATE src)

# =========================================================
# Libraries
# =========================================================

target_link_libraries(gecko PRIVATE nlohmann_json::nlohmann_json httplib::httplib ftxui::component ftxui::dom ftxui::screen)
target_link_libraries(gpm PRIVATE nlohmann_json::nlohmann_json httplib::httplib)

# =========================================================
# Features
# =========================================================

set_project_warnings(gecko)
set_project_warnings(gpm)

enable_sanitizers(gecko)
enable_sanitizers(gpm)

enable_lto(gecko)
enable_lto(gpm)

# =========================================================
# Output Directories
# =========================================================

set_target_properties(gecko gpm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# =========================================================
# Testing
# =========================================================

enable_testing()

option(IGECKO_BUILD_TESTS "Build tests" OFF)

if(IGECKO_BUILD_TESTS)
    # Create a list of sources excluding the cli main to avoid duplicate main definition
    set(TEST_INTERPRETER_SOURCES ${INTERPRETER_SOURCES})
    list(REMOVE_ITEM TEST_INTERPRETER_SOURCES src/cli/main.cpp)
    
    set(TEST_SOURCES
        tests/main.cpp
        tests/LexerTest.cpp
        tests/ParserTest.cpp
    )
    
    add_executable(gecko_tests ${TEST_SOURCES} ${TEST_INTERPRETER_SOURCES})
    target_include_directories(gecko_tests PRIVATE src)
    target_link_libraries(gecko_tests PRIVATE Catch2::Catch2WithMain nlohmann_json::nlohmann_json httplib::httplib)
    
    add_test(
        NAME gecko_tests
        COMMAND gecko_tests
    )
endif()

# =========================================================
# Formatting
# =========================================================

find_program(CLANG_FORMAT clang-format)

if(CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        CONFIGURE_DEPENDS
        src/*.cpp
        src/*.hpp
        src/*.h
    )

    add_custom_target(format
        COMMAND ${CLANG_FORMAT}
        -i
        --style=file
        ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format"
    )
endif()

# =========================================================
# Install
# =========================================================

include(GNUInstallDirs)

install(TARGETS gecko gpm
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES src/cli/gpm/gpm.json
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY src/stdlib
    DESTINATION ${CMAKE_INSTALL_DATADIR}/igecko
)
