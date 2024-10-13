project('yael', 'cpp')

prefix_library_path=[get_option('prefix')+'/lib', get_option('prefix')+'/lib/x86_64-linux-gnu', '/usr/local/lib', '/usr/local/lib/x86_64-linux-gnu']

compile_args = ['-Wall', '-std=c++17', '-Wextra']

if get_option('buildtype') == 'debug'
    compile_args = compile_args + ['-DDEBUG']
else
    compile_args = compile_args + ['-DNDEBUG']
endif

incdir = include_directories('include')
botan_dir = include_directories(get_option('botan_dir'))

cpp = meson.get_compiler('cpp')

gtest_dep = dependency('gtest')
gflags_dep = dependency('gflags')
thread_dep = dependency('threads')
log_dep = dependency('glog')
botan_dep = cpp.find_library('botan-2', dirs: prefix_library_path)

subdir('src')

yael = shared_library('yael', yael_cpp_files, include_directories:[incdir, botan_dir], dependencies: [log_dep, thread_dep, botan_dep], install: true, cpp_args: compile_args, link_args: ['-lrt'])
yael_dep = declare_dependency(link_with: yael)

many_clients = executable('message-test', ['test/messages.cpp'],link_with: yael, dependencies: [log_dep, thread_dep], include_directories: [incdir], cpp_args: compile_args)

churn = executable('churn-test', ['test/churn.cpp'],link_with: yael, dependencies: [log_dep, thread_dep], include_directories: [incdir], cpp_args: compile_args)

subdir('test/unit')

unit_tests_bin = executable('yael-test', test_cpp_files, link_with: yael, dependencies: [log_dep, thread_dep, gtest_dep, gflags_dep], include_directories: [incdir], cpp_args: compile_args)

inc_dir = 'include/yael'
install_subdir(inc_dir, install_dir : 'include')

yael_headers = files(
    join_paths(inc_dir, 'DelayedNetworkSocketListener.h'),
    join_paths(inc_dir, 'EventLoop.h'),
    join_paths(inc_dir, 'yael.h'),
    join_paths(inc_dir, 'NetworkSocketListener.h'),
    join_paths(inc_dir, 'EventListener.h'),
    join_paths(inc_dir, 'TimeEventListener.h'),
    join_paths(inc_dir, 'network/Address.h'),
    join_paths(inc_dir, 'network/buffer.h'),
    join_paths(inc_dir, 'network/MessageSlicer.h'),
    join_paths(inc_dir, 'network/Socket.h'),
    join_paths(inc_dir, 'network/TcpSocket.h'),
    join_paths(inc_dir, 'network/TlsSocket.h'),
)

clangtidy = find_program('clang-tidy', required: false)

if clangtidy.found()
    run_target(
        'lint',
        command: [
            'scripts/clang-tidy.sh',
            clangtidy.path(),
            meson.source_root(),
            meson.build_root()
        ] + yael_cpp_files + yael_headers, # + test_cpp_files,
    depends: [yael])
endif

run_target(
    'unit-tests',
    command: unit_tests_bin,
    depends: [unit_tests_bin])

run_target(
    'integration-tests',
    command: [
        'scripts/integration-tests.sh',
    ],
    depends: [churn, many_clients])
