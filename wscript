#! /usr/bin/env python

import waflib

def configure(ctx):
    if ctx.env.GSTREAMER_PLUGINS == False:
        return

    ctx.check_cfg(package='gstreamer-1.0', args='--cflags --libs', uselib_store='gstreamer')
    ctx.check_cfg(package='gstreamer-video-1.0', args='--cflags --libs',
                  uselib_store='gstreamer_video')

def build(ctx):

    ctx.env.ZCM_GSTREAMER_PLUGINS_ROOT = ctx.path.get_src().abspath()

    if not waflib.Options.options.skip_git:
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

    if ctx.variant == 'sign':
        return

    ctx.recurse('src')
