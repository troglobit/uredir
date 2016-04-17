EXTRA_DIST      = configure README.md LICENSE
doc_DATA        = README.md LICENSE
dist_man1_MANS  = uredir.1
bin_PROGRAMS    = uredir
uredir_SOURCES  = uredir.c

## Target to run when building a release
release: dist
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
