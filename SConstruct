# Build file PQ-Tree library and unit test.  See pqtree.h for more
# information on the library.  This file is a build rule file designed for use
# with scons (http://www.scons.org/).
#
# Author: Greg Grothaus (ggrothau@gmail.com)


Help("""
Run: 'scons pqtest' to build the PQ-Tree library unit test.
Run: 'scons -c' to clean up non-src files.
""")

env = Environment()
env.Program('pqtest', ['pqnode.cc', 'pqtest.cc', 'pqtree.cc'])
