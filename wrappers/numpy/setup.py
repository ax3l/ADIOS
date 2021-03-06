#!/usr/bin/env python
# Author:  Jong Choi
# Contact: choij@ornl.gov

from distutils.extension import Extension
import numpy as np

# Use mpi4py dist utils: https://bitbucket.org/mpi4py/mpi4py
from mpidistutils import setup

import subprocess

m1 = Extension('adios', 
               sources=['adios.cpp'], 
               define_macros=[('ADIOS_USE_READ_API_1', None)],
               include_dirs = [np.get_include()],
               library_dirs = [],
               libraries = [],
               extra_objects = [])

p = subprocess.Popen(["adios_config", "-c"], stdout=subprocess.PIPE)
for path in p.communicate()[0].strip().split(" "):
    if path.startswith('-I'):
        m1.include_dirs.append(path.replace('-I', '', 1))

p = subprocess.Popen(["adios_config", "-l"], stdout=subprocess.PIPE)
for path in p.communicate()[0].strip().split(" "):
    if path.startswith('-L'):
        m1.library_dirs.append(path.replace('-L', '', 1))
    if path.startswith('-l'):
        m1.libraries.append(path.replace('-l', '', 1))

setup(name = 'Adios',
      version = '1.0',
      description = 'Python Module for Adios',
      url = 'http://www.olcf.ornl.gov/center-projects/adios/',
      ext_modules = [m1])
