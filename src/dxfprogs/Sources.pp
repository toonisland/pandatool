#define UNIX_SYS_LIBS m
#define USE_PACKAGES fftw

#define OTHER_LIBS \
    p3egg:c pandaegg:m \
    p3pipeline:c p3event:c p3pstatclient:c panda:m \
    p3pandabase:c p3pnmimage:c p3mathutil:c p3linmath:c p3putil:c p3express:c \
    pandaexpress:m \
    p3interrogatedb p3prc  \
    p3dtoolutil:c p3dtoolbase:c p3dtool:m \
    $[if $[WANT_NATIVE_NET],p3nativenet:c] \
    $[if $[and $[HAVE_NET],$[WANT_NATIVE_NET]],p3net:c p3downloader:c]

#begin bin_target
  #define TARGET dxf-points
  #define LOCAL_LIBS \
    p3progbase p3dxf

  #define SOURCES \
    dxfPoints.cxx dxfPoints.h

#end bin_target

#begin bin_target
  #define TARGET dxf2egg
  #define LOCAL_LIBS p3dxf p3dxfegg p3eggbase p3progbase

  #define SOURCES \
    dxfToEgg.cxx dxfToEgg.h

#end bin_target

#begin bin_target
  #define TARGET egg2dxf
  #define LOCAL_LIBS p3dxf p3eggbase p3progbase

  #define SOURCES \
    eggToDXF.cxx eggToDXF.h \
    eggToDXFLayer.cxx eggToDXFLayer.h

#end bin_target
