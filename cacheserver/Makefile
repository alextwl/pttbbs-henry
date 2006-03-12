# $Id: Makefile 3281 2006-03-12 16:07:06Z kcwu $
.include "../pttbbs.mk"

PROGRAMS=	utmpserver utmpsync

all:	${PROGRAMS}

.SUFFIXES: .c .o
.c.o:	
	$(CCACHE) $(CC) $(CFLAGS) -c $*.c

.for fn in ${PROGRAMS}
${fn}: ${fn}.o
	${CC} ${CFLAGS} ${LDFLAGS} -o ${fn} ${fn}.o ../util/util_*.o
.endfor

clean:
	rm -f *~ ${PROGRAMS}
