############################################################ LICENSE
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
#
############################################################ IDENT(1)
#
# $Title: bhotkeys-lid-state - Super+Z lid policy $
# $Copyright: 2026 Devin Teske. All rights reserved. $
# $FrauBSD: bhotkeys-lid-state/Makefile 2026-10-05 12:20:35 -0700 Devin Teske $
#
############################################################ PATHS

PREFIX?=	/usr/local
BINDIR?=	${PREFIX}/bin
SBINDIR?=	${PREFIX}/sbin
PLUGDIR?=	${PREFIX}/share/bhotkeys/plugins.d
RCDIR?=		${PREFIX}/etc/rc.d
MAN1DIR?=	${PREFIX}/share/man/man1
MAN8DIR?=	${PREFIX}/share/man/man8

CC?=		cc
CFLAGS?=	-O2 -Wall -Wextra

############################################################ FILES

# lid-switchd applies hw.acpi.lid_switch_state and serves the socket
# lid-policy uses.
BIN=		bin/lid-wake-toggle bin/lid-policy
SBIN=		sbin/lid-switchd
RC=		rc.d/lid-switchd
PLUG=		plugins.d/lid
MAN1=		lid-wake-toggle lid-policy
MAN8=		lid-switchd

############################################################ TARGETS

.PHONY: all

all: ${SBIN} ${RC} man/lid-switchd.8

${SBIN}: src/lid-switchd.c
	mkdir -p sbin
	${CC} ${CFLAGS} -o ${.TARGET} src/lid-switchd.c

${RC}: ${RC}.in Makefile
	sed -e 's|@PREFIX@|${PREFIX}|g' ${RC}.in > ${RC}
	chmod 555 ${RC}

.for m in ${MAN8}
man/${m}.8: man/${m}.8.in Makefile
	sed -e 's|@PREFIX@|${PREFIX}|g' man/${m}.8.in > man/${m}.8
.endfor

.PHONY: install

install: all
	mkdir -p ${DESTDIR}${BINDIR} ${DESTDIR}${SBINDIR} \
	    ${DESTDIR}${PLUGDIR} ${DESTDIR}${RCDIR} \
	    ${DESTDIR}${MAN1DIR} ${DESTDIR}${MAN8DIR}
	install -m 755 ${BIN} ${DESTDIR}${BINDIR}
	install -m 755 ${SBIN} ${DESTDIR}${SBINDIR}
	install -m 555 ${RC} ${DESTDIR}${RCDIR}
	install -m 644 ${PLUG} ${DESTDIR}${PLUGDIR}/lid
.for m in ${MAN1}
	gzip -cn man/${m}.1 > ${DESTDIR}${MAN1DIR}/${m}.1.gz
	chmod 444 ${DESTDIR}${MAN1DIR}/${m}.1.gz
.endfor
.for m in ${MAN8}
	gzip -cn man/${m}.8 > ${DESTDIR}${MAN8DIR}/${m}.8.gz
	chmod 444 ${DESTDIR}${MAN8DIR}/${m}.8.gz
.endfor

.PHONY: clean

clean:
	rm -f ${SBIN} ${RC} man/lid-switchd.8
	rmdir sbin 2> /dev/null || :

################################################################################
# END
################################################################################
