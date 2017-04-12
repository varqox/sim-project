#!/bin/bash

# TODO: rewrite as C++ - to read credentials safely
echo "[client]" > .mysql.cnf
chmod 0600 .mysql.cnf
echo "user=$(head -n 1 .db.config)" >> .mysql.cnf
echo "password=$(head -n 2 .db.config | tail -n 1)" >> .mysql.cnf
db=$(head -n 3 .db.config | tail -n 1)
mysqldump --defaults-file=.mysql.cnf --result-file=dump.sql --extended-insert=FALSE $db

git init
git config --local user.name "Sim backuper"
git add solutions dump.sql
git commit -m "Backup $(date '+%Y-%m-%d %H:%M:%S')"

rm dump.sql
rm .mysql.cnf
