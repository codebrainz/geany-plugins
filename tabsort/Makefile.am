include $(top_srcdir)/build/vars.build.mk
plugin = tabsort

geanyplugins_LTLIBRARIES = tabsort.la

tabsort_la_SOURCES = tabsort.c
tabsort_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"Tabsort\" \
	-DDATADIR=\""$(datadir)/geany-plugins/tabsort"\"
tabsort_la_LIBADD = $(COMMONLIBS)

uidir = $(datadir)/geany-plugins/tabsort
ui_DATA = ui.glade

include $(top_srcdir)/build/cppcheck.mk
