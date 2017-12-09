/**
 * \file
 * \brief This is a helper, that helps making sense out of a SIGSEGV
 *        or unhandled exception although it is not perfect ...
 *
 * \author Max Resch
 * \date 2012-11-04
 *
 *
 * \todo
 * 	- Check if addr2line is even accessible
 * 	- Only build this file when compiled with -g
 * 	- Look into threading
 */

#ifndef STACKTRACE_H_
#define STACKTRACE_H_

#include <stdio.h>
#include <unistd.h>
#include <execinfo.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#ifdef __cplusplus
#include <exception>

extern "C"
{

#endif // __cplusplus

/**
 * \brief This function registers the handler.
 *
 * \details
 * 	The handler should be registered on top of the
 * 	main function. The argv is needed so that the
 * 	filename of the executable can be determined.
 *
 * \code
 * int main(int argc, char** argv)
 * {
 * 	register_handler(argv)
 * ...
 */
void register_handler (char** argv);

/**
 * \brief This function prints the stacktrace to stderr
 */
void __trace ();

//----------------------------------------------------------------------------//
//------------------------------ IMPLEMENTATION ------------------------------//
//----------------------------------------------------------------------------//

// maximum size of the stacktrace
#define __MAX_STACK 256

// maximum size of function names, file names, calls etc.
// basically all strings.
#define __MAX_FUNC 1024

/// Filename of this executable
char* __command;

/**
 * \brief Get the functionname, sourcefile name and line number corresponding to
 *        the address.
 *
 * \details
 * 	This function uses the tool addr2line to determine the data, the program
 * 	must be compiled with -g to include these debug symbols.
 */
int __get_source_line (size_t addr, char* cmd, char* file, size_t file_len, char* func, size_t func_len, int* line)
{
	char buf[__MAX_FUNC] = "";
	char* p;

	snprintf(buf, __MAX_FUNC, "/usr/bin/addr2line -Cfe '%s' -i 0x%lx", cmd, addr);
	FILE* f = popen(buf, "r");

	if (f == NULL)
		return 1;

	fgets(buf, __MAX_FUNC, f);
	if (buf[0] != '?')
	{
		for (char* a = buf; *a != '\0'; ++a)
			if (*a == '\n')
			{
				*a = '\0';
				break;
			}
		strncpy(func, buf, func_len);
	}
	else
	{	fclose(f);
		return 2;
	}

	fgets(buf, __MAX_FUNC, f);
	fclose(f);
	if (buf[0] != '?')
	{
		p = buf;
		while (*p != ':')
		{
			p++;
		}
		*p++ = '\0';
		*line = atol(p);
		strncpy(file, buf, file_len);
	}
	else
	{
		return 2;
	}
	return 0;
}

/**
 * \brief Resolve shared library function names according to memory map
 *
 * \details
 * 	Functions in shared libs cannot be read statically by addr2line. So the
 * 	address of the function in the library must be calculated according to
 * 	the memory map in /proc/self/maps
 */
void __get_address_map (size_t addr,  char* file, size_t file_len, char* func, size_t func_len, int* line)
{
	char buff[__MAX_FUNC] = "";
	pid_t pid = getpid();
	snprintf(buff, __MAX_FUNC, "/proc/%d/maps", pid);

	FILE *f = fopen(buff, "r");
	for (fgets(buff, __MAX_FUNC, f); buff != 0; fgets(buff, __MAX_FUNC, f))
	{
		size_t begin = 0, end = 0;

		sscanf(buff, "%lx-%lx", &begin, &end);

		if (addr >= begin && addr <= end)
		{
			char* p;
			for (p = buff + 1; *p != '\0'; ++p)
				if (*p == ' ' && *(p - 1) == ' ')
					break;
			if (*p != '\0')
			{
				for (; *p != '\0'; ++p)
					if (*p != ' ')
						break;
				if (*p != '\0')
				{
					p[strlen(p) - 1] = '\0';
					__get_source_line(addr - begin, p, file, file_len, func, func_len, line);
				}
			}
			fclose(f);
			return;
		}
	}
	fclose(f);
}

void __trace ()
{
	void* address[__MAX_STACK];
	size_t addr_size = 0;
	addr_size = backtrace(address, __MAX_STACK);

	if (addr_size == 0)
	{
		fprintf(stderr, "Invalid stacktrace\n");
		return;
	}

	fprintf(stderr, "Stacktrace:\n");

	//char** symbollist = backtrace_symbols(address, __MAX_STACK);
	char file[__MAX_FUNC] = "";
	char recent_file[__MAX_FUNC] = "";
	char function[__MAX_FUNC] = "";
	//size_t function_len = __MAX_FUNC;

	for (int i = 0; i < addr_size; i++)
	{
		size_t inst_pointer = (size_t) address[i];
		int line;
		int status = __get_source_line(inst_pointer, __command, recent_file, __MAX_FUNC, function, __MAX_FUNC, &line);
		if (status == 2)
		{
			__get_address_map(inst_pointer, recent_file, __MAX_FUNC, function, __MAX_FUNC, &line);
			status = 0;
		}

		if (strncmp(function, "__trace", __MAX_FUNC) == 0)
			continue;
		if (strncmp(function, "__handler_signal", __MAX_FUNC) == 0)
			continue;

		if (strncmp(file, recent_file, __MAX_FUNC) != 0)
		{
			if (status == 0)
			{
				strncpy(file, recent_file, __MAX_FUNC);
			}
			fprintf(stderr, "In %s\n", file);
		}

		fprintf(stderr, "  %s: %d\n", function, line);
	}

	_exit(6);
}

void __handler_signal (int sig)
{
	fprintf(stderr, "\nCaught signal: %s\n\n", strsignal(sig));
	__trace();
}

void register_handler (char** argv)
{
	__command = argv[0];
	signal(SIGSEGV, __handler_signal);
	signal(SIGABRT, __handler_signal);
#ifdef __cplusplus
	std::set_terminate(__trace);
#endif // __cplusplus
}

#ifdef __cplusplus
}  // extern "C"
#endif // __cplusplus
#endif // STACKTRACE_H_
