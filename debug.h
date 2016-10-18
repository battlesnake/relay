#pragma once

#if defined DEBUG_FD
#include <sys/stat.h>
static void analyse_fd(int fd)
{
	struct stat s;
	if (fstat(fd, &s)) {
		log_debug("fdstat(%d) failed (%d)", fd, errno);
		return;
	}
	static char buf[512];
	char *p = buf;
	p += sprintf(p, "File descriptor #%d:", fd);
	if (S_ISREG(s.st_mode)) {
		p += sprintf(p, " regular");
	}
	if (S_ISDIR(s.st_mode)) {
		p += sprintf(p, " directory");
	}
	if (S_ISCHR(s.st_mode)) {
		p += sprintf(p, " character");
	}
	if (S_ISBLK(s.st_mode)) {
		p += sprintf(p, " block");
	}
	if (S_ISFIFO(s.st_mode)) {
		p += sprintf(p, " pipe");
	}
	if (S_ISLNK(s.st_mode)) {
		p += sprintf(p, " symlink");
	}
	if (S_ISSOCK(s.st_mode)) {
		p += sprintf(p, " socket");
	}
	p += sprintf(p, "\n");
	log_debug("%s", buf);
}
#endif
