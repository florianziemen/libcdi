from __future__ import print_function
from distutils.command.install_lib import install_lib
from distutils.command.build_ext import build_ext
import re
import os
import sys

class lt_install_lib(install_lib):
    # re-assemble libtool install command
    libtool_cmd = []
    for env_var in ['LIBTOOL', 'AM_LIBTOOLFLAGS', 'LIBTOOLFLAGS', 'INSTALL']:
        libtool_cmd += os.environ[env_var].split()

    def copy_tree(self, infile, outfile,
                  preserve_mode=1, preserve_times=1, preserve_symlinks=0,
                  level=1):
        """almost identical to dir_util.copy_tree but for the special
        treatment of .libs directories and .la files
        """
        from distutils.file_util import copy_file
        from distutils.dir_util import mkpath

        verbose=1
        update=0
        if not self.dry_run and not os.path.isdir(infile):
            raise DistutilsFileError, \
                "cannot copy tree '%s': not a directory" % infile
        try:
            names = os.listdir(infile)
        except os.error, (errno, errstr):
            if self.dry_run:
                names = []
            else:
                raise DistutilsFileError, \
                    "error listing files in '%s': %s" % (infile, errstr)

        if not self.dry_run:
            mkpath(outfile, verbose=verbose)

        outputs = []

        for n in names:
            infile_name = os.path.join(infile, n)
            outfile_name = os.path.join(outfile, n)

            if n.startswith('.nfs') or n == '.libs':
                # skip NFS rename files and libtool-internals
                continue

            if n.endswith('.la'):
                print('installing libtool library', n, file=sys.stderr)
                self.spawn(self.libtool_cmd + [infile_name, outfile_name])
                continue

            if preserve_symlinks and os.path.islink(infile_name):
                link_dest = os.readlink(infile_name)
                if verbose >= 1:
                    log.info("linking %s -> %s", outfile_name, link_dest)
                if not self.dry_run:
                    os.symlink(link_dest, outfile_name)
                outputs.append(outfile_name)

            elif os.path.isdir(infile_name):
                outputs.extend(
                    self.copy_tree(infile_name, outfile_name, preserve_mode,
                                   preserve_times, preserve_symlinks))
            else:
                copy_file(infile_name, outfile_name, preserve_mode,
                          preserve_times, update, verbose=verbose,
                          dry_run=self.dry_run)
                outputs.append(outfile_name)

        return outputs

# customize the compiler to use libtool naming
class lt_build_ext(build_ext):
    def my_compiler__compile(self, obj, src, ext, cc_args, extra_postargs,
                             pp_opts):
        if not os.path.isfile(src):
            srcdir_fname = os.path.join(os.environ['abs_srcdir'], src)
            if os.path.isfile(srcdir_fname):
                src = srcdir_fname
        self.compiler.orig__compile(obj, src, ext, cc_args, extra_postargs,
                                    pp_opts)

    def build_extension(self, ext):
        self.compiler.shared_lib_extension = '.la'
        self.compiler.obj_extension = '.lo'
        self.compiler.orig__compile=self.compiler._compile
        self.compiler._compile=self.my_compiler__compile
        build_ext.build_extension(self,ext)

    def get_ext_filename(self, ext_name):
        name = build_ext.get_ext_filename(self, ext_name)
        name = re.sub('\.[^.]*$', '.la', name)
        return name

