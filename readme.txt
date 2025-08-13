Source Files:
-cppProject1.cpp
Holds the one main function that runs both processes
The user program is read into memory before the process split
After the split, memory waits for read/write calls from the CPU
The CPU reads instructions from memory one by one
It runs every instruction as detailed in the instruction set
The interrupt timer is at the top of the CPU instruction loop
It counts until the timer threshold then causes an interrupt
It returns from an interrupt with an IRet instruction
The program ends when it reads a End instruction

Compiling:
There is only one file, so you should be able to easily compile in the command line with a command like g++. You can then run user programs in the same folder.
Format: cppProject1.cpp <userProgram> <interruptTimerValue>
Example: cppProject1.cpp sample1.txt 30