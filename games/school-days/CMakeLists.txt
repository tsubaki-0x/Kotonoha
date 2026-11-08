cmake_minimum_required(VERSION 3.18)
project(SchoolDays VERSION 0.1.0 LANGUAGES C CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(CTest)
endif()
if(NOT TARGET Kotonoha::Engine)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../lib"
        "${CMAKE_CURRENT_BINARY_DIR}/kotonoha")
endif()

find_package(ZLIB REQUIRED)
add_library(school_days STATIC
    src/KtrfAssetResolver.cpp
    src/KtrfStateStore.cpp
    src/SchoolDaysHistory.cpp
    src/SchoolDaysPersistenceCodec.cpp
    src/SchoolDaysPersistence.cpp
    src/SchoolDaysBackRules.cpp
    src/SchoolDaysKtrfProfile.cpp
    src/SchoolDaysKtrfSession.cpp
    src/SchoolDaysKtrfGameplayBridge.cpp
    src/SchoolDaysGpkArchive.cpp
    src/SchoolDaysGpkIndex.cpp
    src/SchoolDaysAssetMount.cpp
    src/SchoolDaysCmap.cpp
    src/SchoolDaysTitleModel.cpp
    src/SchoolDaysTitleRenderPlan.cpp
    src/SchoolDaysTitleProfile.cpp
    src/SchoolDaysEndBackgroundResolver.cpp
    src/SchoolDaysRouteMapModel.cpp
    src/SchoolDaysRouteMapOracle.cpp
    src/SchoolDaysConfigMenuModel.cpp
    src/SchoolDaysConfigMenuRenderPlan.cpp
    src/SchoolDaysRetailConfigPolicy.cpp
    src/SchoolDaysDisplayPolicy.cpp
    src/SchoolDaysConfigStorage.cpp
    src/SchoolDaysMenuBarPresenter.cpp
    src/SchoolDaysMenuBarLayout.cpp
    src/SchoolDaysTopMeterModel.cpp
    src/SchoolDaysTopMeterRenderPlan.cpp
    src/SchoolDaysChoiceTextPlan.cpp
    src/SchoolDaysSaveLoadModel.cpp
    src/SchoolDaysSaveLoadRenderPlan.cpp
    src/SchoolDaysSaveFeelModel.cpp
    src/SchoolDaysSaveFeelRenderPlan.cpp
    src/SchoolDaysExitViewModel.cpp
    src/SchoolDaysExitViewRenderPlan.cpp
    src/SchoolDaysReplayModel.cpp
    src/SchoolDaysReplayRenderPlan.cpp
    src/SchoolDaysReplayScriptOracle.cpp
    src/SchoolDaysReplayPlaybackSession.cpp
    src/SchoolDaysPlayDataSession.cpp
    src/SchoolDaysRouteMapRenderPlan.cpp
    src/SchoolDaysFontData.cpp
    src/SchoolDaysSaveProfile.cpp
    src/SchoolDaysSaveStorage.cpp
    src/SchoolDaysSaveProvenance.cpp
    src/SchoolDaysSaveMetadata.cpp
    src/SchoolDaysSaveMetadataState.cpp
    src/SchoolDaysSaveService.cpp
    src/SchoolDaysSaveServiceRead.cpp
    src/SchoolDaysBackLog.cpp
    src/SchoolDaysBackLogPresenter.cpp
    src/SchoolDaysSubtitleTypography.cpp
    src/SchoolDaysSystemUi.cpp
    src/SchoolDaysAssets.cpp
    src/SchoolDaysDisplayNarrativeMarkup.cpp
    src/SchoolDaysTranslationCoverage.cpp
    src/SchoolDaysRuntime.cpp
    src/SchoolDaysScenes.cpp
    src/SchoolDaysLauncher.cpp
)
target_include_directories(school_days PUBLIC include src)
target_link_libraries(school_days PUBLIC Kotonoha::Engine PRIVATE ZLIB::ZLIB)
target_compile_features(school_days PUBLIC cxx_std_17)
set_target_properties(school_days PROPERTIES CXX_EXTENSIONS OFF)

kotonoha_add_app(SchoolDays src/main.cpp)
target_link_libraries(SchoolDays PRIVATE school_days)
kotonoha_configure_app(SchoolDays
    BUNDLE_ID me.hirameki.kotonoha.schooldays GAME school-days)

# The Windows executable uses the black-and-white icon supplied specifically
# for School Days HQ. Only register the resource once the approved artwork has
# been converted to an .ico; a missing icon must not break existing L4 builds.
if(WIN32 AND NOT ANDROID)
    set(SCHOOL_DAYS_WINDOWS_ICON
        "${CMAKE_CURRENT_SOURCE_DIR}/windows/SchoolDays.ico")
    if(EXISTS "${SCHOOL_DAYS_WINDOWS_ICON}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${SCHOOL_DAYS_WINDOWS_ICON}")
        enable_language(RC)
        set(SCHOOL_DAYS_WINDOWS_RC
            "${CMAKE_CURRENT_SOURCE_DIR}/windows/SchoolDays.rc")
        set_source_files_properties("${SCHOOL_DAYS_WINDOWS_RC}" PROPERTIES
            INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/windows")
        target_sources(SchoolDays PRIVATE "${SCHOOL_DAYS_WINDOWS_RC}")
    else()
        message(STATUS "School Days Windows icon pending: ${SCHOOL_DAYS_WINDOWS_ICON}")
    endif()
endif()

# Use only the player's authorized School Days HQ key visual. Always copy it
# from the source tree to the binary folder, including asset-only rebuilds.
# The single SchoolDays.exe loads this PNG; no separate launcher EXE is needed.
if(WIN32 AND NOT ANDROID)
    set(SCHOOL_DAYS_LAUNCHER_HERO
        "${CMAKE_CURRENT_SOURCE_DIR}/launcher-art/SchoolDaysHQ-Launcher.png")
    if(EXISTS "${SCHOOL_DAYS_LAUNCHER_HERO}")
        get_property(_school_days_hero_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
        if(_school_days_hero_multi_config)
            set(_school_days_hero_out "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets")
        else()
            set(_school_days_hero_out "${CMAKE_CURRENT_BINARY_DIR}/assets")
        endif()
        add_custom_target(SchoolDaysLauncherArtwork
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "${_school_days_hero_out}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SCHOOL_DAYS_LAUNCHER_HERO}"
                "${_school_days_hero_out}/SchoolDaysHQ-Launcher.png"
            COMMENT "Sync School Days HQ launcher image (no retail GPK changes)"
            VERBATIM)
        add_dependencies(SchoolDays SchoolDaysLauncherArtwork)
    endif()
endif()

# Ship the user's replacement General, Sound and Display Options artwork outside
# the commercial System.GPK. The School Days-only overlay is mounted by default
# when no explicit --asset-overlay is selected. Other games are unaffected.
if(WIN32 AND NOT ANDROID)
    set(SCHOOL_DAYS_DISPLAY_OVERLAY "${CMAKE_CURRENT_SOURCE_DIR}/display-options")
    if(EXISTS "${SCHOOL_DAYS_DISPLAY_OVERLAY}/System/Option/Option_Display.png")
        get_property(_school_days_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
        if(_school_days_multi_config)
            set(_school_days_overlay_output "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/display-options")
        else()
            set(_school_days_overlay_output "${CMAKE_CURRENT_BINARY_DIR}/display-options")
        endif()
        # Always sync assets, including edits made without recompiling C++.
        # POST_BUILD alone does not run when the executable is already current.
        add_custom_target(SchoolDaysOptionAssets
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${SCHOOL_DAYS_DISPLAY_OVERLAY}"
                "${_school_days_overlay_output}"
            COMMENT "Syncing School Days option artwork (retail System.GPK unchanged)"
            VERBATIM)
        add_dependencies(SchoolDays SchoolDaysOptionAssets)
    endif()
endif()

if(BUILD_TESTING AND NOT ANDROID)
    add_subdirectory(tests)
endif()
