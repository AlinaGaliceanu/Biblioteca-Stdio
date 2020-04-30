#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <paths.h>
#include "so_stdio.h"


#define BUFF_SIZE 4096

struct _so_file {
	char *buffer;
	int fd;
	int lastOp; /* 1 pentru scris , 0 pentru citit */
	int bytesRead;
	int flagError;
	int flagEnd;
	int pid;
	long size; /* lungime fisier */
	long pos; /* positia caracterului in buffer; */
	long tell; /* pozitia curenta in fisier */

};

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	struct _so_file *my_file = malloc(sizeof(SO_FILE));

	if (my_file == NULL)
		return NULL;

	my_file->fd = -1;
	/* read */
	if (mode[0] == 'r') {
		if (strlen(mode) > 1) {
			if (mode[1] == '+') {
				my_file->fd = open(pathname, O_RDWR);
				if (my_file->fd < 1) {
					free(my_file);
					return NULL;
				}
			}
		} else {
			my_file->fd = open(pathname, O_RDONLY);
			if (my_file->fd < 1) {
				free(my_file);
				return NULL;
			}
		}
	} else {
		/* write */
		if (mode[0] == 'w') {
			if (strlen(mode) > 1) {
				if (mode[1] == '+')
					my_file->fd = open(pathname, O_RDWR |
					 O_CREAT | O_TRUNC);
			} else
				my_file->fd = open(pathname, O_WRONLY |
					 O_CREAT | O_TRUNC);
		} else {
			/* append */
			if  (mode[0] == 'a') {
				if (strlen(mode) > 1) {
					if (mode[1] == '+')
						my_file->fd = open(pathname,
						 O_RDWR | O_CREAT | O_APPEND);
				} else
					my_file->fd = open(pathname, O_APPEND |
						 O_WRONLY | O_CREAT);
			}
		}
	}
	/* operatia de deschidere a esuat */
	if (my_file->fd == -1) {
		free(my_file);
		return NULL;
	}
	my_file->buffer = calloc(BUFF_SIZE, sizeof(char));
	if (my_file->buffer == NULL)
		return NULL;
	my_file->pos  = 0;
	my_file->lastOp = 0;
	my_file->tell = 0;
	my_file->bytesRead = 0;
	my_file->flagError = 0;
	my_file->flagEnd = 0;
	my_file->pid = 0;
	return my_file;
}


int so_fclose(SO_FILE *stream)
{
	/* eroare aparuta in scierea datelor in fisier*/
	if (so_fflush(stream) < 0) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}
	if (close(stream->fd) < 0) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}
	free(stream->buffer);
	free(stream);
	return 0;
}

int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_fflush(SO_FILE *stream)
{
	if (stream->lastOp == 1) {
		int tmp = write(stream->fd, stream->buffer, stream->pos % 4096);

		if (tmp < 0) {
			stream->flagError = 1;
			return SO_EOF;
		}
		while (tmp <  stream->pos % 4096) {
			int aux = write(stream->fd, stream->buffer + tmp,
					 stream->pos % 4096 - tmp);

			tmp += aux;
			if (aux == 0 || tmp < 0)
				return 0;
		}
		stream->pos = 0;
	}
	return 0;
}

int so_fgetc(SO_FILE *stream)
{
	if ((stream->pos == 0 || stream->bytesRead == 0)
		|| stream->pos == stream->bytesRead) {
		stream->pos = read(stream->fd, stream->buffer, BUFF_SIZE);
		if (stream->pos <=  0) {
			stream->flagError = 1;
			if (stream->pos == 0)
				stream->flagEnd = 1;
			return SO_EOF;
		}
		stream->bytesRead = 0;
	}
	return (int)stream->buffer[stream->bytesRead++];
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int totalToRead = nmemb * size;
	int pos;

	for (pos = 0; pos < totalToRead; pos++) {
		int chr = so_fgetc(stream);

		if (stream->flagEnd == 1)
			break;
		if (chr == SO_EOF && stream->flagError == 1)
			return 0;
		*((char *)ptr + pos) = chr;
	}
	stream->tell += nmemb;
	return pos / size;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	if (stream == NULL)
		return SO_EOF;
	int bytesWrite = nmemb * size;

	for (int pos = 0; pos < bytesWrite; ++pos) {
		int chr = *((char *)ptr + pos);

		so_fputc(chr, stream);
	}
	stream->tell += bytesWrite;
	return nmemb;
}

int so_fputc(int c, SO_FILE *stream)
{
	stream->buffer[stream->pos % 4096] = (char)c;
	stream->pos++;
	stream->lastOp = 1;
	if (stream->pos % BUFF_SIZE == 0) {
		int tmp = write(stream->fd, stream->buffer, BUFF_SIZE);

		if (tmp < 0) {
			stream->flagError = 1;
			return SO_EOF;
		}
		while (tmp < BUFF_SIZE) {
			tmp += write(stream->fd, stream->buffer + tmp,
					 BUFF_SIZE - tmp);
			if (tmp < 0) {
				stream->flagError = 1;
				return SO_EOF;
			}
		}
		stream->lastOp = 0;
		stream->pos = 0;
	}
	if (stream == NULL)
		return SO_EOF;
	return c;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{

	so_fflush(stream); /* scriere date in fisier, daca exista */
	stream->lastOp = 0;
	/* mutare cursor in fisier la pozitia offset */
	stream->tell = lseek(stream->fd, offset, whence);
	if (stream->tell < 0)
		return -1;
	stream->bytesRead = 0;
	return 0;
}

long so_ftell(SO_FILE *stream)
{
	return stream->tell;
}

int so_feof(SO_FILE *stream)
{
	if (stream->flagError == 0)
		return 0;
	return SO_EOF;
}

int so_ferror(SO_FILE *stream)
{
	if (stream->flagError == 1)
		return SO_EOF;
	return 0;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	SO_FILE *my_file;
	int pdes[2] = {0, 0}, pid = -1;

	pipe(pdes);
	my_file = malloc(sizeof(SO_FILE));
	if (my_file == NULL)
		return NULL;

	my_file->buffer = malloc(BUFF_SIZE * sizeof(char));
	if (my_file->buffer == NULL)
		return NULL;
	my_file->fd = 0;
	my_file->pid = 0;


	switch (pid = fork()) {
	case -1:
		close(pdes[0]);
		close(pdes[1]);
		free(my_file->buffer);
		free(my_file);
		return NULL;
	case 0:
		if (type[0] == 'r') {
			if (pdes[1] != STDOUT_FILENO) {
				dup2(pdes[1], STDOUT_FILENO);
				close(pdes[1]);
				pdes[1] = STDOUT_FILENO;
			}
			close(pdes[0]);
		} else {
			if (pdes[0] != STDIN_FILENO) {
				dup2(pdes[0], STDIN_FILENO);
				close(pdes[0]);
			}
			close(pdes[1]);
		}
		/* procesul copil */
		execlp(_PATH_BSHELL, "sh", "-c", command, NULL);
		return NULL;
	}


	/* procesul parinte */
	if (type[0] == 'r') {
		my_file->fd = pdes[0];
		close(pdes[1]);
	} else {
		my_file->fd = pdes[1];
		close(pdes[0]);
	}
	my_file->pid = pid;
	my_file->pos  = 0;
	my_file->lastOp = 0;
	my_file->tell = 0;
	my_file->bytesRead = 0;
	my_file->flagError = 0;
	my_file->flagEnd = 0;
	return my_file;
}

int so_pclose(SO_FILE *stream)
{

	int pid = -1, pstatus = -1;
	int waitedPid = stream->pid;

	so_fclose(stream);
	pid = waitpid(waitedPid, &pstatus, 0);

	if (pid == -1)
		return -1;

	return pstatus;
}
