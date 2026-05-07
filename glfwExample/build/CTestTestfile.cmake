# CMake generated Testfile for 
# Source directory: F:/glfwExamle/glfwExample/glfwExample
# Build directory: F:/glfwExamle/glfwExample/glfwExample/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(mf_recording_smoke "F:/glfwExamle/glfwExample/glfwExample/build/Debug/mf_recording_smoke.exe")
  set_tests_properties(mf_recording_smoke PROPERTIES  _BACKTRACE_TRIPLES "F:/glfwExamle/glfwExample/glfwExample/CMakeLists.txt;199;add_test;F:/glfwExamle/glfwExample/glfwExample/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(mf_recording_smoke "F:/glfwExamle/glfwExample/glfwExample/build/Release/mf_recording_smoke.exe")
  set_tests_properties(mf_recording_smoke PROPERTIES  _BACKTRACE_TRIPLES "F:/glfwExamle/glfwExample/glfwExample/CMakeLists.txt;199;add_test;F:/glfwExamle/glfwExample/glfwExample/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(mf_recording_smoke "F:/glfwExamle/glfwExample/glfwExample/build/MinSizeRel/mf_recording_smoke.exe")
  set_tests_properties(mf_recording_smoke PROPERTIES  _BACKTRACE_TRIPLES "F:/glfwExamle/glfwExample/glfwExample/CMakeLists.txt;199;add_test;F:/glfwExamle/glfwExample/glfwExample/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(mf_recording_smoke "F:/glfwExamle/glfwExample/glfwExample/build/RelWithDebInfo/mf_recording_smoke.exe")
  set_tests_properties(mf_recording_smoke PROPERTIES  _BACKTRACE_TRIPLES "F:/glfwExamle/glfwExample/glfwExample/CMakeLists.txt;199;add_test;F:/glfwExamle/glfwExample/glfwExample/CMakeLists.txt;0;")
else()
  add_test(mf_recording_smoke NOT_AVAILABLE)
endif()
