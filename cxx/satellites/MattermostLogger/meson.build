# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if not get_option('satellite_mattermost_logger')
  subdir_done()
endif

# cpr might require curl and openssl, disable test and unnecessary features in case of fallbak
dependency('openssl', default_options: ['warning_level=0'])
dependency('libcurl', default_options: [
  'tool=disabled',
  'tests=disabled',
  'unittests=disabled',
  'brotli=disabled',
  'cookies=disabled',
  'doh=disabled',
  'form-api=disabled',
  'getoptions=disabled',
  'gsasl=disabled',
  'libz=disabled',
  'mime=disabled',
  'netrc=disabled',
  'parsedate=disabled',
  'progress-meter=disabled',
  'psl=disabled',
  'sspi=disabled',
  'zstd=disabled',
  'aws=disabled',
  'basic-auth=disabled',
  'bearer-auth=disabled',
  'digest-auth=disabled',
  'kerberos-auth=disabled',
  'negotiate-auth=disabled',
  'gss-api=disabled',
  'ntlm=disabled',
  'ssh=disabled',
  'ssl=enabled',
  'dict=disabled',
  'file=disabled',
  'ftp=disabled',
  'gopher=disabled',
  'imap=disabled',
  'ldap=disabled',
  'ldaps=disabled',
  'mqtt=disabled',
  'pop3=disabled',
  'rtmp=disabled',
  'rtsp=disabled',
  'smb=disabled',
  'smtp=disabled',
  'telnet=disabled',
  'tftp=disabled',
  'warning_level=0',
])
cpr_dep = dependency('cpr')

satellite_type = 'MattermostLogger'

satellite_sources = files(
  'MattermostLoggerSatellite.cpp',
)

satellite_dependencies = [listener_dep, cpr_dep]

satellites_to_build += [[satellite_type, satellite_sources, satellite_dependencies]]
