#! /bin/bash

# Pick any flags you like here
CHECKS='-hicpp-*,-readability-implicit-bool-conversion,-cppcoreguidelines-*,-clang-diagnostic*,-llvm-include-order,-bugprone*'

BIN=$1
PROJECT_ROOT=$2
MESON_ROOT=$3
FILES=$4

# Execute in a different directory to ensure we don't mess with the meson config
TIDY_DIR=${PROJECT_ROOT}/build-tidy

mkdir -p ${TIDY_DIR}
cp  ${MESON_ROOT}/compile_commands.json ${TIDY_DIR}

# Replace meson commands clang does not understand
sed -i 's/-pipe//g' ${TIDY_DIR}/compile_commands.json

$BIN -checks=${CHECKS} -warnings-as-errors=* -p ${TIDY_DIR} ${FILES}
