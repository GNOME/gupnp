#!/usr/bin/env python3

import os
import shutil

references = [
    'doc/gupnp-1.6',
]

sourceroot = os.environ.get('MESON_SOURCE_ROOT')
buildroot = os.environ.get('MESON_BUILD_ROOT')
distroot = os.environ.get('MESON_DIST_ROOT')

for reference in references:
    src_path = os.path.join(buildroot, reference)
    if os.path.isdir(src_path):
        dst_path = os.path.join(distroot, reference)
        shutil.copytree(src_path, dst_path)

