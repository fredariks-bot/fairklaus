# CMake generated Testfile for 
# Source directory: /workspace/github__fredariks-bot__fairklaus/tests
# Build directory: /workspace/github__fredariks-bot__fairklaus/build-asan/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[dsp]=] "/workspace/github__fredariks-bot__fairklaus/build-asan/tests/test_dsp")
set_tests_properties([=[dsp]=] PROPERTIES  _BACKTRACE_TRIPLES "/workspace/github__fredariks-bot__fairklaus/tests/CMakeLists.txt;5;add_test;/workspace/github__fredariks-bot__fairklaus/tests/CMakeLists.txt;0;")
add_test([=[host_sim]=] "/workspace/github__fredariks-bot__fairklaus/build-asan/tests/host_sim" "/workspace/github__fredariks-bot__fairklaus/build-asan/VST3/Debug/PunishChild 670.vst3")
set_tests_properties([=[host_sim]=] PROPERTIES  _BACKTRACE_TRIPLES "/workspace/github__fredariks-bot__fairklaus/tests/CMakeLists.txt;15;add_test;/workspace/github__fredariks-bot__fairklaus/tests/CMakeLists.txt;0;")
