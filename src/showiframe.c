/*
 * showiframe.c
 *
 * Copyright (C) 2005 Felix Domke <tmbinc@elitedvb.net>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <linux/dvb/video.h>

#ifdef __aarch64__

struct video_frame {
	uint64_t pts;
	ssize_t bytes[8];
	const uint8_t *data[8];
	int is_phys_addr[8];
};

#define VIDEO_SET_FRAME _IOWR('o', 64, struct video_frame)

#endif

void c(int a)
{
	if (a < 0)
	{
		perror("ioctl");
		exit(6);
	}
}

int main(int argc, char **argv)
{
	struct stat s;
	if (argc != 2)
	{
		printf("usage: %s <iframe>\n", *argv);
		return 3;
	}

	int f = open(argv[1], O_RDONLY);
	if (f < 0)
	{
		perror(argv[1]);
		return 4;
	}
	fstat(f, &s);

	int fd = open("/dev/dvb/adapter0/video0", O_WRONLY);

	if (fd <= 0)
	{
		perror("/dev/dvb/adapter0/video0");
		return 2;
	}
	else if (fork() != 0)
		return 0;
	else
	{
		size_t pos=0;
		int seq_end_avail = 0;
		unsigned char pes_header[] = { 0x00, 0x00, 0x01, 0xE0, 0x00, 0x00, 0x80, 0x00, 0x00 };
		unsigned char seq_end[] = { 0x00, 0x00, 0x01, 0xB7 };
		unsigned char iframe[s.st_size];
		unsigned char stuffing[8192];
		memset(stuffing, 0, 8192);
		read(f, iframe, s.st_size);
		ioctl(fd, VIDEO_SET_STREAMTYPE, 0); // set to mpeg2
		c(ioctl(fd, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY));
		c(ioctl(fd, VIDEO_PLAY));
		c(ioctl(fd, VIDEO_CONTINUE));
		while(pos <= (s.st_size-4) && !(seq_end_avail = (!iframe[pos] && !iframe[pos+1] && iframe[pos+2] == 1 && iframe[pos+3] == 0xB7)))
			++pos;
#ifndef __aarch64__
		c(ioctl(fd, VIDEO_CLEAR_BUFFER));
		if ((iframe[3] >> 4) != 0xE) // no pes header
			write(fd, pes_header, sizeof(pes_header));
		else {
			iframe[4] = iframe[5] = 0x00;
		}
		write(fd, iframe, s.st_size);
		if (!seq_end_avail)
			write(fd, seq_end, sizeof(seq_end));
		write(fd, stuffing, 8192);
#else
		{
			struct video_frame fr;
			int pos = 0;
			memset(&fr, 0, sizeof(fr));
			fr.bytes[pos] = sizeof(iframe);
			fr.data[pos++] = iframe;
			fr.pts = 0;
			if (!seq_end_avail) {
				fr.bytes[pos] = sizeof(seq_end);
				fr.data[pos++] = seq_end;
			}
			fr.bytes[pos] = sizeof(stuffing);
			fr.data[pos++] = stuffing;
			c(ioctl(fd, VIDEO_SET_FRAME, &fr));
		}
#endif
		usleep(150000);
		c(ioctl(fd, VIDEO_STOP, 0));
		c(ioctl(fd, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX));
	}
	return 0;
}

