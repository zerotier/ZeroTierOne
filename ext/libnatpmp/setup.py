#! /usr/bin/python
# $Id: setup.py,v 1.3 2012/03/05 04:54:01 nanard Exp $
#
# python script to build the libnatpmp module under unix
#
# replace libnatpmp.a by libnatpmp.so for shared library usage
from distutils.core import setup, Extension
from distutils import sysconfig
sysconfig.get_config_vars()["OPT"] = ''
sysconfig.get_config_vars()["CFLAGS"] = ''
setup(name="libnatpmp", version="1.0",
      ext_modules=[
        Extension(name="libnatpmp", sources=["libnatpmpmodule.c"],
                  extra_objects=["libnatpmp.a"],
                  define_macros=[('ENABLE_STRNATPMPERR', None)]
        )]
     )

