context_manager_impl = []
context_manager_args = []
system_deps = []

if host_machine.system() == 'windows'
    context_manager_impl += 'gupnp-windows-context-manager.c'
    system_deps += cc.find_library('ws2_32', required : true)
    system_deps += cc.find_library('iphlpapi', required : true)
else
    if ifaddrs_available
        context_manager_impl += 'gupnp-unix-context-manager.c'
    endif
    if get_option('context_manager') == 'network-manager'
        context_manager_impl += 'gupnp-network-manager.c'
        context_manager_args += '-DUSE_NETWORK_MANAGER'
    endif
    if get_option('context_manager') == 'connman'
        context_manager_impl += 'gupnp-connman-manager.c'
        context_manager_args += '-DUSE_CONNMAN'
    endif
endif

if netlink_available
    context_manager_impl += 'gupnp-linux-context-manager.c'
endif

if get_option('context_manager') == 'linux' or (netlink_available and get_option('context_manager') == 'system')
    context_manager_args += '-DUSE_NETLINK'
endif

enums = gnome.mkenums(
    'gupnp-enums',
    sources : [
        'gupnp-error.h',
        'gupnp-service-introspection.h'
    ],
    identifier_prefix : 'GUPnP',
    symbol_prefix : 'gupnp',
    c_template : 'gupnp-enums.c.template',
    h_template : 'gupnp-enums.h.template',
    install_header : true,
    install_dir : join_paths (get_option('includedir'), GUPNP_API_NAME, 'libgupnp')
)

headers = files(
    'gupnp-acl.h',
    'gupnp-context.h',
    'gupnp-context-filter.h',
    'gupnp-context-manager.h',
    'gupnp-control-point.h',
    'gupnp-device.h',
    'gupnp-device-info.h',
    'gupnp-device-proxy.h',
    'gupnp-error.h',
    'gupnp.h',
    'gupnp-resource-factory.h',
    'gupnp-root-device.h',
    'gupnp-service.h',
    'gupnp-service-info.h',
    'gupnp-service-introspection.h',
    'gupnp-service-proxy.h',
    'gupnp-types.h',
    'gupnp-uuid.h',
    'gupnp-xml-doc.h'
)
install_headers(headers, subdir : GUPNP_API_NAME / 'libgupnp')

sources = files(
    'gupnp-acl.c',
    'gupnp-context.c',
    'gupnp-context-filter.c',
    'gupnp-context-manager.c',
    'gupnp-control-point.c',
    'gupnp-device.c',
    'gupnp-device-info.c',
    'gupnp-device-proxy.c',
    'gupnp-error.c',
    'gupnp-resource-factory.c',
    'gupnp-root-device.c',
    'gupnp-service.c',
    'gupnp-service-action.c',
    'gupnp-service-info.c',
    'gupnp-service-introspection.c',
    'gupnp-service-proxy.c',
    'gupnp-service-proxy-action.c',
    'gupnp-simple-context-manager.c',
    'gupnp-types.c',
    'gupnp-xml-doc.c',
    'gvalue-util.c',
    'http-headers.c',
    'xml-util.c'
)

v = meson.project_version().split('.')
soversion = 0
library_minor = v[0].to_int() * 100 + v[1].to_int()
library_micro = v[2].to_int()

version = '@0@.@1@.@2@'.format(soversion, library_minor, library_micro)
version_arr = version.split('.')
major_version = version_arr[0].to_int()
minor_version = version_arr[1].to_int()
micro_version = version_arr[2].to_int()
current = major_version + minor_version + 1
interface_age = micro_version
darwin_versions = [current, '@0@.@1@'.format(current, interface_age)]

libgupnp = library(
    GUPNP_API_NAME,
    sources + context_manager_impl + enums,
    version : version,
    darwin_versions : darwin_versions,
    dependencies : dependencies + system_deps,
    c_args : context_manager_args,
    include_directories: [include_directories('..'), config_h_inc],
    install: true
)

gupnp = declare_dependency(
    link_with: libgupnp,
    include_directories : include_directories('..'),
    dependencies : dependencies
)

meson.override_dependency(GUPNP_API_NAME, gupnp)

pkg.generate(
    libgupnp,
    subdirs: GUPNP_API_NAME,
    name : GUPNP_API_NAME,
    requires : [
      'glib-2.0',
      'gio-2.0',
      'gobject-2.0',
      'gssdp-1.6',
      'libsoup-3.0',
      'libxml-2.0'
    ],
    description : 'GObject-based UPnP library',
    version : meson.project_version(),
    filebase : GUPNP_API_NAME
)

if get_option('introspection')
    gir_includes = ['GObject-2.0', 'Gio-2.0', 'Soup-3.0', 'libxml2-2.0']
    if gssdp_dep.type_name() == 'internal'
        gir_includes += subproject('gssdp-1.6').get_variable('gir').get(0)
    else
        gir_includes += 'GSSDP-1.6'
    endif
    gir = gnome.generate_gir(
        libgupnp,
        sources : headers + sources + enums,
        namespace : 'GUPnP',
        nsversion : GUPNP_API_VERSION,
        header : 'libgupnp/gupnp.h',
        symbol_prefix : 'gupnp',
        identifier_prefix : 'GUPnP',
        export_packages : GUPNP_API_NAME,
        includes : gir_includes,
        install : true,
        extra_args : '-DGOBJECT_INTROSPECTION_SKIP'
    )
endif
