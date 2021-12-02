#!/bin/sh
set -e

[ $# -lt 2 ] && /bin/echo -e "Usage: $0 <user> <server_domain>...\nE.g. $0 some_user sim.server.com sim2.server.com" && false
[ "$(whoami)" = "root" ] || (echo "error: you cannot run this script unless you are root" && false)

user=$1
shift
main_domain="$1"
domains_hex=$(for dom in "$@"; do printf "${dom}\000"; done | xxd -plain)

# Configuration options
meta_sim_instance_name='sim' # Default name, you can change the values below in a more fine-grained way
meta_email_receivers='' # Default email receivers separated by commas, you can change the values below in a more fine-grained way
sim_local_address='127.0.0.1:8888'

# More fine-grained configuration options
sim_src_dir="$(su - "$user" -c pwd)/${meta_sim_instance_name}"
database_user="${meta_sim_instance_name}"
database_name="${meta_sim_instance_name}db"
database_user_password="${meta_sim_instance_name}_passwd"
certbot_cert_name="${meta_sim_instance_name}"
email_service_errors_recepients="root@localhost, ${meta_email_receivers}"
setup_sslh=false
setup_nginx=true
nginx_sim_site_name="${meta_sim_instance_name}"
nginx_sim_ssl_certs_dir="/etc/nginx/${nginx_sim_site_name}_ssl"
setup_cron_to_start_sim_at_boot=true
setup_daily_reboot=true
setup_daily_backup=true
backup_name="${meta_sim_instance_name}"
backup_to_unmounted_partition=false
backup_unmounted_partition_uuid='' # E.g. value '7920e99f-984f-4484-9959-aac74f132b47'
setup_mail_sending_via_postfix=false # If true, installs and configures postfix, but only to send emails, not receive them
postfix_mail_send_test_recipients="${meta_email_receivers}" # Separated by commas
# Sending to Gmail needs SPF TXT record in DNS, to add it see: https://support.google.com/a/answer/10684623
# If you cannot make it work, you can use SASL with Gmail as the relay, just set the below variable to true and the two variables below
postfix_use_gmail_relay_via_sasl=false
postfix_use_gmail_relay_via_sasl_gmail_address='' # E.g. value 'abc@gmail.com'
postfix_use_gmail_relay_via_sasl_gmail_password='' # E.g. value 'my_plain_text_password', you may need to read this: https://support.google.com/accounts/answer/6010255
setup_emailing_access_logs=false
email_access_logs_recipients="${meta_email_receivers}" # Separated by commas
setup_emailing_sim_error_logs=false
email_sim_error_logs_recipients="${meta_email_receivers}" # Separated by commas
setup_unattended_upgrades=true

exit_message='\033[1;31mSetup failed\033[m'
trap '/bin/echo -e "${exit_message}"' EXIT

/bin/echo -e '\033[1;32m==>\033[0;1m Prepare apt for installing packages\033[m'
apt update

/bin/echo -e '\033[1;32m==>\033[0;1m Install required packages\033[m'
apt install sudo git g++-multilib fpc mariadb-server libmariadb-dev libseccomp-dev libzip-dev libssl-dev pkgconf expect meson -y

/bin/echo -e '\033[1;32m==>\033[0;1m Prepare database and database user\033[m'
expect -c 'spawn mysql_secure_installation; send "\ry\rn\ry\ry\ry\ry\r"; interact'
mysql -e "CREATE USER IF NOT EXISTS '${database_user}'@localhost IDENTIFIED BY '${database_user_password}'"
mysql -e "CREATE DATABASE IF NOT EXISTS \`${database_name}\`"
mysql -e "GRANT ALL ON \`${database_name}\`.* TO '${database_user}'@localhost"

user_cmd() {
    # Use sudo instead of su, because su does not kill the descendant processes when
    # the script is being killed, but sudo does
    sudo -u "$user" -i sh -c "$1"
}

/bin/echo -e '\033[1;32m==>\033[0;1m Setup sim repo\033[m'
user_cmd "test -e '${sim_src_dir}'/" || user_cmd "git clone --recursive https://github.com/varqox/sim --branch develop '${sim_src_dir}'"

/bin/echo -e '\033[1;32m==>\033[0;1m Build sim\033[m'
user_cmd "(cd '${sim_src_dir}' && meson setup build/ -Dbuildtype=release)"
user_cmd "(cd '${sim_src_dir}' && meson configure build/ --prefix \"\$(pwd)/sim\")"
user_cmd "(cd '${sim_src_dir}' && meson compile -C build)"
user_cmd "(cd '${sim_src_dir}' && meson test -C build)"

/bin/echo -e '\033[1;32m==>\033[0;1m Configure sim\033[m'
user_cmd "(cd '${sim_src_dir}' && mkdir --parents sim && /bin/echo -e \"user: '${database_user}'\npassword: '${database_user_password}'\ndb: '${database_name}'\nhost: 'localhost'\" > sim/.db.config)"
user_cmd "(cd '${sim_src_dir}' && meson install -C build)"
# Change sim address
user_cmd "grep -P '^address:.*\$' '${sim_src_dir}/sim/sim.conf' -q" || (echo "error: couldn't find address in sim.conf" && false)
user_cmd "sed 's/^address.*\$/address: ${sim_local_address}/' '${sim_src_dir}/sim/sim.conf' -i" || (echo "error: couldn't find address in sim.conf" && false)

/bin/echo -e '\033[1;32m==>\033[0;1m Run sim\033[m'
apt install curl -y
user_cmd "'${sim_src_dir}/sim/manage' stop"
if (curl --silent "${sim_local_address}" > /dev/null; test $? -eq 7); then
    user_cmd "'${sim_src_dir}/sim/manage' restart -b"
else
    echo "error: address ${sim_local_address} is already in use" && false
fi

/bin/echo -e '\033[1;32m==>\033[0;1m Install certbot\033[m'
apt install certbot python3-certbot-nginx -y

/bin/echo -e '\033[1;32m==>\033[0;1m Run certbot\033[m'
echo "${domains_hex}" | xxd -revert -plain | \
    xargs --null --max-lines=1 printf "--domain\000%s\000" | \
    xargs --null certbot certonly --non-interactive --register-unsafely-without-email --agree-tos --nginx --expand --cert-name "${certbot_cert_name}"
/bin/echo -e '\033[1;32m==>\033[0;1m Setup emailing service errors\033[m'
cat > "/etc/systemd/system/email-service-error@.service" << HEREDOCEND
[Unit]
Description=Email service errors

[Service]
Type=oneshot
NoNewPrivileges=true
PrivateTmp=true
BindReadOnlyPaths=/:/:rbind
ReadWritePaths=/var/spool/postfix
ExecStart=!sh -c 'systemctl status "%i" | mail -s "[sim] [\$\$(hostname)] %i failed" "${email_service_errors_recepients}"'
HEREDOCEND
systemctl daemon-reload

nginx_https_port=443
if ${setup_sslh}; then
    nginx_https_port=8443
    /bin/echo -e '\033[1;32m==>\033[0;1m Install sslh\033[m'
    env DEBIAN_FRONTEND=noninteractive apt -yq install sslh iptables

    /bin/echo -e '\033[1;32m==>\033[0;1m Setup sslh\033[m'
    sed "s@^DAEMON_OPTS=.*@DAEMON_OPTS=\"--user sslh --transparent --listen 0.0.0.0:443 --ssh 127.0.0.1:22 --tls 127.0.0.1:${nginx_https_port} --pidfile /var/run/sslh/sslh.pid\"@" -i /etc/default/sslh
    mkdir -p "/etc/systemd/system/sslh.service.d"
    cat > "/etc/systemd/system/sslh.service.d/override.conf" << HEREDOCEND
[Unit]
Before=nginx.service
OnFailure=email-service-error@%n.service

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
HEREDOCEND
    systemctl daemon-reload
    systemctl restart sslh
fi

if ${setup_nginx}; then
    /bin/echo -e '\033[1;32m==>\033[0;1m Install nginx\033[m'
    apt install nginx -y

    /bin/echo -e '\033[1;32m==>\033[0;1m Generate dhparam for nginx\033[m'
    mkdir -p "${nginx_sim_ssl_certs_dir}/"
    chmod 700 "${nginx_sim_ssl_certs_dir}/"
    test -e "${nginx_sim_ssl_certs_dir}/${nginx_sim_site_name}_dhparam.pem" || openssl dhparam -out "${nginx_sim_ssl_certs_dir}/${nginx_sim_site_name}_dhparam.pem" 2048

    /bin/echo -e '\033[1;32m==>\033[0;1m Setup nginx\033[m'
    cat > /etc/nginx/sites-enabled/${nginx_sim_site_name} << HEREDOCEND
# Based on https://gist.github.com/gavinhungry/7a67174c18085f4a23eb
server {
    listen [::]:80;
    listen      80;

$(echo "${domains_hex}" | xxd -revert -plain | xargs --null --max-lines=1 printf "    server_name %s;\n")

    # Redirect all non-https requests
    rewrite ^ https://\$host\$request_uri? permanent;
}

server {
    listen [::]:${nginx_https_port} ssl;
    listen      ${nginx_https_port} ssl;

$(echo "${domains_hex}" | xxd -revert -plain | xargs --null --max-lines=1 printf "    server_name %s;\n")

    # Certificate(s) and private key
    ssl_certificate /etc/letsencrypt/live/${certbot_cert_name}/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/${certbot_cert_name}/privkey.pem;

    ssl_dhparam ${nginx_sim_ssl_certs_dir}/${nginx_sim_site_name}_dhparam.pem;

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

    location / {
        proxy_pass http://${sim_local_address};
        proxy_set_header Host \$host;
        proxy_set_header X-Real_IP \$remote_addr;
        proxy_hide_header Strict-Transport-Security;
        proxy_hide_header X-Frame-Options;
        proxy_hide_header X-Content-Type-Options;
        proxy_hide_header X-XSS-Protection;
    }
}
HEREDOCEND
    rm -f /etc/nginx/sites-enabled/default

    mkdir -p "/etc/systemd/system/nginx.service.d"
    cat > "/etc/systemd/system/nginx.service.d/override.conf" << HEREDOCEND
[Unit]
OnFailure=email-service-error@%n.service
HEREDOCEND

    systemctl daemon-reload
    systemctl restart nginx
fi

# Args: <crontab_line>
add_line_to_user_crontab() {
    if ! command -v crontab > /dev/null; then
        /bin/echo -e '\033[1;32m==>\033[0;1m Install cron\033[m'
        apt install cron -y
    fi
    if ! user_cmd 'crontab -l' > /dev/null; then
        # No crontab
        echo "$1" | user_cmd 'crontab -'
    elif ! user_cmd 'crontab -l' | grep -F "$1" -q; then
        # crontab without $1
        (user_cmd 'crontab -l'; echo "$1") | user_cmd "crontab -"
    fi
}

if ${setup_cron_to_start_sim_at_boot}; then
    /bin/echo -e '\033[1;32m==>\033[0;1m Setup cron to start sim at system startup\033[m'
    add_line_to_user_crontab "@reboot sh -c 'until test -e /var/run/mysqld/mysqld.sock; do sleep 0.4; done; '${sim_src_dir}/sim/manage' start&'"
fi

if ${setup_daily_reboot}; then
    /bin/echo -e '\033[1;32m==>\033[0;1m Setup daily reboot\033[m'
    cat > /etc/systemd/system/daily-reboot.service << HEREDOCEND
[Unit]
Description=Reboot the machine
OnFailure=email-service-error@%n.service

[Service]
Type=oneshot
ExecStart=/bin/systemctl reboot
HEREDOCEND
    cat > /etc/systemd/system/daily-reboot.timer << HEREDOCEND
[Unit]
Description=Reboot the machine daily

[Timer]
OnCalendar=*-*-* 05:00:00
RandomizedDelaySec=1000
AccuracySec=60

[Install]
WantedBy=timers.target
HEREDOCEND
    systemctl daemon-reload
    systemctl restart daily-reboot.timer
    systemctl enable daily-reboot.timer
fi

if ${setup_daily_backup}; then
    /bin/echo -e '\033[1;32m==>\033[0;1m Setup daily backup\033[m'
    mkdir --parents --mode=0700 /opt/sim_backups
    unit_name="sim-backup:$(systemd-escape "${user}"):$(systemd-escape --path "${sim_src_dir}")"
    cat > "/etc/systemd/system/${unit_name}.service" << HEREDOCEND
[Unit]
Description=Back up Sim and copy backup
OnFailure=email-service-error@%n.service

[Service]
Type=oneshot
User=${user}
NoNewPrivileges=true
PrivateTmp=true
BindReadOnlyPaths=/:/:rbind
ReadWritePaths="${sim_src_dir}/sim"

# If you want to backup to an unmounted partition (variant 1), uncomment the below
# "MountImages" line, change "UUID" to UUID of your partition, and comment the below
# "ReadWritePaths" and "BindPaths" lines (variant 2)

# Variant 1: backup to an unmounted partition
$(${backup_to_unmounted_partition} || printf '#')MountImages=/dev/disk/by-uuid/$(test -z "${backup_unmounted_partition_uuid}" && printf 'UUID' || printf "${backup_unmounted_partition_uuid}"):/tmp/mnt/disk/

# Variant 2: backup to a mounted directory: /opt/sim_backups
$(${backup_to_unmounted_partition} && printf '#')ReadWritePaths=/opt/sim_backups/
$(${backup_to_unmounted_partition} && printf '#')BindPaths=/opt/sim_backups:/tmp/mnt/disk/sim_backups:rbind

# Hide backups from unprivileged users
ExecStartPre=!chmod 0700 /tmp/mnt/
ExecStartPre=!mkdir --parents --mode=0700 /tmp/mnt/disk/sim_backups/
# Run bin/backup as the unprivileged user
ExecStart=nice "${sim_src_dir}/sim/bin/backup"
# Copy backup to the protected backup location i.e accessible for privileged users only
ExecStart=!sh -c '\\
    git init --bare /tmp/mnt/disk/sim_backups/${backup_name}.git --initial-branch main && \\
    git --git-dir=/tmp/mnt/disk/sim_backups/${backup_name}.git config feature.manyFiles true && \\
    git --git-dir=/tmp/mnt/disk/sim_backups/${backup_name}.git config core.bigFileThreshold 16m && \\
    git --git-dir=/tmp/mnt/disk/sim_backups/${backup_name}.git config pack.packSizeLimit 1g && \\
    git --git-dir=/tmp/mnt/disk/sim_backups/${backup_name}.git config gc.bigPackThreshold 500m && \\
    git --git-dir=/tmp/mnt/disk/sim_backups/${backup_name}.git config gc.autoPackLimit 0 && \\
    git --git-dir=/tmp/mnt/disk/sim_backups/${backup_name}.git config pack.window 0 && \\
    git --git-dir=/tmp/mnt/disk/sim_backups/${backup_name}.git config pack.deltaCacheSize 128m && \\
    git --git-dir=/tmp/mnt/disk/sim_backups/${backup_name}.git config pack.windowMemory 128m && \\
    git --git-dir=/tmp/mnt/disk/sim_backups/${backup_name}.git config pack.threads 1 && \\
    git --git-dir=/tmp/mnt/disk/sim_backups/${backup_name}.git config gc.auto 500 && \\
    nice git --git-dir=/tmp/mnt/disk/sim_backups/${backup_name}.git fetch "${sim_src_dir}/sim" HEAD:HEAD --progress'

# To restore from backup use:
# git --git-dir=/path/to/sim_backups/${backup_name}.git --work-tree=/path/to/where/to/restore/ checkout HEAD -- .
# To check working tree against backup:
# git --git-dir=/path/to/sim_backups/${backup_name}.git --work-tree=/path/to/where/to/restore/ status
HEREDOCEND
    cat > "/etc/systemd/system/${unit_name}.timer" << HEREDOCEND
[Unit]
Description=Backup sim daily

[Timer]
OnCalendar=*-*-* 05:20:00
RandomizedDelaySec=100
AccuracySec=60
Persistent=true

[Install]
WantedBy=timers.target
HEREDOCEND
    systemctl daemon-reload
    systemctl restart "${unit_name}.timer"
    systemctl enable "${unit_name}.timer"
fi

if ${setup_mail_sending_via_postfix}; then
    /bin/echo -e '\033[1;32m==>\033[0;1m Install postfix\033[m'
    env DEBIAN_FRONTEND=noninteractive apt -yq install postfix mailutils
    test -e "/etc/postfix/dhparams.pem" || openssl dhparam -out "/etc/postfix/dhparams.pem" 2048

    /bin/echo -e '\033[1;32m==>\033[0;1m Setup postfix for sending emails\033[m'
    postconf -e "inet_interfaces = loopback-only"
    postconf -e "myhostname = ${main_domain}"
    postconf -e "myorigin = \$myhostname"
    postconf -e "smtp_sasl_path = private/auth"
    postconf -e "smtp_sasl_security_options = noanonymous"
    postconf -e "smtp_sasl_type = cyrus"
    postconf -e "smtp_tls_CApath = /etc/ssl/certs"
    postconf -e "smtp_tls_exclude_ciphers = aNULL, eNULL, EXPORT, DES, RC4, MD5, PSK, aECDH, EDH-DSS-DES-CBC3-SHA, EDH-RSA-DES-CDB3-SHA, KRB5-DES, CBC3-SHA"
    postconf -e "smtp_tls_loglevel = 1"
    postconf -e "smtp_tls_session_cache_database = btree:\${data_directory}/smtp_scache"
    postconf -e "smtp_use_tls = yes"
    if ${postfix_use_gmail_relay_via_sasl}; then
        postconf -e "smtp_tls_security_level = verify"
        postconf -e "relayhost = [smtp.gmail.com]:587"
        postconf -e "smtp_sasl_auth_enable = yes"
        postconf -e "smtp_sasl_password_maps = hash:/etc/postfix/sasl_passwd"
        echo "[smtp.gmail.com]:587 ${postfix_use_gmail_relay_via_sasl_gmail_address}:${postfix_use_gmail_relay_via_sasl_gmail_password}" > "/etc/postfix/sasl_passwd"
        chmod 600 "/etc/postfix/sasl_passwd"
        postmap "/etc/postfix/sasl_passwd"
    else
        postconf -e "smtp_tls_security_level = may" # change "may" to "verify" if you want encrypt every mail and verify recipient
        postconf -e "relayhost ="
        postconf -e "smtp_sasl_auth_enable = no"
        postconf -e "smtp_sasl_password_maps ="
        echo -n > "/etc/postfix/sasl_passwd"
        chmod 600 "/etc/postfix/sasl_passwd"
        postmap "/etc/postfix/sasl_passwd"
    fi
    postconf -e "smtpd_data_restrictions = reject_unauth_pipelining"
    postconf -e "smtpd_helo_required = yes"
    postconf -e "smtpd_relay_restrictions = permit_mynetworks permit_sasl_authenticated defer_unauth_destination"
    postconf -e "smtpd_sasl_auth_enable = yes"
    postconf -e "smtpd_sasl_path = private/auth"
    postconf -e "smtpd_sasl_security_options = forward_secrecy"
    postconf -e "smtpd_sasl_type = dovecot"
    postconf -e "smtpd_tls_auth_only = yes"
    postconf -e "smtpd_tls_cert_file = /etc/letsencrypt/live/${certbot_cert_name}/fullchain.pem"
    postconf -e "smtpd_tls_dh1024_param_file = /etc/postfix/dhparams.pem"
    postconf -e "smtpd_tls_exclude_ciphers = aNULL, eNULL, EXPORT, DES, RC4, MD5, PSK, aECDH, EDH-DSS-DES-CBC3-SHA, EDH-RSA-DES-CDB3-SHA, KRB5-DES, CBC3-SHA"
    postconf -e "smtpd_tls_key_file = /etc/letsencrypt/live/${certbot_cert_name}/privkey.pem"
    postconf -e "smtpd_tls_loglevel = 1"
    postconf -e "smtpd_tls_received_header = yes"
    postconf -e "smtpd_tls_security_level = verify"
    postconf -e "smtpd_tls_session_cache_database = btree:\${data_directory}/smtpd_scache"
    postconf -e "smtpd_use_tls = yes"

    systemctl restart postfix
    echo "If you received this email, then postfix send-only email setup works" | mail -s "[sim] [$(hostname)] postfix email setup test" "${postfix_mail_send_test_recipients}" -r "root@${main_domain}"
    /bin/echo -e "\033[1;33mSent a test email to ${postfix_mail_send_test_recipients}\033[m"
fi

if ${setup_emailing_access_logs}; then
    /bin/echo -e '\033[1;32m==>\033[0;1m Setup emailing access logs\033[m'
    cat > "/etc/systemd/system/email-access-logs.service" << HEREDOCEND
[Unit]
Description=Email the access logs
Before=sshd.service
OnFailure=email-service-error@%n.service

[Service]
Type=notify
NoNewPrivileges=true
PrivateTmp=true
BindReadOnlyPaths=/:/:rbind
ReadWritePaths=/var/spool/postfix
ExecStart=sh -c "\\
    MAINPID=\$\$\$\$; \\
    journalctl --follow --identifier=sshd --lines=0 | \\
    (read x; systemd-notify --ready --pid=\$\$MAINPID; \\
     grep ': Accepted' --line-buffered | \\
     xargs --max-lines=1 --delimiter='\n' sh -c 'echo \"\$\$@\" | mail -s \"[sim] [\$\$(hostname)] Access logs\" \"${email_access_logs_recipients}\"' _)"

[Install]
WantedBy=multi-user.target
HEREDOCEND
    systemctl daemon-reload
    systemctl restart "email-access-logs.service"
    systemctl enable "email-access-logs.service"
fi

if ${setup_emailing_sim_error_logs}; then
    /bin/echo -e '\033[1;32m==>\033[0;1m Setup emailing the sim error logs\033[m'
    for kind in web job; do
        unit_name="email-sim-${kind}-server-error-logs:$(systemd-escape "${user}"):$(systemd-escape --path "${sim_src_dir}")"
        cat > "/etc/systemd/system/${unit_name}.service" << HEREDOCEND
[Unit]
Description=Email the sim ${kind} server error logs
Before=mariadb.service
OnFailure=email-service-error@%n.service

[Service]
Type=notify
NoNewPrivileges=true
PrivateTmp=true
BindReadOnlyPaths=/:/:rbind
ReadWritePaths=/var/spool/postfix
ExecStart=sh -c "\\
    FILE_SIZE=\$\$(stat --format=%%s \"${sim_src_dir}/sim/logs/${kind}-server-error.log\"); \\
    systemd-notify --ready && \\
    tail --follow \"${sim_src_dir}/sim/logs/${kind}-server-error.log\" --bytes=+\$\$((FILE_SIZE + 1)) | \\
    xargs --max-lines=1 --delimiter='\n' sh -c '\\
        exec 3>/tmp/lock && \\
        flock 3 && \\
        echo \"\$\$@\" | sh -c \"\\
            if test ! -f /tmp/chunk; then \\
                (exec 3>&- && \\
                 sleep 4 && \\
                 exec 3>/tmp/lock && \\
                 flock 3 && \\
                 cat /tmp/chunk | mail -s \\\\\"[sim] [\$\$(hostname)] ${meta_sim_instance_name}: $(echo "${kind}" | awk '{ print toupper(substr($0,1,1))substr($0,2) }') server error logs\\\\\" \\\\\"${email_sim_error_logs_recipients}\\\\\"; \\
                 rm /tmp/chunk)& \\
            fi; \\
            cat >> /tmp/chunk\"' _"

[Install]
WantedBy=multi-user.target
HEREDOCEND
        systemctl daemon-reload
        systemctl restart "${unit_name}.service"
        systemctl enable "${unit_name}.service"
    done
fi

if ${setup_unattended_upgrades}; then
    /bin/echo -e '\033[1;32m==>\033[0;1m Setup unattended-upgrades\033[m'
    apt install unattended-upgrades -y
    echo unattended-upgrades unattended-upgrades/enable_auto_updates boolean true | debconf-set-selections
    dpkg-reconfigure -f noninteractive unattended-upgrades
fi

/bin/echo -e '\033[1;32m==>\033[0;1m Done.\033[m'
/bin/echo -e "\033[1;33mRemember to change password for Sim's root user: sim\033[m"
exit_message="\033[1;32mSetup successful.\033[m"
