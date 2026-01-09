add_subdirectory(unit)

if(COVERAGE)
  set(COVERAGE_OUT "${CMAKE_BINARY_DIR}/coverage")

  set(IGNORE_REGEX "'build/|stb_.*|test/unit/|tilem/emu|vendor/'")

  set(OBJECTS "$<TARGET_FILE:unit-tests>")

  add_custom_target(
    make-coverage
    COMMAND rm -rf "${COVERAGE_OUT}"
    COMMAND mkdir "${COVERAGE_OUT}"
    COMMAND env LLVM_PROFILE_FILE="${COVERAGE_OUT}/%m.profraw" ctest
            --output-on-failure
    COMMAND llvm-profdata merge -o merged.profdata "${COVERAGE_OUT}"
    COMMAND
      llvm-cov show --format html -output-dir "${COVERAGE_OUT}"
      -ignore-filename-regex "${IGNORE_REGEX}" --instr-profile merged.profdata
      -Xdemangler c++filt -Xdemangler -n ${OBJECTS}
    COMMAND
      llvm-cov export --format lcov -ignore-filename-regex "${IGNORE_REGEX}"
      --instr-profile merged.profdata -Xdemangler c++filt -Xdemangler -n
      ${OBJECTS} > "${CMAKE_BINARY_DIR}/report.lcov"
    DEPENDS unit-tests)
endif()
