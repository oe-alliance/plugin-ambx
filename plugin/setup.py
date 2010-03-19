from distutils.core import setup, Extension

plugdir = '/usr/lib/enigma2/python/Plugins/Extensions/amBX'

setup (name = 'enigma2-plugin-extensions-ambx',
       version = '0.1',
       description = 'Show ambient effects on a Philips amBX USB kit',
       packages = ['amBX'],
       package_data = {'amBX': ['amBX/*.png']}
       )
