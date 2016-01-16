all: mquery mhttp

mhttp: mhttp.o mdnsd.o 1035.o sdtxt.o xht.o

mquery: mquery.o mdnsd.o 1035.o

clean:
	-@$(RM) mquery mhttp

distclean:
	-@$(RM) *.o *~
