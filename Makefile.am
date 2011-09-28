SUBDIRS = src
dist_doc_DATA = README.md

if HAVE_DOXYGEN

doxyfile.stamp:
	$(DOXYGEN) Doxyfile
	echo Timestamp > doxyfile.stamp

CLEANFILES = doxyfile.stamp
all-local: doxyfile.stamp
clean-local:
	rm -rf doxy

endif
