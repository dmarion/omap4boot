/*
 * Copyright (C) 2010 The Android Open Source Project
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
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>

#include "usb.h"

typedef struct tocentry {
	unsigned offset;
	unsigned length;
	unsigned flags;
	unsigned align;
	unsigned spare;
	char name[12];
} tocentry;

#define USE_TOC 0

int usb_boot(usb_handle *usb,
	     void *data, unsigned sz, 
	     void *data2, unsigned sz2)
{
	uint32_t msg_boot = 0xF0030002;
	uint32_t msg_size = sz;

	fprintf(stderr,"sending 2ndstage to target...\n");
	usb_write(usb, &msg_boot, sizeof(msg_boot));
	usb_write(usb, &msg_size, sizeof(msg_size));
	usb_write(usb, data, sz);

	if (data2) {
		fprintf(stderr,"waiting for 2ndstage response...\n");
		usb_read(usb, &msg_size, sizeof(msg_size));
		if (msg_size != 0xaabbccdd) {
			fprintf(stderr,"unexpected 2ndstage response\n");
			return -1;
		}
		msg_size = sz2;
		fprintf(stderr,"sending image to target...\n");
		usb_write(usb, &msg_size, sizeof(msg_size));
		usb_write(usb, data2, sz2);
	}
	
	return 0;
}

int match_omap4_bootloader(usb_ifc_info *ifc)
{
	if (ifc->dev_vendor != 0x0451)
		return -1;
	if (ifc->dev_product != 0xd00f)
		return -1;
	return 0;
}

void *load_file(const char *file, unsigned *sz)
{
	void *data;
	struct stat s;
	int fd;
	
	fd = open(file, O_RDONLY);
	if (fd < 0)
		return 0;
	
	if (fstat(fd, &s))
		goto fail;
	
	data = malloc(s.st_size);
	if (!data)
		goto fail;
	
	if (read(fd, data, s.st_size) != s.st_size) {
		free(data);
		goto fail;
	}
	
	close(fd);
	*sz = s.st_size;
	return data;
	
fail:
	close(fd);
	return 0;
}

extern void _binary_out_aboot_bin_start;
extern void _binary_out_aboot_bin_end;

int main(int argc, char **argv)
{
	void *data, *data2;
	unsigned sz, sz2;
	usb_handle *usb;
	int once = 1;

	fprintf(stderr,"?\n");
	if (argc < 2) {
		fprintf(stderr,"usage: usbboot [ <2ndstage> ] <image>\n");
		return 0;
	}

	if (argc < 3) {
		fprintf(stderr,"using built-in 2ndstage.bin\n");
		data = &_binary_out_aboot_bin_start;
		sz = &_binary_out_aboot_bin_end - &_binary_out_aboot_bin_start;
	} else {
		data = load_file(argv[1], &sz);
		if (data == 0) {
			fprintf(stderr,"cannot load '%s'\n", argv[1]);
			return -1;
		}
		argc--;
		argv++;
	}
	
	data2 = load_file(argv[1], &sz2);
	if (data2 == 0) {
		fprintf(stderr,"cannot load '%s'\n", argv[1]);
		return -1;
	}

	for (;;) {
		usb = usb_open(match_omap4_bootloader);
		if (usb)
			return usb_boot(usb, data, sz, data2, sz2);
		if (once) {
			once = 0;
			fprintf(stderr,"waiting for OMAP44xx device...\n");
		}
		usleep(250);
	}
	
	return -1;    
}
