PROGRAMS = webserv
LIBRARIES =
CCFLAGS = -g

all: $(LIBRARIES) $(PROGRAMS)
	@echo "Done!"

webserv: webserv.c
	gcc $(CCFLAGS) $< -o $@

clean:
	rm -rf webserv hist.dat hist.jpg exectemp TemporaryFile.txt convertedHtml.html