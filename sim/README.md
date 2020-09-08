# Sim [![Build Status](https://travis-ci.org/varqox/sim.svg?branch=master)](https://travis-ci.org/varqox/sim) [![Coverity Scan Build Status](https://scan.coverity.com/projects/6466/badge.svg)](https://scan.coverity.com/projects/varqox-sim) [![Gitter chat](https://badges.gitter.im/varqox/sim.png)](https://gitter.im/varqox/sim)

Sim is an open source platform for carrying out algorithmic contests

<div align="center">
  <img src="http://varqox.github.io/img/sim.png"/>
</div>


## Installation

### Dependencies:

- gcc/g++ (with 32 bit support &ndash; for compiling submissions) with C++17 support (Debian package: _g++-multilib_)
- MariaDB (Debian package: _mariadb-server_)
- MariaDB client library (Debian packages: _libmariadbclient-dev_)
- libseccomp (Debian package: _libseccomp-dev_)
- GNU/Make (Debian package: _make_ version >= 4.2.1)
- libzip  (Debian package: _libzip-dev_)

#### Debian

  ```sh
  sudo apt-get install g++-multilib mariadb-server libmariadbclient-dev libseccomp-dev libzip-dev make libssl-dev
  ```
  
  Ubuntu is not officially supported, you may try it, it may (not) work. _Modern_ versions of some of the above packages are needed to build sim sucessfully.

#### Arch Linux

  ```sh
  sudo pacman -S gcc mariadb mariadb-libs libseccomp libzip make && \
  sudo mysql_install_db --user=mysql --basedir=/usr --datadir=/var/lib/mysql && \
  sudo systemctl enable mariadb && sudo systemctl start mariadb
  ```

### Instructions

1. In case you installed MariaDB server for the first time, you should run:
  ```sh
  sudo mysql_secure_installation
  ```

2. First of all clone the Sim repository and all its submodules

  ```sh
  git clone --recursive https://github.com/varqox/sim && cd sim
  ```

3. Build

  ```sh
  make -j $(nproc)
  ```

4. Make sure that you have created MariaDB account and database for Sim, use command below to create user `sim@localhost` and database `simdb` (when asked for password, enter your mariadb root password, by default it is empty &ndash; if that does not work try running the below command with `sudo`):

  ```sh
  mysql -e "CREATE USER sim@localhost IDENTIFIED BY 'sim'; CREATE DATABASE simdb; GRANT ALL ON simdb.* TO 'sim'@'localhost';" -u root -p
  ```

5. Install

  ```sh
  make install
  ```
  It will ask for MariaDB credentials. By default, step 4 created MariaDB username `sim`, password `sim`, database `simdb` and user host `localhost`.

  If you want to install Sim in other location that `sim/`, type

  ```sh
  make install DESTDIR=where-you-want-Sim-to-install
  ```

6. Run sim-server and job-machine

  ```sh
  make run
  ```

  If you have not installed Sim in the default location use command:

  ```sh
  make run DESTDIR=where-you-installed-Sim
  ```

  You can combine building, installation and running commands into:
  ```sh
  make all install run
  ```

7. Enter http://127.7.7.7:8080 via your web browser, by default a Sim root account was created there
  ```
  username: sim
  password: sim
  ```

  Remember to change the password now (or later) if you want to make Sim website accessible to others. Do not make hacker's life easier!

8. Well done! You have just installed Sim. There is a sim-server configuration file `where-you-installed-Sim/sim.conf` (`sim/sim.conf` by default) where server parameters like `address`, `workers` etc. are. Also, there are log files `log/*.log` that you would find useful if something didn't work.

9. If you want to run Sim at system startup then you can use `crontab` -- just add these lines to your crontab (using command `crontab -e`):
```
@reboot sh -c 'until test -e /var/run/mysqld/mysqld.sock; do sleep 0.4; done; BUILD="where-you-installed-Sim"; "$BUILD/sim-server"& "$BUILD/job-server"&'
```

`where-you-installed-Sim` = absolute path to Sim build directory e.g. `/home/your_username/sim/sim`

10. Feel free to report any bugs or things you don't like.

### Upgrading
Be aware that sometimes incompatible database or other inner changes won't allow for a smooth upgrade.

To upgrade just type:
```sh
git pull && git submodule update --init --recursive && make -j $(nproc) install run
```
