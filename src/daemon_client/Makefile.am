include $(top_srcdir)/buildutils/config.mk

if HAVE_CHECK
SUBDIRS = test
endif

DIST_SUBDIRS =

include_HEADERS = trq_auth_daemon.h

AM_CFLAGS = -Wall -pthread -ldl -lrt -lssl -lcrypto

sbin_PROGRAMS = trqauthd

trqauthd_SOURCES = trq_auth_daemon.c trq_main.c
trqauthd_LDFLAGS = -L../lib/Libpbs/.libs -ltorque
