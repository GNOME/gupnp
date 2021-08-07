if gssdp_dep.type_name() == 'internal'
    gssdp_vala_package = subproject('gssdp-1.6').get_variable('vapi')
    gssdp_gir_dirs = subproject('gssdp-1.6').get_variable('gssdp_gir_dir')
else
    gssdp_vala_package = 'gssdp-1.6'
    gssdp_gir_dirs = []
endif

gnome.generate_vapi(GUPNP_API_NAME,
                    sources : [gir.get(0), 'gupnp-custom.vala'],
                    packages : [gssdp_vala_package, 'gio-2.0', 'libsoup-3.0', 'libxml-2.0'],
                    gir_dirs : gssdp_gir_dirs,
                    install : true
)
