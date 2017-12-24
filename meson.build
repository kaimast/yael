project('yael', 'cpp', default_options: ['cpp_std=c++1z'])

incdir = include_directories('include')

gtest = dependency('gtest', main : true)
gmock = dependency('gmock')

thread_dep = dependency('threads')
cpp = meson.get_compiler('cpp')
log_dep = cpp.find_library('glog')

yael = shared_library('yael', ['src/network/Socket.cpp', 'src/network/Address.cpp', 'src/NetworkSocketListener.cpp', 'src/EventLoop.cpp'], include_directories:[incdir], dependencies: [log_dep, thread_dep], install: true)
yael_dep = declare_dependency(link_with: yael)

cpp_args = ['-Wextra', '-Werror']

peerdb_bug = executable('test-peerdb-bug', ['test/peerdb_bug.cpp'], cpp_args: cpp_args, link_with: yael, dependencies: [log_dep, thread_dep], include_directories: [incdir])
many_clients = executable('test-many-clients', ['test/many_clients.cpp'], cpp_args: cpp_args, link_with: yael, dependencies: [log_dep, thread_dep], include_directories: [incdir])

install_subdir('include/yael', install_dir : 'include')
