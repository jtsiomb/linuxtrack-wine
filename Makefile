src = $(wildcard src/*.c) $(wildcard ltlib/*.c)
obj = $(src:.c=.o)
lib = npclient.dll.so

instdir = $(shell winepath c:\linuxtrack)
cfgdir = $(HOME)/.ltrdll

ifeq ($(shell uname -m), x86_64)
	build32 = -m32
	link32 = -m32 -L/usr/local/lib32 -Wl,-rpath=/usr/local/lib32
endif

CC = winegcc
CFLAGS = $(build32) -pedantic -Wall -g
LDFLAGS = $(link32) -shared npclient.spec -mno-cygwin -lpthread -llinuxtrack -lX11 -lexpat
REGEDIT = regedit


$(lib): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(lib)

# winepath runs first to force an abort if the path we got in $(instdir) is bogus
.PHONY: install
install: $(lib)
	winepath
	mkdir -p $(cfgdir)
	cp data/apps.xml $(cfgdir)/apps.xml
	cp data/config $(cfgdir)/config
	mkdir -p $(instdir)
	cp $(lib) $(instdir)/NPClient.dll
	regedit data/dllpath.reg

.PHONY: uninstall
uninstall:
	winepath
	rm -f $(instdir)/NPClient.dll
	rmdir $(instdir)
	regedit -d `cat data/dllpath.reg | grep HKEY | sed s/^.// | sed s/\\\\NATURALPOINT.*//`

.PHONY: purge
purge: uninstall
	rm -f $(cfgdir)/apps.xml
	rm -f $(cfgdir)/config
	rmdir $(cfgdir)
