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
add_dependencies(host_sim punishchild660)
add_test(NAME host_sim COMMAND host_sim $<TARGET_PROPERTY:punishchild660,SMTG_PLUGIN_PACKAGE_PATH>)
endif()
