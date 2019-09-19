# os161-1.99

OS/161: OS/161 is a simple operating system kernel, which is made available to you along with a small set
of user-level libraries and programs that can be used for testing. The baseline OS/161 that we distribute
to you has very limited functionality. Each of the CS350 assignments will ask you to improve OS/161
in some way to add additional functionality to the baseline

Sys/161: Sys/161 is a machine simulator. It emulates the physical hardware on which OS/161 runs. Apart
from floating point support and certain issues relating to cache management, it provides an accurate
emulation of a server with a MIPS R3000 processor. You will use Sys/161 each time you want to run
OS/161. However, you are neither expected nor permitted to make any changes to Sys/161.

Implemented Kernel Synchronization Primitives, implemented several OS/161 process-related system calls, replaced dumbvm with
a new virtual memory system that relaxes a few of dumbvm’s limitations
