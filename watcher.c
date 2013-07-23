/*
 * Copyright (c) 2010, The Android Open Source Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of The Android Open Source Project nor the names
 *    of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <cutils/properties.h>

#include <sys/atomics.h>

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>


extern prop_area *__system_property_area__;

typedef struct pwatch pwatch;

struct pwatch
{
    const prop_info *pi;
    unsigned serial;
};

static pwatch watchlist[1024];

static void announce(const prop_info *pi)
{
    char name[PROP_NAME_MAX];
    char value[PROP_VALUE_MAX];
    char *x;

    __system_property_read(pi, name, value);

    for(x = value; *x; x++) {
        if((*x < 32) || (*x > 127)) *x = '.';
    }

    fprintf(stderr,"%10d %s = '%s'\n", (int) time(0), name, value);
}

int main(int argc, char *argv[])
{
    prop_area *pa = __system_property_area__;
    unsigned serial = pa->serial;
    unsigned count = pa->count;
    unsigned n;

    if(count >= 1024) exit(1);

    for(n = 0; n < count; n++) {
        watchlist[n].pi = __system_property_find_nth(n);
        watchlist[n].serial = watchlist[n].pi->serial;
    }

    for(;;) {
        do {
            __futex_wait(&pa->serial, serial, 0);
        } while(pa->serial == serial);

        while(count < pa->count){
            watchlist[count].pi = __system_property_find_nth(count);
            watchlist[count].serial = watchlist[n].pi->serial;
            announce(watchlist[count].pi);
            count++;
            if(count == 1024) exit(1);
        }

        for(n = 0; n < count; n++){
            unsigned tmp = watchlist[n].pi->serial;
            if(watchlist[n].serial != tmp) {
                announce(watchlist[n].pi);
                watchlist[n].serial = tmp;
            }
        }
    }
    return 0;
}
