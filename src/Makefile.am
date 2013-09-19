# this is example-file: src/Makefile.am

# additional include pathes necessary to compile the C++ library
AM_CXXFLAGS = -I$(top_srcdir)/src @AM_CXXFLAGS@

###############################################################################
# THE LIBRARIES TO BUILD
###############################################################################

# the library names to build (note we are building static libs only)
lib_LIBRARIES = libeio.a

# where to install the headers on the system
libeio_adir = $(includedir)/eio

# the list of header files that belong to the library (to be installed later)
libeio_a_HEADERS =	\
	buffer.h \
	dns.h \
	io.h \
	sockaddr.h \
	timer.h \
	transport.h

# the sources to add to the library and to add to the source distribution
libeio_a_SOURCES =	\
	$(libeio_a_HEADERS) \
	buffer.cc \
	transport.cc


###############################################################################
