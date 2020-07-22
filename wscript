#! /usr/bin/env python

def configure(ctx):
    ctx.check_cfg(package='gstreamer-1.0', args='--cflags --libs', uselib_store='gstreamer')
    ctx.check_cfg(package='gstreamer-video-1.0', args='--cflags --libs',
                  uselib_store='gstreamer_video')

def build(ctx):

    bldpath = ctx.path.get_bld().abspath()
    srcpath = ctx.path.get_src().abspath()

    ctx(rule = 'OUTPUT=$(cd %s && git rev-parse HEAD && \
               ((git tag --contains ; echo "<no-tag>") | head -n1) \
                && git diff) && echo "$OUTPUT" > ${TGT}' % (srcpath),
        target = 'zcm_gstreamer_plugins.gitid',
        always = True)

    gitidNode = ctx.path.get_bld().find_or_declare('zcm_gstreamer_plugins.gitid')
    ctx.install_files('${PREFIX}',
                      gitidNode,
                      cwd = ctx.srcnode,
                      relative_trick = True)

    ctx.zcmgen(name = 'zcm_gstreamer_plugins_zcmtypes',
               lang = ['c_shlib'],
               build = True,
               source = ctx.path.ant_glob('**/*.zcm'))

    DEPS = ['default', 'zcm', 'gstreamer', 'gstreamer_video',
            'zcm_gstreamer_plugins_zcmtypes_c_shlib']
    THIS_LIB = 'zcm_gstreamer_plugins'

    source = ctx.path.ant_glob('**/*.c', excl='test')
    ctx.shlib(target   = THIS_LIB,
              use      = DEPS,
              source   = source,
              includes = [ srcpath, bldpath ])
