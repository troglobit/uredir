bin_PROGRAMS    = uredir
dist_man1_MANS  = uredir.1
uredir_SOURCES  = uredir.c uredir.h youdp.c
uredir_CFLAGS   = $(uev_CFLAGS)
uredir_LDADD    = $(uev_LIBS)
doc_DATA        = README.md LICENSE
EXTRA_DIST      = $(doc_DATA)

package:
	@debuild -uc -us -B --lintian-opts --profile debian -i -I

## Target to run when building a release
release: distcheck
	@for file in $(DIST_ARCHIVES); do	\
		md5sum $$file > ../$$file.md5;	\
	done
	@mv $(DIST_ARCHIVES) ../
	@echo
	@echo "Resulting release files:"
	@echo "================================================================="
	@for file in $(DIST_ARCHIVES); do					\
		printf "$$file    \tDistribution tarball\n";			\
		printf "$$file.md5\t"; cat ../$$file.md5 | cut -f1 -d' ';	\
	done
