CFLAGS = -O0 -std=gnu99

ARTIFACT = exploit
SOURCE  = exploit.c

all: $(ARTIFACT)

$(PROGRAM): $(SOURCE) ; $(CC) $(CFLAGS) -o $(ARTIFACT) $(SOURCE)

clean: ; rm -f $(ARTIFACT)
