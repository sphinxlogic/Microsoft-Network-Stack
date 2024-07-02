TARGETPATH=obj

IASDIR=$(PROJECT_ROOT)\ias

PASS0_HEADERDIR=$(IASDIR)\inc\$(O)

!ifndef IAS_NO_PCH
PRECOMPILED_CXX=1
PRECOMPILED_INCLUDE=$(IASDIR)\inc\ias.h
PRECOMPILED_OPTION=/Yuias.h /Fp$(O)\pch.pch
PRECOMPILED_TARGET=$(O)\pch.pch
PRECOMPILED_OBJ=$(O)\pch.obj
!endif

USE_ATL=1
USE_MSVCRT=1
USE_NATIVE_EH=1
USE_VCCOM=1
USE_STL=1