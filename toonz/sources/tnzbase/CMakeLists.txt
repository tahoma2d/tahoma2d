set(MOC_HEADERS
    ../include/tcacheresourcepool.h
    ../common/tfx/trendererP.h
)

set(HEADERS ${MOC_HEADERS}
    ../include/permissionsmanager.h
    ../include/tcubicbezier.h
    ../include/tdoublekeyframe.h
    ../include/tdoubleparam.h
    ../common/tparam/tdoubleparamfile.h
    ../include/tdoubleparamrelayproperty.h
    ../include/tfxparam.h
    ../include/tnotanimatableparam.h
    ../include/tparam.h
    ../include/tparamchange.h
    ../include/tparamcontainer.h
    ../include/tparamset.h
    ../include/tspectrumparam.h
    ../include/ttonecurveparam.h
    ../include/tcolumnset.h
    ../include/tbasefx.h
    ../include/texternfx.h
    ../include/tfx.h
    ../include/tfxattributes.h
    ../include/tcacheresource.h
    ../include/tpassivecachemanager.h
    ../include/tpredictivecachemanager.h
    ../include/tfxcachemanager.h
    ../include/tfxutil.h
    ../include/tmacrofx.h
    ../include/trenderer.h
    ../include/trenderresourcemanager.h
    ../include/ttzpimagefx.h
    ../include/tcli.h
    ../include/tcolorutils.h
    ../include/tparamundo.h
    ../include/ttest.h
    ../include/texpression.h
    ../include/tgrammar.h
    ../include/tparser.h
    ../include/ttokenizer.h
    ../include/tunit.h
    ../include/tzeraryfx.h
)

set(SOURCES
    permissionsmanager.cpp
    stringtable.cpp
    ../common/tparam/tcubicbezier.cpp
    ../common/tparam/tdoublekeyframe.cpp
    ../common/tparam/tdoubleparam.cpp
    ../common/tparam/tdoubleparamfile.cpp
    ../common/tparam/tdoubleparamrelayproperty.cpp
    ../common/tparam/tnotanimatableparam.cpp
    ../common/tparam/tparam.cpp
    ../common/tparam/tparamcontainer.cpp
    ../common/tparam/tparamset.cpp
    ../common/tparam/tpixelparam.cpp
    ../common/tparam/tpointparam.cpp
    ../common/tparam/trangeparam.cpp
    ../common/tparam/tspectrumparam.cpp
    ../common/tparam/ttonecurveparam.cpp
    ../common/txsheet/tcolumnset.cpp
    ../common/tfx/binaryFx.cpp
    texternfx.cpp
    ../common/tfx/tfx.cpp
    ../common/tfx/tfxcachemanager.cpp
    ../common/tfx/tcacheresource.cpp
    ../common/tfx/tcacheresourcepool.cpp
    ../common/tfx/tpassivecachemanager.cpp
    ../common/tfx/tpredictivecachemanager.cpp
    tfxattributes.cpp
    tfxutil.cpp
    ../common/tfx/tmacrofx.cpp
    trasterfx.cpp
    ../common/tfx/trenderer.cpp
    ../common/tfx/trenderresourcemanager.cpp
    ../common/tfx/ttzpimagefx.cpp
    ../common/tfx/unaryFx.cpp
    ../common/tfx/zeraryFx.cpp
    ../common/tapptools/tcli.cpp
    ../common/tapptools/tcolorutils.cpp
    ../common/tapptools/tparamundo.cpp
    ../common/ttest/ttest.cpp
    ../common/expressions/texpression.cpp
    ../common/expressions/tgrammar.cpp
    ../common/expressions/tparser.cpp
    ../common/expressions/ttokenizer.cpp
    ../common/tunit/tunit.cpp
)

if(BUILD_ENV_APPLE)
    #set_source_files_properties(${OBJCSOURCES} PROPERTIES LANGUAGE OBJCXX)
    set_source_files_properties(${OBJCSOURCES} PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

qt5_wrap_cpp(SOURCES ${MOC_HEADERS})

add_library(tnzbase SHARED ${HEADERS} ${SOURCES} ${OBJCSOURCES})

if(BUILD_ENV_APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-install_name,@rpath/libtnzbase.dylib")
    add_dependencies(tnzbase tnzcore)
endif()

add_definitions(
    -DTNZBASE_EXPORTS
    -DTPARAM_EXPORTS
    -DTXSHEET_EXPORTS
    -DTFX_EXPORTS
    -DTAPPTOOLS_EXPORTS
    -DTTEST_EXPORTS
)

message("subdir: tnzbase")
message("Bin: " ${CMAKE_CURRENT_BINARY_DIR})
#message("Lib: " ${LIBRARY_OUTPUT_PATH})
#message("Exe: " ${EXECUTABLE_OUTPUT_PATH})
#message("Sources:" ${SOURCES})

include_directories(
    SYSTEM
    ../common/flash
    ${SDKROOT}/libusb/libusb-1.0.9/include
)

if(BUILD_ENV_MSVC)
    set(EXTRA_LIBS
        tnzcore
    )
elseif(BUILD_ENV_APPLE)
    find_library(IOKIT_LIB IOKit)
    find_library(COCOA_LIB Cocoa)

    set(EXTRA_LIBS
        "$<TARGET_FILE:tnzcore>"
        ${USB_LIB}
        ${IOKIT_LIB}
        ${COCOA_LIB}
    )
elseif(BUILD_ENV_UNIXLIKE)
    _find_toonz_library(EXTRA_LIBS "tnzcore")
endif()

target_link_libraries(tnzbase Qt5::Core Qt5::Gui ${EXTRA_LIBS})
