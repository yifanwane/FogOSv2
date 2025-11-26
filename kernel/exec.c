#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"

static int loadseg(pde_t *, uint64, struct inode *, uint, uint);

int flags2perm(int flags)
{
    int perm = 0;
    if(flags & 0x1)
      perm = PTE_X;
    if(flags & 0x2)
      perm |= PTE_W;
    return perm;
}

int
kexec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();
  
  // Counter to prevent infinite recursion
  int recursion_depth = 0;

  begin_op();

 retry:
  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);

  // --- MODIFIED READ LOGIC START ---
  // Read the header. Note: Scripts might be smaller than sizeof(elf).
  int n = readi(ip, 0, (uint64)&elf, 0, sizeof(elf));
  if(n < 2) 
    goto bad; // File too short to be anything useful
  // --- MODIFIED READ LOGIC END ---

  // Check if it is an ELF file
  // It must be at least sizeof(elf) AND have the magic number
  if(n < sizeof(elf) || elf.magic != ELF_MAGIC){
      // Not an ELF. Check for Shebang (#!).
      char *hdr = (char*)&elf;
      if(hdr[0] == '#' && hdr[1] == '!'){
          if(recursion_depth > 5) {
              goto bad;
          }
          recursion_depth++;

          // Parse interpreter path
          char interpreter[MAXPATH];
          int j = 0;
          int k = 2;

          while(k < n && (hdr[k] == ' ' || hdr[k] == '\t')) k++;

          while(k < n && hdr[k] != '\n' && hdr[k] != '\r' && hdr[k] != 0){
              if(j < MAXPATH - 1)
                  interpreter[j++] = hdr[k];
              k++;
          }
          interpreter[j] = 0;

          if(j == 0) goto bad;

          iunlockput(ip);
          ip = 0;

          // Reconstruct argv
          int count;
          for(count = 0; argv[count]; count++);

          if(count >= MAXARG - 1){
              end_op();
              return -1;
          }

          for(int m = count; m >= 0; m--){
              argv[m+1] = argv[m];
          }

          char *new_interp_str = kalloc();
          if(new_interp_str == 0){
              end_op();
              return -1;
          }
          safestrcpy(new_interp_str, interpreter, PGSIZE);
          argv[0] = new_interp_str;

          path = new_interp_str;
          goto retry;
      }
      // Not ELF and not Shebang -> Fail
      goto bad;
  }

  // --- ELF LOADING LOGIC (Unchanged) ---
  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    uint64 sz1;
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
      goto bad;
    sz = sz1;
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  p = myproc();
  uint64 oldsz = p->sz;

  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + (USERSTACK+1)*PGSIZE, PTE_W)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz-(USERSTACK+1)*PGSIZE);
  sp = sz;
  stackbase = sp - USERSTACK*PGSIZE;

  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; 
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  p->trapframe->a1 = sp;

  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry; 
  p->trapframe->sp = sp; 
  proc_freepagetable(oldpagetable, oldsz);

  return argc; 

 bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
  uint i, n;
  uint64 pa;

  for(i = 0; i < sz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i);
    if(pa == 0)
      panic("loadseg: address should exist");
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, 0, (uint64)pa, offset+i, n) != n)
      return -1;
  }
  
  return 0;
}
