#!/bin/sh
set -e

# Configuration constants
sim_address="127.0.0.1:8888"
database_user="camp_sim"
database_name="camp_simdb"
database_user_password="camp_simpasswd"
setup_nginx=true
setup_cron=true

user=$1
[ -z "$user" ] && echo "Usage: $0 <user>" && false
[ "$(whoami)" = "root" ] || (echo "error: you cannot run this script unless you are root" && false)

/bin/echo -e '\033[1;32m==>\033[0;1m Prepare apt for installing packages\033[m'
echo 'deb http://deb.debian.org/debian buster-backports main' > /etc/apt/sources.list.d/buster-backports.list
apt update

/bin/echo -e '\033[1;32m==>\033[0;1m Install required packages\033[m'
apt install sudo git g++-multilib mariadb-server libmariadbclient-dev libseccomp-dev libzip-dev libssl-dev pkgconf expect -y
apt install -t buster-backports meson clang-11 -y

/bin/echo -e '\033[1;32m==>\033[0;1m Prepare database and database user\033[m'
expect -c 'spawn mysql_secure_installation; send "\rn\rY\rY\rY\rY\r"; interact'
mysql -e "CREATE USER IF NOT EXISTS '${database_user}'@localhost IDENTIFIED BY '${database_user_password}'"
mysql -e "CREATE DATABASE IF NOT EXISTS \`${database_name}\`"
mysql -e "GRANT ALL ON \`${database_name}\`.* TO '${database_user}'@localhost"

user_cmd() {
    # Use sudo instead of su, because su does not kill the descendant processes when
    # the script is being killed, but sudo does
    sudo -u "$user" -i sh -c "$1"
}

/bin/echo -e '\033[1;32m==>\033[0;1m Setup sim repo\033[m'
user_cmd 'test -e camp_sim/' || user_cmd 'git clone --recursive https://github.com/varqox/sim -b develop camp_sim'

/bin/echo -e '\033[1;32m==>\033[0;1m Build sim\033[m'
user_cmd '(cd camp_sim && CC=clang-11 CXX=clang++-11 meson setup build/ -Dbuildtype=release)'
user_cmd '(cd camp_sim && meson configure build/ --prefix "$(pwd)/sim")'
user_cmd '(cd camp_sim && meson compile -C build)'
user_cmd '(cd camp_sim && meson test -C build)'

/bin/echo -e '\033[1;32m==>\033[0;1m Configure sim\033[m'
user_cmd "(cd camp_sim && mkdir -p sim && /bin/echo -e \"user: '${database_user}'\npassword: '${database_user_password}'\ndb: '${database_name}'\nhost: 'localhost'\" > sim/.db.config)"
user_cmd '(cd camp_sim && meson install -C build)'
# Change sim address
user_cmd "grep -P '^address:.*\$' camp_sim/sim/sim.conf -q" || (echo "error: couldn't find address in sim.conf" && false)
user_cmd "sed 's/^address.*\$/address: ${sim_address}/' camp_sim/sim/sim.conf -i" || (echo "error: couldn't find address in sim.conf" && false)

/bin/echo -e '\033[1;32m==>\033[0;1m Run sim\033[m'
user_cmd 'camp_sim/sim/manage restart -b'

if ${setup_nginx}; then
    /bin/echo -e '\033[1;32m==>\033[0;1m Install nginx\033[m'
    apt install nginx -y

    /bin/echo -e '\033[1;32m==>\033[0;1m Generate SSL certificate for nginx\033[m'
    mkdir -p /etc/nginx/camp_sim_ssl/
    chmod 700 /etc/nginx/camp_sim_ssl/
    expect -c 'spawn openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout /etc/nginx/camp_sim_ssl/camp_sim.key -out /etc/nginx/camp_sim_ssl/camp_sim.crt; send ".\r.\r.\r.\r.\rcamp_sim\r.\r"; interact'
    test -e /etc/nginx/camp_sim_ssl/dhparam.pem || openssl dhparam -out /etc/nginx/camp_sim_ssl/dhparam.pem 2048

    /bin/echo -e '\033[1;32m==>\033[0;1m Setup nginx\033[m'
    cat > /etc/nginx/sites-enabled/camp_sim << HEREDOCEND
# Based on https://gist.github.com/gavinhungry/7a67174c18085f4a23eb
server {
    listen [::]:80 default_server;
    listen      80 default_server;
    server_name _;

    # Redirect all non-https requests
    rewrite ^ https://\$host\$request_uri? permanent;
}

server {
    listen [::]:443 ssl default_server;
    listen      443 ssl default_server;

    server_name _;

    # Certificate(s) and private key
    ssl_certificate /etc/nginx/camp_sim_ssl/camp_sim.crt;
    ssl_certificate_key /etc/nginx/camp_sim_ssl/camp_sim.key;

    ssl_dhparam /etc/nginx/camp_sim_ssl/dhparam.pem;

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
    proxy_pass http://${sim_address};
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
    systemctl restart nginx
fi

if ${setup_cron}; then
    /bin/echo -e '\033[1;32m==>\033[0;1m Install cron\033[m'
    apt install cron -y

    /bin/echo -e '\033[1;32m==>\033[0;1m Setup cron to start sim at system startup\033[m'
    crontab_line=$(user_cmd "echo \"@reboot sh -c 'until test -e /var/run/mysqld/mysqld.sock; do sleep 0.4; done; \\\"\$(pwd)/camp_sim/sim/manage\\\" start&'\"")
    if ! user_cmd 'crontab -l' > /dev/null; then
        # No crontab
        echo "${crontab_line}" | user_cmd 'crontab -'
    elif ! user_cmd 'crontab -l' | grep -F "${crontab_line}" -q; then
        # crontab without ${crontab_line}
        (user_cmd 'crontab -l'; echo "${crontab_line}") | user_cmd "crontab -"
    fi
fi

/bin/echo -e '\033[1;32m==>\033[0;1m Done.\033[m'
/bin/echo -e '\033[1;33mRemember to change password for user: sim\033[m'
