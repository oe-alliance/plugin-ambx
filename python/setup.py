from distutils.core import setup, Extension

pyambx = Extension('pyambx',
		libraries = ['ambx', 'grabproc', 'rt', 'pthread'],
		include_dirs = ['..'],
		sources = ['pyambx.c', '../Fader.c'])

setup (name = 'python-pyambx',
       version = '0.1',
       description = 'Python interface to amBX and grabber',
       ext_modules = [pyambx])
