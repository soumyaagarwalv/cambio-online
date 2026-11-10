cmake_minimum_required(VERSION 3.20)
project(cambio_ai_online LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Boost.Beast/Asio are header-oriented for this server.
find_package(Boost CONFIG REQUIRED)

add_library(cambio_engine
    src/Card.cpp
    src/Deck.cpp
    src/Player.cpp
    src/GameState.cpp
    src/GameEngine.cpp
)
target_include_directories(cambio_engine PUBLIC include)
target_compile_options(cambio_engine PRIVATE -Wall -Wextra -Wpedantic)

add_library(cambio_ai
    src/AI.cpp
)
target_include_directories(cambio_ai PUBLIC include)
target_link_libraries(cambio_ai PUBLIC cambio_engine)
target_compile_options(cambio_ai PRIVATE -Wall -Wextra -Wpedantic)

add_library(cambio_server_core
    src/server/GameView.cpp
    src/server/Protocol.cpp
    src/server/GameRoom.cpp
    src/server/RoomManager.cpp
)
target_include_directories(cambio_server_core PUBLIC include)
if(TARGET Boost::headers)
    target_link_libraries(cambio_server_core PUBLIC cambio_engine Boost::headers)
elseif(TARGET Boost::boost)
    target_link_libraries(cambio_server_core PUBLIC cambio_engine Boost::boost)
else()
    target_link_libraries(cambio_server_core PUBLIC cambio_engine)
    if(Boost_INCLUDE_DIRS)
        target_include_directories(cambio_server_core PUBLIC ${Boost_INCLUDE_DIRS})
    endif()
endif()
target_compile_definitions(cambio_server_core PUBLIC BOOST_ERROR_CODE_HEADER_ONLY)

enable_testing()

add_executable(cambio_engine_smoke src/main.cpp)
target_link_libraries(cambio_engine_smoke PRIVATE cambio_engine)
add_test(NAME cambio_engine_smoke COMMAND cambio_engine_smoke)

add_executable(cambio_flip_tests tests/GameEngineFlipTests.cpp)
target_link_libraries(cambio_flip_tests PRIVATE cambio_engine)
add_test(NAME cambio_flip_tests COMMAND cambio_flip_tests)

add_executable(cambio_engine_rules_tests tests/EngineRulesTests.cpp)
target_link_libraries(cambio_engine_rules_tests PRIVATE cambio_engine)
add_test(NAME cambio_engine_rules_tests COMMAND cambio_engine_rules_tests)

add_executable(cambio_king_tests tests/KingPowerTests.cpp)
target_link_libraries(cambio_king_tests PRIVATE cambio_server_core)
add_test(NAME cambio_king_tests COMMAND cambio_king_tests)

add_executable(cambio_server_tests tests/ServerViewTests.cpp)
target_link_libraries(cambio_server_tests PRIVATE cambio_server_core)
add_test(NAME cambio_server_tests COMMAND cambio_server_tests)

add_executable(cambio_room_tests tests/RoomManagerTests.cpp)
target_link_libraries(cambio_room_tests PRIVATE cambio_server_core)
add_test(NAME cambio_room_tests COMMAND cambio_room_tests)

add_executable(cambio_ai_tests tests/AIRuntimeTests.cpp)
target_link_libraries(cambio_ai_tests PRIVATE cambio_ai)
add_test(NAME cambio_ai_tests COMMAND cambio_ai_tests)

add_executable(cambio_ai_benchmark src/ai_benchmark.cpp)
target_link_libraries(cambio_ai_benchmark PRIVATE cambio_ai)

add_executable(cambio_server src/server/main.cpp)
target_include_directories(cambio_server PRIVATE include)
target_link_libraries(cambio_server PRIVATE cambio_server_core)
if(TARGET Boost::headers)
    target_link_libraries(cambio_server PRIVATE Boost::headers)
elseif(TARGET Boost::boost)
    target_link_libraries(cambio_server PRIVATE Boost::boost)
elseif(Boost_INCLUDE_DIRS)
    target_include_directories(cambio_server PRIVATE ${Boost_INCLUDE_DIRS})
endif()
target_compile_definitions(cambio_server PRIVATE BOOST_ERROR_CODE_HEADER_ONLY)
if(UNIX)
    target_link_libraries(cambio_server PRIVATE pthread)
endif()
