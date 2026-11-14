cmake_minimum_required(VERSION 3.21)
project(cryengine_bohemia_alife_sim VERSION 1.0.0 LANGUAGES CXX)

option(BOHEMIA_BUILD_TESTS "Build deterministic CTest verification suites" ON)
option(BOHEMIA_BUILD_SHOWCASE_EXPORTER "Build benchmark and PNG showcase exporter" ON)
option(BOHEMIA_BUILD_LIVE_SIM "Build the interactive Win32 simulator" ON)

add_library(bohemia_core STATIC
    src/core/ArmorBallistics.cpp
    src/core/BehaviorObjects.cpp
    src/core/SimLodAndCrime.cpp
    src/core/SubbrainArchitecture.cpp)
target_include_directories(bohemia_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(bohemia_core PUBLIC cxx_std_20)
set_target_properties(bohemia_core PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(bohemia_core PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus /wd4324)
else()
    target_compile_options(bohemia_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

if(BOHEMIA_BUILD_TESTS)
    include(CTest)
    add_executable(BohemiaAlifeTests tests/BohemiaAlifeTests.cpp)
    target_link_libraries(BohemiaAlifeTests PRIVATE bohemia_core)
    target_compile_features(BohemiaAlifeTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(BohemiaAlifeTests PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus /wd4324)
    endif()
    foreach(test_name IN ITEMS
        SubbrainPreemption_ExecutesCleanupTreeAndReleasesSmartObjectLease
        SmartAreaConstraintSatisfaction_ReassignsPepaWhenFrantaIsAbsentFromDiceTable
        NavigationSmartObject_InjectsLockpickSubtreeOnLockedDoorEdge
        MultiAgentSituation_CoordinatesAccuserGuardAndBystanderRoles
        SimLodHydrationHysteresis_PreservesExactPositionAndSanitizesRecycledBodySlot
        CommitmentHysteresis_PreventsUtilityPingPongOscillationBetweenEqualNeeds
        TwoStageCrimeDeduction_WallBlocksVisualSight_CorrelatesAudioClickWithDoorExit
        SchedulePhaseJitterAndTimeSlicing_EliminatesThunderingHerdFrameSpikes
        WilliamsFourLayerArmor_ObliqueBladeGlances_AndBluntShockDegradesPaddingAndStamina
        ZeroHeapAllocation_SteadyState1000NpcTick_AllocatesZeroBytes)
        add_test(NAME "${test_name}" COMMAND BohemiaAlifeTests "${test_name}")
    endforeach()
endif()

if(BOHEMIA_BUILD_SHOWCASE_EXPORTER)
    add_executable(BohemiaShowcaseExporter tools/BohemiaShowcaseExporter.cpp)
    target_link_libraries(BohemiaShowcaseExporter PRIVATE bohemia_core)
    target_compile_features(BohemiaShowcaseExporter PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(BohemiaShowcaseExporter PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus /wd4324)
    endif()
endif()

if(WIN32 AND BOHEMIA_BUILD_LIVE_SIM)
    add_executable(BohemiaLiveSim WIN32 tools/BohemiaLiveSim.cpp)
    target_link_libraries(BohemiaLiveSim PRIVATE bohemia_core user32 gdi32 winmm)
    target_compile_features(BohemiaLiveSim PRIVATE cxx_std_20)
    set_target_properties(BohemiaLiveSim PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release")
    if(MSVC)
        target_compile_options(BohemiaLiveSim PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus /wd4324)
    endif()
endif()
