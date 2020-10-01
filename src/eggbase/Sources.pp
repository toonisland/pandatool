#begin ss_lib_target
  #define TARGET p3eggbase
  #define LOCAL_LIBS \
    p3progbase p3converter
  #define OTHER_LIBS \
    p3pipeline:c p3event:c p3pstatclient:c panda:m \
    p3pandabase:c p3pnmimage:c p3mathutil:c p3linmath:c p3putil:c p3express:c \
    p3interrogatedb p3prc  \
    p3dtoolutil:c p3dtoolbase:c p3dtool:m \
    $[if $[WANT_NATIVE_NET],p3nativenet:c] \
    $[if $[and $[HAVE_NET],$[WANT_NATIVE_NET]],p3net:c p3downloader:c]

  #define SOURCES \
     eggBase.h eggConverter.h eggFilter.h \
     eggMakeSomething.h \
     eggMultiBase.h eggMultiFilter.h \
     eggReader.h eggSingleBase.h \
     eggToSomething.h eggWriter.h \
     somethingToEgg.h

  #define COMPOSITE_SOURCES \
     eggBase.cxx eggConverter.cxx eggFilter.cxx \
     eggMakeSomething.cxx \
     eggMultiBase.cxx \
     eggMultiFilter.cxx eggReader.cxx eggSingleBase.cxx \
     eggToSomething.cxx \
     eggWriter.cxx somethingToEgg.cxx

  #define INSTALL_HEADERS \
    eggBase.h eggConverter.h eggFilter.h \
    eggMakeSomething.h \
    eggMultiBase.h eggMultiFilter.h \
    eggReader.h eggSingleBase.h eggToSomething.h eggWriter.h somethingToEgg.h

#end ss_lib_target
