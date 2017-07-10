NAME = gopher
BIN = gopher.filter.dpi
OBJ = gopher.filter.dpi.o io.o dpi.o
DILLO_DIR = ~/.dillo
DPI_DIR = $(DILLO_DIR)/dpi
DPIDRC = $(DILLO_DIR)/dpidrc

all: $(BIN)

$(BIN): $(OBJ)

$(DPIDRC):
	cp /etc/dillo/dpidrc $@

install-proto: $(DPIDRC)
	echo 'proto.gopher=gopher/gopher.filter.dpi' >> $<

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
