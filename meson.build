project('yael', 'cpp')

compile_args = ['-Wall', '-std=c++17', '-Werror']

incdir = include_directories('include')
botan_dir = include_directories('/usr/include/botan-2')


cpp = meson.get_compiler('cpp')

gtest_dep = dependency('gtest')
gflags_dep = cpp.find_library('gflags')
thread_dep = dependency('threads')
log_dep = cpp.find_library('glog')
botan_dep = cpp.find_library('botan-2')

subdir('src')

yael = shared_library('yael', yael_cpp_files, include_directories:[incdir, botan_dir], dependencies: [log_dep, thread_dep, botan_dep], install: true, cpp_args: compile_args)
yael_dep = declare_dependency(link_with: yael)

peerdb_bug = executable('test-peerdb-bug', ['test/peerdb_bug.cpp'], link_with: yael, dependencies: [log_dep, thread_dep], include_directories: [incdir], cpp_args: compile_args)
many_clients = executable('multi-client-test', ['test/multi_client.cpp'],link_with: yael, dependencies: [log_dep, thread_dep], include_directories: [incdir], cpp_args: compile_args)

subdir('test')

executable('yael-test', test_cpp_files, link_with: yael, dependencies: [log_dep, thread_dep, gtest_dep, gflags_dep], include_directories: [incdir], cpp_args: compile_args)

install_subdir('include/yael', install_dir : 'include')
