cmake_minimum_required(VERSION 3.25)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/uintDebuggerVersion.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/uintDebuggerVersion.h
    @ONLY
)

# Architecture-specific pre-built libraries
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BEAENGINE_LIB "${CMAKE_CURRENT_SOURCE_DIR}/BeaEngine/BeaEngine_x64.lib")
    set(DBGHELP_LIB   "${CMAKE_CURRENT_SOURCE_DIR}/clsDebugger/dbghelp_x64.lib")
else()
    set(BEAENGINE_LIB "${CMAKE_CURRENT_SOURCE_DIR}/BeaEngine/BeaEngine_x86.lib")
    set(DBGHELP_LIB   "${CMAKE_CURRENT_SOURCE_DIR}/clsDebugger/dbghelp_x86.lib")
endif()

set(SOURCES
    # Debugger core
    clsDebugger/clsDebugger.cpp
    clsDebugger/clsDebuggerCallbacks.cpp
    clsDebugger/clsDebuggerPublicAPI.cpp

    # Core engine
    clsAPIImport.cpp
    clsAppSettings.cpp
    clsBreakpointHardware.cpp
    clsBreakpointManager.cpp
    clsBreakpointMemory.cpp
    clsCommandParser.cpp
    clsBreakpointSoftware.cpp
    clsCallstackWorker.cpp
    clsClipboardHelper.cpp
    clsCrashHandler.cpp
    clsDisassembler.cpp
    clsFunctionsViewWorker.cpp
    clsHelperClass.cpp
    clsHexViewWorker.cpp
    clsMemDump.cpp
    clsMemManager.cpp
    clsMemoryProtector.cpp
    clsMemPool.cpp
    clsPEFile.cpp
    clsPEManager.cpp
    clsProcessDump.cpp
    clsProjectFile.cpp
    clsStringViewWorker.cpp
    clsSymbolAndSyntax.cpp
    main.cpp

    # UI dialogs
    qtDLGAbout.cpp
    qtDLGAssembler.cpp
    qtDLGAttach.cpp
    qtDLGBookmark.cpp
    qtDLGBreakPointManager.cpp
    qtDLGCallstack.cpp
    qtDLGDebugStrings.cpp
    qtDLGDetailInfo.cpp
    qtDLGDisassembler.cpp
    qtDLGExceptionAsk.cpp
    qtDLGExceptionEdit.cpp
    qtDLGFunctions.cpp
    qtDLGGoToDialog.cpp
    qtDLGHandleView.cpp
    qtDLGHeapView.cpp
    qtDLGHexView.cpp
    qtDLGLogView.cpp
    qtDLGMemoryView.cpp
    qtDLGUintDebugger.cpp
    qtDLGUintDebuggerMenuActions.cpp
    qtDLGOpenNewFile.cpp
    qtDLGOption.cpp
    qtDLGPEBView.cpp
    qtDLGPatchManager.cpp
    qtDLGPEEditor.cpp
    qtDLGProcessPrivilege.cpp
    qtDLGRegEdit.cpp
    qtDLGRegisters.cpp
    qtDLGSourceViewer.cpp
    qtDLGStack.cpp
    qtDLGStringView.cpp
    qtDLGTIBView.cpp
    qtDLGTrace.cpp
    qtDLGWindowView.cpp

    # Updater UI (compiled into main app)
    ../uintDebuggerUpdater/UpdaterWidget/ucheckupdateswidget.cpp
    ../uintDebuggerUpdater/UpdaterWidget/ufiledownloader.cpp
    ../uintDebuggerUpdater/UpdaterWidget/ugitHubReleaseChecker.cpp
    ../uintDebuggerUpdater/UpdaterWidget/updateitem.cpp
    ../uintDebuggerUpdater/UpdaterWidget/uupdatesmodel.cpp
    ../uintDebuggerUpdater/UpdaterWidget/uupdatestableview.cpp
    ../uintDebuggerUpdater/UpdaterWidget/uupdatewidget.cpp

    # Headers with Q_OBJECT (needed for AUTOMOC)
    clsDebugger/clsDebugger.h
    clsAPIImport.h
    clsAppSettings.h
    clsBreakpointHardware.h
    clsBreakpointManager.h
    clsBreakpointMemory.h
    clsCommandParser.h
    clsBreakpointSoftware.h
    clsCallstackWorker.h
    clsClipboardHelper.h
    clsCrashHandler.h
    clsDisassembler.h
    clsFunctionsViewWorker.h
    clsHelperClass.h
    clsHexViewWorker.h
    clsMemDump.h
    clsMemManager.h
    clsMemoryProtector.h
    clsMemPool.h
    clsPEFile.h
    clsPEManager.h
    clsProcessDump.h
    clsProjectFile.h
    clsStringViewWorker.h
    clsSymbolAndSyntax.h
    qtDLGAbout.h
    qtDLGAssembler.h
    qtDLGAttach.h
    qtDLGBookmark.h
    qtDLGBreakPointManager.h
    qtDLGCallstack.h
    qtDLGDebugStrings.h
    qtDLGDetailInfo.h
    qtDLGDisassembler.h
    qtDLGExceptionAsk.h
    qtDLGExceptionEdit.h
    qtDLGFunctions.h
    qtDLGGoToDialog.h
    qtDLGHandleView.h
    qtDLGHeapView.h
    qtDLGHexView.h
    qtDLGLogView.h
    qtDLGMemoryView.h
    qtDLGUintDebugger.h
    qtDLGOpenNewFile.h
    qtDLGOption.h
    qtDLGPEBView.h
    qtDLGPEEditor.h
    qtDLGPatchManager.h
    qtDLGProcessPrivilege.h
    qtDLGRegEdit.h
    qtDLGRegisters.h
    qtDLGSourceViewer.h
    qtDLGStack.h
    qtDLGStringView.h
    qtDLGTIBView.h
    qtDLGTrace.h
    qtDLGWindowView.h
    ../uintDebuggerUpdater/UpdaterWidget/ucheckupdateswidget.h
    ../uintDebuggerUpdater/UpdaterWidget/ufiledownloader.h
    ../uintDebuggerUpdater/UpdaterWidget/ugitHubReleaseChecker.h
    ../uintDebuggerUpdater/UpdaterWidget/updateitem.h
    ../uintDebuggerUpdater/UpdaterWidget/uupdatesmodel.h
    ../uintDebuggerUpdater/UpdaterWidget/uupdatestableview.h
    ../uintDebuggerUpdater/UpdaterWidget/uupdatewidget.h
)

qt_add_executable(uintDebugger WIN32 ${SOURCES})

target_include_directories(uintDebugger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/BeaEngine
    ${CMAKE_CURRENT_SOURCE_DIR}/clsDebugger
    ${CMAKE_CURRENT_SOURCE_DIR}/../uintDebuggerUpdater/UpdaterWidget
)

# AUTOUIC needs to find .ui files for UpdaterWidget sources
set_property(TARGET uintDebugger PROPERTY AUTOUIC_SEARCH_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../uintDebuggerUpdater/UpdaterWidget
)

target_compile_definitions(uintDebugger PRIVATE
    UNICODE
    _UNICODE
    WIN32
    _WINDOWS
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG QT_NO_DEBUG>
)

target_link_libraries(uintDebugger PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    ${BEAENGINE_LIB}
    ${DBGHELP_LIB}
    advapi32
    shell32
    psapi
    user32
)

if(MSVC)
    target_link_libraries(uintDebugger PRIVATE legacy_stdio_definitions)
endif()

if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    set(QT_DEPLOY_MODE $<$<CONFIG:Debug>:--debug>$<$<NOT:$<CONFIG:Debug>>:--release>)
    add_custom_command(TARGET uintDebugger POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            ${QT_DEPLOY_MODE}
            --no-translations
            --dir "$<TARGET_FILE_DIR:uintDebugger>"
            "$<TARGET_FILE:uintDebugger>"
        COMMENT "Deploying Qt runtime for uintDebugger"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
endif()

# Deploy Qt DLLs next to the executable on Windows
qt_generate_deploy_app_script(
    TARGET uintDebugger
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
