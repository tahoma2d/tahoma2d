set(VERSION 1.4)

set(MOC_HEADERS
    addfilmstripframespopup.h
    adjustlevelspopup.h
    adjustthicknesspopup.h
    antialiaspopup.h
    audiorecordingpopup.h
    autoinputcellnumberpopup.h
    batches.h
    batchserversviewer.h
    binarizepopup.h
    boardsettingspopup.h
    brightnessandcontrastpopup.h
    cachefxcommand.h
    camerasettingspopup.h
    canvassizepopup.h
    castviewer.h
    colormodelbehaviorpopup.h
    colormodelviewer.h
    commandbar.h
    commandbarpopup.h
    convertpopup.h
    duplicatepopup.h
    dvdirtreeview.h
    dvitemview.h
    dvwidgets.h
    exportlevelpopup.h
    exportpanel.h
    exportscenepopup.h
    filebrowser.h
    filebrowsermodel.h
    filebrowserpopup.h
    fileinfopopup.h
    filmstrip.h
    flipbook.h
    formatsettingspopups.h
    frameheadgadget.h
    fxparameditorpopup.h
    histogrampopup.h
    historypane.h
    imageviewer.h
    insertfxpopup.h
    layerfooterpanel.h
    layerheaderpanel.h
    levelcreatepopup.h
    levelsettingspopup.h
    linesfadepopup.h
    lipsyncpopup.h
    loadfolderpopup.h
    locatorpopup.h
    magpiefileimportpopup.h
    mainwindow.h
    matchline.h
    menubar.h
    menubarpopup.h
    meshifypopup.h
    messagepanel.h
    onionskinmaskgui.h
    outputsettingspopup.h
    overwritepopup.h
    pane.h
    pltgizmopopup.h
    preferencespopup.h
    previewer.h
    previewfxmanager.h
    projectpopup.h
    psdsettingspopup.h
    reframepopup.h
    renumberpopup.h
    reslist.h
    ruler.h
    savepresetpopup.h
    scenesettingspopup.h
    sceneviewer.h
    sceneviewercontextmenu.h
    scriptconsolepanel.h
    separatecolorspopup.h
    separatecolorsswatch.h
    shortcutpopup.h
    soundtrackexport.h
    startuppopup.h
    styleshortcutswitchablepanel.h
    svncleanupdialog.h
    svncommitdialog.h
    svndeletedialog.h
    svnlockdialog.h
    svnlockframerangedialog.h
    svnpurgedialog.h
    svnrevertdialog.h
    svnupdateandlockdialog.h
    svnupdatedialog.h
    tapp.h
    tasksviewer.h
    testpanel.h
    timestretchpopup.h
    toolbar.h
    tpanels.h
    trackerpopup.h
	vectorguideddrawingpane.h
    vectorizerpopup.h
    vectorizerswatch.h
    versioncontrol.h
    versioncontroltimeline.h
    versioncontrolwidget.h
    viewerpane.h
    xshcellviewer.h
    xshcolumnviewer.h
    xsheetviewer.h
    xshnoteviewer.h
    xshrowviewer.h
    xshtoolbar.h
	xdtsimportpopup.h
    ../stopmotion/stopmotion.h
    ../stopmotion/stopmotioncontroller.h
    ../stopmotion/webcam.h
    ../stopmotion/jpgconverter.h
    ../stopmotion/canon.h
    ../stopmotion/stopmotionserial.h
    ../stopmotion/stopmotionlight.h
    cameracapturelevelcontrol.h
    penciltestpopup.h
)

set(HEADERS
    ${MOC_HEADERS}
    celldata.h
    cellkeyframedata.h
    cellkeyframeselection.h
    cellselection.h
    columncommand.h
    columnselection.h
    curveio.h
    drawingdata.h
    exportlevelcommand.h
    filedata.h
    fileselection.h
    filmstripcommand.h
    filmstripselection.h
    floatingpanelcommand.h
    history.h
    iocommand.h
    keyframedata.h
    keyframemover.h
    keyframeselection.h
    kis_tablet_support_win8.h
    menubarcommandids.h
    moviegenerator.h
    sceneviewerevents.h
    selectionutils.h
    subcameramanager.h
    subscenecommand.h
    tfarmstuff.h
    versioncontrolgui.h
    versioncontrolxmlreader.h
    viewerdraw.h
    viewerpopup.h
    xshcellmover.h
    xsheetdragtool.h
    castselection.h
    ../include/cellposition.h
    ../include/orientation.h
    ../include/saveloadqsettings.h
    xdtsio.h
# Tracker file
    ObjectTracker.h
    dummyprocessor.h
    metnum.h
    processor.h
    predict3d.h
)

set(SOURCES
    floatingpanelcommand.cpp
    canvassizepopup.cpp
    commandbar.cpp
    commandbarpopup.cpp
    history.cpp
    loadfoldercommand.cpp
    loadfolderpopup.cpp
    main.cpp
    mainwindow.cpp
    matchline.cpp
    menubar.cpp
    menubarpopup.cpp
    mergecolumns.cpp
    moviegenerator.cpp
    pane.cpp
    previewer.cpp
    previewfxmanager.cpp
    ruler.cpp
    tapp.cpp
    toolbar.cpp
    batchserversviewer.cpp
    castviewer.cpp
    exportlevelpopup.cpp
    exportscenepopup.cpp
    colormodelviewer.cpp
    exportpanel.cpp
    filebrowser.cpp
    filebrowserversioncontrol.cpp
    filmstrip.cpp
    matchlinecommand.cpp
    messagepanel.cpp
    tasksviewer.cpp
    testpanel.cpp
    tpanels.cpp
    xshcellmover.cpp
    xshcellviewer.cpp
    xshcolumnviewer.cpp
    xsheetdragtool.cpp
    xsheetviewer.cpp
    xshnoteviewer.cpp
    xshrowviewer.cpp
    xshtoolbar.cpp
    sceneviewer.cpp
    sceneviewerevents.cpp
    viewerdraw.cpp
    viewerpane.cpp
    castselection.cpp
    cellselection.cpp
    columnselection.cpp
    fileselection.cpp
    filmstripselection.cpp
    keyframeselection.cpp
    addfilmstripframespopup.cpp
    camerasettingspopup.cpp
    convertpopup.cpp
    duplicatepopup.cpp
    dvdirtreeview.cpp
    filebrowserpopup.cpp
    formatsettingspopups.cpp
    fxparameditorpopup.cpp
    histogrampopup.cpp
    insertfxpopup.cpp
    layerfooterpanel.cpp
    layerheaderpanel.cpp
    levelcreatepopup.cpp
    levelsettingspopup.cpp
    lipsyncpopup.cpp
    magpiefileimportpopup.cpp
    outputsettingspopup.cpp
    overwritepopup.cpp
    pltgizmopopup.cpp
    preferencespopup.cpp
    projectpopup.cpp
    psdsettingspopup.cpp
    renumberpopup.cpp
    runscriptcommand.cpp
    savepresetpopup.cpp
    sceneviewercontextmenu.cpp
    scenesettingspopup.cpp
    scriptconsolepanel.cpp
    shortcutpopup.cpp
    soundtrackexport.cpp
    startuppopup.cpp
    subcameramanager.cpp
    kis_tablet_support_win8.cpp
    timestretchpopup.cpp
    trackerpopup.cpp
    vectorguideddrawingpane.cpp
    vectorizerpopup.cpp
    vectorizerswatch.cpp
    versioncontrol.cpp
    versioncontroltimeline.cpp
    versioncontrolxmlreader.cpp
    versioncontrolwidget.cpp
    viewerpopup.cpp
    cellselectioncommand.cpp
    columncommand.cpp
    filmstripcommand.cpp
    iocommand.cpp
    levelcommand.cpp
    print.cpp
    subscenecommand.cpp
    svncleanupdialog.cpp
    svncommitdialog.cpp
    svndeletedialog.cpp
    svnlockdialog.cpp
    svnlockframerangedialog.cpp
    svnrevertdialog.cpp
    svnupdateandlockdialog.cpp
    svnupdatedialog.cpp
    svnpurgedialog.cpp
    vcrcommand.cpp
    xsheetcmd.cpp
    celldata.cpp
    drawingdata.cpp
    filedata.cpp
    imageviewer.cpp
    keyframedata.cpp
    dvitemview.cpp
    dvwidgets.cpp
    flipbook.cpp
    frameheadgadget.cpp
    onionskinmaskgui.cpp
    batches.cpp
    tfarmstuff.cpp
    cellkeyframedata.cpp
    cellkeyframeselection.cpp
    keyframemover.cpp
    cachefxcommand.cpp
    exportlevelcommand.cpp
    adjustlevelspopup.cpp
    adjustthicknesspopup.cpp
    binarizepopup.cpp
    brightnessandcontrastpopup.cpp
    linesfadepopup.cpp
    rendercommand.cpp
    curveio.cpp
    meshifypopup.cpp
    selectionutils.cpp
    antialiaspopup.cpp
    mergecmapped.cpp
    historypane.cpp
    audiorecordingpopup.cpp
    locatorpopup.cpp
    styleshortcutswitchablepanel.cpp
	reframepopup.cpp
	autoinputcellnumberpopup.cpp
	colormodelbehaviorpopup.cpp
	boardsettingspopup.cpp
	separatecolorsswatch.cpp
	separatecolorspopup.cpp
    xdtsio.cpp
	xdtsimportpopup.cpp
# Tracker file
    dummyprocessor.cpp
    metnum.cpp
    ObjectTracker.cpp
    predict3d.cpp
    ../stopmotion/stopmotion.cpp
    ../stopmotion/stopmotioncontroller.cpp
    ../stopmotion/webcam.cpp
    ../stopmotion/jpgconverter.cpp
    ../stopmotion/canon.cpp
    ../stopmotion/stopmotionserial.cpp
    ../stopmotion/stopmotionlight.cpp
	cameracapturelevelcontrol.cpp
    penciltestpopup.cpp
)

add_translation(toonz ${HEADERS} ${SOURCES})

set(OBJCSOURCES filebrowsermodel.cpp)

# Resource file(s)
set(RESOURCES toonz.qrc)

if(BUILD_ENV_MSVC)
    set(RC_FILE toonz.rc)
endif()

qt5_add_resources(SOURCES ${RESOURCES})

if(BUILD_ENV_APPLE)
    #set_source_files_properties(${OBJCSOURCES} PROPERTIES LANGUAGE OBJCXX)
    set_source_files_properties(${OBJCSOURCES} PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

qt5_wrap_cpp(SOURCES ${MOC_HEADERS})

if(BUILD_ENV_MSVC)
    add_executable(Tahoma WIN32 ${HEADERS} ${SOURCES} ${OBJCSOURCES} ${RESOURCES} ${RC_FILE})
else()
    set(MACOSX_BUNDLE_ICON_FILE "Tahoma.icns")
    set_source_files_properties(Tahoma.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(Tahoma MACOSX_BUNDLE Tahoma.icns ${HEADERS} ${SOURCES} ${OBJCSOURCES} ${RESOURCES})
    #add_definitions(
    #    -DTNZBASE_EXPORTS
    #    -DTPARAM_EXPORTS
    #    -DTXSHEET_EXPORTS
    #    -DTFX_EXPORTS
    #    -DTAPPTOOLS_EXPORTS
    #    -DTTEST_EXPORTS
    #)
endif()

if(BUILD_TARGET_WIN)
    add_definitions(
        -DUNICODE
        -D_UNICODE
    )
endif()

message("subdir: toonz")
message("Bin: " ${CMAKE_CURRENT_BINARY_DIR})
#message("Lib: " ${LIBRARY_OUTPUT_PATH})
#message("Exe: " ${EXECUTABLE_OUTPUT_PATH})
#message("Sources:" ${SOURCES})

include_directories(
    ../../sources/toonzfarm/include
    ../../sources/toonzqt
    ../../sources/stopmotion
)

if (WITH_CANON)
    add_definitions(-DWITH_CANON)
endif()

if(BUILD_ENV_APPLE)
    include_directories(
       ../../sources/mousedragfilter
       ${TURBOJPEG_INCLUDE_DIRS}
    )
endif()



if(BUILD_TARGET_WIN)
    if(PLATFORM EQUAL 32)
        include_directories(
            SYSTEM
            ${SDKROOT}/quicktime/QT73SDK/CIncludes
        )
    endif()
endif()

if(BUILD_ENV_MSVC AND WITH_CANON)
    target_link_libraries(Tahoma
        Qt5::WinMain Qt5::Core Qt5::Gui Qt5::Network Qt5::OpenGL Qt5::Svg Qt5::Xml
        Qt5::Script Qt5::Widgets Qt5::PrintSupport Qt5::Multimedia Qt5::SerialPort
        ${GL_LIB} ${GLUT_LIB} 
        ${CANON_LIB} ${TURBOJPEG_LIB} ${OpenCV_LIBS} strmiids
        tnzcore tnzbase toonzlib colorfx tnzext image sound toonzqt tnztools tnzstdfx tfarm
    )
elseif(BUILD_ENV_MSVC)
    target_link_libraries(Tahoma
        Qt5::WinMain Qt5::Core Qt5::Gui Qt5::Network Qt5::OpenGL Qt5::Svg Qt5::Xml
        Qt5::Script Qt5::Widgets Qt5::PrintSupport Qt5::Multimedia Qt5::SerialPort
        ${GL_LIB} ${GLUT_LIB} ${TURBOJPEG_LIB} ${OpenCV_LIBS} strmiids
        tnzcore tnzbase toonzlib colorfx tnzext image sound toonzqt tnztools tnzstdfx tfarm
    )
elseif(BUILD_ENV_APPLE AND WITH_CANON)
    find_library(COCOA_LIB Cocoa)

    _find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase;toonzlib;colorfx;tnzext;image;sound;toonzqt;tnztools")

    # 変なところにライブラリ生成するカスども
    set(EXTRA_LIBS ${EXTRA_LIBS} "$<TARGET_FILE:tnzstdfx>" "$<TARGET_FILE:tfarm>")

    add_dependencies(Tahoma tnzcore tnzbase toonzlib colorfx tnzext image sound toonzqt tnztools tnzstdfx tfarm)

    target_link_libraries(Tahoma
        Qt5::Core Qt5::Gui Qt5::Network Qt5::OpenGL Qt5::Svg Qt5::Xml
        Qt5::Script Qt5::Widgets Qt5::PrintSupport Qt5::Multimedia Qt5::MultimediaWidgets Qt5::SerialPort
        ${GL_LIB} ${GLUT_LIB} ${CANON_LIB} ${TURBOJPEG_LIB} ${OpenCV_LIBS}
        ${COCOA_LIB} ${EXTRA_LIBS} mousedragfilter
    )

elseif(BUILD_ENV_APPLE)
    find_library(COCOA_LIB Cocoa)

    _find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase;toonzlib;colorfx;tnzext;image;sound;toonzqt;tnztools")

    # 変なところにライブラリ生成するカスども
    set(EXTRA_LIBS ${EXTRA_LIBS} "$<TARGET_FILE:tnzstdfx>" "$<TARGET_FILE:tfarm>")

    add_dependencies(Tahoma tnzcore tnzbase toonzlib colorfx tnzext image sound toonzqt tnztools tnzstdfx tfarm)

    target_link_libraries(Tahoma
        Qt5::Core Qt5::Gui Qt5::Network Qt5::OpenGL Qt5::Svg Qt5::Xml
        Qt5::Script Qt5::Widgets Qt5::PrintSupport Qt5::Multimedia Qt5::MultimediaWidgets Qt5::SerialPort
        ${GL_LIB} ${GLUT_LIB} ${TURBOJPEG_LIB} ${OpenCV_LIBS}
        ${COCOA_LIB} ${EXTRA_LIBS} mousedragfilter
    )

elseif(BUILD_ENV_UNIXLIKE)
    _find_toonz_library(EXTRA_LIBS "tnzcore;tnzbase;toonzlib;colorfx;tnzext;image;sound;toonzqt;tnztools")

    # 変なところにライブラリ生成するカスども
    set(EXTRA_LIBS ${EXTRA_LIBS} "$<TARGET_FILE:tnzstdfx>" "$<TARGET_FILE:tfarm>")

    set(EXTRA_LIBS ${EXTRA_LIBS} ${Boost_LIBRARIES} ${OPENBLAS_LIB})

    if(BUILD_TARGET_WIN)
        set(EXTRA_LIBS ${EXTRA_LIBS} Qt5::WinMain -lstrmiids -mwindows)
    endif()

    target_link_libraries(
        Tahoma Qt5::Core Qt5::Gui Qt5::Network Qt5::OpenGL Qt5::Svg Qt5::Xml
        Qt5::Script Qt5::Widgets Qt5::PrintSupport Qt5::Multimedia Qt5::SerialPort
        ${GL_LIB} ${GLUT_LIB} ${GLU_LIB} ${TURBOJPEG_LIB} ${OpenCV_LIBS}
        ${EXTRA_LIBS}
    )
endif()

if(BUILD_ENV_APPLE)
    # CMAKE_RUNTIME_OUTPUT_DIRECTORY should be equivalent to usage on windows despite empty
    # OSX だと CMAKE_RUNTIME_OUTPUT_DIRECTORY が空だが Windows 版と同じ使い方ができるようにしておく
    get_target_property(bin Tahoma LOCATION)
    get_filename_component(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${bin} DIRECTORY)
    # show CMAKE_RUNTIME_OUTPUT_DIRECTORY
    message("CMAKE_RUNTIME_OUTPUT_DIRECTORY:" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

# copy utilty executables onto the directory after build
if(LZODRIVER_FOUND)
    add_custom_command(TARGET Tahoma POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lzocompress> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} DEPENDS lzocompress)
    add_custom_command(TARGET Tahoma POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lzodecompress> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} DEPENDS lzodecompress)
endif()

if(BUILD_ENV_APPLE)
    get_target_property(loc Tahoma MACOSX_BUNDLE_NAME)
    message(" ==> App Bundle: " ${loc})
    message(" ==> Mach-o: " ${bin})
    foreach(lib ${EXTRA_LIBS})
        message(" copy:" ${lib} "==>" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
        add_custom_command(TARGET Tahoma POST_BUILD COMMAND cp ${lib} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    endforeach()

    add_custom_command(TARGET Tahoma POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:tcomposer> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} DEPENDS tcomposer)
    add_custom_command(TARGET Tahoma POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:tconverter> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} DEPENDS tconverter)
    add_custom_command(TARGET Tahoma POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:tfarmcontroller> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} DEPENDS tfarmcontroller)
    add_custom_command(TARGET Tahoma POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:tfarmserver> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} DEPENDS tfarmserver)

    add_custom_command(TARGET Tahoma POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/../Resources)
    add_custom_command(TARGET Tahoma POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/../install/SystemVar.ini ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/../Resources)

    add_custom_command(TARGET Tahoma POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/../../../qss)

    add_custom_command(TARGET Tahoma
        POST_BUILD COMMAND
        ${CMAKE_INSTALL_NAME_TOOL} -add_rpath @executable_path/. ${bin} || true
    )
    add_custom_command(TARGET Tahoma
        POST_BUILD COMMAND
        ${CMAKE_INSTALL_NAME_TOOL} -add_rpath ${CMAKE_INSTALL_RPATH} ${bin} || true
    )

elseif(BUILD_ENV_UNIXLIKE AND BUILD_TARGET_WIN)
    # Setup files on Windows platform while cross compilation.
    install(
        TARGETS
            Tahoma
        DESTINATION bin
    )

    install(
        PROGRAMS
            "$<TARGET_FILE:tcomposer>"
            "$<TARGET_FILE:tconverter>"
            "$<TARGET_FILE:tfarmcontroller>"
            "$<TARGET_FILE:tfarmserver>"
        DESTINATION bin
    )

    if(LZODRIVER_FOUND)
        install(
            PROGRAMS
                "$<TARGET_FILE:lzocompress>"
                "$<TARGET_FILE:lzodecompress>"
            DESTINATION bin
        )
    endif()

    install(
        FILES
            "$<TARGET_FILE:tnzcore>"
            "$<TARGET_FILE:tnzbase>"
            "$<TARGET_FILE:toonzlib>"
            "$<TARGET_FILE:colorfx>"
            "$<TARGET_FILE:tnzext>"
            "$<TARGET_FILE:image>"
            "$<TARGET_FILE:sound>"
            "$<TARGET_FILE:toonzqt>"
            "$<TARGET_FILE:tnztools>"
            "$<TARGET_FILE:tnzstdfx>"
            "$<TARGET_FILE:tfarm>"
        DESTINATION bin
    )
    
    install(
        DIRECTORY
            ${CMAKE_SOURCE_DIR}/../../stuff
        DESTINATION share/tahoma
    )
elseif(BUILD_ENV_UNIXLIKE AND NOT BUILD_TARGET_WIN)
    # Setup files on Linux/Unix platform.
    # not essential since the binary can run in-place,
    # but useful for package managers.
    #
    # Note that its assumed the destination can be either
    # - /opt/tahoma
    # - /usr
    install(
        TARGETS
            Tahoma
        DESTINATION bin
    )

    # Shell script that copies files to the home directory as needed
    # and sets the library preload path.
    file(WRITE ${CMAKE_BINARY_DIR}/bin/tahoma
"#!/bin/sh
TAHOMA_BASE=\$(dirname \"\$0\")/..

if [ ! -d \$HOME/.config/Tahoma ]; then
    mkdir -p \$HOME/.config/Tahoma
fi

if [ ! -d \$HOME/.config/Tahoma/stuff ]; then
    cp -r \$TAHOMA_BASE/share/tahoma/stuff \$HOME/.config/Tahoma
fi

if [ ! -d \$HOME/.config/Tahoma/stuff/projects/library ]; then
    mkdir -p \$HOME/.config/Tahoma/stuff/projects/library
fi

if [ ! -d \$HOME/.config/Tahoma/stuff/projects/fxs ]; then
    mkdir -p \$HOME/.config/Tahoma/stuff/projects/fxs
fi

if [ ! -e \$HOME/.config/Tahoma/SystemVar.ini ]; then
    cat << EOF > $HOME/.config/Tahoma/SystemVar.ini
[General]
TAHOMAROOT=\"\$HOME/.config/Tahoma/stuff\"
TahomaPROFILES=\"\$HOME/.config/Tahoma/stuff/profiles\"
TOONZCACHEROOT=\"\$HOME/.config/Tahoma/stuff/cache\"
TOONZCONFIG=\"\$HOME/.config/Tahoma/stuff/config\"
TOONZFXPRESETS=\"\$HOME/.config/Tahoma/stuff/fxs\"
TOONZLIBRARY=\"\$HOME/.config/Tahoma/stuff/library\"
TOONZPROFILES=\"\$HOME/.config/Tahoma/stuff/profiles\"
TOONZPROJECTS=\"$HOME/.config/Tahoma/stuff/projects\"
TOONZROOT=\"\$HOME/.config/Tahoma/stuff\"
TOONZSTUDIOPALETTE=\"\$HOME/.config/Tahoma/stuff/studiopalette\"
EOF
fi

export LD_LIBRARY_PATH=\${TAHOMA_BASE}/lib/tahoma:\${LD_LIBRARY_PATH}

exec \$TAHOMA_BASE/bin/Tahoma \"\$@\"
")
    # only needed for executing without installing
    execute_process(COMMAND chmod +x ${CMAKE_BINARY_DIR}/bin/tahoma)

    install(
        PROGRAMS
            ${CMAKE_BINARY_DIR}/bin/tahoma
            "$<TARGET_FILE:tcomposer>"
            "$<TARGET_FILE:tconverter>"
            "$<TARGET_FILE:tfarmcontroller>"
            "$<TARGET_FILE:tfarmserver>"
        DESTINATION bin
    )

    if(LZODRIVER_FOUND)
        install(
            PROGRAMS
                "$<TARGET_FILE:lzocompress>"
                "$<TARGET_FILE:lzodecompress>"
            DESTINATION bin
        )
    endif()

    install(
        FILES
            "$<TARGET_FILE:tnzcore>"
            "$<TARGET_FILE:tnzbase>"
            "$<TARGET_FILE:toonzlib>"
            "$<TARGET_FILE:colorfx>"
            "$<TARGET_FILE:tnzext>"
            "$<TARGET_FILE:image>"
            "$<TARGET_FILE:sound>"
            "$<TARGET_FILE:toonzqt>"
            "$<TARGET_FILE:tnztools>"
            "$<TARGET_FILE:tnzstdfx>"
            "$<TARGET_FILE:tfarm>"
        DESTINATION lib/tahoma
    )

    install(
        DIRECTORY
            ${CMAKE_SOURCE_DIR}/../../stuff
        DESTINATION share/tahoma
    )

endif()
