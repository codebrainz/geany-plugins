AC_DEFUN([GP_CHECK_TABSORT], [
    GP_ARG_DISABLE([tabsort], [auto])
    GP_CHECK_PLUGIN_DEPS([tabsort], [TABSORT], [$GP_GTK_PACKAGE >= 2.8])
    GP_COMMIT_PLUGIN_STATUS([Tabsort])
    AC_CONFIG_FILES([tabsort/Makefile])
])
