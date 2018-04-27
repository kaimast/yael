project('yael', 'cpp')

compile_args = ['-Wall', '-std=c++17', '-Werror']

incdir = include_directories('include')

gtest = dependency('gtest')
gmock = dependency('gmock')

thread_dep = dependency('threads')
cpp = meson.get_compiler('cpp')
log_dep = cpp.find_library('glog')

yael = shared_library('yael', ['src/network/Socket.cpp', 'src/network/Address.cpp', 'src/NetworkSocketListener.cpp', 'src/EventLoop.cpp'], include_directories:[incdir], dependencies: [log_dep, thread_dep], install: true, cpp_args: compile_args)
yael_dep = declare_dependency(link_with: yael)

peerdb_bug = executable('test-peerdb-bug', ['test/peerdb_bug.cpp'], link_with: yael, dependencies: [log_dep, thread_dep], include_directories: [incdir], cpp_args: compile_args)
many_clients = executable('test-many-clients', ['test/many_clients.cpp'],link_with: yael, dependencies: [log_dep, thread_dep], include_directories: [incdir], cpp_args: compile_args)

install_subdir('include/yael', install_dir : 'include')
