#pragma once

#include <cstdint>
#include <cstdio>
#include <utility>
#include <ostream>

struct midOsProg {

public:

    enum class Register {
        R1 = 1, R2, R3, R4, R5,
        R6, R7, R8, R9, R10
    };

private:

    std::ostream& emitter;

    static std::string get_register(Register r) {
        return "r" + std::to_string(static_cast<int>(r));
    }

    struct Instructions {
        static std::string addr(Register r1, Register r2) {
            return "addr " + get_register(r1) + ", " + get_register(r2) +
                " ; r" + std::to_string(static_cast<int>(r1)) + " <- r" +
                std::to_string(static_cast<int>(r1)) + " + r" +
                std::to_string(static_cast<int>(r2)) + "\n";
        }

        static std::string addi(Register r, int32_t v) {
            return "addi " + get_register(r) + ", #" + std::to_string(v) +
                " ; r" + std::to_string(static_cast<int>(r)) + " <- r" +
                std::to_string(static_cast<int>(r)) + " + " + std::to_string(v) + "\n";
        }

        static std::string incr(Register r) {
            return "incr " + get_register(r) +
                " ; r" + std::to_string(static_cast<int>(r)) + "++\n";
        }

        static std::string movi(Register r, int32_t v) {
            return "movi " + get_register(r) + ", #" + std::to_string(v) +
                " ; r" + std::to_string(static_cast<int>(r)) + " <- " + std::to_string(v) + "\n";
        }

        static std::string movr(Register r1, Register r2) {
            return "movr " + get_register(r1) + ", " + get_register(r2) +
                " ; r" + std::to_string(static_cast<int>(r1)) + " <- r" +
                std::to_string(static_cast<int>(r2)) + "\n";
        }

        static std::string pushr(Register r) {
            return "pushr " + get_register(r) +
                " ; push r" + std::to_string(static_cast<int>(r)) + "\n";
        }

        static std::string pushi(int32_t v) {
            return "pushi #" + std::to_string(v) +
                " ; push " + std::to_string(v) + "\n";
        }

        static std::string popr(Register r) {
            return "popr " + get_register(r) +
                " ; pop -> r" + std::to_string(static_cast<int>(r)) + "\n";
        }

        static std::string cmpi(Register r, int32_t v) {
            return "cmpi " + get_register(r) + ", #" + std::to_string(v) +
                " ; compare r" + std::to_string(static_cast<int>(r)) +
                " with " + std::to_string(v) + "\n";
        }

        static std::string cmpr(Register r1, Register r2) {
            return "cmpr " + get_register(r1) + ", " + get_register(r2) +
                " ; compare r" + std::to_string(static_cast<int>(r1)) +
                " with r" + std::to_string(static_cast<int>(r2)) + "\n";
        }

        static std::string jmpi(int32_t offset) {
            return "jmpi #" + std::to_string(offset) +
                " ; jump " + std::to_string(offset) + " bytes\n";
        }

        static std::string jmpa(uint32_t addr) {
            return "jmpa #" + std::to_string(addr) +
                " ; jump to address " + std::to_string(addr) + "\n";
        }

        static std::string jei(int32_t offset) {
            return "jei #" + std::to_string(offset) +
                " ; if equal, jump " + std::to_string(offset) + " bytes\n";
        }

        static std::string jea(uint32_t addr) {
            return "jea #" + std::to_string(addr) +
                " ; if equal, jump to " + std::to_string(addr) + "\n";
        }

        static std::string jlti(int32_t offset) {
            return "jlti #" + std::to_string(offset) +
                " ; if less, jump " + std::to_string(offset) + " bytes\n";
        }

        static std::string jlta(uint32_t addr) {
            return "jlta #" + std::to_string(addr) +
                " ; if less, jump to " + std::to_string(addr) + "\n";
        }

        static std::string jgti(int32_t offset) {
            return "jgti #" + std::to_string(offset) +
                " ; if greater, jump " + std::to_string(offset) + " bytes\n";
        }

        static std::string jgta(uint32_t addr) {
            return "jgta #" + std::to_string(addr) +
                " ; if greater, jump to " + std::to_string(addr) + "\n";
        }

        static std::string printr(Register r) {
            return "printr " + get_register(r) +
                " ; print r" + std::to_string(static_cast<int>(r)) + "\n";
        }

        static std::string printm(Register r) {
            return "printm " + get_register(r) +
                " ; print mem[r" + std::to_string(static_cast<int>(r)) + "]\n";
        }

        static std::string printcr(Register r) {
            return "printcr " + get_register(r) +
                " ; print r" + std::to_string(static_cast<int>(r)) + " as char\n";
        }

        static std::string printcm(Register r) {
            return "printcm " + get_register(r) +
                " ; print mem[r" + std::to_string(static_cast<int>(r)) + "] as char\n";
        }

        static std::string call(Register r) {
            return "call " + get_register(r) +
                " ; call at offset r" + std::to_string(static_cast<int>(r)) + "\n";
        }

        static std::string ret() {
            return "ret ; return\n";
        }

        static std::string exit() {
            return "exit ; terminate\n";
        }
    };
    
    

public:

    midOsProg(std::ostream& out) : emitter(out) {}

    // Arithmetic expressions - uses a pure stack machine approach
    // by popping the top 2 recent values, calculating, then putting
    // it back into the stack.

    void push_value(Register r, uint32_t v) {
        emitter <<
        "; ---- PUSHING VALUE ONTO STACK BY ALLOCATING INTO REGISTER AND PUSHING ---- \n" <<
        Instructions::movi(r, v) <<
        Instructions::pushr(r);
    }

    void add() {
        emitter <<
        "; ---- POPPING THE TOP TWO VALUES FROM THE STACK INTO R1 AND R2 TO ADD ---- \n"
        << Instructions::popr(Register::R1)
        << Instructions::popr(Register::R2)
        << Instructions::addr(Register::R1, Register::R2)
        << Instructions::pushr(Register::R1);
    }

    void sub() {
        emitter
        << "; ---- SUBTRACTION: r1 - r2 ----\n"
        << Instructions::popr(Register::R2)
        << Instructions::popr(Register::R1)
        << Instructions::cmpr(Register::R1, Register::R2)
        << Instructions::pushr(Register::R1);
    }

    void mult() {
        emitter
        << "; ------------------------ MULTIPLICATION ------------------------ \n"
        << "; R1 acts as the accumulator, R2 is the LHS and decrements,\n"
        << "; R3 is RHS, and R4 acts as a negation flag if RHS is negative.\n"
        << "; ---------------------------------------------------------------- \n"
        << Instructions::movi(Register::R4, 0)
        << Instructions::popr(Register::R3)
        << Instructions::popr(Register::R2)
        << Instructions::cmpr(Register::R2, Register::R4)
        << Instructions::jgti(99)
        << Instructions::pushr(Register::R2);
        neg();
        emitter
        << Instructions::popr(Register::R2)
        << Instructions::incr(Register::R4)
        << Instructions::movi(Register::R1, 0)
        << Instructions::addr(Register::R1, Register::R3)
        << Instructions::addi(Register::R2, -1)
        << Instructions::cmpi(Register::R2, 0)
        << Instructions::jgti(-27)
        << Instructions::cmpi(Register::R4, 0)
        << Instructions::jei(90)
        << Instructions::pushr(Register::R1);
        neg();
        emitter
        << Instructions::popr(Register::R1)
        << Instructions::pushr(Register::R1)
        << "; ------------------------ END MULTIPLICATION -------------------- \n";
    }

    void div() {
        emitter
        << "; --------------------------- DIVISION --------------------------- \n"
        << Instructions::popr(Register::R2)
        << Instructions::popr(Register::R1)
        << Instructions::movi(Register::R3, 0)
        << Instructions::movi(Register::R4, 0)
        << Instructions::movi(Register::R5, 0)
        << Instructions::cmpr(Register::R2, Register::R4)
        << Instructions::jgti(117)
        << Instructions::pushr(Register::R1)
        << Instructions::pushr(Register::R2);
        neg();
        emitter
        << Instructions::popr(Register::R2)
        << Instructions::popr(Register::R1)
        << Instructions::incr(Register::R4)
        << Instructions::cmpr(Register::R1, Register::R5)
        << Instructions::jgti(117)
        << Instructions::pushr(Register::R2)
        << Instructions::pushr(Register::R1);
        neg();
        emitter
        << Instructions::popr(Register::R1)
        << Instructions::popr(Register::R2)
        << Instructions::incr(Register::R5)
        << Instructions::pushr(Register::R1)
        << Instructions::pushr(Register::R2);
        sub();
        emitter
        << Instructions::popr(Register::R1)
        << Instructions::incr(Register::R3)
        << Instructions::cmpi(Register::R1, 0)
        << Instructions::jgti(-81)
        << Instructions::pushr(Register::R3)
        << Instructions::movr(Register::R3, Register::R1)
        << Instructions::addr(Register::R3, Register::R2)
        << Instructions::jei(54)
        << Instructions::pushi(1);
        sub();
        emitter
        << Instructions::cmpr(Register::R4, Register::R5)
        << Instructions::jei(72);
        neg();
        emitter
        << "; ------------------------- END DIVISION ------------------------- \n";
    }

    void mod() {
        emitter
        << Instructions::popr(Register::R7)
        << Instructions::popr(Register::R6)
        << Instructions::pushr(Register::R6)
        << Instructions::pushr(Register::R7);
        div();
        emitter
        << Instructions::cmpi(Register::R5, 0)
        << Instructions::pushr(Register::R3)
        << Instructions::jei(72);
        neg();
        emitter
        << Instructions::popr(Register::R1);
    }

    void neg() {
        emitter
        << "; ---- Negation via subtracting from register ----\n"
        // workaround for sub expecting rhs to be on top of stack:
        // make the operand being subtracted from below 0. 
        << Instructions::popr(Register::R1)
        << Instructions::pushi(0)
        << Instructions::pushr(Register::R1);
        sub();
    }
        
};