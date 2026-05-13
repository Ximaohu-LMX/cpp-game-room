set(PROTO_FILES
    ${PROJECT_SOURCE_DIR}/proto/common.proto
    ${PROJECT_SOURCE_DIR}/proto/login.proto
    ${PROJECT_SOURCE_DIR}/proto/match.proto
    ${PROJECT_SOURCE_DIR}/proto/room.proto
    ${PROJECT_SOURCE_DIR}/proto/game.proto
    ${PROJECT_SOURCE_DIR}/proto/rank.proto
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

set(GAME_CORE_SOURCES
    server/game_server.cpp
    server/service_context.cpp
    server/message_dispatcher.cpp
    net/tcp_server.cpp
    net/tcp_connection.cpp
    net/session.cpp
    net/packet.cpp
    net/codec.cpp
    protocol/proto_helper.cpp
    player/player.cpp
    player/player_manager.cpp
    login/login_service.cpp
    match/match_queue.cpp
    match/match_service.cpp
    room/room.cpp
    room/room_manager.cpp
    game/input_buffer.cpp
    game/game_room.cpp
    game/game_loop.cpp
    game/state_sync.cpp
    game/settlement_service.cpp
    storage/mysql_client.cpp
    storage/redis_client.cpp
    storage/player_repository.cpp
    storage/battle_repository.cpp
    rank/rank_service.cpp
    timer/timer_manager.cpp
    util/logger.cpp
    config/config_manager.cpp
)

add_library(game_core STATIC ${GAME_CORE_SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})

target_include_directories(game_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${PROJECT_SOURCE_DIR}
        ${Protobuf_INCLUDE_DIRS}
)

target_link_libraries(game_core
    PUBLIC
        Boost::system
        protobuf::libprotobuf
        Threads::Threads
)

if(GAME_USE_MYSQL)
    find_path(GAME_MYSQL_INCLUDE_DIR
        NAMES mysql.h
        PATH_SUFFIXES mysql mariadb
    )
    find_library(GAME_MYSQL_LIBRARY
        NAMES mysqlclient mariadb
    )
    if(NOT GAME_MYSQL_INCLUDE_DIR OR NOT GAME_MYSQL_LIBRARY)
        message(FATAL_ERROR "GAME_USE_MYSQL=ON requires MySQL/MariaDB development files, for example libmysqlclient-dev or libmariadb-dev")
    endif()
    target_include_directories(game_core PUBLIC ${GAME_MYSQL_INCLUDE_DIR})
    target_link_libraries(game_core PUBLIC ${GAME_MYSQL_LIBRARY})
    target_compile_definitions(game_core PUBLIC GAME_USE_MYSQL=1)
endif()

if(GAME_USE_REDIS)
    find_path(GAME_HIREDIS_INCLUDE_DIR
        NAMES hiredis/hiredis.h
    )
    find_library(GAME_HIREDIS_LIBRARY
        NAMES hiredis
    )
    if(NOT GAME_HIREDIS_INCLUDE_DIR OR NOT GAME_HIREDIS_LIBRARY)
        message(FATAL_ERROR "GAME_USE_REDIS=ON requires hiredis development files, for example libhiredis-dev")
    endif()
    target_include_directories(game_core PUBLIC ${GAME_HIREDIS_INCLUDE_DIR})
    target_link_libraries(game_core PUBLIC ${GAME_HIREDIS_LIBRARY})
    target_compile_definitions(game_core PUBLIC GAME_USE_REDIS=1)
endif()

if(spdlog_FOUND)
    target_link_libraries(game_core PUBLIC spdlog::spdlog)
    target_compile_definitions(game_core PUBLIC GAME_USE_SPDLOG=1)
endif()

set(GAME_ABSL_TARGETS
    absl::base
    absl::strings
    absl::log_severity
    absl::raw_logging_internal
    absl::log_internal_check_op
    absl::log_internal_conditions
    absl::log_internal_message
    absl::log_internal_nullguard
    absl::log_internal_proto
)

foreach(GAME_ABSL_TARGET ${GAME_ABSL_TARGETS})
    if(TARGET ${GAME_ABSL_TARGET})
        target_link_libraries(game_core PUBLIC ${GAME_ABSL_TARGET})
    endif()
endforeach()

find_library(GAME_ABSL_CHECK_OP_LIBRARY
    NAMES absl_log_internal_check_op
    HINTS
        "$ENV{CONDA_PREFIX}/lib"
        "/home/lmx/anaconda3/lib"
)
if(GAME_ABSL_CHECK_OP_LIBRARY)
    target_link_libraries(game_core PUBLIC ${GAME_ABSL_CHECK_OP_LIBRARY})
endif()

add_executable(game_server main.cpp)
target_link_libraries(game_server PRIVATE game_core protobuf::libprotobuf)
