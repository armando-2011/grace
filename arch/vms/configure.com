$!
$! configure for GRACE -- VMS version
$! Rolf Niepraschk, 12/97, niepraschk@ptb.de
$!
$ echo := WRITE SYS$OUTPUT
$!
$ DPML = ""
$ OPTIMIZE = "Yes"
$ FLOAT = ""
$ N = 1
$PLOOP:
$ P = P'N'
$ IF (P .EQS. "NODPML")     THEN DPML = "No"
$ IF (P .EQS. "NOOPTIMIZE") THEN OPTIMIZE = "No"
$ IF (P .EQS. "D_FLOAT")    THEN FLOAT = "D_FLOAT"
$ IF (P .EQS. "G_FLOAT")    THEN FLOAT = "G_FLOAT"
$ IF (P .EQS. "IEEE")       THEN FLOAT = "IEEE"
$ N = N + 1
$ IF (N .LE. 8) THEN GOTO PLOOP
$!
$! save the current directory and set default to the vms directory
$!
$ CURDIR = F$ENVIRONMENT ("DEFAULT")
$ VMSDIR = F$ELEMENT (0, "]", F$ENVIRONMENT ("PROCEDURE")) + "]"
$ SET DEFAULT 'VMSDIR'
$!
$! define symbols for the other directories
$!
$ MAIN_DIR := [--]
$ CEPHES_DIR := [--.CEPHES]
$ T1LIB_DIR := [--.T1LIB]
$ SRC_DIR := [--.SRC]
$ GRCONVERT_DIR := [--.GRCONVERT]
$ EXAMPLES_DIR := [--.EXAMPLES]
$!
$! get versions, hardware types, etc
$!
$ VMSVERSION = F$GETSYI ("NODE_SWVERS")
$ VMS_MAJOR = F$ELEMENT (0, ".", VMSVERSION) - "V"
$ VMS_MINOR = F$ELEMENT (1, ".", VMSVERSION)
$ HW = F$GETSYI("ARCH_NAME")
$ @SYS$UPDATE:DECW$GET_IMAGE_VERSION SYS$SHARE:DECW$XLIBSHR.EXE DECWVERSION
$ DECWVERSION = F$ELEMENT (0, "-", F$ELEMENT (1, " ", DECWVERSION))
$ @SYS$UPDATE:DECW$GET_IMAGE_VERSION SYS$SYSTEM:DECC$COMPILER.EXE DECCVERSION
$ DECCVERSION = F$ELEMENT (0, "-", F$ELEMENT (1, " ", DECCVERSION))
$ IF (DPML .EQS. "")
$ THEN
$   IF (F$SEARCH("SYS$LIBRARY:DPML$SHR.EXE") .NES. "")
$   THEN
$     DPML = "Yes"
$   ELSE
$     DPML = "No"
$   ENDIF
$ ENDIF
$ FLOAT_VAX = "G_FLOAT"
$ FLOAT_ALPHA = "IEEE"
$ IF (FLOAT .EQS. "") THEN FLOAT = FLOAT_'HW'
$!
$ echo "Configuration of GRACE for VMS"
$ echo ""
$ echo "VMS version: V", VMS_MAJOR, ".", VMS_MINOR
$ echo "Hardware:    ", HW
$ echo "GUI:         Motif ", DECWVERSION
$ echo "DPML:        ", DPML
$ echo "DECC:        ", DECCVERSION
$ echo "Optimize:    ", OPTIMIZE
$ echo "Float:       ", FLOAT
$ echo ""
$!
$! copy build files
$!
$ COPY DESCRIP.MMS 'MAIN_DIR'DESCRIP.MMS
$ COPY CEPHES.MMS 'CEPHES_DIR'DESCRIP.MMS
$ COPY MAKE.DEP_CEPHES 'CEPHES_DIR'MAKE.DEP
$ COPY T1LIB.MMS 'T1LIB_DIR'DESCRIP.MMS
$ COPY SRC.MMS 'SRC_DIR'DESCRIP.MMS
$ COPY MAKE.DEP_SRC 'SRC_DIR'MAKE.DEP
$ COPY GRCONVERT.MMS 'GRCONVERT_DIR'DESCRIP.MMS
$ COPY XDR.OPT 'GRCONVERT_DIR'XDR.OPT
$ COPY DOTEST.COM 'EXAMPLES_DIR'
$ IF (VMS_MAJOR .LT. 7) THEN COPY PWD.H_V6 PWD.H
$!
$! copy files that are soft links in the tar file
$!
$ COPY [--.T1LIB.T1LIB]T1LIB.H 'T1LIB_DIR'T1LIB.H
$ COPY [--.FONTS.ENC]ISOLATIN1.ENC [--.FONTS.ENC]DEFAULT.ENC
$!
$! create config.h and make.conf
$!
$ IF VMS_MAJOR .LT. 7
$ THEN
$   CONFIG_H_OS := CONFIG.H-VMS6
$   MAKE_OS := MAKE.CONF-VMS6
$ ELSE
$   CONFIG_H_OS := CONFIG.H-VMS7
$   MAKE_OS := MAKE.CONF-VMS7
$ ENDIF
$ IF (DECWVERSION .EQS. "V1.2")
$ THEN
$   MAKE_GUI := MAKE.CONF-M1_2
$ ELSE
$   MAKE_GUI := MAKE.CONF-M1_1
$ ENDIF
$ CONFIG_H_HW := CONFIG.H-'HW'
$ IF (DPML)
$ THEN
$   CONFIG_H_MATH := CONFIG.H-DPML
$ ELSE
$   CONFIG_H_MATH := CONFIG.H-NO_DPML
$ ENDIF
$ IF (HW .EQS. "Alpha" .OR. DECCVERSION .GES. "V6.0")
$ THEN
$   CONFIG_H_ALLOCA := CONFIG.H-__ALLOCA
$ ELSE
$   CONFIG_H_ALLOCA := CONFIG.H-NO__ALLOCA
$ ENDIF
$!
$ CREATE/FDL=SYS$INPUT 'MAIN_DIR'MAKE.CONF
RECORD ; FORMAT STREAM_LF
$ APPEND MAKE.CONF-COMMON,'MAKE_OS','MAKE_GUI' 'MAIN_DIR'MAKE.CONF
$ STR = "CFLAGS0=/PREFIX=ALL/FLOAT=" + FLOAT
$ IF (.NOT. OPTIMIZE) THEN STR = STR + "/NOOPTIMIZE"
$ OPEN/APPEND OUT 'MAIN_DIR'MAKE.CONF
$ WRITE OUT ""
$ WRITE OUT "# C flags"
$ WRITE OUT STR
$ WRITE OUT ""
$ WRITE OUT "# alloca object, if necessary
$ IF (HW .EQS. "Alpha" .OR. DECCVERSION .GES. "V6.0")
$ THEN
$   WRITE OUT "ALLOCA="
$ ELSE
$   WRITE OUT "ALLOCA=alloca.obj"
$ ENDIF
$ CLOSE OUT
$!
$ CREATE/FDL=SYS$INPUT 'MAIN_DIR'CONFIG.H
RECORD ; FORMAT STREAM_LF
$ APPEND SYS$INPUT 'MAIN_DIR'CONFIG.H
/* config.h.  Generated automatically by configure. -- VMS -- */
#ifndef __CONFIG_H
#define __CONFIG_H

$ APPEND 'CONFIG_H_OS','CONFIG_H_HW','CONFIG_H_MATH','CONFIG_H_ALLOCA',-
    CONFIG.H-COMMON -
    'MAIN_DIR'CONFIG.H
$!
$ APPEND SYS$INPUT 'MAIN_DIR'CONFIG.H

#endif /* __CONFIG_H */
$!
$! restore directory and exit
$!
$ SET DEFAULT 'CURDIR'
$ EXIT
