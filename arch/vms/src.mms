##########################################
#      Makefile for GRACE (VMS)          #
##########################################

# Rolf Niepraschk, 5/98, niepraschk@ptb.de

TOP = [-]
ECHO = WRITE SYS$OUTPUT

VMSDIR = [-.ARCH.VMS]
CEPHESDIR = [-.CEPHES]
T1LIBDIR = [-.T1LIB]

INCLUDE $(TOP)Make.conf

CEPHES_LIB = ,$(CEPHESDIR)libcephes.olb/LIBRARY
T1_LIB = ,$(T1LIBDIR)libt1lib.olb/LIBRARY

MYSTIC = ,"lines=lines_"

CFLAGS = $(CFLAGS0)/INCLUDE=("''F$TRNLNM("TOP_IN_UNIX_FORMAT")'"$(NETCDF_INC)) \
  /DEFINE=(GRACE_HOME="""$(GRACE_HOME)""",GRACE_HELPVIEWER="""$(HELPVIEWER)""" \
  ,PRINT_CMD="""$(PRINT_CMD)"""$(MYSTIC))

LIBS = $(GUI_LIBS)$(CEPHES_LIB)$(NETCDF_LIBS)$(FFTW_LIB) \
       $(T1_LIB)$(PDF_LIB)$(GD_LIB)$(NOGUI_LIBS)$(DL_LIB)

.FIRST
	@ @[-.ARCH.VMS]CONFIGURE.COM DEFINE_TOP_IN_UNIX_FORMAT

ALL : msg $(GRACE)
	@ !

INCLUDE Make.common

msg :
        @ $(ECHO) ""
        @ $(ECHO) "Making $(GRACE) ..."
        @ $(ECHO) ""

#INCLUDE Make.dep

$(GRACE) : $(GROBJS) $(GUIOBJS)
	LINK /EXECUTABLE=$@ $(LDFLAGS) $+ $(LIBS)
        PURGE *$(O)

clean :
        IF F$SEARCH("*$(O)").NES."" THEN $(RM) *$(O);*

distclean : clean
	IF F$SEARCH("$(GRACE)").NES."" THEN $(RM) $(GRACE);*
