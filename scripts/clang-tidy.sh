#! /bin/bash

BIN=$1 && shift
PROJECT_ROOT=$1 && shift
MESON_ROOT=$1 && shift

# Execute in a different directory to ensure we don't mess with the meson config
TIDY_DIR=${PROJECT_ROOT}/build-tidy

mkdir -p ${TIDY_DIR}
cp ${MESON_ROOT}/compile_commands.json ${TIDY_DIR}

# Replace meson commands clang does not understand
sed -i 's/-pipe//g' ${TIDY_DIR}/compile_commands.json

$BIN -warnings-as-errors=* -p ${TIDY_DIR} $@
