cmake_minimum_required(VERSION 4.1)
project(allocators LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ALLOCATORS_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer in Debug." ON)
option(ALLOCATORS_ENABLE_CLANG_TIDY "Run clang-tidy while building when it is available." OFF)

set(ALLOCATORS_CORE_SOURCES
		allocator.c
		system_allocator.c
		stack_allocator.c
)

add_executable(allocators main.c ${ALLOCATORS_CORE_SOURCES})
add_executable(allocators_leakcheck main.c ${ALLOCATORS_CORE_SOURCES})
add_executable(allocators_tests tests/test_allocator_suite.c ${ALLOCATORS_CORE_SOURCES})
add_executable(allocators_tests_sanitized tests/test_allocator_suite.c ${ALLOCATORS_CORE_SOURCES})
add_executable(allocators_tests_leakcheck tests/test_allocator_suite.c ${ALLOCATORS_CORE_SOURCES})
add_executable(allocators_misuse_tests tests/test_allocator_misuse.c ${ALLOCATORS_CORE_SOURCES})

foreach (target allocators allocators_leakcheck allocators_tests allocators_tests_sanitized allocators_tests_leakcheck allocators_misuse_tests)
	target_include_directories(${target} PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}
			${CMAKE_CURRENT_SOURCE_DIR}/tests
	)
endforeach ()

foreach (target allocators allocators_leakcheck allocators_tests allocators_tests_sanitized allocators_tests_leakcheck allocators_misuse_tests)
	target_compile_options(${target} PRIVATE
			-Wall
			-Wextra
			-Wpedantic
			-Wshadow
			-Wstrict-prototypes
			$<$<CONFIG:Debug>:-O0>
			$<$<CONFIG:Debug>:-g3>
			$<$<CONFIG:Debug>:-fno-omit-frame-pointer>
			$<$<CONFIG:Debug>:-fno-optimize-sibling-calls>
			$<$<CONFIG:Debug>:-fno-inline>
	)
endforeach ()

set(ALLOCATORS_SANITIZER_ENV
		"ASAN_OPTIONS=abort_on_error=1:strict_string_checks=1"
		"UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
)

set(_allocators_supports_sanitizers FALSE)
if (ALLOCATORS_ENABLE_SANITIZERS)
	if (CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
		set(_allocators_supports_sanitizers TRUE)
		foreach (target allocators allocators_tests_sanitized)
			target_compile_options(${target} PRIVATE
				$<$<CONFIG:Debug>:-fsanitize=address,undefined>
			)
			target_link_options(${target} PRIVATE
				$<$<CONFIG:Debug>:-fsanitize=address,undefined>
			)
		endforeach ()
	else ()
		message(WARNING "Sanitizers requested, but compiler '${CMAKE_C_COMPILER_ID}' is not configured for this project.")
	endif ()
endif ()

find_program(CLANG_TIDY_PROGRAM NAMES clang-tidy)
if (ALLOCATORS_ENABLE_CLANG_TIDY)
	if (CLANG_TIDY_PROGRAM)
		set_target_properties(allocators PROPERTIES
				C_CLANG_TIDY "${CLANG_TIDY_PROGRAM};-p=${CMAKE_BINARY_DIR};--checks=clang-analyzer-*,bugprone-*"
		)
	else ()
		message(WARNING "ALLOCATORS_ENABLE_CLANG_TIDY is ON, but clang-tidy was not found.")
	endif ()
endif ()

include(CTest)

if (BUILD_TESTING)
	add_test(NAME allocators.unit COMMAND allocators_tests)

	if (_allocators_supports_sanitizers)
		add_test(NAME allocators.sanitizers COMMAND allocators_tests_sanitized)
		set_tests_properties(allocators.sanitizers PROPERTIES
				ENVIRONMENT "${ALLOCATORS_SANITIZER_ENV}"
		)
	endif ()

	add_test(
			NAME allocators.misuse.current_without_push
			COMMAND ${CMAKE_COMMAND}
			-DEXPECTED_COMMAND=$<TARGET_FILE:allocators_misuse_tests>
			-DEXPECTED_ARGS=current_without_push
			-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_failure.cmake
	)

	add_test(
			NAME allocators.misuse.zero_size_allocate
			COMMAND ${CMAKE_COMMAND}
			-DEXPECTED_COMMAND=$<TARGET_FILE:allocators_misuse_tests>
			-DEXPECTED_ARGS=zero_size_allocate
			-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_failure.cmake
	)

	add_test(
			NAME allocators.misuse.stack_reallocate_unsupported
			COMMAND ${CMAKE_COMMAND}
			-DEXPECTED_COMMAND=$<TARGET_FILE:allocators_misuse_tests>
			-DEXPECTED_ARGS=stack_reallocate_unsupported
			-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_failure.cmake
	)

	add_test(
			NAME allocators.misuse.stack_deallocate_unsupported
			COMMAND ${CMAKE_COMMAND}
			-DEXPECTED_COMMAND=$<TARGET_FILE:allocators_misuse_tests>
			-DEXPECTED_ARGS=stack_deallocate_unsupported
			-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_failure.cmake
	)

	if (APPLE)
		find_program(LEAKS_PROGRAM NAMES leaks)
		if (LEAKS_PROGRAM)
			add_test(
					NAME allocators.leaks
					COMMAND ${LEAKS_PROGRAM} --atExit -- $<TARGET_FILE:allocators_tests_leakcheck>
			)
		else ()
			message(WARNING "The macOS 'leaks' tool was not found; leak test target will be unavailable.")
		endif ()
	endif ()

	add_custom_target(analyze
			COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG> -R "^allocators\\."
			DEPENDS allocators allocators_leakcheck allocators_tests allocators_tests_sanitized allocators_tests_leakcheck allocators_misuse_tests
			USES_TERMINAL
			COMMENT "Running sanitizer and leak analysis"
	)
endif ()

