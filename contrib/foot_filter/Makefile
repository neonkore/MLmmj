all: foot_filter
dev: tags splint foot_filter
.PHONY: splint clean clobber
tags: foot_filter.c
	ctags --excmd=number '--regex-c=-/\*[[:blank:]]*tag:[[:blank:]]*([[:alnum:]_]+)-\1-' foot_filter.c
splint:
	splint +unixlib -exitarg -initallelements foot_filter.c
foot_filter: foot_filter.c
	gcc -Wall -g -o foot_filter foot_filter.c -O3
clean:
	-rm tags
clobber: clean
	-rm foot_filter
	-rm test
