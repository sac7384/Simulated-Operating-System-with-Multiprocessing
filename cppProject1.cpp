#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>

int main(int argCount, char* args[]) {
    //OS variables
    //User program and interrupt time are saved
    //OS starts in user mode
    std::string fileName = args[1];
    int maxTimer = atoi(args[2]);
    int timerValue = -1;
    bool kernelMode = false;
    bool running = true;
    srand(time(0)); //Setting random seed for the random int instruction

    //Memory variables
    //Allocates memory and initializes it to 0
    int* addresses = (int*) malloc(2000 * sizeof(int));
    for (int i = 0; i < 2000; i++) {addresses[i] = 0;}

    //CPU variables
    //PC starts reading at address 0 and the user stack starts at address 999
    int PC = 0, SP = 999, IR, AC = 0, X = 0, Y = 0;

    //Temp variables and opening file
    std::string tempString;
    std::string lineValue;
    std::ifstream userProgram; userProgram.open(fileName);
    if (userProgram.is_open()) {
        //Reads through entire input file
        int addressIndex = 0;
        while (userProgram) {
            std::getline (userProgram, tempString);
            if (tempString.empty()) {continue;}
            lineValue = tempString.at(0);
            //.integer line changes addressIndex to store following instructions in system memory
            if (lineValue == ".") {
                lineValue = tempString.at(1);
                for (int i = 2; i < tempString.length(); i++) { //
                    if (isdigit(tempString.at(i))) { lineValue = lineValue + tempString.at(i); }
                    else { break; }
                }
                addressIndex = stoi(lineValue);
                continue;
            }
            //Skip other lines that don't start with an integer
            else if (!isdigit(tempString.at(0))){ continue;}
            //Normal Instruction
            else {
                //Reads the full integer instruction then ignores the rest of the line
                for (int i = 1; i < tempString.length(); i++) { //
                    if (isdigit(tempString.at(i))) { lineValue = lineValue + tempString.at(i); }
                    else { break; }
                }
                //Stores instruction in memory
                addresses[addressIndex] = stoi(lineValue);
            }
            addressIndex++;
        }
    }//End of reading user program

    //Split processes and create pipes
    int MemToCPU[2];
    int CPUToMem[2];
    pipe(MemToCPU);
    pipe(CPUToMem);
    pid_t pid = fork();
    int RWI[3];

    //Memory, aka child process
    if (pid == 0) {
        //Close pipe ends that aren't needed
        close(MemToCPU[0]);
        close(CPUToMem[1]);

        //Running memory loop
        while (true) {
            //Reads an array of 3 integers from CPU
            //RWI[0] tells memory if this is a read or write to memory, read=1, write=2, exit=3
            //RWI[1] is the address
            //RWI[2] is the data being written (0 if this is a read)
            RWI[0] = 0; RWI[1] = 0; RWI[2] = 0;
            read(CPUToMem[0], RWI, sizeof(RWI));
            if (RWI[0] == 1) { //Read instruction, sends memory address to CPU
                //Storing read value in RWI[2] because write won't take single integers for whatever reason
                RWI[2] = addresses[RWI[1]];
                write(MemToCPU[1], RWI, sizeof(RWI));
            }
            else if (RWI[0] == 2) { //Write instruction, writes data into the address
                addresses[RWI[1]] = RWI[2];
            }
            else if (RWI[0] == 3) { //Close pipes and then exit
                close(MemToCPU[1]);
                close(CPUToMem[0]);
                break;
            }
        }
    }//End of memory/child process

    //CPU, aka parent process
    if (pid > 0) {
        //Close pipe ends that aren't needed
        close(MemToCPU[1]);
        close(CPUToMem[0]);

        //Running CPU loop
        while (running) {
            //Check for timer interrupt (jump to 1000)
            timerValue++;
            if (timerValue >= maxTimer) {
                timerValue = 0;
                if (!kernelMode) {
                    //Push SP to system stack
                    RWI[0] = 2; RWI[1] = 1999; RWI[2] = SP;
                    write(CPUToMem[1], RWI, sizeof(RWI)); //Write to an address
                    //Push PC to system stack
                    RWI[0] = 2; RWI[1] = 1998; RWI[2] = PC - 1;
                    write(CPUToMem[1], RWI, sizeof(RWI)); //Write to an address
                    SP = 1998; PC = 1000; kernelMode = true;
                }
            }

            //Read next instruction
            RWI[0] = 1; RWI[1] = PC; RWI[2] = 0;
            write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
            read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
            IR = RWI[2];

            //Execute instruction
            switch (IR) {
                case 1:  //AC = value
                    PC++;
                    //Read value to store in AC
                    RWI[0] = 1; RWI[1] = PC; RWI[2] = 0;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    AC = RWI[2];
                    break;

                case 2:  //AC = value at address
                    PC++;
                    //Read address
                    RWI[0] = 1; RWI[1] = PC; RWI[2] = 0;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    //Read value to store in AC
                    RWI[0] = 1; RWI[1] = RWI[2];

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    AC = RWI[2];
                    break;

                case 3:  //Load value at address of address, address > address > value >> AC
                    PC++;
                    //Read address
                    RWI[0] = 1; RWI[1] = PC; RWI[2] = 0;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    //Read second address
                    RWI[0] = 1; RWI[1] = RWI[2];

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    //Read value to store in AC
                    RWI[0] = 1; RWI[1] = RWI[2];

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    AC = RWI[2];
                    break;

                case 4:  //AC = value at address + X
                    PC++;
                    //Read address
                    RWI[0] = 1; RWI[1] = PC; RWI[2] = 0;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    //Read value to store in AC
                    RWI[0] = 1; RWI[1] = RWI[2] + X;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    AC = RWI[2];
                    break;

                case 5:  //AC = value at address + Y
                    PC++;
                    //Read address
                    RWI[0] = 1; RWI[1] = PC; RWI[2] = 0;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    //Read value to store in AC
                    RWI[0] = 1; RWI[1] = RWI[2] + Y;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    AC = RWI[2];
                    break;

                case 6:  //AC = value at SP + X
                    //Read value to store in AC
                    RWI[0] = 1; RWI[1] = SP + X; RWI[2] = 0;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    AC = RWI[2];
                    break;

                case 7:  //Store AC in address
                    PC++;
                    //Read address
                    RWI[0] = 1; RWI[1] = PC; RWI[2] = 0;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    //Write AC to address
                    RWI[0] = 2; RWI[1] = RWI[2]; RWI[2] = AC;
                    write(CPUToMem[1], RWI, sizeof(RWI)); //Write to an address
                    break;

                case 8:  //AC = random int 1-100
                    AC = 1 + (rand() % 100);
                    break;

                case 9:  //If 1, prints AC as int, if 2 prints AC as char
                    PC++;
                    //Read address
                    RWI[0] = 1; RWI[1] = PC; RWI[2] = 0;
                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    //Print AC
                    if (RWI[2] == 1) {std::cout << AC;} //Integer
                    else if (RWI[2] == 2) {std::cout << char(AC);} //Character
                    break;

                case 10: //AC += X
                    AC += X;
                    break;

                case 11: //AC += Y
                    AC += Y;
                    break;

                case 12: //AC -= X
                    AC -= X;
                    break;

                case 13: //AC -= Y
                    AC -= Y;
                    break;

                case 14: //X = AC
                    X = AC;
                    break;

                case 15: //AC = X
                    AC = X;
                    break;

                case 16: //Y = AC
                    Y = AC;
                    break;

                case 17: //AC = Y
                    AC = Y;
                    break;

                case 18: //SP = AC
                    SP = AC;
                    break;

                case 19: //AC = SP
                    AC = SP;
                    break;

                case 20: //Jump to address
                    PC++;
                    //Read address
                    RWI[0] = 1; RWI[1] = PC; RWI[2] = 0;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    //Change PC to address
                    PC = RWI[2] - 1;
                    break;

                case 21: //Jump if AC = 0
                    PC++;
                    //Read address
                    RWI[0] = 1; RWI[1] = PC; RWI[2] = 0;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    //If AC == 0 then jump
                    if (AC == 0) {PC = RWI[2] - 1;}
                    break;

                case 22: //Jump if AC != 0
                    PC++;
                    //Read address
                    RWI[0] = 1; RWI[1] = PC; RWI[2] = 0;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    //If AC != 0 then jump
                    if (AC != 0) {PC = RWI[2] - 1;}
                    break;

                case 23: //Push next PC onto stack, jump to address
                    PC++;
                    //Read address
                    RWI[0] = 1; RWI[1] = PC; RWI[2] = 0;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    IR = RWI[2]; //Using IR as a temp variable
                    //Push PC to the stack
                    SP--;
                    RWI[0] = 2; RWI[1] = SP; RWI[2] = PC;
                    write(CPUToMem[1], RWI, sizeof(RWI)); //Write to an address
                    //Jump to address
                    PC = IR - 1;
                    break;

                case 24: //Pop PC from the stack, jump to PC
                    RWI[0] = 1; RWI[1] = SP; RWI[2] = 0;

                    if (!kernelMode && RWI[1] > 999) { //Exit with error if user tries to access system memory
                        RWI[0] = 3;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                        running = false;
                        std::cout << "Error: User not able to access system memory\n";
                    }

                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    PC = RWI[2]; SP++;
                    break;

                case 25: //X++
                    X++;
                    break;

                case 26: //X--
                    X--;
                    break;

                case 27: //Push AC to stack
                    SP--;
                    RWI[0] = 2; RWI[1] = SP; RWI[2] = AC;
                    write(CPUToMem[1], RWI, sizeof(RWI)); //Write to an address
                    break;

                case 28: //Pop from stack into AC
                    RWI[0] = 1; RWI[1] = SP; RWI[2] = 0;
                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    AC = RWI[2]; SP++;
                    break;

                case 29: //Interrupt, save SP and PC to system stack, jump to 1500
                    if (!kernelMode) { //Don't interrupt during system calls
                        //Push SP to system stack
                        RWI[0] = 2; RWI[1] = 1999; RWI[2] = SP;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Write to an address
                        //Push PC to system stack
                        RWI[0] = 2; RWI[1] = 1998; RWI[2] = PC;
                        write(CPUToMem[1], RWI, sizeof(RWI)); //Write to an address
                        SP = 1998; PC = 1499; kernelMode = true;
                    }
                    break;

                case 30: //Return from interrupt
                    //Pop PC from the system stack
                    RWI[0] = 1; RWI[1] = SP; RWI[2] = 0;
                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    PC = RWI[2]; SP++;
                    //Pop SP from the system stack
                    RWI[0] = 1; RWI[1] = SP; RWI[2] = 0;
                    write(CPUToMem[1], RWI, sizeof(RWI)); //Ask to read an address
                    read (MemToCPU[0], RWI, sizeof(RWI)); //Instruction is in RWI[2]
                    SP = RWI[2]; kernelMode = false;
                    break;

                case 50: //End execution
                    RWI[0] = 3;
                    write(CPUToMem[1], RWI, sizeof(RWI)); //Tell memory to exit
                    running = false;
                    break;

            }//End of switch statement
            PC++;
        }//End of instruction loop
    }//End of CPU/parent process
}//End of main