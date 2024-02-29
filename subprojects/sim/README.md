# Sim

Sim is an open source platform for carrying out algorithmic contests

<div align="center">
  <img src="http://varqox.github.io/img/sim.png"/>
</div>

## How to build

> **_NOTE:_** There is a script `scripts/setup_sim_on_debian_12.py`. So if you need to look at very precise instructions that set up Sim, you can check out these scripts.
> If you intend to use it, first look at help, by just running the script without arguments, choose what you need and the script will guide you through the rest.

You will need `meson` build system to be installed (on most platforms it is in the _meson_ package).

### Dependencies:

- Meson build system
- gcc/g++ (with 32 bit support &ndash; for compiling submissions) with C++17 support
- MariaDB server
- MariaDB client library
- libseccomp
- libzip

#### Debian

```sh
sudo apt install g++ mariadb-server libmariadb-dev libseccomp-dev libzip-dev libssl-dev libcap-dev rustc fpc pkgconf meson
```

Ubuntu is not officially supported, you may try it, it may (not) work. _Modern_ versions of some of the above packages are needed to build sim successfully.

#### Arch Linux

```sh
sudo pacman -S gcc mariadb mariadb-libs libseccomp libzip libcap rust fpc meson && \
sudo mysql_install_db --user=mysql --basedir=/usr --datadir=/var/lib/mysql && \
sudo systemctl enable mariadb && sudo systemctl start mariadb
```

### Instructions: release build and installation step by step

1. In case you installed MariaDB server for the first time, you should run:
```sh
sudo mysql_secure_installation
```

2. First of all clone the repository and all its submodules
```sh
git clone https://github.com/varqox/sim-project && cd sim-project/subprojects/sim
```

3. Then setup build directory:
```sh
meson setup release-build/ -Dbuildtype=release
```

4. Build:
```
ninja -C release-build/
```

5. <a name="create-mariadb-account"></a>Make sure that you have created a MariaDB account and a database for Sim, use command below to create an user `sim@localhost` and a database `simdb` (when asked for password, enter your mariadb root password, by default it is empty &ndash; if that does not work, try running the below command with `sudo`):
```sh
mysql -e "CREATE USER sim@localhost IDENTIFIED BY 'sim'; CREATE DATABASE simdb; GRANT ALL ON simdb.* TO 'sim'@'localhost';" -u root -p
```

6. Configure a directory for a Sim instance
```sh
meson configure release-build --prefix $PWD/sim
```
This sets Sim instance's directory to `$PWD/sim` i.e. subdirectory `sim` of the current working directory. If you want to install Sim in other location, replace `$PWD/sim` with it.

7. Install
```sh
meson install -C release-build
```
It will ask for MariaDB credentials. In [previous step](#create-mariadb-account) you created a MariaDB user and database, by default: username `sim`, password `sim`, database `simdb` and user host `localhost`.

8. Run servers
If you installed Sim in the default location:
```sh
sim/manage start
```
If you installed Sim elsewhere:
```sh
where-you-installed-sim/manage start
```
> You can easily make it run in a background by adding `-b` parameter:
> ```sh
> sim/manage -b start
> ```
> Meson allows building and installing in one go:
> ```sh
> meson install -C release-build
> ```
> So building, installing and running can be combined into:
> ```sh
> meson install -C release-build && sim/manage -b start
> ```

9. Visit http://127.7.7.7:8080 via your web browser. By default a Sim root account was created there with following credentials:
```
username: sim
password: sim
```
>Remember to change the password now (or later) if you want to make Sim website accessible to others. Do not make hacker's life easier!

10. Well done! You have just fully set up Sim. There is a sim-server configuration file `where-you-installed-sim/sim.conf` (`sim/sim.conf` by default) where server parameters like `address`, `workers` etc. reside. Also, there are log files `where-you-installed-sim/log/*.log` that you can find useful if something didn't work.

11. If you want to run Sim at system startup then you can use `crontab` -- just add these lines to your crontab (using command `crontab -e`):
```
@reboot sh -c 'until test -e /var/run/mysqld/mysqld.sock; do sleep 0.4; done; where-you-installed-sim/manage start&'
```
`where-you-installed-sim` = an absolute path to Sim instance's directory e.g. `/home/your_username/sim/sim`

12. Feel free to report any bugs or things you would like to see.

#### Upgrading
Be aware that sometimes incompatible database or other inner changes won't allow for a smooth upgrade. In such cases `sim-upgrader` may come handy, grep the commit messages for it to check if it is needed.

To upgrade just type:
```sh
git pull --rebase --autostash
git submodule update --init --recursive
meson install -C release-build
sim/manage -b start
```

### Release build
```sh
meson setup release-build/ -Dbuildtype=release -Ddefault_library=both
ninja -C release-build/
```

### Development build
```sh
meson setup build/ -Dc_args=-DDEBUG -Dcpp_args='-DDEBUG -D_GLIBCXX_DEBUG' -Db_sanitize=undefined -Db_lundef=false
ninja -C build/
```

## Installing
Run after building:
```sh
# we will use release-build build directory, if you use other just change all release-build below
meson configure release-build --prefix $PWD/sim # if you want to install to sim/ subdirectory
meson install -C release-build
```

## Managing the Sim instance i.e. starting / stopping servers
For this, there is `manage` program in the instance's main directory.
To start servers type:
```sh
sim/manage start # or other instance's directory
```
To stop servers:
```sh
sim/manage stop
```
For more options:
```sh
sim/manage help
```

## Running tests
```sh
ninja -C build/ test # or other build directory
```

## Development build targets

### Formating C/C++ sources
To format all sources (clang-format is required):
```sh
ninja -C build format
```

### Linting C/C++ sources

#### All sources
To lint all sources:
```sh
ninja -C build tidy
```
or
```sh
./tidy
```

#### Specified sources
To lint specified sources:
```sh
./tidy path/to/source.cc other/source.h
```

### Static analysis
```sh
ninja -C build scan-build
```
