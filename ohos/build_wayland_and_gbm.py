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
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either expressor implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import sys
import ntpath
import os
if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("must input the OpenHarmony directory and the product name and the source dir")
        exit(-1)
    script_dir = os.path.split(os.path.abspath( __file__))[0]
    run_cross_pross_cmd = 'python3 ' + script_dir + '/meson_cross_process.py ' + sys.argv[1] + ' ' + sys.argv[2]
    os.system(run_cross_pross_cmd)

    run_build_cmd = 'PKG_CONFIG_PATH=./pkgconfig '
    run_build_cmd += 'meson setup '+ sys.argv[3] + ' builddir '
    run_build_cmd += '-Dvulkan-drivers= -Ddri-drivers= -Dgallium-drivers=panfrost \
                      -Dplatforms=wayland -Dglx=disabled -Dtools=panfrost --buildtype=release '
    run_build_cmd += '--cross-file=cross_file '
    run_build_cmd += '--prefix=' + os.getcwd() + '/builddir/install'
    print("build command: %s" %run_build_cmd)
    os.system(run_build_cmd)
    os.system('ninja -C builddir -j26')
    os.system('ninja -C builddir install')
