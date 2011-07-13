/* -*- tab-width: 4; indent-tabs-mode: nil -*- */

/*
 * suns_app.c
 * $Id: $
 *
 * This implements the command-line UI to the features in the test app.
 *
 *
 * Copyright (c) 2011, John D. Blair <jdb@moship.net>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the name of John D. Blair nor his lackeys may be used
 *       to endorse or promote products derived from this software
 *       without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * JOHN D. BLAIR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <modbus.h>
#include <errno.h>
#include <endian.h>

#include "trx/macros.h"
#include "trx/debug.h"
#include "suns_app.h"
#include "suns_model.h"
#include "suns_output.h"
#include "suns_parser.h"
#include "suns_lang.tab.h"


void suns_app_init(suns_app_t *app)
{
    memset(app, 0, sizeof(suns_app_t));

    app->baud = 9600;
    app->serial_port = "/dev/ttyUSB0";
    app->hostname = "127.0.0.1";
    app->tcp_port = 502;
    app->test_server = 0;
    app->transport = SUNS_TCP;
    app->run_mainloop = 1;
    app->addr = 1;
    app->export_fmt = NULL;
    app->output_fmt = "text";
}


/* parse command-line options */
int suns_app_getopt(int argc, char *argv[], suns_app_t *app)
{
    int opt;

    /* option_error is used to signal that some invalid combination of
       arguments has been used.  if option_error is non-zero getopt()
       will exit the application after trying to parse all options */

    int option_error = 0;

    /* FIXME: add long options */

    while ((opt = getopt(argc, argv, "t:i:P:p:b:M:m:o:sx:va:")) != -1) {
        switch (opt) {
        case 't':
            if (strcasecmp(optarg, "tcp") == 0) {
                app->transport = SUNS_TCP;
                break;
            }
            if (strcasecmp(optarg, "rtu") == 0) {
                app->transport = SUNS_RTU;
                break;
            }
            error("unknown transport type: %s, must choose \"tcp\" or \"rtu\"",
                  optarg);
            option_error = 1;
            break;
            
        case 'i':
            app->hostname = optarg;
            break;
            
        case 'P':
            if (sscanf(optarg, "%d", &(app->tcp_port)) != 1) {
                error("unknown port number format: %s, "
                      "must provide decimal number", optarg);
                option_error = 1;
            }
            break;
            
        case 'p':
            app->serial_port = optarg;
            break;
            
        case 'b':
            if (sscanf(optarg, "%d", &(app->baud)) != 1) {
                error("unknown baud rate format: %s, "
                      "must provide decimal number", optarg);
                option_error = 1;
            }
            break;
            
        case 'm':
            /* keep running even if there are parsing errors */
            verbose(1, "parsing model file %s", optarg);
            suns_parse_model_file(optarg);
            break;
            
        case 's':
            app->test_server = 1;
            break;
            
        case 'v':
            verbose_level++;
            break;

        case 'a':
            if (sscanf(optarg, "%d", &(app->addr)) != 1) {
                error("must provide decimal number modbus address");
                option_error = 1;
            }
            break;

        case 'x':
            app->export_fmt = optarg;
            break;

        case 'o':
            app->output_fmt = optarg;
            break;
                        
        default:
            suns_app_help(argc, argv);
            exit(EXIT_SUCCESS);
        }
    }    
    /* always force localhost for server mode */
    if (app->test_server) {
        debug("forcing hostname to localhost (127.0.0.1)");
        app->hostname = "127.0.0.1";
    }
 
    /* bail if we hit an error */
    if (option_error) {
        exit(EXIT_FAILURE);
    }

    return 0;
}


void suns_app_help(int argc, char *argv[])
{
    printf("Usage: %s: \n", argv[0]);
    printf("      -o: output mode for data (text, csv, sql)\n");
    printf("      -x: export model description (slang, csv, sql)\n");
    printf("      -t: transport type: tcp or rtu (default: tcp)\n");
    printf("      -a: modbus slave address (default: 1)\n");
    printf("      -i: ip address to use for modbus tcp "
           "(default: localhost)\n");
    printf("      -P: port number for modbus tcp (default: 502)\n");
    printf("      -p: serial port for modbus rtu (default: /dev/ttyUSB0)\n");
    printf("      -b: baud rate for modbus rtu (default: 9600)\n");
    printf("      -m: specify model file\n");
    printf("      -s: run as a test server\n");
    printf("      -v: verbose level (up to -vvvv for most verbose)\n");
    printf("\n");
}

int suns_app_test_server(suns_app_t *app)
{
    modbus_mapping_t *mapping;
    int header_length;
    int offset = 0;
    list_node_t *c;
    int socket;
    int rc = 0;
    uint8_t *q;
    suns_parser_state_t *parser = suns_get_parser_state();

    header_length = modbus_get_header_length(app->mb_ctx);

    if (app->transport == SUNS_TCP) {
        q = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
    } else {
        q = malloc(MODBUS_RTU_MAX_ADU_LENGTH);
    }

    /* FIXME: should not use hard-coded mapping size */
    mapping = modbus_mapping_new(0, 0, 4096, 4096);
    
    if (mapping == NULL) {
        error("failed to allocate mapping: %s",
              modbus_strerror(errno));
        return -1;
        modbus_free(app->mb_ctx);
    }

    /* put sunspec id in the first 2 registers */
    mapping->tab_registers[offset] = SUNS_ID_HIGH;
    mapping->tab_registers[offset+1] = SUNS_ID_LOW;
    /* duplicate in the input registers space also */
    mapping->tab_input_registers[offset] = SUNS_ID_HIGH;
    mapping->tab_input_registers[offset+1] = SUNS_ID_LOW;
    
    offset += 2;

    /* add the data blocks in the order they are read in
       note that this means the common block data must be
       defined first */

    list_for_each(parser->data_block_list, c) {
        suns_data_block_t *dblock = c->data;
        debug("copying data block \"%s\" to register map "
              "starting at offset %d", dblock->name, offset);
    
        /* gross hack!

           libmodbus stores registers in host byte order, but
           we store our test data in modbus (be) byte order.
           this means we have to convert the data.  what a pita!
        */
           
        int r;  /* register offset, starting at zero */
        for (r = 0; r < buffer_len(dblock->data); r += 2) {
            mapping->tab_registers[offset] =
                ((uint8_t) buffer_data(dblock->data)[r] << 8) +
                ((uint8_t) buffer_data(dblock->data)[r + 1]);

            mapping->tab_input_registers[offset] =
                ((uint8_t) buffer_data(dblock->data)[r] << 8) +
                ((uint8_t) buffer_data(dblock->data)[r + 1]);

            verbose(4, "mapping->tab_registers[%d] = %04x",
                    offset, mapping->tab_registers[offset]);
                  
            offset++;
        }
    }

    /* tack on the end marker */
    mapping->tab_registers[offset] = 0xFFFF;
    mapping->tab_input_registers[offset] = 0xFFFF;

    /* start listening if we are a modbus tcp server */
    if (app->transport == SUNS_TCP) {
        socket = modbus_tcp_listen(app->mb_ctx, 1);
        if (socket < 0) {
            error("modbus_tcp_listen() returned %d: %s",
                  socket, modbus_strerror(errno));
            return rc;
        }
    }
    

    while (1) {
        /* wait for connection if modbus tcp */
        if (app->transport == SUNS_TCP) {
            rc = modbus_tcp_accept(app->mb_ctx, &socket);
            if (rc < 0) {
                error("modbus_tcp_accept() returned %d: %s",
                      rc, modbus_strerror(errno));
                return rc;
            }
        }

        /* loop forever, servicing client requests */
        while (1) {
            debug("top of loop");
            
            rc = modbus_receive(app->mb_ctx, -1, q);
            if (rc < 0) {
                debug("modbus_receive() returned %d: %s",
                      rc, modbus_strerror(errno));
                break;
            }
        
            /* the libmodbus machinery will service the
               request */
            rc = modbus_reply(app->mb_ctx, q, rc, mapping);
            if (rc < 0) {
                debug("modbus_reply() returned %d: %s",
                      rc, modbus_strerror(errno));
                break;
            }
        }
    }

    debug("exited main loop");

    if (app->transport == SUNS_TCP) {
        close(socket);
    }
    modbus_mapping_free(mapping);
    free(q);
    modbus_free(app->mb_ctx);

    return 0;
}


int suns_init_modbus(suns_app_t *app)
{
    int rc = 0;
    struct timeval timeout;

    if (app->transport == SUNS_TCP) {
        debug("modbus tcp mode: %s:%d", app->hostname, app->tcp_port);
        app->mb_ctx = modbus_new_tcp(app->hostname, app->tcp_port);
    } else if (app->transport == SUNS_RTU) {
        debug("modbus rtu mode");
        /* note we don't let the user set the parity,
           byte length or stop bit */
        app->mb_ctx = modbus_new_rtu(app->serial_port, app->baud,
                                     'N', 8, 1);
    } else {
        error("invalid transport type: %d", app->transport);
        exit(EXIT_FAILURE);
    }

    if (app->mb_ctx == NULL) {
        error("cannot initialize modbus context: %s",
              modbus_strerror(errno));
        return -1;
    }

    if (verbose_level > 3) {
        debug("setting libmodbus debug mode = 1");
        modbus_set_debug(app->mb_ctx, 1);
    }

    rc = modbus_set_slave(app->mb_ctx, app->addr);
    if (rc < 0) {
        debug("modbus_set_slave() failed: %s",
              modbus_strerror(errno));
    }

    /* modbus_connect() needs to be called by both the slave
       and the master to open the modbus layer */
    rc = modbus_connect(app->mb_ctx);
    if (rc < 0) {
        error("modbus_connect() returned %d: %s",
              rc, modbus_strerror(errno));
        return rc;
    }

    /* set timeout to 4 seconds */
    timeout.tv_sec = 4;
    timeout.tv_usec = 0;
    modbus_set_timeout_begin(app->mb_ctx, &timeout);

    return 0;
}


/* FIXME: here lies a gross hack

   the purpose of this function is to copy data from an array of
   uint16_t and put it into an array of unsigned char in big-endian
   byte order

   libmodbus presents retrieved registers in an array of uint16_t in 
   host byte order.  on x86, which is little ending, this means they've
   been swapped from the modbus big-endian on-the-wire format.

   this wouldn't be such a big deal if we were only reading 16 bit integers,
   but sunspec is composed of other structured data types, and the rest
   of the code i've already developed assumes it is reading an un-tampered
   buffer representing data pulled straight off the wire.

   my plan is to eliminate this by submitting a patch to libmodbus
   which fixes the issue
*/
   
int suns_app_swap_registers(uint16_t *reg,
                            int num_regs,
                            unsigned char *buf)
{
    int i;

    for (i = 0; i < num_regs; i++) {
        ((uint16_t*)buf)[i] = htobe16(reg[i]);
    }

    return 0;
}



int suns_app_read_device(suns_app_t *app)
{
    int rc = 0;
    int i;
    uint16_t regs[1024];
    unsigned char buf[2048];
    suns_model_did_t *did;
    int offset = 0;
    uint16_t len;
    suns_dataset_t *data;  /* holds decoded datapoints */

    /* we need the parser state to gain access to the data model definitions */
    suns_parser_state_t *sps = suns_get_parser_state();    

    /* places to look for the sunspec signature */
    int search_registers[] = { 1, 40001, 50001, -1 };
    int base_register = -1;

    /* look for sunspec signature */
    for (i = 0; search_registers[i] >= 0; i++) {
        debug("i = %d", i);
        /* libmodbus uses zero as the base address */
        rc = modbus_read_registers(app->mb_ctx, search_registers[i] - 1,
                                         2, regs);
        if (rc < 0) {
            debug("modbus_read_registers() returned %d: %s",
                  rc, modbus_strerror(errno));
            error("modbus_read_registers() failed: register %d on address %d",
                  search_registers[i], app->addr);
            continue;
        }
        if ( (regs[0] == SUNS_ID_HIGH) &&
             (regs[1] == SUNS_ID_LOW) ) {
            base_register = search_registers[i];
            verbose(1, "found sunspec signature at register %d",
                    base_register);
            break;
        }
    }

    /* check if we found the sunspec block */
    if (base_register < 0) {
        error("sunspec block not found on device");
        return -1;
    }

    offset = 2;
    
    /* loop over all data models as they are discovered */
    while (1) {
        /* read the next 2 registers to get the did and the length */
        debug("looking for sunspec data block at %d",
              base_register + offset);

        rc = modbus_read_registers(app->mb_ctx,
                                   base_register + offset - 1,
                                   2, regs);
        if (rc < 0) {
            debug("modbus_read_registers() returned %d: %s",
                  rc, modbus_strerror(errno));
            error("modbus_read_registers() failed: register %d on address %d",
                  base_register + offset, app->addr);
            rc = -1;
            break;
        }

        /* did we stumble upon an end marker? */
        if (regs[0] == 0xFFFF) {
            verbose(1, "found end marker at register %d",
                    base_register + offset - 1);
            rc = 0;
            break;
        }

        /* since we're a test program we need to check for a missing
           end marker.  all we can really do is check for zero, since
           we can't tell the difference between a did we don't know
           and some other data. */
        if (regs[0] == 0) {
            warning("found 0x0000 where we should have found "
                    "an end marker or another did.");
            rc = -1;
            break;
        }

        len = regs[1];
        debug("found did = %d, len = %d", regs[0], len);
        did = suns_find_did(sps->did_list, regs[0]);
        
        if (did == NULL) {
            warning("unknown did: %d", regs[0]);
            /* skip ahead the reported length */
            /* offset += len + 2; */
        } else {
            /* we found a did we know about */
            
            /* is the length what we expect? */
            /* FIXME: it is possible for a repeatable data block to 
               be larger than what can fit into a single modbus frame.
               in this case we need to retrieve it in multiple passes! */
            /* check out this pointer indirection!! */
            suns_dp_block_t *last_dp_block = did->model->dp_blocks->tail->data;
            if ((did->model->len != len) &&
                (last_dp_block->repeating &&
                 (((len - did->model->base_len) %
                   (did->model->len - did->model->base_len)) != 0))) {
                error("data model length %d does not match expected length %d",
                      len, did->model->len);
            }
        }

        /* retrieve data block, including the did and len */
        /* add 2 to len to include did & len registers */
        rc = modbus_read_registers(app->mb_ctx,
                                   base_register + offset - 1,
                                   len + 2, regs);
        if (rc < 0) {
            debug("modbus_read_registers() returned %d: %s",
                  rc, modbus_strerror(errno));
            error("modbus_read_registers() failed: register %d on address %d",
                  base_register + offset, app->addr);
            rc = -1;
            break;
        }

        /* kludge around the way libmodbus works */
        suns_app_swap_registers(regs, len + 2, buf);

        if (did) {
            /* suns_decode_data() requires the length in bytes, not
               modbus registers */
            /* add 2 to len to include did & len registers */
            data = suns_decode_data(sps->did_list, buf, (len + 2) * 2);
            suns_dataset_output(app->output_fmt, data, stdout);
            suns_dataset_free(data);
        } else {
            /* unknown data block */
            if (verbose_level > 0) {
                dump_buffer(stdout, buf, (len + 2) * 2);
            }
        }
        
        /* jump ahead to next data block */
        offset += len + 2;
    }

    return rc;
}



int suns_app_read_data_model(modbus_t *ctx)
{
    return 0;
}


int main(int argc, char **argv)
{
    /* list_node_t *c; */
    suns_app_t app;
    list_node_t *c;

    /* global parser state */
    suns_parser_state_t *sps = suns_get_parser_state();

    /* initialize suns app global state */
    suns_app_init(&app);

    /* initialize parser globals */
    suns_parser_init();

    /* parse options */
    /* this has the side effect of parsing any specified model files */
    suns_app_getopt(argc, argv, &app);

    /* fill in offset data in any parsed model files */
    /* maybe this should be part of suns_parser.c? */
    list_for_each(sps->model_list, c) {
        suns_model_fill_offsets(c->data);
    }

    /* display options in debug mode */
    if (app.transport == SUNS_TCP) {
        debug("transport: TCP");
    } else if (app.transport == SUNS_RTU) {
        debug("transport: RTU");
    } else {
        debug("unknown transport: %d", app.transport);
    }
    debug("hostname: %s", app.hostname);
    debug("tcp_port: %d", app.tcp_port);
    debug("serial_port: %s", app.serial_port);
    debug("baud: %d", app.baud);
    debug("test_server: %d", app.test_server);
    debug("export_fmt: %s", app.export_fmt);
    
    /* are we invoked in model export mode? */
    if (app.export_fmt != NULL) {
        suns_model_export_all(app.export_fmt, sps->model_list, stdout);
        exit(EXIT_SUCCESS);
    }

    /* initialize the modbus layer (same for server and client) */
    suns_init_modbus(&app);

    /* run server / slave */
    if (app.test_server) {
        debug("test server mode - acting as modbus slave");
        suns_app_test_server(&app);
    } else {
        /* run client / master */
        debug("suns client (master) mode");
        suns_app_read_device(&app);
    }

    exit(EXIT_SUCCESS);
}


