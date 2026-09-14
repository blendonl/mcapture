CC       = x86_64-w64-mingw32-gcc
WINDRES  = x86_64-w64-mingw32-windres

VERSION  = 0.1.0

VER_MAJOR := $(word 1,$(subst ., ,$(VERSION)))
VER_MINOR := $(word 2,$(subst ., ,$(VERSION)))
VER_PATCH := $(word 3,$(subst ., ,$(VERSION)))

CFLAGS   = -O2 -s -flto -mwindows \
           -DUNICODE -D_UNICODE \
           -DMCAPTURE_VERSION='"$(VERSION)"' \
           -Wall -Wextra -Wno-unused-parameter \
           $(CFLAGS_EXTRA)

CFLAGS_EXTRA ?=

RCFLAGS  = -DVER_MAJOR=$(VER_MAJOR) \
           -DVER_MINOR=$(VER_MINOR) \
           -DVER_PATCH=$(VER_PATCH)

LDLIBS   = -luser32 -lgdi32 -lshell32 -lole32 -luuid -ldwmapi -lwindowscodecs

SRC_DIR  = src

MCAPTURE_SRCS = $(SRC_DIR)/mcapture.c  \
                $(SRC_DIR)/cli.c       \
                $(SRC_DIR)/geom.c      \
                $(SRC_DIR)/console.c   \
                $(SRC_DIR)/screen.c    \
                $(SRC_DIR)/select.c    \
                $(SRC_DIR)/encode.c    \
                $(SRC_DIR)/clipboard.c \
                $(SRC_DIR)/output.c

MCAPTURE_OBJS = $(MCAPTURE_SRCS:.c=.o)
RES_OBJ       = $(SRC_DIR)/mcapture.res.o
HEADERS       = $(SRC_DIR)/mcapture.h $(SRC_DIR)/cli.h $(SRC_DIR)/geom.h

TARGET        = mcapture.exe

DISTNAME   = mcapture-$(VERSION)-win64
DISTDIR    = dist/$(DISTNAME)
DIST_FILES = README.md CHANGELOG.md MANUAL-TESTS.md LICENSE

HOST_CC   = cc
TEST_DIR  = test
TEST_BINS = $(TEST_DIR)/test_cli $(TEST_DIR)/test_geom

.PHONY: all bump clean dist test print-version

all: $(TARGET)

bump:
	-git fetch --tags --quiet
	python3 tools/bump.py

print-version:
	@echo $(VERSION)

VERSION_STAMP = .version-$(VERSION)

$(VERSION_STAMP):
	@rm -f .version-*
	@touch $@

$(MCAPTURE_OBJS) $(RES_OBJ): $(VERSION_STAMP)

$(TARGET): $(MCAPTURE_OBJS) $(RES_OBJ)
	@echo "  LINK  $@"
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@echo "  CC    $<"
	$(CC) $(CFLAGS) -c -o $@ $<

$(RES_OBJ): $(SRC_DIR)/mcapture.rc $(SRC_DIR)/mcapture.exe.manifest
	@echo "  RC    $<"
	$(WINDRES) $(RCFLAGS) -I$(SRC_DIR) -O coff -i $< -o $@

dist: $(TARGET)
	@echo "  DIST  $(DISTNAME)"
	rm -rf "$(DISTDIR)" "dist/$(DISTNAME).zip"
	mkdir -p "$(DISTDIR)"
	cp $(TARGET)     "$(DISTDIR)/"
	cp $(DIST_FILES) "$(DISTDIR)/"
	cd dist && python3 -m zipfile -c "$(DISTNAME).zip" "$(DISTNAME)"
	@echo "  ->    dist/$(DISTNAME).zip"

$(TEST_DIR)/test_cli: $(TEST_DIR)/test_cli.c $(TEST_DIR)/tests.h $(SRC_DIR)/cli.c $(SRC_DIR)/cli.h $(SRC_DIR)/geom.c $(SRC_DIR)/geom.h
	@echo "  HOSTCC $@"
	$(HOST_CC) -O1 -Wall -Wextra -o $@ $(TEST_DIR)/test_cli.c $(SRC_DIR)/cli.c $(SRC_DIR)/geom.c

$(TEST_DIR)/test_geom: $(TEST_DIR)/test_geom.c $(TEST_DIR)/tests.h $(SRC_DIR)/geom.c $(SRC_DIR)/geom.h
	@echo "  HOSTCC $@"
	$(HOST_CC) -O1 -Wall -Wextra -o $@ $(TEST_DIR)/test_geom.c $(SRC_DIR)/geom.c

test: $(TEST_BINS)
	@echo "  TEST"
	@fail=0; for t in $(TEST_BINS); do ./$$t || fail=1; done; \
	 if [ $$fail -ne 0 ]; then echo "  TESTS FAILED"; exit 1; fi; \
	 echo "  all tests passed"

clean:
	rm -f $(TARGET) $(MCAPTURE_OBJS) $(RES_OBJ) $(TEST_BINS)
	rm -f .version-*
