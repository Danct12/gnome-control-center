name = 'connection-editor'

sources = files(
  'cc-connection-editor.c',
  'ce-security-page.c',
  'ce-details-page.c',
  'ce-ip-page.c',
  'ce-password-row.c',
  'ce-page-8021x-security.c',
  'ce-page-details.c',
  'ce-page-ethernet.c',
  'ce-page-ip4.c',
  'ce-page-ip6.c',
  'ce-page-security.c',
  'ce-page-vpn.c',
  'ce-page-wifi.c',
  'ce-page.c',
  'cc-hidden-wifi-dialog.c',
  'net-connection-editor.c',
  'vpn-helpers.c'
)

resource_data = files(
  'cc-connection-editor.ui',
  'ce-security-page.ui',
  'ce-details-page.ui',
  'ce-ip-page.ui',
  'ce-password-row.ui',
  '8021x-security-page.ui',
  'connection-editor.ui',
  'details-page.ui',
  'ethernet-page.ui',
  'ip4-page.ui',
  'ip6-page.ui',
  'security-page.ui',
  'vpn-page.ui',
  'wifi-page.ui',
  'cc-hidden-wifi-dialog.ui',
)

c_name = 'net-' + name

sources += gnome.compile_resources(
  c_name + '-resources',
  name + '.gresource.xml',
  c_name: c_name.underscorify(),
  dependencies: resource_data,
  export: true
)

libconnection_editor = static_library(
  name,
  sources: sources,
  include_directories: [top_inc, common_inc, network_inc, wireless_security_inc],
  dependencies: deps,
  c_args: cflags,
  link_with: libwireless_security
)
