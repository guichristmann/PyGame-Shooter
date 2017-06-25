from distutils.core import setup, Extension
setup(name='comm', version='1.0',
      ext_modules=[Extension('comm', ['comm.c'])])
