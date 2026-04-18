#pragma once

#include "x86Instructions.hpp"
#include <sys/mman.h>
#include <cstdio>
#include <ostream>
#include <utility>
#include <iostream>
#include <iomanip>

struct x86Prog {

    x86::Instruction prog_fn = x86::compose();

    void append(x86::Instruction with) {
        prog_fn = x86::compose(
            std::move(prog_fn),
            std::move(with)
        );
    }
    
    std::size_t size() {
        return prog_fn.byte_size;
    }
    
    static void run_prog(const x86Prog& p, int max_size = 50000) {
        unsigned char* prog = (unsigned char*) mmap(0, max_size,
                                PROT_EXEC | PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (prog == MAP_FAILED) {
            perror("mmap failed");
        }

        int ptr = 0;
        
        p.prog_fn.write_bytes(prog, ptr);

        // for (int i = 0; i < ptr; i++) {
        //     std::cout << std::hex << std::setw(2) << std::setfill('0')
        //           << static_cast<unsigned int>(prog[i]) << " ";
        // }

        ((int (*) (void)) prog) ();
        munmap(prog, max_size);
    }

    static void emit_prog(const x86Prog& p, std::ostream& o) {
        o << p.prog_fn.emitted_content;
    }
        
};