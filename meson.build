project('gupnp', 'c', version : '1.6.9', meson_version : '>= 0.54.0')
gnome = import('gnome')
pkg = import('pkgconfig')

cc = meson.get_compiler('c')

GUPNP_API_VERSION='1.6'
GUPNP_API_NAME='gupnp-@0@'.format(GUPNP_API_VERSION)
api_config = configuration_data()
api_config.set('GUPNP_API_VERSION', GUPNP_API_VERSION)
api_config.set('GUPNP_API_NAME', GUPNP_API_NAME)

prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))

netlink_available = cc.has_header('linux/rtnetlink.h')
ifaddrs_available = cc.has_header('ifaddrs.h')

ifa_flags_exists = cc.compiles('''#include <linux/if_addr.h>
int main(int argc, char *argv[])
{
  if (IFA_FLAGS) {};
  return 0;
}''', name: 'IFA_FLAGS is defined')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set('HAVE_NETLINK', netlink_available)
conf.set('HAVE_IFADDRS_H', ifaddrs_available)
conf.set('HAVE_LINUX_WIRELESS_H', cc.has_header('linux/wireless.h'))
conf.set('HAVE_IFA_FLAGS', ifa_flags_exists)

glib_version = '2.70'
add_project_arguments(cc.get_supported_arguments('-Werror=deprecated-declarations'), language: 'c')
conf.set('GLIB_VERSION_MIN_REQUIRED', 'GLIB_VERSION_@0@'.format(glib_version.underscorify()))
conf.set('GLIB_VERSION_MAX_ALLOWED', 'GLIB_VERSION_@0@'.format(glib_version.underscorify()))

subdir('internal')

gssdp_dep = dependency('gssdp-1.6', version : '>= 1.6.2', default_options: ['sniffer=false', 'manpages=false', 'gtk_doc=false', 'introspection=' + get_option('introspection').to_string()], fallback: 'gssdp-1.6')
gio_unix = dependency('gio-unix-2.0', version: '>= 2.44', required: host_machine.system() != 'windows')
libxml_dep = dependency('libxml-2.0')

dependencies = [
    dependency('glib-2.0', version : '>= ' + glib_version),
    dependency('gio-2.0', version : '>= ' + glib_version),
    dependency('gmodule-2.0', version : '>= ' + glib_version),
    dependency('gobject-2.0', version : '>= ' + glib_version),
    dependency('libsoup-3.0', version : '>= 2.99.0'),
    gssdp_dep,
    libxml_dep
]

subdir('libgupnp')
subdir('tests')
subdir('tools')

gidocgen_dep = dependency('gi-docgen', version: '>= 2021.1',
                          fallback: ['gi-docgen', 'dummy_dep'],
                          native: true,
                          required: get_option('gtk_doc') and get_option('introspection')
                      )

subdir('doc')

if get_option('vapi') and get_option('introspection')
    subdir('vala')
endif

if get_option('examples')
    subdir('examples')
endif

if not meson.is_subproject()
    meson.add_dist_script('build-aux/dist-docs.py')
endif

summary(
  {
    'Version': meson.project_version(),
    'API Version' : GUPNP_API_VERSION,
    'Library version': version,
    'Darwin version': darwin_versions
  },
  section: 'Versioning'
)

