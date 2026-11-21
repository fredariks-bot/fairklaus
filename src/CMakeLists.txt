#--- PunishChild 670 VST3 target --------------------------------------------
set(SDK_ROOT ${CMAKE_SOURCE_DIR}/vst3sdk)
set(public_sdk_SOURCE_DIR ${SDK_ROOT}/public.sdk)
set(pluginterfaces_SOURCE_DIR ${SDK_ROOT}/pluginterfaces)

list(APPEND CMAKE_MODULE_PATH ${SDK_ROOT}/cmake/modules)
include(SMTG_VST3_SDK)

set(fairklaus_sources
    ${SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PunishChild670.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PunishChild670.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Parameters.h
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/Wdf.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/Triode6386.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/WdfTransformer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/TubeStage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/SignalAmplifier.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/Sidechain.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/TimeConstantNetwork.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/Fairchild670.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/Oversampling.h
)

if(FAIRKLAUS_EDITOR_AVAILABLE)
    list(APPEND fairklaus_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/editor/Editor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/editor/Editor.cpp
    )
endif()

smtg_add_vst3plugin(punishchild670
    PACKAGE_NAME "PunishChild 670"
    SOURCES_LIST ${fairklaus_sources}
)

target_link_libraries(punishchild670 PRIVATE sdk)
target_include_directories(punishchild670 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(FAIRKLAUS_EDITOR_AVAILABLE)
    target_compile_definitions(punishchild670 PRIVATE FAIRKLAUS_WITH_EDITOR=1)
    target_link_libraries(punishchild670 PRIVATE vstgui_support)
endif()
