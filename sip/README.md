# sip [![Build Status](https://travis-ci.org/varqox/sip.svg)](https://travis-ci.org/varqox/sip)

Sip is a tool for preparing and managing Sim problem packages

## How to build?

### Dependencies
- gcc/g++ (with 32 bit support) with C++14 support
- libseccomp
- libarchive
- GNU/Make

#### Debian / Ubuntu

  ```sh
  sudo apt-get install g++-multilib libseccomp-dev libarchive-dev make
  ```

#### Arch Linux

  ```sh
  sudo pacman -S gcc libseccomp libarchive make
  ```

### Instructions
1. First of all clone the repository and all its submodules

  ```sh
  git clone --recursive https://github.com/varqox/sip && cd sip
  ```

2. Build

  ```sh
  make -j $(nproc) all
  ```

The above command creates a dynamic executable `src/sip`, a static one `src/sip-static` can be build with command:

  ```sh
  make -j $(nproc) static
  ```
4. Install

  ```sh
  sudo make install
  ```

5. Well done! You have just installed Sip. Now you can use sip command:

  ```sh
  sip
  ```

For more information about how to use it and what it can do type:

  ```sh
  sip help
  ```

6. Feel free to contribute.

### Upgrading
Just type:

  ```sh
  git pull
  git submodule update --recursive
  make -j $(nproc) all
  sudo make install
  ```
