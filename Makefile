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
# $FrauBSD: bhotkeys-lid-state/Makefile 2026-10-03 22:42:48 -0700 Devin Teske $
#
############################################################ PATHS

PREFIX?=	/usr/local
BINDIR?=	${PREFIX}/bin
SBINDIR?=	${PREFIX}/sbin
PLUGDIR?=	${PREFIX}/share/bhotkeys/plugins.d
RCDIR?=		${PREFIX}/etc/rc.d

CC?=		cc
CFLAGS?=	-O2 -Wall -Wextra

############################################################ FILES

# lid-switchd applies hw.acpi.lid_switch_state and serves the socket
# lid-policy uses.
BIN=		bin/lid-wake-toggle bin/lid-policy
SBIN=		sbin/lid-switchd
RC=		rc.d/lid-switchd
PLUG=		plugins.d/lid

############################################################ TARGETS

.PHONY: all

all: ${SBIN} ${RC}

${SBIN}: src/lid-switchd.c
	mkdir -p sbin
	${CC} ${CFLAGS} -o ${.TARGET} src/lid-switchd.c

${RC}: ${RC}.in Makefile
	sed -e 's|@PREFIX@|${PREFIX}|g' ${RC}.in > ${RC}
	chmod 555 ${RC}

.PHONY: install

install: all
	mkdir -p ${DESTDIR}${BINDIR} ${DESTDIR}${SBINDIR} \
	    ${DESTDIR}${PLUGDIR} ${DESTDIR}${RCDIR}
	install -m 755 ${BIN} ${DESTDIR}${BINDIR}
	install -m 755 ${SBIN} ${DESTDIR}${SBINDIR}
	install -m 555 ${RC} ${DESTDIR}${RCDIR}
	install -m 644 ${PLUG} ${DESTDIR}${PLUGDIR}/lid

.PHONY: clean

clean:
	rm -f ${SBIN} ${RC}
	rmdir sbin 2> /dev/null || :

################################################################################
# END
################################################################################
