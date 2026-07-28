cmake_minimum_required(VERSION 3.15)
project(hyperliquid-cpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_WEBSOCKET "Build WebSocket support (fetches IXWebSocket)" ON)

# Find dependencies
find_package(CURL REQUIRED)
find_package(OpenSSL 3.0 REQUIRED)

# nlohmann/json - header-only
find_package(nlohmann_json 3.10.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3)
    FetchContent_MakeAvailable(json)
endif()

# msgpack-c (header-only) - fetch without building
find_package(msgpack-cxx QUIET)
if(NOT msgpack-cxx_FOUND)
    message(STATUS "msgpack-cxx not found, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(msgpack
        GIT_REPOSITORY https://github.com/msgpack/msgpack-c.git
        GIT_TAG cpp-3.3.0)
    FetchContent_GetProperties(msgpack)
    if(NOT msgpack_POPULATED)
        FetchContent_Populate(msgpack)
    endif()
    # Create interface library for msgpack headers
    add_library(msgpack-cxx INTERFACE)
    target_include_directories(msgpack-cxx INTERFACE ${msgpack_SOURCE_DIR}/include)
endif()

# IXWebSocket - websocket transport
#
# Not libcurl: curl only speaks ws/wss when built with --enable-websockets, and
# the libcurl shipped in the macOS SDK is not. It links fine and then fails at
# runtime, so the SDK would break on a stock toolchain.
if(BUILD_WEBSOCKET)
    # Raw-DEFLATE inflate for fastAssetCtxs payloads. Already present on every
    # supported platform (libcurl links it), so this costs no new install step.
    find_package(ZLIB REQUIRED)

    include(FetchContent)
    # IXWebSocket declares cmake_minimum_required(VERSION 3.4.1...3.17.2) and
    # CMake 4.x removed compatibility with < 3.5, so the fetch errors out without
    # this. Scoped to this directory so it does not relax the project's own floor.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(USE_TLS ON CACHE BOOL "" FORCE)
    set(USE_OPEN_SSL ON CACHE BOOL "" FORCE)  # project already requires OpenSSL 3.0
    set(USE_ZLIB OFF CACHE BOOL "" FORCE)     # permessage-deflate is not needed
    set(USE_WS OFF CACHE BOOL "" FORCE)       # skip the bundled ws CLI tool
    set(USE_TEST OFF CACHE BOOL "" FORCE)
    set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(ixwebsocket
        GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
        GIT_TAG v12.0.1)
    FetchContent_MakeAvailable(ixwebsocket)
endif()

# Library source files
set(HYPERLIQUID_SOURCES
    src/api.cpp
    src/info.cpp
    src/exchange.cpp
    src/types.cpp
    src/utils/signing.cpp
    src/utils/conversions.cpp
    src/utils/crypto/eip712.cpp
    src/utils/crypto/keccak.cpp
    src/utils/crypto/ecdsa.cpp
)

if(BUILD_WEBSOCKET)
    list(APPEND HYPERLIQUID_SOURCES src/websocket_manager.cpp src/ws_types.cpp)
endif()

# Create library
add_library(hyperliquid ${HYPERLIQUID_SOURCES})

target_include_directories(hyperliquid
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(hyperliquid
    PUBLIC
        nlohmann_json::nlohmann_json
        msgpack-cxx
    PRIVATE
        CURL::libcurl
        OpenSSL::Crypto
)

# PRIVATE: ix::WebSocket is forward declared in websocket_manager.hpp, so
# IXWebSocket's headers never reach consumers of this library.
if(BUILD_WEBSOCKET)
    target_link_libraries(hyperliquid PRIVATE ixwebsocket ZLIB::ZLIB)
    target_compile_definitions(hyperliquid PUBLIC HYPERLIQUID_WEBSOCKET)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(hyperliquid PRIVATE /W4 /WX-)
else()
    target_compile_options(hyperliquid PRIVATE -Wall -Wextra -pedantic)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install rules
include(GNUInstallDirs)
install(TARGETS hyperliquid
    EXPORT hyperliquid-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/hyperliquid
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
