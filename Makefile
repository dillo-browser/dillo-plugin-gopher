NAME = gopher
BIN = gopher.filter.dpi
OBJ = gopher.filter.dpi.o io.o dpi.o
DILLO_PREFIX ?=
DILLO_DIR = ~/.dillo
DPI_DIR = $(DILLO_DIR)/dpi
DPIDRC = $(DILLO_DIR)/dpidrc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200112L

all: $(BIN)

$(BIN): $(OBJ)

$(DPIDRC):
	if [ ! -f $(DILLO_PREFIX)/etc/dillo/dpidrc ]; then \
		echo "Can't find $(DILLO_PREFIX)/etc/dillo/dpidrc, is dillo installed?"; \
		echo "If so, set DILLO_PREFIX to the installation prefix of dillo"; \
		false; \
	fi
	cp $(DILLO_PREFIX)/etc/dillo/dpidrc $@

install-proto: $(DPIDRC)
	echo 'proto.gopher=gopher/gopher.filter.dpi' >> $<
	dpidc stop || true

install: $(BIN) install-proto
	mkdir -p $(DPI_DIR)/$(NAME)
	cp -f $(BIN) $(DPI_DIR)/$(NAME)

link: $(BIN) install-proto
	mkdir -p $(DPI_DIR)/$(NAME)
	ln -frs $(BIN) $(DPI_DIR)/$(NAME)

uninstall: $(BIN)
	rm -f $(DPI_DIR)/$(NAME)/$(BIN)

clean:
	rm $(BIN) $(OBJ)

.PHONY:
	all install uninstall clean
