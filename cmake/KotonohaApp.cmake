option(KOTONOHA_BUILD_MACOS_BUNDLES "Build macOS application bundles"
    OFF)

function(kotonoha_add_app target)
    if(ANDROID)
        add_library(${target} SHARED ${ARGN})
    else()
        add_executable(${target} ${ARGN})
    endif()
endfunction()

function(kotonoha_configure_app target)
    cmake_parse_arguments(APP "" "BUNDLE_ID;GAME;PYTHON_STDLIB;PYTHON_VERSION" "" ${ARGN})
    if((APP_PYTHON_STDLIB OR APP_PYTHON_VERSION) AND
       NOT APP_GAME STREQUAL "doki-doki-literature-club")
        message(FATAL_ERROR "Only Doki Doki Literature Club may bundle Python")
    endif()
    if(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND KOTONOHA_BUILD_MACOS_BUNDLES)
        set_target_properties(${target} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST "${KOTONOHA_PROJECT_ROOT}/cmake/Info.plist.in"
            MACOSX_BUNDLE_BUNDLE_NAME "${target}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${APP_BUNDLE_ID}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_ICON_FILE "icon.icns"
            BUILD_RPATH "@executable_path/../Frameworks"
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
        set(icon "${KOTONOHA_ENGINE_ASSETS}/icon.icns")
        set_source_files_properties("${icon}" PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources HEADER_FILE_ONLY TRUE)
        target_sources(${target} PRIVATE "${icon}")
    endif()

    kotonoha_copy_assets(${target} GAME "${APP_GAME}")

    if(WIN32 AND NOT ANDROID)
        set(windows_dependency_dirs
            "$<TARGET_FILE_DIR:KotonohaEngine>"
            "$<TARGET_FILE_DIR:kotonoha_imgui>"
            "$<TARGET_FILE_DIR:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:SDL3_ttf::SDL3_ttf>"
            ${FFMPEG_LIBRARY_DIRS} ${JSON_C_LIBRARY_DIRS} ${LIBASS_LIBRARY_DIRS})
        foreach(library_dir IN LISTS FFMPEG_LIBRARY_DIRS JSON_C_LIBRARY_DIRS LIBASS_LIBRARY_DIRS)
            if(library_dir)
                get_filename_component(dependency_prefix "${library_dir}" DIRECTORY)
                list(APPEND windows_dependency_dirs "${dependency_prefix}/bin")
            endif()
        endforeach()
        if(FREETYPE_LIBRARY_RELEASE)
            get_filename_component(freetype_lib_dir "${FREETYPE_LIBRARY_RELEASE}" DIRECTORY)
            get_filename_component(freetype_prefix "${freetype_lib_dir}" DIRECTORY)
            list(APPEND windows_dependency_dirs "${freetype_prefix}/bin")
        endif()
        list(REMOVE_DUPLICATES windows_dependency_dirs)
        add_custom_command(TARGET ${target} POST_BUILD
            # Always refresh the four direct runtime DLLs before dependency
            # discovery; otherwise an older DLL already beside the executable
            # wins Windows' normal lookup order and gets repackaged again.
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:KotonohaEngine>" "$<TARGET_FILE_DIR:${target}>"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:kotonoha_imgui>" "$<TARGET_FILE_DIR:${target}>"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:${target}>"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:SDL3_ttf::SDL3_ttf>" "$<TARGET_FILE_DIR:${target}>"
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP_EXECUTABLE=$<TARGET_FILE:${target}>"
                "-DAPP_DESTINATION=$<TARGET_FILE_DIR:${target}>"
                "-DDEPENDENCY_DIRS=${windows_dependency_dirs}"
                -P "${KOTONOHA_PROJECT_ROOT}/cmake/PackageWindowsRuntime.cmake"
            VERBATIM)
    endif()

    if(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND KOTONOHA_BUILD_MACOS_BUNDLES)
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
            "${KOTONOHA_PROJECT_ROOT}/cmake/BundleApp.cmake"
            "${KOTONOHA_PROJECT_ROOT}/cmake/CheckBundleContents.cmake"
            "${KOTONOHA_PROJECT_ROOT}/cmake/PackageAssets.cmake"
            "${KOTONOHA_PROJECT_ROOT}/cmake/CheckBundledAssets.cmake")
        set(search_dirs
            "$<TARGET_FILE_DIR:KotonohaEngine>"
            "$<TARGET_FILE_DIR:kotonoha_imgui>"
            "$<TARGET_FILE_DIR:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:SDL3_ttf::SDL3_ttf>"
            ${FFMPEG_LIBRARY_DIRS} ${FFMPEG_STATIC_LIBRARY_DIRS}
            ${JSON_C_LIBRARY_DIRS})
        if(FREETYPE_LIBRARY_RELEASE)
            get_filename_component(freetype_directory "${FREETYPE_LIBRARY_RELEASE}" DIRECTORY)
            list(APPEND search_dirs "${freetype_directory}")
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP_EXECUTABLE=$<TARGET_FILE:${target}>"
                "-DBUNDLE_CONTENTS=$<TARGET_BUNDLE_CONTENT_DIR:${target}>"
                "-DAPP_GAME=${APP_GAME}"
                "-DDEPENDENCY_DIRS=${search_dirs}"
                "-DPYTHON_STDLIB=${APP_PYTHON_STDLIB}"
                "-DPYTHON_VERSION=${APP_PYTHON_VERSION}"
                -P "${KOTONOHA_PROJECT_ROOT}/cmake/BundleApp.cmake"
            VERBATIM)
        if(BUILD_TESTING)
            add_test(NAME "Kotonoha.BundleLayout.${target}"
                COMMAND "${CMAKE_COMMAND}"
                    "-DBUNDLE_CONTENTS=$<TARGET_BUNDLE_CONTENT_DIR:${target}>"
                    "-DAPP_GAME=${APP_GAME}"
                    "-DPYTHON_VERSION=${APP_PYTHON_VERSION}"
                    -P "${KOTONOHA_PROJECT_ROOT}/cmake/CheckBundleContents.cmake")
        endif()
    endif()
endfunction()
