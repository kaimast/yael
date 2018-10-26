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
gflags_dep = cpp.find_library('gflags')
thread_dep = dependency('threads')
log_dep = cpp.find_library('glog')


botan_dep = cpp.find_library('botan-2', dirs: prefix_library_path)


subdir('src')

yael = shared_library('yael', yael_cpp_files, include_directories:[incdir, botan_dir], dependencies: [log_dep, thread_dep, botan_dep], install: true, cpp_args: compile_args, link_args: ['-lrt'])
yael_dep = declare_dependency(link_with: yael)

many_clients = executable('multi-client-test', ['test/multi_client.cpp'],link_with: yael, dependencies: [log_dep, thread_dep], include_directories: [incdir], cpp_args: compile_args)

churn = executable('churn-test', ['test/churn.cpp'],link_with: yael, dependencies: [log_dep, thread_dep], include_directories: [incdir], cpp_args: compile_args)

subdir('test')

executable('yael-test', test_cpp_files, link_with: yael, dependencies: [log_dep, thread_dep, gtest_dep, gflags_dep], include_directories: [incdir], cpp_args: compile_args)

install_subdir('include/yael', install_dir : 'include')
