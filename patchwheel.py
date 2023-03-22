"""
Patch lavavu_osmesa wheels to fix auditwheel library issues
(Patch wheels built with cibuildwheel and corrected by auditwheel. Linux only)

Based on:
https://github.com/arbor-sim/arbor/blob/master/scripts/patchwheel.py
https://github.com/arbor-sim/arbor/blob/master/LICENSE

Copyright 2016-2020 Eidgenössische Technische Hochschule Zürich and Forschungszentrum Jülich GmbH.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""
import shutil
import subprocess
import argparse
from pathlib import Path
import sys

if len(sys.argv) < 2:
    raise(Exception(f"Args: {wheels_dir}"))

#First arg is the auditwheel output dir
#Second arg is the auditwheel input wheel name
#We need to take the name but use the output dir
wheelpath = Path(sys.argv[1])
package_name = 'lavavu_osmesa'
for inwheel in wheelpath.glob(f"{package_name}*.whl"):
    zipdir = Path(f"{inwheel}.unzip")
    # shutil.unpack_archive(inwheel,zipdir,'zip') # Disabled, because shutil (and ZipFile) don't preserve filemodes
    subprocess.check_call(f"unzip {inwheel} -d {zipdir}", shell=True)

    #libs = list(zipdir.glob("{package_name}.libs/lib*.so*"))
    libs = list(zipdir.glob(f"{package_name}.libs/libLLVM*.so*"))
    print(libs)
    if len(libs) == 0: raise(Exception("No libs found!"))
    for lib in libs:
        print(f"PROCESSING {lib}")
        subprocess.check_call(f"patchelf --force-rpath --set-rpath '$ORIGIN' {lib}", shell=True)

    # TODO? correct checksum/bytecounts in *.dist-info/RECORD.
    # So far, Python does not report mismatches
    print("Re-compressing;")

    outwheel = Path(shutil.make_archive(inwheel, "zip", zipdir))
    inwheel.unlink()
    Path.rename(outwheel, inwheel)
    shutil.rmtree(zipdir)

