pkglib_LTLIBRARIES = libgom-1.0.la

libgom_1_0_la_SOURCES = \
    gom-miner.c \
    gom-miner.h \
    gom-tracker.c \
    gom-tracker.h \
    gom-utils.c \
    gom-utils.h \
    tracker-ioprio.c \
    tracker-ioprio.h \
    tracker-sched.c \
    tracker-sched.h \
    $(NULL)

libgom_1_0_la_CPPFLAGS = \
    -DG_LOG_DOMAIN=\"Gom\" \
    -DG_DISABLE_DEPRECATED \
    -I$(top_srcdir)/src/ \
    $(GIO_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(GOA_CFLAGS) \
    $(TRACKER_CFLAGS) \
    $(NULL)

libgom_1_0_la_LIBADD = \
    $(GIO_LIBS) \
    $(GLIB_LIBS) \
    $(GOA_LIBS) \
    $(TRACKER_LIBS) \
    $(NULL)

libgom_1_0_la_LDFLAGS = \
    -avoid-version \
    $(NULL)

libexec_PROGRAMS = \
    gom-facebook-miner \
    gom-flickr-miner \
    gom-gdata-miner \
    gom-owncloud-miner \
    gom-zpj-miner \
    $(NULL)

gom_facebook_miner_SOURCES = \
    gom-facebook-miner-main.c \
    gom-facebook-miner.c \
    gom-facebook-miner.h \
    $(NULL)

gom_facebook_miner_CPPFLAGS = \
    -DG_LOG_DOMAIN=\"Gom\" \
    -DG_DISABLE_DEPRECATED \
    -I$(top_srcdir)/src \
    $(GFBGRAPH_CFLAGS) \
    $(GIO_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(GOA_CFLAGS) \
    $(TRACKER_CFLAGS) \
    $(NULL)

gom_facebook_miner_LDADD = \
    libgom-1.0.la  \
    $(GFBGRAPH_LIBS) \
    $(GIO_LIBS) \
    $(GLIB_LIBS) \
    $(GOA_LIBS) \
    $(TRACKER_LIBS) \
    $(NULL)

gom_flickr_miner_SOURCES = \
    gom-flickr-miner-main.c \
    gom-flickr-miner.c \
    gom-flickr-miner.h \
    $(NULL)

gom_flickr_miner_CPPFLAGS = \
    -DG_LOG_DOMAIN=\"Gom\" \
    -DG_DISABLE_DEPRECATED \
    -I$(top_srcdir)/src \
    $(GIO_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(GOA_CFLAGS) \
    $(GRILO_CFLAGS) \
    $(TRACKER_CFLAGS) \
    $(NULL)

gom_flickr_miner_LDADD = \
    libgom-1.0.la  \
    $(GIO_LIBS) \
    $(GLIB_LIBS) \
    $(GOA_LIBS) \
    $(GRILO_LIBS) \
    $(TRACKER_LIBS) \
    $(NULL)

gom_gdata_miner_SOURCES = \
    gom-gdata-miner-main.c \
    gom-gdata-miner.c \
    gom-gdata-miner.h \
    $(NULL)

gom_gdata_miner_CPPFLAGS = \
    -DG_LOG_DOMAIN=\"Gom\" \
    -DG_DISABLE_DEPRECATED \
    -I$(top_srcdir)/src \
    $(GDATA_CFLAGS) \
    $(GIO_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(GOA_CFLAGS) \
    $(TRACKER_CFLAGS) \
    $(NULL)

gom_gdata_miner_LDADD = \
    libgom-1.0.la  \
    $(GDATA_LIBS) \
    $(GIO_LIBS) \
    $(GLIB_LIBS) \
    $(GOA_LIBS) \
    $(TRACKER_LIBS) \
    $(NULL)

gom_owncloud_miner_SOURCES = \
    gom-owncloud-miner-main.c \
    gom-owncloud-miner.c \
    gom-owncloud-miner.h \
    $(NULL)

gom_owncloud_miner_CPPFLAGS = \
    -DG_LOG_DOMAIN=\"Gom\" \
    -DG_DISABLE_DEPRECATED \
    -I$(top_srcdir)/src \
    $(GIO_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(GOA_CFLAGS) \
    $(TRACKER_CFLAGS) \
    $(NULL)

gom_owncloud_miner_LDADD = \
    libgom-1.0.la  \
    $(GIO_LIBS) \
    $(GLIB_LIBS) \
    $(GOA_LIBS) \
    $(TRACKER_LIBS) \
    $(NULL)

gom_zpj_miner_SOURCES = \
    gom-zpj-miner-main.c \
    gom-zpj-miner.c \
    gom-zpj-miner.h \
    $(NULL)

gom_zpj_miner_CPPFLAGS = \
    -DG_LOG_DOMAIN=\"Gom\" \
    -DG_DISABLE_DEPRECATED \
    -I$(top_srcdir)/src \
    $(GIO_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(GOA_CFLAGS) \
    $(TRACKER_CFLAGS) \
    $(ZAPOJIT_CFLAGS) \
    $(NULL)

gom_zpj_miner_LDADD = \
    libgom-1.0.la  \
    $(GIO_LIBS) \
    $(GLIB_LIBS) \
    $(GOA_LIBS) \
    $(TRACKER_LIBS) \
    $(ZAPOJIT_LIBS) \
    $(NULL)

EXTRA_DIST = \
    gom-miner-main.c \
    $(NULL)

-include $(top_srcdir)/git.mk
