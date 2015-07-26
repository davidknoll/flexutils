PREFIX=/usr/local

all: flex2sr sr2flex mkflexfs

install: all
	install -m755 -oroot -groot flex2sr  $(PREFIX)/bin
	install -m755 -oroot -groot sr2flex  $(PREFIX)/bin
	install -m755 -oroot -groot mkflexfs $(PREFIX)/bin

clean:
	rm -f flex2sr sr2flex mkflexfs *~
