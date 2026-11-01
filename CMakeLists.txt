cmake_minimum_required(VERSION 3.20)
project(FootballUniverse VERSION 0.12.2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Link the MSVC runtime statically so players do not need Visual C++ installed.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
add_library(fu_core src/Game.cpp)
target_include_directories(fu_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(WIN32)
    add_executable(FootballUniverse WIN32 src/WinMain.cpp)
    target_link_libraries(FootballUniverse PRIVATE fu_core comctl32 gdi32 user32 gdiplus)
    target_compile_definitions(FootballUniverse PRIVATE UNICODE _UNICODE NOMINMAX)
    add_custom_command(TARGET FootballUniverse POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:FootballUniverse>/data"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/data/players.csv"
            "$<TARGET_FILE_DIR:FootballUniverse>/data/players.csv"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/clubs"
            "$<TARGET_FILE_DIR:FootballUniverse>/assets/clubs")
    if(MSVC)
        target_compile_options(FootballUniverse PRIVATE /utf-8 /W4)
        target_compile_options(fu_core PRIVATE /utf-8 /W4)
    endif()
else()
    add_executable(fu_demo src/DemoMain.cpp)
    target_link_libraries(fu_demo PRIVATE fu_core)
endif()
add_executable(fu_tests tests/test_game.cpp)
target_link_libraries(fu_tests PRIVATE fu_core)
enable_testing()
add_test(NAME core_simulation COMMAND fu_tests)
set_tests_properties(core_simulation PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(fu_lineup_tests tests/test_lineup.cpp)
target_link_libraries(fu_lineup_tests PRIVATE fu_core)
add_test(NAME match_lineup_and_substitutions COMMAND fu_lineup_tests)

add_executable(fu_features_tests tests/test_features.cpp)
target_link_libraries(fu_features_tests PRIVATE fu_core)
add_test(NAME scouting_captaincy_team_talks COMMAND fu_features_tests)
set_tests_properties(scouting_captaincy_team_talks PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(fu_transfer_tests tests/test_transfers.cpp)
target_link_libraries(fu_transfer_tests PRIVATE fu_core)
add_test(NAME multi_stage_transfer_negotiations COMMAND fu_transfer_tests)
set_tests_properties(multi_stage_transfer_negotiations PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(fu_advanced_transfers_tests tests/test_advanced_transfers.cpp)
target_link_libraries(fu_advanced_transfers_tests PRIVATE fu_core)
add_test(NAME advanced_transfers_loans_finances COMMAND fu_advanced_transfers_tests)
set_tests_properties(advanced_transfers_loans_finances PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(fu_development_tests tests/test_development.cpp)
target_link_libraries(fu_development_tests PRIVATE fu_core)
add_test(NAME academy_staff_mentors_progression COMMAND fu_development_tests)
set_tests_properties(academy_staff_mentors_progression PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(fu_competitions_tests tests/test_competitions.cpp)
target_link_libraries(fu_competitions_tests PRIVATE fu_core)
add_test(NAME cup_calendar_and_history COMMAND fu_competitions_tests)
set_tests_properties(cup_calendar_and_history PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(fu_careers_tests tests/test_careers.cpp)
target_link_libraries(fu_careers_tests PRIVATE fu_core)
add_test(NAME promotions_and_manager_jobs COMMAND fu_careers_tests)
set_tests_properties(promotions_and_manager_jobs PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(fu_matchday_tests tests/test_matchday.cpp)
target_link_libraries(fu_matchday_tests PRIVATE fu_core)
add_test(NAME detailed_matchday_stats_and_tactics COMMAND fu_matchday_tests)
set_tests_properties(detailed_matchday_stats_and_tactics PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(fu_pitch_scene_tests tests/test_pitch_scene.cpp)
target_link_libraries(fu_pitch_scene_tests PRIVATE fu_core)
add_test(NAME animated_pitch_determinism COMMAND fu_pitch_scene_tests)
set_tests_properties(animated_pitch_determinism PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(fu_real_roster_tests tests/test_real_rosters.cpp)
target_link_libraries(fu_real_roster_tests PRIVATE fu_core)
add_test(NAME real_wikidata_rosters_and_save COMMAND fu_real_roster_tests)
set_tests_properties(real_wikidata_rosters_and_save PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(fu_real_database_tests tests/test_real_database.cpp)
target_link_libraries(fu_real_database_tests PRIVATE fu_core)
add_test(NAME bundled_real_player_values COMMAND fu_real_database_tests)
set_tests_properties(bundled_real_player_values PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(fu_role_powers_tests tests/test_role_powers.cpp)
target_link_libraries(fu_role_powers_tests PRIVATE fu_core)
add_test(NAME five_career_roles_are_distinct COMMAND fu_role_powers_tests)
set_tests_properties(five_career_roles_are_distinct PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(fu_pitch_motion_tests tests/test_pitch_motion.cpp)
target_link_libraries(fu_pitch_motion_tests PRIVATE fu_core)
add_test(NAME no_abrupt_player_teleportation COMMAND fu_pitch_motion_tests)

add_executable(fu_match_continuity_tests tests/test_match_continuity.cpp)
target_link_libraries(fu_match_continuity_tests PRIVATE fu_core)
add_test(NAME continuous_match_possession_and_replay COMMAND fu_match_continuity_tests)
set_tests_properties(continuous_match_possession_and_replay PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
