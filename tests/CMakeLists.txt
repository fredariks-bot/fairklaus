#--- DSP Unit-Tests (ohne SDK) ----------------------------------------------
add_executable(test_dsp test_dsp.cpp)
target_include_directories(test_dsp PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_features(test_dsp PRIVATE cxx_std_17)
add_test(NAME dsp COMMAND test_dsp)

#--- Host-Simulation: lädt das gebaute .vst3 und prozessiert Offline ----------
# (Linux/macOS only: nutzt dlopen/LoadLibrary; auf Windows nur DSP-Test)
if(NOT WIN32)
add_executable(host_sim host_sim.cpp)
target_include_directories(host_sim PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/vst3sdk)
target_compile_features(host_sim PRIVATE cxx_std_17)
target_link_libraries(host_sim PRIVATE sdk ${CMAKE_DL_LIBS})
add_dependencies(host_sim punishchild670)
add_test(NAME host_sim COMMAND host_sim $<TARGET_PROPERTY:punishchild670,SMTG_PLUGIN_PACKAGE_PATH>)
endif()

#--- GUI-Smoke-Test (nur Linux mit Editor-Build; CI fuehrt ihn via xvfb-run aus)
if(FAIRKLAUS_EDITOR_AVAILABLE AND NOT WIN32 AND X11_FOUND)
    add_executable(gui_smoke gui_smoke.cpp)
    target_include_directories(gui_smoke PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/vst3sdk ${CMAKE_SOURCE_DIR}/vst3sdk/vstgui4)
    target_compile_features(gui_smoke PRIVATE cxx_std_17)
    # WICHTIG: kein vstgui statisch linken — sonst doppelte Platform-Singletons (Segfault).
    # Alle VSTGUI-Aufrufe laufen ueber die Vtable der .so im Plugin.
    target_link_libraries(gui_smoke PRIVATE sdk ${CMAKE_DL_LIBS} ${X11_LIBRARIES})
    add_dependencies(gui_smoke punishchild670)
    find_program(XVFB_RUN_EXEC xvfb-run)
    if(XVFB_RUN_EXEC)
        add_test(NAME gui_smoke COMMAND ${XVFB_RUN_EXEC} -a $<TARGET_FILE:gui_smoke> $<TARGET_PROPERTY:punishchild670,SMTG_PLUGIN_PACKAGE_PATH>)
    endif()
endif()
