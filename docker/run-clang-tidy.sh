#!/bin/bash

set -e
set -x
export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export PYTHONPATH=${HOME}/local/lib/python3.6/site-packages

cd ..

export TIDY=clang-tidy-6.0

function clang_tidy_works {
    if ! hash $TIDY 2> /dev/null; then
        return 1
    fi

    return 0
}

if ! clang_tidy_works; then
    echo "Clang Tidy isn't available!"
    exit 0
fi

export CC=clang-6.0
export CXX=clang++-6.0

output_dir=clang-tidy
if [[ "$1" != "tidy-only" ]]; then
    [[ -e "$output_dir" ]] && rm -r "$output_dir"
    mkdir "$output_dir"
    cd "$output_dir" || exit 1
    if meson .. --prefix=$HOME/local --buildtype debug; then
        # This has to be done with else because with '!' it won't work on Mac OS X
        true
    else
        exit $? #abort on failure
    fi

    
    ninja
    ninja

    sed -i 's/-pipe//g' compile_commands.json
else
    cd "$output_dir" || exit 1
fi

# Run checks on all files in the source directory except the ones written by Intel
find ../src -iname *.cpp ! -iname sgx_* ! -name 'ias_ra.cpp' | xargs $TIDY -p='.' -checks='-clang-diagnostic-*,modernize-*,-modernize-raw-string-literal,-modernize-pass-by-value,-modernize-use-nullptr,-cppcoreguidelines-pro-type-vararg,-cppcoreguidelines-pro-type-reinterpret-cast,-cppcoreguidelines-pro-bounds-array-to-pointer-decay,-cppcoreguidelines-pro-bounds-constant-array-index,-cppcoreguidelines-pro-bounds-pointer-arithmetic,-cppcoreguidelines-pro-type-const-cast,-cppcoreguidelines-no-malloc,-cppcoreguidelines-pro-type-member-init,readability-*,-readability-else-after-return,-readability-implicit-bool-cast,llvm-header-guard,misc-noexcept-move-constructor,misc-move-const-arg' -warnings-as-errors='*,-readability-inconsistent-declaration-parameter-name' -header-filter='(.*lib/.*\\.h(pp)?|.*test/.*\\.h(pp)?)'
