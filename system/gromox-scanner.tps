[Unit]
Description=Gromox scanner service
Documentation=man:scanner(8gx) man:scanner.cfg(5gx)
PartOf=gromox-sa.target

[Service]
Type=simple
ExecStart=@libexecdir@/gromox/scanner

[Install]
WantedBy=multi-user.target