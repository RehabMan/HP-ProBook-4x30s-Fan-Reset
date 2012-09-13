MODULE_NAME = Fanreset
MODULE_DESCRIPTION = This module resets the fan EC registers for HP ProBook
MODULE_AUTHOR = "RehabMan"
MODULE_VERSION = "0.1"
MODULE_COMPAT_VERSION = "1.2.0"
MODULE_START = $(MODULE_NAME)_start
MODULE_DEPENDENCIES = 
MODULE_DEFINITION = m

DIR = Fanreset.git

MODULE_OBJS = Fanreset.o

include MakeInc.dir
