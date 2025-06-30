# Makefile for WDL_433, a program to log to an sql database
# the weather data published by rtl_433 in JSON format
#
# For the sqlite3 version, use 'make'
# For the Mariadb/MySQL version, use 'make CFLAGS=-DUSE_MYSQL=1'

#2025.03 Inital version
#Author: HDTodd@gmail.com
#

PROJ = WDL_433
TARGET_DIR = /usr/local/
SYSSERVICE = /etc/systemd/system/

CC = gcc

BIN = $(TARGET_DIR)/bin/
ETC = $(TARGET_DIR)/etc/

ifdef DEBUG
CFLAGS += -g
endif
ifdef USE_MYSQL
CFLAGS += -D USE_MYSQL
endif
LDFLAGS += -lmosquitto
LDFLAGS += -lsqlite3

LIBS = `mariadb_config --libs`

OBJS   = WDL_433.o GetSetParams.o WDL_procs.o WDL_DBMgr.o mjson.o

all:	${PROJ}

.SUFFIXES: .c

.c.o:	
	$(CC) $(CFLAGS) -c $<

${PROJ}: ${OBJS}
	$(CC) -o $@ ${OBJS} $(LDFLAGS) $(LIBS)

clean:
	/bin/rm -f *~ *.o ${PROJ} ${PROJ}.service a.out

install:
	mkdir -p   ${BIN}
	cp ${PROJ} ${BIN}
	mkdir -p   ${ETC}
	cp ${PROJ}.ini ${ETC}
#       if using MySQL, protect username and password from public
ifdef USE_MYSQL
	chown root:root ${ETC}/${PROJ}.ini
	chmod 700 ${ETC}/${PROJ}.ini
endif
#       Use sed to replace $(BIN) with target binary directory name
	sed s:$$\(BIN\):${BIN}: ${PROJ}.service.prototype > ${PROJ}.service
	cp ${PROJ}.service ${SYSSERVICE}
	systemctl enable ${PROJ}.service
	systemctl start ${PROJ}.service

uninstall:
	systemctl disable ${PROJ}.service
	systemctl stop    ${PROJ}.service
	rm ${SYSSERVICE}/${PROJ}.service
	rm ${BIN}${PROJ}
	rm ${ETC}${PROJ}.ini
	rm ${ETC}${PROJ}_Sensor_Aliases.ini
