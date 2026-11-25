#define READ(a,n,v) do { if(!c->read(c->board,(a),(n),&(v))) {c->fault_addr=(a);c->fault_size=(n);return PPC_READ;} } while(0)
#define WRITE(a,n,v) do { if(!c->write(c->board,(a),(n),(v))) {c->fault_addr=(a);c->fault_size=(n);return PPC_WRITE;} } while(0)
