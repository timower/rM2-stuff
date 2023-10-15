add_subdirectory(unit)

if (COVERAGE)
  set(COVERAGE_OUT "${CMAKE_BINARY_DIR}/coverage")
  add_custom_target(make-coverage
    COMMAND rm -rf "${COVERAGE_OUT}"
    COMMAND mkdir "${COVERAGE_OUT}"
    COMMAND ctest
    COMMAND llvm-profdata merge -o merged.profdata
                   "${CMAKE_BINARY_DIR}/test/unit/default.profraw"
    COMMAND llvm-cov show
               --format html -output-dir "${COVERAGE_OUT}"
               -ignore-filename-regex "'build/|stb_.*|test/unit/'"
               --instr-profile merged.profdata
               -Xdemangler c++filt -Xdemangler -n
               "${CMAKE_BINARY_DIR}/test/unit/unit-tests"
    COMMAND llvm-cov export
               --format lcov
               -ignore-filename-regex "'build/|stb_.*|test/unit/'"
               --instr-profile merged.profdata
               -Xdemangler c++filt -Xdemangler -n
               "${CMAKE_BINARY_DIR}/test/unit/unit-tests" > "${CMAKE_BINARY_DIR}/report.lcov"

    DEPENDS unit-tests
  )
endif()
