SYSNAME:=${shell uname}
SYSNAME!=uname
include ../config.mk.${SYSNAME}

LIBANODE_OBJS= \
  impl/aes.o \
  impl/dictionary.o \
  impl/dns_txt.o \
  impl/ec.o \
  impl/environment.o \
  impl/misc.o \
  impl/thread.o \
  address.o \
  aes_digest.o \
  errors.o \
  identity.o \
  network_address.o \
  secure_random.o \
  system_transport.o \
  uri.o
#  zone.o

all:	$(LIBANODE_OBJS)
	ar rcs libanode.a $(LIBANODE_OBJS)
	ranlib libanode.a
	$(CC) $(CFLAGS) -o utils/anode-make-identity utils/anode-make-identity.c $(LIBANODE_OBJS) $(LIBANODE_LIBS)

clean: force
	rm -f $(LIBANODE_OBJS)
	rm -f libanode.$(DLLEXT) libanode.a
	rm -f utils/anode-make-identity

force: ;
