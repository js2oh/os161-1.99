# os161-1.99

OS/161: OS/161 is a simple operating system kernel, which is made available to you along with a small set
of user-level libraries and programs that can be used for testing. The distributed baseline OS/161 has very limited functionality. 

Sys/161: Sys/161 is a machine simulator. It emulates the physical hardware on which OS/161 runs. Apart
from floating point support and certain issues relating to cache management, it provides an accurate
emulation of a server with a MIPS R3000 processor. Use Sys/161 each time running OS/161.

Implemented kernel synchronization primitives, implemented several OS/161 process-related system calls, replaced dumbvm with
a new virtual memory system that relaxes a few of dumbvm’s limitations to OS/161.
