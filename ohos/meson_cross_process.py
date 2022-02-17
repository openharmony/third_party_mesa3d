#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright (c) 2021 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import sys
import ntpath
import os

sysroot_stub = 'sysroot_stub'
project_stub = 'project_stub'

corss_file_content='''
[properties]
needs_exe_wrapper = true

c_args = [
    '-march=armv7-a',
    '-mfloat-abi=softfp',
    '-mtune=generic-armv7-a',
    '-mfpu=neon',
    '-mthumb',
    '--target=arm-linux-ohosmusl',
    '--sysroot=sysroot_stub',
    '-fPIC']

cpp_args = [
    '-march=armv7-a',
    '--target=arm-linux-ohosmusl',
    '--sysroot=sysroot_stub',
    '-fPIC']
    
c_link_args = [
    '-march=armv7-a',
    '--target=arm-linux-ohosmusl',
    '-fPIC',
    '--sysroot=sysroot_stub',
    '-Lsysroot_stub/usr/lib/arm-linux-ohos',
    '-Lproject_stub/prebuilts/clang/ohos/linux-x86_64/llvm/lib/clang/10.0.1/lib/arm-linux-ohos',
    '-Lproject_stub/prebuilts/clang/ohos/linux-x87_64/llvm/lib/arm-linux-ohos/c++',
    '--rtlib=compiler-rt',
    ]

cpp_link_args = [
    '-march=armv7-a',
    '--target=arm-linux-ohosmusl',
    '--sysroot=sysroot_stub',
    '-Lsysroot_stub/usr/lib/arm-linux-ohos',
    '-Lproject_stub/prebuilts/clang/ohos/linux-x86_64/llvm/lib/clang/10.0.1/lib/arm-linux-ohos',
    '-Lproject_stub/prebuilts/clang/ohos/linux-x87_64/llvm/lib/arm-linux-ohos/c++',
    '-fPIC',
    '-Wl,--exclude-libs=libunwind_llvm.a',
    '-Wl,--exclude-libs=libc++_static.a',
    '-Wl,--exclude-libs=libvpx_assembly_arm.a',
    '-Wl,--warn-shared-textrel',
    '--rtlib=compiler-rt',
    ]
	
[binaries]
ar = 'project_stub/prebuilts/clang/ohos/linux-x86_64/llvm/bin/llvm-ar'
c = ['ccache', 'project_stub/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang']
cpp = ['ccache', 'project_stub/prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang++']
c_ld= 'lld'
cpp_ld = 'lld'
strip = 'project_stub/prebuilts/clang/ohos/linux-x86_64/llvm/bin/llvm-strip'
pkgconfig = '/usr/bin/pkg-config'

[host_machine]
system = 'linux'
cpu_family = 'arm'
cpu = 'armv7'
endian = 'little'
'''

def generate_cross_file(project_stub, sysroot_stub):
    with open("cross_file", 'w+') as file:
        result = corss_file_content.replace("project_stub", project_stub)
        result = result.replace("sysroot_stub", sysroot_stub)
        file.write(result)
    print("generate_cross_file")

def generate_pc_file(file_raw, project_dir, product_name):
    print(file_raw)
    filename = 'pkgconfig/'+ ntpath.basename(file_raw)
    with open(file_raw, 'r+') as file_raw:
        with open(filename, "w+") as file:
            raw_content = file_raw.read()
            raw_content = raw_content.replace("ohos_project_directory_stub", project_dir)
            raw_content = raw_content.replace("ohos-arm-release", product_name)
            file.write(raw_content)
    print("generate_pc_file")

def process_pkgconfig(project_dir, product_name):
    files = os.listdir(os.path.split(os.path.abspath( __file__))[0] + r"/pkgconfig_template")
    for file in files:
        if not os.path.isdir(file):
            generate_pc_file(r"pkgconfig_template/" + file, project_dir, product_name)
    print("process_pkgconfig")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("must input the OpenHarmony directory and the product name")
        exit(-1)
    project_stub = sys.argv[1]
    sysroot_stub = sys.argv[1] + r"/out/" + sys.argv[2].lower() + r"/obj/third_party/musl"
    generate_cross_file(sys.argv[1], sysroot_stub)
    process_pkgconfig(sys.argv[1], sys.argv[2].lower())
