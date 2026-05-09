add_executable(game_bot
    bot_main.cpp
    bot_client.cpp
    bot_manager.cpp
)

target_link_libraries(game_bot PRIVATE game_core)
target_link_libraries(game_bot PRIVATE protobuf::libprotobuf)
