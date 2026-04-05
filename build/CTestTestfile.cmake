# CMake generated Testfile for 
# Source directory: /Users/tedkleinbergman/dev/allocators
# Build directory: /Users/tedkleinbergman/dev/allocators/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[allocators.sanitizers]=] "/Users/tedkleinbergman/dev/allocators/build/allocators")
set_tests_properties([=[allocators.sanitizers]=] PROPERTIES  ENVIRONMENT "ASAN_OPTIONS=abort_on_error=1:strict_string_checks=1;UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1" _BACKTRACE_TRIPLES "/Users/tedkleinbergman/dev/allocators/CMakeLists.txt;72;add_test;/Users/tedkleinbergman/dev/allocators/CMakeLists.txt;0;")
add_test([=[allocators.leaks]=] "/usr/bin/leaks" "--atExit" "--" "/Users/tedkleinbergman/dev/allocators/build/allocators_leakcheck")
set_tests_properties([=[allocators.leaks]=] PROPERTIES  _BACKTRACE_TRIPLES "/Users/tedkleinbergman/dev/allocators/CMakeLists.txt;81;add_test;/Users/tedkleinbergman/dev/allocators/CMakeLists.txt;0;")
