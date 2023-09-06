###############################################################################
#
# 
#
###############################################################################
THIS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
x86_64_netweb_tyrone_g48c210r_INCLUDES := -I $(THIS_DIR)inc
x86_64_netweb_tyrone_g48c210r_INTERNAL_INCLUDES := -I $(THIS_DIR)src
x86_64_netweb_tyrone_g48c210r_DEPENDMODULE_ENTRIES := init:x86_64_netweb_tyrone_g48c210r ucli:x86_64_netweb_tyrone_g48c210r

