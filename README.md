# sim [![Build Status](https://travis-ci.org/krzyk240/sim.svg?branch=master)](https://travis-ci.org/krzyk240/sim) [![Coverity Scan Build Status](https://scan.coverity.com/projects/6466/badge.svg)](https://scan.coverity.com/projects/krzyk240-sim) [![Gitter chat](https://badges.gitter.im/krzyk240/sim.png)](https://gitter.im/krzyk240/sim)

SIM is open source platform for carrying out algorithmic contests

<div align="center">
  <img src="http://krzyk240.github.io/img/sim.png"/>
</div>


## Instalation

### Dependencies:

- gcc/g++ (32 bit version)
- MySQL (Debian packages: _mysql-server mysql-client_)
- [MySQL Connector/C++](http://dev.mysql.com/downloads/connector/cpp/) (Debian package: _libmysqlcppconn-dev_)
- GNU/Make
- zip + unzip

#### Ubuntu / Debian

  ```sh
  sudo apt-get install g++-multilib mysql-server mysql-client libmysqlcppconn-dev make zip unzip
  ```

### Instructions

1. First of all clone the SIM repository and all its submodules

  ```sh
  git clone --recursive https://github.com/krzyk240/sim
  cd sim
  ```

2. Build

  ```sh
  make -j 4
  ```

3. Make sure that you have created MySQL account and database for SIM, use command below to create user sim@localhost (password: sim) and database sim:

  ```sh
  mysql -e "CREATE USER sim@localhost IDENTIFIED BY 'sim'; CREATE DATABASE sim; GRANT ALL ON sim.* TO 'sim'@'localhost';" -u root -p
  ```

4. Install

  ```sh
  make install
  ```
  It will ask for MySQL credentials

  If you want to install SIM in other location that build/ type

  ```sh
  make install DESTDIR=where-you-want-SIM-to-install
  ```

5. Run sim-server and judge-machine

  ```sh
  make run
  ```

  If you have not installed SIM in default location use command:

  ```sh
  make run DESTDIR=where-you-installed-SIM
  ```

  You can combine building, installation and running commands into:
  ```sh
  make install run
  ```

6. Enter http://127.7.7.7:8080 via your internet browser, by default there was created SIM root account
  ```
  username: sim
  password: sim
  ```

7. Well done! You have just installed SIM. There is a sim-server configuration file `where-you-installed-SIM/server.conf` in which are server parameters like ADDRESS etc. There are also log files `*.log` which you would find useful if something did not work. Feel free to report bugs and irregularities.

### Upgrading
Just type (be aware of incompatible database (and other inner) changes)
```sh
git pull
git submodule update
make -j4 install run
```

### Task packages
You can find some example task packages in problems/ folder.
