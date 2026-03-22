#!/usr/bin/env python3
# -*- python -*-

# Copyright (C) 2004-2023 Intel Corporation.
# SPDX-License-Identifier: MIT
#

import sys
import os
import shutil
import subprocess

if not 'SNIPER_ROOT' in os.environ:
    os.environ['SNIPER_ROOT'] = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

sniper_root = os.environ['SNIPER_ROOT']
sde_root = "/u/jrs6qe/opt/sde" # Use system SDE for build
arch = "intel64"

pin_root = os.path.join(sde_root, 'pinkit')
pin_gpp = os.path.join(pin_root, arch, 'pinrt', 'bin', 'pin-g++')

if not os.path.exists(pin_gpp):
    print(f"Could not find pin-g++ at {pin_gpp}, falling back to g++")
    pin_gpp = "g++"

objdir = f"obj-{arch}"
if not os.path.exists(objdir):
    os.makedirs(objdir)

# Includes
includes = [
    f"-I{os.path.join(pin_root, 'source', 'include', 'pin')}",
    f"-I{os.path.join(pin_root, 'source', 'include', 'pin', 'gen')}",
    f"-I{os.path.join(pin_root, 'source', 'tools', 'PinPoints')}",
    f"-I{os.path.join(pin_root, 'extras', 'components', 'include')}",
    f"-I{os.path.join(pin_root, 'extras', f'xed-{arch}', 'include', 'xed')}",
    f"-I{os.path.join(pin_root, 'pinplay', 'include')}",
    f"-I{os.path.join(pin_root, 'source', 'tools', 'InstLib')}",
    f"-I{os.path.join(pin_root, 'sde-example', 'include')}",
    "-isystem", os.path.join(pin_root, arch, 'pinrt', 'include', 'adaptor'),
    f"-I./sift",
    f"-I{os.path.join(sniper_root, 'include')}",
]

common_dir = os.path.join(sniper_root, "common")
for item in os.listdir(common_dir):
    d = os.path.join(common_dir, item)
    if os.path.isdir(d):
        includes.append(f"-I{d}")
        # Add subdirectories like core/memory_subsystem
        for subitem in os.listdir(d):
            sd = os.path.join(d, subitem)
            if os.path.isdir(sd):
                includes.append(f"-I{sd}")

# Sources
sources = [
    'emulation.cc', 'globals.cc', 'papi.cc', 'pinboost_debug.cc',
    'recorder_base.cc', 'recorder_control.cc', 'sift_recorder.cc',
    'syscall_modeling.cc', 'threads.cc', 'trace_rtn.cc', 'mtng.cc',
    'intrabarrier_mtng.cc', 'onlinebbv_count.cc', 'to_json.cc',
    'bbv_count_cluster.cc', 'tool_warmup.cc', 'sift_warmup.cc',
    'cond.cc', 'trietree.cc', 'intrabarrier_common.cc', 'pin_lock.cc'
]

cxxflags = [
    "-DBIGARRAY_MULTIPLIER=1", "-DUSING_XED", "-DSDE_INIT", "-DPINPLAY",
    "-DPIN_RT", "-D_LIBCPP_HAS_MUSL_LIBC", "-DPIN_DISABLE_CRT_REG_DEF",
    "-fPIC", "-O2", "-g", "-Wall", "-Wno-unknown-pragmas",
    "-Wno-unused-function", "-Wno-unused-value", "-Wno-dangling-pointer",
    "-Wno-sign-compare", "-std=c++1z"
]

# Compile
objs = []
for src in sources:
    obj = os.path.join(objdir, src.replace('.cc', '.o'))
    cmd = [pin_gpp] + cxxflags + includes + ["-c", src, "-o", obj]
    # print(" ".join(cmd))
    subprocess.check_call(cmd)
    objs.append(obj)

# Link
toolname = os.path.join(objdir, "sde_sift_recorder.so")
ldflags = [
    "-shared", "-Wl,-Bsymbolic",
    f"-Wl,--version-script={os.path.join(pin_root, 'source', 'include', 'pin', 'pintool.ver')}",
    f"-L{os.path.join(pin_root, arch, 'lib')}",
    f"-L{os.path.join(pin_root, 'extras', f'xed-{arch}', 'lib')}",
    f"-L{os.path.join(pin_root, 'sde-example', 'lib', arch)}",
    f"-L{os.path.join(pin_root, 'pinplay', arch)}",
    "-L./sift/obj-intel64",
    "-lpinplay", "-lsde", "-lsift", "-lbz2", "-lzlib", "-lpin", "-lpinrt-adaptor-static",
    "-lxed", "-lpindwarf", "-ldwarf", "-lunwind-dynamic"
]

cmd = [pin_gpp] + ldflags + objs + ["-o", toolname]
print(" ".join(cmd))
subprocess.check_call(cmd)

print("SUCCESS")
