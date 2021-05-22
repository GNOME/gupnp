if gssdp_dep.type_name() == 'internal'
    gssdp_vala_package = subproject('gssdp-1.2').get_variable('vapi')
    gssdp_gir_dirs = subproject('gssdp-1.2').get_variable('gssdp_gir_dir')
else
    gssdp_vala_package = 'gssdp-1.2'
    gssdp_gir_dirs = []
endif

gnome.generate_vapi('gupnp-1.2',
                    sources : [gir.get(0), 'gupnp-1.2-custom.vala'],
                    packages : [gssdp_vala_package, 'gio-2.0', 'libsoup-2.4', 'libxml-2.0'],
                    gir_dirs : gssdp_gir_dirs,
                    install : true
)
