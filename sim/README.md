# sim [![Build Status](https://travis-ci.org/varqox/sim.svg?branch=master)](https://travis-ci.org/varqox/sim) [![Coverity Scan Build Status](https://scan.coverity.com/projects/6466/badge.svg)](https://scan.coverity.com/projects/varqox-sim) [![Gitter chat](https://badges.gitter.im/varqox/sim.png)](https://gitter.im/varqox/sim)

SIM is an open source platform for carrying out algorithmic contests

<div align="center">
  <img src="http://varqox.github.io/img/sim.png"/>
</div>


## Installation

### Dependencies:

- gcc/g++ (with 32 bit support) with C++14 support
- MariaDB (Debian packages: _mariadb-server_)
- MariaDB client library (Debian packages: _libmariadbclient-dev_)
- libseccomp (Debian packages: _libseccomp-dev_)
- GNU/Make
- libarchive

#### Debian

  ```sh
  sudo apt-get install g++-multilib mariadb-server libmariadbclient-dev libseccomp-dev libarchive-dev make
  ```

#### Ubuntu

  ```sh
  sudo apt-get install g++-multilib mysql-server libmariadbclient-dev libseccomp-dev libarchive-dev make
  ```

#### Arch Linux

  ```sh
  sudo pacman -S gcc mariadb libmariadbclient libseccomp libarchive make
  sudo mysql_install_db --user=mysql --basedir=/usr --datadir=/var/lib/mysql
  sudo systemctl enable mariadb && sudo systemctl start mariadb
  ```

### Instructions

1. First of all clone the SIM repository and all its submodules

  ```sh
  git clone --recursive https://github.com/varqox/sim
  cd sim
  ```

2. Build

  ```sh
  make -j $(nproc)
  ```

3. Make sure that you have created MySQL account and database for SIM, use command below to create user sim@localhost and database sim (when asked for password, enter your mariadb root password, by default it is empty):

  ```sh
  mysql -e "CREATE USER sim@localhost IDENTIFIED BY 'sim'; CREATE DATABASE sim; GRANT ALL ON sim.* TO 'sim'@'localhost';" -u root -p
  ```

4. Install

  ```sh
  make install
  ```
  It will ask for MySQL credentials. By default, step 3 created MariaDB username 'sim', password 'sim', database 'sim' and user host 'localhost'.

  If you want to install SIM in other location that build/ type

  ```sh
  make install DESTDIR=where-you-want-SIM-to-install
  ```

5. Run sim-server and job-machine

  ```sh
  make run
  ```

  If you have not installed SIM in default location use command:

  ```sh
  make run DESTDIR=where-you-installed-SIM
  ```

  You can combine building, installation and running commands into:
  ```sh
  make all install run
  ```

6. Enter http://127.7.7.7:8080 via your web browser, by default there was created SIM root account
  ```
  username: sim
  password: sim
  ```

  Remember to change the password later (or now) if you want to make SIM site public. Do not make hacker's life easier!

7. Well done! You have just installed SIM. There is a sim-server configuration file `where-you-installed-SIM/sim.conf` in which are server parameters like ADDRESS etc. There are also log files `log/*.log` which you would find useful if something did not work.

8. Feel free to report bugs and irregularities.

### Upgrading
Just type (be aware of incompatible database (and other inner) changes)
```sh
git pull
git submodule update --recursive
make -j $(nproc) install run
```
