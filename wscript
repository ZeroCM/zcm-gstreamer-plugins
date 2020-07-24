#! /usr/bin/env python

def configure(ctx):
    ctx.check_cfg(package='gstreamer-1.0', args='--cflags --libs', uselib_store='gstreamer')
    ctx.check_cfg(package='gstreamer-video-1.0', args='--cflags --libs',
                  uselib_store='gstreamer_video')
    ctx.env.GSTREAMER_PLUGINS = True

def build(ctx):

    ctx.env.ZCM_GSTREAMER_PLUGINS_ROOT = ctx.path.get_src().abspath()

    ctx(rule = 'OUTPUT=$(cd %s && git rev-parse HEAD && \
               ((git tag --contains ; echo "<no-tag>") | head -n1) \
                && git diff) && echo "$OUTPUT" > ${TGT}' %
                (ctx.env.ZCM_GSTREAMER_PLUGINS_ROOT),
        target = 'zcm_gstreamer_plugins.gitid',
        always = True)

    gitidNode = ctx.path.get_bld().find_or_declare('zcm_gstreamer_plugins.gitid')
    ctx.install_files('${PREFIX}',
                      gitidNode,
                      cwd = ctx.srcnode,
                      relative_trick = True)

    ctx.recurse('src')
