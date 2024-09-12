#!/usr/bin/env python3
import argparse
import sys
import os
import subprocess

parser = argparse.ArgumentParser(prog=sys.argv[0], allow_abbrev=False)
parser.add_argument('--db', help="MariaDB database name.")
parser.add_argument('--db-user', help="MariaDB database user.")
parser.add_argument('--db-user-password', help="MariaDB database user's password.")
parser.add_argument('--sim-path', help="Path where sim project will be cloned to.")
parser.add_argument('--sim-local-address', default='127.0.0.1:8888', help="Address Sim will listen at locally (default: 127.0.0.1:8888).")
parser.add_argument('--start-sim-at-boot', action='store_true', help="Start sim at boot.")
parser.add_argument('--cert-name', help="SSL certificate name.")
parser.add_argument('--certbot', action='store_true', help="Use certbot for generating a valid SSL certificate.")
parser.add_argument('--domain', help="Domain address for certbot, postfix and nginx")
parser.add_argument('--postfix', action='store_true', help="Set up postfix for sending emails.")
parser.add_argument('--postfix-test-email-recipients', metavar="EMAILS", help="Emails to send a test email to, separated by comma (,) e.g. a@a,b@b.")
parser.add_argument('--postfix-use-gmail-relay-via-sasl', action='store_true', help="Send emails using gmail relay via sasl")
parser.add_argument('--postfix-use-gmail-relay-via-sasl-gmail-address', metavar='GMAIL_ADDRESS', help="Gmail relay via sasl gmail address")
parser.add_argument('--postfix-use-gmail-relay-via-sasl-gmail-password', metavar='GMAIL_PASSWORD', help="Gmail relay via sasl gmail password")
parser.add_argument('--email-service-errors-to', metavar="EMAILS", help="Emails to send service running errors to, separated by comma (,) e.g. a@a,b@b.")
parser.add_argument('--sslh', action='store_true', help="Set up sslh.")
parser.add_argument('--nginx-site-filename', help="Nginx site filename for Sim.")
parser.add_argument('--daily-reboot', action='store_true', help="Set up daily reboot.")
parser.add_argument('--unattended-upgrades', action='store_true', help="Set up unattended upgrades.")
parser.add_argument('--daily-backup', action='store_true', help="Set up daily backup.")
parser.add_argument('--daily-backup-filename', metavar="BACKUP_FILENAME", help="Backup filename.")
parser.add_argument('--daily-backup-to-partition', metavar="UUID", help="Set up daily backup to partition identified by UUID.")
parser.add_argument('--email-access-logs-to', metavar="EMAILS", help="Emails to send sshd access logs to, separated by comma (,) e.g. a@a,b@b.")
parser.add_argument('--email-sim-error-logs-to', metavar="EMAILS", help="Emails to send Sim error logs to, separated by comma (,) e.g. a@a,b@b.")
parser.add_argument('user')
args = parser.parse_args()

def root_cmd(cmd):
    subprocess.check_call(['sh', '-c', cmd])

def user_cmd(cmd):
    subprocess.check_call([
        'systemd-run',
        '-M',
        f'{args.user}@',
        '--user',
        '--quiet',
        '--collect',
        '--pipe',
        '--wait',
        'sh',
        '-c',
        cmd
    ])

completed = set()

def update_apt():
    if update_apt in completed: return
    completed.add(update_apt)
    root_cmd('apt update')

def apt_install(pkgs_str):
    update_apt()
    root_cmd('env DEBIAN_FRONTEND=noninteractive apt install -y ' + pkgs_str)

def setup_database():
    if setup_database in completed: return
    completed.add(setup_database)
    if args.db is None:
        raise Exception('You need to specify database name with --db <DATABASE NAME>')
    if args.db_user is None:
        raise Exception('You need to specify database user with --db-user <DATABASE USER>')
    if args.db_user_password is None:
        raise Exception('You need to specify database user with --db-user-password <DATABASE USER PASSWORD>')

    print('\033[1;32m==>\033[0;1m Prepare database and database user\033[m')
    apt_install('mariadb-server expect')
    root_cmd("""expect -c 'spawn mysql_secure_installation; send "\ry\rn\ry\ry\ry\ry\r"; interact'""")
    root_cmd(f"mysql -e 'CREATE USER IF NOT EXISTS '\"'\"'{args.db_user}'\"'\"'@localhost IDENTIFIED BY '\"'\"'{args.db_user_password}'\"'\"")
    root_cmd(f"mysql -e 'CREATE DATABASE IF NOT EXISTS `{args.db}`'")
    root_cmd(f"mysql -e 'GRANT ALL ON `{args.db}`.* TO '\"'\"'{args.db_user}'\"'\"'@localhost'")

if args.db is not None or args.db_user is not None or args.db_user_password is not None:
    setup_database()

def setup_sim():
    if setup_sim in completed: return
    completed.add(setup_sim)
    if args.sim_path is None:
        raise Exception('You need to specify sim repo\'s path with --sim-path <SIM_PATH>')

    setup_database()

    print('\033[1;32m==>\033[0;1m Set up sim repo\033[m')
    apt_install('git')
    user_cmd(f"test -e '{args.sim_path}' || git clone https://github.com/varqox/sim-project --branch develop '{args.sim_path}'")

    print('\033[1;32m==>\033[0;1m Build sim\033[m')
    apt_install('meson pkgconf g++ libmariadb-dev libseccomp-dev libcap-dev libzip-dev libssl-dev rustc fpc clang')
    user_cmd(f"cd '{args.sim_path}/subprojects/sim' && meson setup build/ -Dbuildtype=release")
    user_cmd(f"cd '{args.sim_path}/subprojects/sim' && meson configure build/ --prefix \"$(pwd)/sim\"")
    user_cmd(f"cd '{args.sim_path}/subprojects/sim' && meson compile -C build/")
    user_cmd(f"cd '{args.sim_path}/subprojects/sim' && meson test -C build/ --print-errorlogs")

    print('\033[1;32m==>\033[0;1m Configure sim\033[m')
    user_cmd(f"cd '{args.sim_path}/subprojects/sim' && mkdir --parents sim/")
    user_cmd(f"""cd '{args.sim_path}/subprojects/sim' && cat > sim/.db.config << HEREDOCEND
user: '{args.db_user}'
password: '{args.db_user_password}'
db: '{args.db}'
host: 'localhost'
HEREDOCEND""")
    user_cmd(f"cd '{args.sim_path}/subprojects/sim' && meson install -C build/")
    # Change sim address
    user_cmd(f"grep -P '^web_server_address:.*$' '{args.sim_path}/subprojects/sim/sim/sim.conf' -q || (echo \"\033[1;31merror: couldn't find field 'address' in the {args.sim_path}/subprojects/sim/sim/sim.conf\033[m\" && false)")
    user_cmd(f"sed 's/^web_server_address:.*$/web_server_address: {args.sim_local_address}/' -i '{args.sim_path}/subprojects/sim/sim/sim.conf'")

    print('\033[1;32m==>\033[0;1m Run sim\033[m')
    apt_install('curl')
    user_cmd(f"'{args.sim_path}/subprojects/sim/sim/manage' stop")
    user_cmd(f"curl --silent '{args.sim_local_address}' > /dev/null; test $? -eq 7 || (echo '\033[1;31merror: address {args.sim_local_address} is already in use\033[m' && false)")
    # user_cmd does not work and we want the job to run in the background
    subprocess.check_call([
        'systemd-run',
        '-M',
        f'{args.user}@',
        '--user',
        '--quiet',
        'sh',
        '-c',
        f"'{args.sim_path}/subprojects/sim/sim/manage' restart",
    ])

if args.sim_path is not None:
    setup_sim()

def start_sim_at_boot():
    if start_sim_at_boot in completed: return
    completed.add(start_sim_at_boot)
    if not args.start_sim_at_boot:
        raise Exception('You need to request starting sim at boot with --start-sim-at-boot')

    setup_sim()

    print('\033[1;32m==>\033[0;1m Set up systemd to start sim at system startup\033[m')
    # Doing it as --user to make systemd specify XDG_RUNTIME_DIR variable that needed by Sim's sandbox
    user_cmd('mkdir -p ".config/systemd/user/"')
    user_cmd(f"""cat > ".config/systemd/user/start_$(systemd-escape --path '{args.sim_path}').service" << HEREDOCEND
[Unit]
Description=Start Sim instance {args.sim_path}

[Service]
ExecStart=sh -c "until test -e /var/run/mysqld/mysqld.sock; do sleep 0.4; done; '{args.sim_path}/subprojects/sim/sim/manage' start"

[Install]
WantedBy=default.target
HEREDOCEND""")
    user_cmd(f"""systemctl --user daemon-reload""")
    user_cmd(f"""systemctl --user enable --now "start_$(systemd-escape --path '{args.sim_path}')".service""")
    root_cmd(f"loginctl enable-linger {args.user}") # make user session start at boot

if args.start_sim_at_boot:
    start_sim_at_boot()

ssl_dhparam_path = None
ssl_certificate_path = None
ssl_certificate_key_path = None
def setup_ssl_certificate():
    if setup_ssl_certificate in completed: return
    completed.add(setup_ssl_certificate)
    if args.cert_name is None:
        raise Exception('You need to specify SSL certificate name with --cert-name <CERT_NAME>')

    global ssl_dhparam_path
    global ssl_certificate_path
    global ssl_certificate_key_path

    root_cmd('mkdir -p /etc/ssl/sim/')
    root_cmd('chmod 700 /etc/ssl/sim/')
    ssl_dhparam_path = f"/etc/ssl/sim/{args.cert_name}_dhparam.pem"
    root_cmd(f"test -e '{ssl_dhparam_path}' || openssl dhparam -out '{ssl_dhparam_path}' 2048")
    if args.certbot:
        if args.domain is None:
            raise Exception('You need to specify domain for certbot with --domain <DOMAIN>')

        print('\033[1;32m==>\033[0;1m Install certbot\033[m')
        apt_install('certbot python3-certbot-nginx')

        print('\033[1;32m==>\033[0;1m Run certbot\033[m')
        root_cmd(f"certbot certonly --non-interactive --register-unsafely-without-email --agree-tos --nginx --expand --cert-name '{args.cert_name}' --domain '{args.domain}' -v")
        ssl_certificate_path = f"/etc/letsencrypt/live/{args.cert_name}/fullchain.pem"
        ssl_certificate_key_path = f"/etc/letsencrypt/live/{args.cert_name}/privkey.pem"
    else:
        apt_install("expect")
        print('\033[1;32m==>\033[0;1m Generate SSL certificate\033[m')
        ssl_certificate_path = f"/etc/ssl/sim/{args.cert_name}.crt"
        ssl_certificate_key_path = f"/etc/ssl/sim/{args.cert_name}.key"
        root_cmd(f"""expect -c 'spawn openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout '{ssl_certificate_key_path}' -out '{ssl_certificate_path}'; send ".\r.\r.\r.\r.\rsim\r.\r"; interact'""")

if args.cert_name is not None or args.certbot:
    setup_ssl_certificate()

def setup_postfix():
    if setup_postfix in completed: return
    completed.add(setup_postfix)
    if not args.postfix:
        raise Exception('You need to request setting up postfix with --postfix')
    if args.domain is None:
        raise Exception('You need to specify domain for postfix with --domain <DOMAIN>')
    if args.postfix_use_gmail_relay_via_sasl or args.postfix_use_gmail_relay_via_sasl_gmail_address is not None or args.postfix_use_gmail_relay_via_sasl_gmail_password is not None:
        if not args.postfix_use_gmail_relay_via_sasl:
            raise Exception('You need to request setting up sending emails using gmail relay via sasl with --postfix-use-gmail-relay-via-sasl')
        if args.postfix_use_gmail_relay_via_sasl_gmail_address is None:
            raise Exception('You need to specify gmail address with --postfix-use-gmail-relay-via-sasl-gmail-address <GMAIL_ADDRESS>')
        if args.postfix_use_gmail_relay_via_sasl_gmail_password is None:
            raise Exception('You need to specify gmail password with --postfix-use-gmail-relay-via-sasl-gmail-password <GMAIL_PASSWORD>')

    setup_ssl_certificate()

    print('\033[1;32m==>\033[0;1m Install postfix\033[m')
    root_cmd('env DEBIAN_FRONTEND=noninteractive apt -yq install postfix mailutils')

    print('\033[1;32m==>\033[0;1m Set up postfix for sending emails\033[m')
    root_cmd("postconf -e 'inet_interfaces = loopback-only'")
    root_cmd(f"postconf -e 'myhostname = {args.domain}'")
    root_cmd("postconf -e 'myorigin = $myhostname'")
    root_cmd("postconf -e 'smtp_sasl_path = private/auth'")
    root_cmd("postconf -e 'smtp_sasl_security_options = noanonymous'")
    root_cmd("postconf -e 'smtp_sasl_type = cyrus'")
    root_cmd("postconf -e 'smtp_tls_CApath = /etc/ssl/certs'")
    root_cmd("postconf -e 'smtp_tls_exclude_ciphers = aNULL, eNULL, EXPORT, DES, RC4, MD5, PSK, aECDH, EDH-DSS-DES-CBC3-SHA, EDH-RSA-DES-CDB3-SHA, KRB5-DES, CBC3-SHA'")
    root_cmd("postconf -e 'smtp_tls_loglevel = 1'")
    root_cmd("postconf -e 'smtp_tls_session_cache_database = btree:${data_directory}/smtp_scache'")
    root_cmd("postconf -e 'smtp_use_tls = yes'")

    if args.postfix_use_gmail_relay_via_sasl:
        root_cmd("postconf -e 'smtp_tls_security_level = verify'")
        root_cmd("postconf -e 'relayhost = [smtp.gmail.com]:587'")
        root_cmd("postconf -e 'smtp_sasl_auth_enable = yes'")
        root_cmd("postconf -e 'smtp_sasl_password_maps = hash:/etc/postfix/sasl_passwd'")
        root_cmd(f"echo '[smtp.gmail.com]:587 {args.postfix_use_gmail_relay_via_sasl_gmail_address}:{args.postfix_use_gmail_relay_via_sasl_gmail_password}' > '/etc/postfix/sasl_passwd'")
    else:
        root_cmd("postconf -e 'smtp_tls_security_level = may'") # change "may" to "verify" if you want encrypt every mail and verify recipient
        root_cmd("postconf -e 'relayhost ='")
        root_cmd("postconf -e 'smtp_sasl_auth_enable = no'")
        root_cmd("postconf -e 'smtp_sasl_password_maps ='")
        root_cmd("printf '' > '/etc/postfix/sasl_passwd'")
    root_cmd("chmod 600 '/etc/postfix/sasl_passwd'")
    root_cmd("postmap '/etc/postfix/sasl_passwd'")

    root_cmd("postconf -e 'smtpd_data_restrictions = reject_unauth_pipelining'")
    root_cmd("postconf -e 'smtpd_helo_required = yes'")
    root_cmd("postconf -e 'smtpd_relay_restrictions = permit_mynetworks permit_sasl_authenticated defer_unauth_destination'")
    root_cmd("postconf -e 'smtpd_sasl_auth_enable = yes'")
    root_cmd("postconf -e 'smtpd_sasl_path = private/auth'")
    root_cmd("postconf -e 'smtpd_sasl_security_options = forward_secrecy'")
    root_cmd("postconf -e 'smtpd_sasl_type = dovecot'")
    root_cmd("postconf -e 'smtpd_tls_auth_only = yes'")
    root_cmd(f"postconf -e 'smtpd_tls_cert_file = {ssl_certificate_path}'")
    root_cmd("test -e '/etc/postfix/dhparams.pem' || openssl dhparam -out '/etc/postfix/dhparams.pem' 2048")
    root_cmd(f"postconf -e 'smtpd_tls_dh1024_param_file = /etc/postfix/dhparams.pem'")
    root_cmd("postconf -e 'smtpd_tls_exclude_ciphers = aNULL, eNULL, EXPORT, DES, RC4, MD5, PSK, aECDH, EDH-DSS-DES-CBC3-SHA, EDH-RSA-DES-CDB3-SHA, KRB5-DES, CBC3-SHA'")
    root_cmd(f"postconf -e 'smtpd_tls_key_file = {ssl_certificate_key_path}'")
    root_cmd("postconf -e 'smtpd_tls_loglevel = 1'")
    root_cmd("postconf -e 'smtpd_tls_received_header = yes'")
    root_cmd("postconf -e 'smtpd_tls_security_level = verify'")
    root_cmd("postconf -e 'smtpd_tls_session_cache_database = btree:${data_directory}/smtpd_scache'")
    root_cmd("postconf -e 'smtpd_use_tls = yes'")

    root_cmd("systemctl restart postfix")
    if args.postfix_test_email_recipients is not None:
        root_cmd(f"""echo 'If you received this email, then postfix send-only email setup works' | mail -s "[sim] [$(hostname)] postfix email setup test" '{args.postfix_test_email_recipients}'""")
        print(f'\033[1;33mSent a test email to {args.postfix_test_email_recipients}\033[m')

if args.postfix or args.postfix_use_gmail_relay_via_sasl or args.postfix_use_gmail_relay_via_sasl_gmail_address is not None or args.postfix_use_gmail_relay_via_sasl_gmail_password is not None:
    setup_postfix()

def setup_emailing_service_errors():
    if setup_emailing_service_errors in completed: return
    completed.add(setup_emailing_service_errors)
    if args.email_service_errors_to is None:
        raise Exception('You need to specify email addresses with --email-service-errors-to <EMAILS>')

    setup_postfix()

    print('\033[1;32m==>\033[0;1m Set up emaling service errors\033[m')
    root_cmd(f"""cat > "/etc/systemd/system/email-service-error@.service" << HEREDOCEND
[Unit]
Description=Email service errors

[Service]
Type=oneshot
NoNewPrivileges=true
PrivateTmp=true
BindReadOnlyPaths=/:/:rbind
ReadWritePaths=/var/spool/postfix
ExecStart=!sh -c 'systemctl status "%i" | mail -s "[sim] [\$\$(hostname)] %i failed" "{args.email_service_errors_to},root@localhost"'
HEREDOCEND""")
    root_cmd('systemctl daemon-reload')

if args.email_service_errors_to is not None:
    setup_emailing_service_errors()

https_port = 443
def setup_sslh():
    if setup_sslh in completed: return
    completed.add(setup_sslh)
    if not args.sslh:
        raise Exception('You need to request setting up sslh with --sslh')

    global https_port
    https_port = 8443

    print('\033[1;32m==>\033[0;1m Install sslh\033[m')
    root_cmd('env DEBIAN_FRONTEND=noninteractive apt -yq install sslh iptables')

    print('\033[1;32m==>\033[0;1m Set up sslh\033[m')
    root_cmd(f"""sed 's@^DAEMON_OPTS=.*@DAEMON_OPTS="--user sslh --transparent --listen 0.0.0.0:443 --ssh 127.0.0.1:22 --tls 127.0.0.1:{https_port} --pidfile /var/run/sslh/sslh.pid"@' -i /etc/default/sslh""")
    root_cmd('mkdir -p "/etc/systemd/system/sslh.service.d"')
    root_cmd(f"""cat > "/etc/systemd/system/sslh.service.d/override.conf" << HEREDOCEND
[Unit]
Before=nginx.service
{'OnFailure=email-service-error@%n.service' if args.email_service_errors_to is not None else ''}

[Service]
# Source: https://github.com/yrutschle/sslh/blob/master/doc/tproxy.md#transparent-proxy-to-one-host
# Set route_localnet = 1 on all interfaces so that ssl can use "localhost" as destination
ExecStartPre=-sysctl -w net.ipv4.conf.default.route_localnet=1
ExecStartPre=-sysctl -w net.ipv4.conf.all.route_localnet=1

# DROP martian packets as they would have been if route_localnet was zero
# Note: packets not leaving the server aren't affected by this, thus sslh will still work
ExecStartPre=-iptables -t raw -A PREROUTING ! -i lo -d 127.0.0.0/8 -j DROP
ExecStartPre=-iptables -t mangle -A POSTROUTING ! -o lo -s 127.0.0.0/8 -j DROP

# Mark all connections made by sslh for special treatment (here sslh is run as user "sslh")
ExecStartPre=-iptables -t nat -A OUTPUT -m owner --uid-owner sslh -p tcp --tcp-flags FIN,SYN,RST,ACK SYN -j CONNMARK --set-xmark 0x01/0x0f

# Outgoing packets that should go to sslh instead have to be rerouted, so mark them accordingly (copying over the connection mark)
ExecStartPre=-iptables -t mangle -A OUTPUT ! -o lo -p tcp -m connmark --mark 0x01/0x0f -j CONNMARK --restore-mark --mask 0x0f

# Configure routing for those marked packets
ExecStartPre=-ip rule add fwmark 0x1 lookup 100
ExecStartPre=-ip route add local 0.0.0.0/0 dev lo table 100
HEREDOCEND""")
    root_cmd('systemctl daemon-reload')
    root_cmd('systemctl restart sslh')
    root_cmd('sleep 1; systemctl is-active sslh || (systemctl status sslh | cat; false)')

if args.sslh:
    setup_sslh()

def setup_nginx():
    if setup_nginx in completed: return
    completed.add(setup_nginx)
    if args.nginx_site_filename is None:
        raise Exception('You need to request setting up nginx with --nginx-site-filename <NGINX_SITE_FILENAME>')
    if args.domain is None:
        raise Exception('You need to specify domain for nginx with --domain <DOMAIN>')

    setup_sim()
    setup_ssl_certificate()

    print('\033[1;32m==>\033[0;1m Install nginx\033[m')
    apt_install('nginx')

    print('\033[1;32m==>\033[0;1m Set up nginx\033[m')
    root_cmd(f"""cat > '/etc/nginx/sites-enabled/{args.nginx_site_filename}' << HEREDOCEND
# Based on https://gist.github.com/gavinhungry/7a67174c18085f4a23eb
server {{
    listen [::]:80;
    listen      80;

    server_name {args.domain};

    # Redirect all non-https requests
    rewrite ^ https://\$host\$request_uri? permanent;
}}

server {{
    listen [::]:{https_port} ssl;
    listen      {https_port} ssl;

    server_name {args.domain};

    # Certificate(s) and private key
    ssl_certificate {ssl_certificate_path};
    ssl_certificate_key {ssl_certificate_key_path};

    ssl_dhparam {ssl_dhparam_path};

    ssl_protocols TLSv1.3 TLSv1.2;
    ssl_prefer_server_ciphers on;
    ssl_ecdh_curve secp521r1:secp384r1;
    ssl_ciphers EECDH+AESGCM:EECDH+AES256;

    ssl_session_cache shared:TLS:2m;
    ssl_buffer_size 4k;

    # OCSP stapling
    ssl_stapling on;
    ssl_stapling_verify on;
    resolver 1.1.1.1 1.0.0.1 [2606:4700:4700::1111] [2606:4700:4700::1001]; # Cloudflare

    # Set HSTS to 365 days
    add_header Strict-Transport-Security 'max-age=31536000; includeSubDomains; preload' always;

    ##################### Above: TLS configuration, below: proxy to sim #####################
    add_header X-Frame-Options sameorigin;
    # add_header X-Content-Type-Options nosniff;
    add_header X-XSS-Protection "1; mode=block";

    client_max_body_size 300M;

    location / {{
        proxy_pass http://{args.sim_local_address};
        proxy_set_header Host \$host;
        proxy_set_header X-Real_IP \$remote_addr;
        proxy_hide_header Strict-Transport-Security;
        proxy_hide_header X-Frame-Options;
        proxy_hide_header X-Content-Type-Options;
        proxy_hide_header X-XSS-Protection;
    }}
}}
HEREDOCEND""")
    root_cmd('rm -f /etc/nginx/sites-enabled/default')
    if args.email_service_errors_to is not None:
        root_cmd('mkdir -p "/etc/systemd/system/nginx.service.d"')
        root_cmd("""cat > "/etc/systemd/system/nginx.service.d/override.conf" << HEREDOCEND
[Unit]
OnFailure=email-service-error@%n.service
HEREDOCEND""")
        root_cmd('systemctl daemon-reload')

    root_cmd('systemctl restart nginx')

if args.nginx_site_filename is not None:
    setup_nginx()

def setup_daily_reboot():
    if setup_daily_reboot in completed: return
    completed.add(setup_daily_reboot)
    if not args.daily_reboot:
        raise Exception('You need to request setting up daily reboot with --daily-reboot')

    print('\033[1;32m==>\033[0;1m Set up daily reboot\033[m')
    root_cmd(f"""cat > /etc/systemd/system/daily-reboot.service << HEREDOCEND
[Unit]
Description=Reboot the machine
{'OnFailure=email-service-error@%n.service' if args.email_service_errors_to else ''}

[Service]
Type=oneshot
ExecStart=/bin/systemctl reboot
HEREDOCEND""")
    root_cmd("""cat > /etc/systemd/system/daily-reboot.timer << HEREDOCEND
[Unit]
Description=Reboot the machine daily

[Timer]
OnCalendar=*-*-* 05:00:00
RandomizedDelaySec=1000
AccuracySec=60

[Install]
WantedBy=timers.target
HEREDOCEND""")
    root_cmd('systemctl daemon-reload');
    root_cmd('systemctl restart daily-reboot.timer');
    root_cmd('systemctl enable daily-reboot.timer');

if args.daily_reboot:
    setup_daily_reboot()

def setup_unattended_upgrades():
    if setup_unattended_upgrades in completed: return
    completed.add(setup_unattended_upgrades)
    if not args.unattended_upgrades:
        raise Exception('You need to request setting up daily reboot with --unattended-upgrades')

    print('\033[1;32m==>\033[0;1m Set up unattended upgrades\033[m')
    apt_install('unattended-upgrades')
    root_cmd('echo unattended-upgrades unattended-upgrades/enable_auto_updates boolean true | debconf-set-selections')
    root_cmd('dpkg-reconfigure -f noninteractive unattended-upgrades')

if args.unattended_upgrades:
    setup_unattended_upgrades()

def setup_daily_backup():
    if setup_daily_backup in completed: return
    completed.add(setup_daily_backup)
    if not args.daily_backup:
        raise Exception('You need to request setting up daily backup with --daily-backup')
    if args.daily_backup_filename is None:
        raise Exception('You need to specify daily backup filename with --daily-backup-filename <BACKUP_FILENAME>')

    setup_sim()

    print('\033[1;32m==>\033[0;1m Set up daily backup\033[m')
    root_cmd("mkdir -p /opt/sim_backups")
    root_cmd("chmod 700 /opt/sim_backups")
    root_cmd(f"""cat > "/etc/systemd/system/sim-backup:$(systemd-escape '{args.user}'):$(systemd-escape --path '{args.sim_path}').service" << HEREDOCEND
[Unit]
Description=Back up Sim and copy backup of {args.user}:{args.sim_path}
{'OnFailure=email-service-error@%n.service' if args.email_service_errors_to else ''}

[Service]
Type=oneshot
User={args.user}
NoNewPrivileges=true
PrivateTmp=true
BindReadOnlyPaths=/:/:rbind
ReadWritePaths="{os.path.join(f"/home/{args.user}", args.sim_path)}/sim"

{f"MountImages=/dev/disk/by-uuid/{args.daily_backup_to_partition}:/tmp/mnt/disk/" if args.daily_backup_to_partition is not None else ''}
{"ReadWritePaths=/opt/sim_backups/" if args.daily_backup_to_partition is None else ''}
{"BindPaths=/opt/sim_backups:/tmp/mnt/disk/sim_backups:rbind" if args.daily_backup_to_partition is None else ''}

# Hide backups from unprivileged users
ExecStartPre=!chmod 0700 /tmp/mnt/
ExecStartPre=!mkdir --parents --mode=0700 /tmp/mnt/disk/sim_backups/
# Run bin/backup as the unprivileged user
ExecStart=nice "{os.path.join(f"/home/{args.user}", args.sim_path)}/sim/bin/backup"
# Copy backup to the protected backup location i.e accessible for privileged users only
ExecStart=!sh -c '\\\\
    git init --bare /tmp/mnt/disk/sim_backups/{args.daily_backup_filename}.git --initial-branch main && \\\\
    git --git-dir=/tmp/mnt/disk/sim_backups/{args.daily_backup_filename}.git config feature.manyFiles true && \\\\
    git --git-dir=/tmp/mnt/disk/sim_backups/{args.daily_backup_filename}.git config core.bigFileThreshold 16m && \\\\
    git --git-dir=/tmp/mnt/disk/sim_backups/{args.daily_backup_filename}.git config pack.packSizeLimit 1g && \\\\
    git --git-dir=/tmp/mnt/disk/sim_backups/{args.daily_backup_filename}.git config gc.bigPackThreshold 500m && \\\\
    git --git-dir=/tmp/mnt/disk/sim_backups/{args.daily_backup_filename}.git config gc.autoPackLimit 0 && \\\\
    git --git-dir=/tmp/mnt/disk/sim_backups/{args.daily_backup_filename}.git config pack.window 0 && \\\\
    git --git-dir=/tmp/mnt/disk/sim_backups/{args.daily_backup_filename}.git config pack.deltaCacheSize 128m && \\\\
    git --git-dir=/tmp/mnt/disk/sim_backups/{args.daily_backup_filename}.git config pack.windowMemory 128m && \\\\
    git --git-dir=/tmp/mnt/disk/sim_backups/{args.daily_backup_filename}.git config pack.threads 1 && \\\\
    git --git-dir=/tmp/mnt/disk/sim_backups/{args.daily_backup_filename}.git config gc.auto 500 && \\\\
    nice git --git-dir=/tmp/mnt/disk/sim_backups/{args.daily_backup_filename}.git fetch "{os.path.join(f"/home/{args.user}", args.sim_path)}/sim" HEAD:HEAD --progress'

# To restore from backup use:
# git --git-dir=/path/to/sim_backups/{args.daily_backup_filename}.git --work-tree=/path/to/where/to/restore/ checkout HEAD -- .
# To check working tree against backup:
# git --git-dir=/path/to/sim_backups/{args.daily_backup_filename}.git --work-tree=/path/to/where/to/restore/ status

HEREDOCEND""")
    root_cmd(f"""cat > "/etc/systemd/system/sim-backup:$(systemd-escape '{args.user}'):$(systemd-escape --path '{args.sim_path}').timer" << HEREDOCEND
[Unit]
Description=Daily backup of {args.user}:{args.sim_path}

[Timer]
OnCalendar=*-*-* 05:20:00
RandomizedDelaySec=100
AccuracySec=60
Persistent=true

[Install]
WantedBy=timers.target
HEREDOCEND""")
    root_cmd('systemctl daemon-reload')
    root_cmd(f"systemctl restart sim-backup:$(systemd-escape '{args.user}'):$(systemd-escape --path '{args.sim_path}').timer")
    root_cmd(f"systemctl enable sim-backup:$(systemd-escape '{args.user}'):$(systemd-escape --path '{args.sim_path}').timer")

if args.daily_backup or args.daily_backup_filename is not None or args.daily_backup_to_partition is not None:
    setup_daily_backup()

def setup_emailing_access_logs():
    if setup_emailing_access_logs in completed: return
    completed.add(setup_emailing_access_logs)
    if args.email_access_logs_to is None:
        raise Exception('You need to specify email addresses with --email-access-logs-to <EMAILS>')

    setup_postfix()

    print('\033[1;32m==>\033[0;1m Set up emailing access logs\033[m')
    root_cmd(f"""cat > '/etc/systemd/system/email-access-logs.service' << HEREDOCEND
[Unit]
Description=Email the access logs
Before=sshd.service
After=postfix.service
{'OnFailure=email-service-error@%n.service' if args.email_service_errors_to else ''}

[Service]
Type=notify
NoNewPrivileges=true
PrivateTmp=true
BindReadOnlyPaths=/:/:rbind
ReadWritePaths=/var/spool/postfix
ExecStart=sh -c "\\\\
    MAINPID=\$\$\$\$; \\\\
    journalctl --follow --identifier=sshd --lines=100 | \\\\
    (systemd-notify --ready --pid=\$\$MAINPID; \\\\
     grep ': Accepted' --line-buffered | \\\\
     xargs -L 1 --delimiter='\\n' sh -c 'echo \\"\$\$@\\" | mail -s \\"[sim] [\$\$(hostname)] Access logs\\" \\"{args.email_access_logs_to}\\"' _)"

[Install]
WantedBy=multi-user.target
HEREDOCEND""")
    root_cmd('systemctl daemon-reload')
    root_cmd('systemctl restart email-access-logs.service')
    root_cmd('systemctl enable email-access-logs.service')

if args.email_access_logs_to is not None:
    setup_emailing_access_logs()

def setup_emailing_sim_error_logs():
    if setup_emailing_sim_error_logs in completed: return
    completed.add(setup_emailing_sim_error_logs)
    if args.email_sim_error_logs_to is None:
        raise Exception('You need to specify email addresses with --email-sim-error-logs-to <EMAILS>')

    setup_sim()
    setup_postfix()

    print('\033[1;32m==>\033[0;1m Set up emailing access logs\033[m')
    for kind in ['web', 'job']:
        root_cmd(f"""cat > "/etc/systemd/system/email-sim-{kind}-error-logs:$(systemd-escape '{args.user}'):$(systemd-escape --path '{args.sim_path}').service" << HEREDOCEND
[Unit]
Description=Email the Sim {kind} error logs of {args.user}:{args.sim_path}
Before=mariadb.service
After=postfix.service
{'OnFailure=email-service-error@%n.service' if args.email_service_errors_to else ''}

[Service]
Type=notify
NoNewPrivileges=true
PrivateTmp=true
BindReadOnlyPaths=/:/:rbind
ReadWritePaths=/var/spool/postfix
ExecStart=sh -c "\\
    FILE_SIZE=\$\$(stat --format=%%s \\"{os.path.join(f"/home/{args.user}", args.sim_path)}/sim/logs/{kind}-server-error.log\\"); \\\\
    systemd-notify --ready && \\\\
    touch /tmp/lock && \\\\
    tail --follow \\"{os.path.join(f"/home/{args.user}", args.sim_path)}/sim/logs/{kind}-server-error.log\\" --bytes=+\$\$((FILE_SIZE + 1)) | \\\\
    xargs -L 1 --delimiter='\\n' sh -c '\\\\
        LINE=\\"\$\$1\\" \\\\
        exec 3<>/tmp/lock && \\\\
        flock 3 && \\\\
        if ! test -f /tmp/chunk; then \\\\
            ( \\\\
                exec 3<&- && \\\\
                sleep 4 && \\\\
                exec 3<>/tmp/lock && \\\\
                flock 3 && \\\\
                cat /tmp/chunk | mail -s \\"[sim] [\$\$(hostname)] {args.sim_path}: {kind} error logs\\" \\"{args.email_sim_error_logs_to}\\"; \\\\
                rm /tmp/chunk \\\\
            )& \\\\
        fi; \\\\
        echo \\"\$\$LINE\\" >> /tmp/chunk \\\\
    ' _"

[Install]
WantedBy=multi-user.target
HEREDOCEND""")
        root_cmd('systemctl daemon-reload')
        root_cmd(f"""systemctl restart email-sim-{kind}-error-logs:$(systemd-escape '{args.user}'):$(systemd-escape --path '{args.sim_path}').service""")
        root_cmd(f"""systemctl enable email-sim-{kind}-error-logs:$(systemd-escape '{args.user}'):$(systemd-escape --path '{args.sim_path}').service""")

if args.email_sim_error_logs_to is not None:
    setup_emailing_sim_error_logs()

print('\033[1;32m==>\033[0;1m Done.\033[m')
if args.sim_path is not None:
    print("\033[1;33mRemember to change password for Sim's root user: sim\033[m")
print('\033[1;32mSetup successful.\033[m')
