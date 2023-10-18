plugin_c_args = ['-DHAVE_CONFIG_H']

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', '1.0')
cdata.set_quoted('PACKAGE', 'gst-plugins')
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('GST_API_VERSION', '1.20')
cdata.set_quoted('GST_PACKAGE_NAME', 'GStreamer template Plug-ins')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://stream.studio')

configure_file(output : 'config.h',
               configuration : cdata)




deepstream_dep = declare_dependency(
      include_directories : [include_directories('/opt/nvidia/deepstream/deepstream/sources/includes/'), include_directories('/usr/include/aarch64-linux-gnu/')],
      link_args: ['-L/opt/nvidia/deepstream/deepstream-6.2/lib', '-L/usr/lib/aarch64-linux-gnu/tegra/', '-lnvbufsurface',  '-lnvdsgst_helper', '-lnvbufsurftransform', '-lnvvpi', '-lnvds_mem_allocator', '-lnvdsgst_meta', '-lnvdsbufferpool', '-lnvbuf_utils']
      )

cuda_dep = dependency('cuda')
gstreamer_base_dep = dependency('gstreamer-plugins-base-1.0')
gstreamer_pbutils_dep = dependency('gstreamer-pbutils-1.0')


vpi_sources = [
'vpi/gstvpi.c',
'vpi/gstvpibasetransform.c',
]

vpi = library('gstvpi',
    vpi_sources,
    dependencies : [gst_dep, cuda_dep, deepstream_dep, gstreamer_base_dep, gstreamer_pbutils_dep],
    c_args: plugin_c_args,
    install : true,
    install_dir : plugins_install_dir,
)
