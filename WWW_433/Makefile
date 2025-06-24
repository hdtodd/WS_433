# Makefile for WWW_433, the PHP files that create graphs of 
# weather history data from database recordings of WDL_433
#
# Primarily, this install script ensures that the ownership
#   of the database directory and Weather.db file are set
#   to 'www-data', else the temporary table creation used
#   to merge temp/pressure data in TP-merge.php fails with
#   a PDO error message.

#2025.06 Inital version
#Author: HDTodd@gmail.com
#

PROJ   = WWW_433
WWW    = /var/www/html/
DBD    = /var/databases/
DBFile = Weather.db

install:
	mkdir -p ${WWW}
	mkdir -p ${DBD}
	chown www-data ${DBD}
	touch ${DBD}/${DBFile}
	chown www-data ${DBD}/${DBFile}
	cp *.php ${WWW}

