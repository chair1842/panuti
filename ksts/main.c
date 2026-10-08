/* SPDX-License-Identifier: BSD-3-Clause */

#include <panuti/syscall/syscallsf.h>
#include <panuti/syscall/syscall.h>
#include <panuti/syscall/syscallno.h>
#include <panuti/errno.h>
#include <string.h>
#include <stdint.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

static void write_str(int fd, const char* s) {
	panutisysf_write(fd, s, strlen(s));
}

static void write_int(int fd, int n) {
	char buf[16];
	int i = 15;
	int neg = 0;
	uint32_t val;
	if (n < 0) {
		neg = 1;
		val = (uint32_t)(-(long long)n);
	} else {
		val = (uint32_t)n;
	}
	if (val == 0) {
		buf[--i] = '0';
	} else {
		while (val > 0) {
			buf[--i] = '0' + (char)(val % 10);
			val /= 10;
		}
	}
	if (neg) {
		buf[--i] = '-';
	}
	panutisysf_write(fd, buf + i, (size_t)(16 - i));
}

static void check(int fd, const char* name, int32_t got, int32_t expect) {
	tests_run++;
	write_str(fd, "  ");
	if (got == expect) {
		tests_passed++;
		write_str(fd, "[PASS] ");
	} else {
		tests_failed++;
		write_str(fd, "[FAIL] ");
	}
	write_str(fd, name);
	write_str(fd, " (got=");
	write_int(fd, (int)got);
	write_str(fd, " expect=");
	write_int(fd, (int)expect);
	write_str(fd, ")\n");
}

static void check_is_error(int fd, const char* name, int32_t got) {
	tests_run++;
	write_str(fd, "  ");
	if (got & 0x80000000) {
		tests_passed++;
		write_str(fd, "[PASS] ");
	} else {
		tests_failed++;
		write_str(fd, "[FAIL] ");
	}
	write_str(fd, name);
	write_str(fd, " (got=");
	write_int(fd, (int)got);
	write_str(fd, " expected error)\n");
}

static void check_is_success(int fd, const char* name, int32_t got) {
	tests_run++;
	write_str(fd, "  ");
	if (!(got & 0x80000000)) {
		tests_passed++;
		write_str(fd, "[PASS] ");
	} else {
		tests_failed++;
		write_str(fd, "[FAIL] ");
	}
	write_str(fd, name);
	write_str(fd, " (got=");
	write_int(fd, (int)got);
	write_str(fd, " expected success)\n");
}

static void section(int fd, const char* name) {
	write_str(fd, "\n--- ");
	write_str(fd, name);
	write_str(fd, " ---\n");
}

/* Runs pur with the given argv, its stdout on a pipe, and collects the output
 * and the exit code. Returns 1 if pur could be run at all. */
static int run_pur(char** pur_argv, int pur_argc, char* out, int cap, int* ec) {
	int rfd, wfd;
	int32_t r = panutisysf_pipe_create(&rfd, &wfd);

	*ec = 0;
	out[0] = '\0';

	if (r < 0) {
		return 0;
	}

	int pur_out[] = { wfd };
	procreate_args_t cargs = {
		.path = "/cd/usr/bin/pur",
		.argv = pur_argv,
		.argc = pur_argc,
		.in_streams = (int*)0,
		.no_in_streams = 0,
		.out_streams = pur_out,
		.no_out_streams = 1,
	};

	pid_t child = panutisysf_procreate(&cargs);
	panutisysf_close(wfd);

	if (child == 0 || (int32_t)child < 0) {
		panutisysf_close(rfd);
		return 0;
	}

	panutisysf_wait(child, ec);

	int len = 0;
	for (;;) {
		char chunk[64];
		int32_t n = panutisysf_read(rfd, chunk, sizeof(chunk));

		if (n <= 0) {
			break;
		}

		for (int32_t k = 0; k < n && len < cap - 1; k++) {
			out[len++] = chunk[k];
		}
	}
	out[len] = '\0';

	panutisysf_close(rfd);
	return 1;
}

/* ------------------------------------------------------------------ */

int main(int argc, char** argv) {
	int console = panutisysf_open("/dvc/console");
	if (console < 0) {
		/* nowhere to output; just die */
		panutisysf_exit(1);
	}

	/* Child mode: procreated with "--child" argument */
	if (argc >= 2 && strcmp(argv[1], "--child") == 0) {
		write_str(console, "  [child] argc=");
		write_int(console, argc);
		write_str(console, " argv[1]=");
		write_str(console, argv[1]);
		write_str(console, "\n");
		panutisysf_close(console);
		panutisysf_exit(42);
	}

	/* Child mode: --write-parent — write "hello parent" to stream 0 (parent's pipe write-end), exit 10 */
	if (argc >= 2 && strcmp(argv[1], "--write-parent") == 0) {
		panutisysf_stream_write(0, "hello parent", 12);
		panutisysf_close(console);
		panutisysf_exit(10);
	}

	/* Child mode: --read-parent — read from stream 0 (parent's pipe read-end), echo to console, exit 11 */
	if (argc >= 2 && strcmp(argv[1], "--read-parent") == 0) {
		char buf[64];
		int32_t n = panutisysf_stream_read(0, buf, sizeof(buf) - 1);
		if (n > 0) {
			buf[n] = '\0';
			write_str(console, "[child-read] ");
			write_str(console, buf);
			write_str(console, "\n");
		} else {
			write_str(console, "[child-read] EOF\n");
		}
		panutisysf_close(console);
		panutisysf_exit(11);
	}

	/* Child mode: --stream-write — write to stream 0, exit 20 */
	if (argc >= 2 && strcmp(argv[1], "--stream-write") == 0) {
		panutisysf_stream_write(0, "stream child\n", 13);
		panutisysf_close(console);
		panutisysf_exit(20);
	}

	int nin = 0;   /* input streams, filled by nstream */
	int nout = 0;  /* output streams, filled by nstream */

	write_str(console, "=== PANUTI SYSCALL STRESS TEST ===\n");

	/* ---- 1. Basic open/write/close on console ---- */
	section(console, "1. Basic open/write/close");

	{
		int fd = panutisysf_open("/dvc/console");
		check_is_success(console, "open /dvc/console", fd);
		int32_t r = panutisysf_write(fd, "hello from ksts\n", 16);
		check(console, "write hello (16 bytes)", r, 16);
		panutisysf_close(fd);
		write_str(console, "  (closed fd)\n");
	}

	/* ---- 2. Open non-existent path ---- */
	section(console, "2. Open non-existent path");

	{
		int fd = panutisysf_open("/no/such/path");
		check_is_error(console, "open /no/such/path -> NOTFOUND", fd);
		check(console, "  error code", fd, PANUTIERRNO_NOTFOUND);
	}

	/* ---- 3. Open a directory ---- */
	section(console, "3. Open a directory");

	{
		int32_t r = panutisysf_mkdir("/testdir");
		check_is_success(console, "mkdir /testdir", r);
		int fd = panutisysf_open("/testdir");
		/* directories are openable: they hand back a handle you readdir on */
		check_is_success(console, "open /testdir (a directory)", fd);
		panutisysf_close(fd);
	}

	/* ---- 4. mkdir edge cases ---- */
	section(console, "4. mkdir edge cases");

	{
		int32_t r = panutisysf_mkdir("/testdir");
		/* creating the same dir again should fail (name collision in walk) */
		check(console, "mkdir /testdir again (collision)", r, -1);
	}

	{
		int32_t r = panutisysf_mkdir("/a/b/c");
		/* /a doesn't exist, so this should fail */
		check(console, "mkdir /a/b/c (parent missing)", r, -1);
	}

	/* ---- 5. Write/Read/Activate/Close with bad fds ---- */
	section(console, "5. Bad file descriptors");

	{
		int32_t r = panutisysf_write(99, "x", 1);
		check(console, "write fd=99 -> BADFD", r, PANUTIERRNO_BADFD);
	}
	{
		int32_t r = panutisysf_read(99, &r, 1);
		check(console, "read fd=99 -> BADFD", r, PANUTIERRNO_BADFD);
	}
	{
		int32_t r = panutisysf_activate(99);
		check(console, "activate fd=99 -> BADFD", r, PANUTIERRNO_BADFD);
	}
	{
		int32_t r = panutisysf_close(99);
		check(console, "close fd=99 -> BADFD", r, PANUTIERRNO_BADFD);
	}

	/* ---- 6. Close an already-closed fd ---- */
	section(console, "6. Close already-closed fd");

	{
		int fd = panutisysf_open("/dvc/console");
		check_is_success(console, "open console", fd);
		panutisysf_close(fd);
		int32_t r = panutisysf_close(fd);
		check(console, "close again -> BADFD", r, PANUTIERRNO_BADFD);
	}

	/* ---- 7. Unsupported operations on console ---- */
	section(console, "7. Unsupported ops on console (write-only)");

	{
		int fd = panutisysf_open("/dvc/console");
		char buf[8];
		int32_t r = panutisysf_read(fd, buf, 1);
		check(console, "read from console -> UNSUPPORTEDOP", r, PANUTIERRNO_UNSUPPORTEDOP);
		r = panutisysf_activate(fd);
		check(console, "activate console -> UNSUPPORTEDOP", r, PANUTIERRNO_UNSUPPORTEDOP);
		r = panutisysf_close(fd);
		check(console, "close console (explicit) -> success", r, 0);
	}

	/* ---- 8. Write zero bytes ---- */
	section(console, "8. Edge case: write zero bytes");

	{
		int fd = panutisysf_open("/dvc/console");
		int32_t r = panutisysf_write(fd, "unchanged", 0);
		check(console, "write 0 bytes -> 0", r, 0);
		panutisysf_close(fd);
	}

	/* ---- 9. Open with NULL-ish paths ---- */
	section(console, "9. Open with bad paths");

	{
		int fd = panutisysf_open("");
		/* empty path should fail (walk returns NULL) */
		check(console, "open \"\" -> NOTFOUND", fd, PANUTIERRNO_NOTFOUND);
	}
	{
		int fd = panutisysf_open("/");
		/* root is a directory, and directories are openable */
		check_is_success(console, "open \"/\" (root dir)", fd);
		panutisysf_close(fd);
	}
	{
		int fd = panutisysf_open("/dvc/console/");
		/* trailing slash on a non-directory: walk tries to descend, fails */
		check_is_error(console, "open \"/dvc/console/\" (trailing slash)", fd);
	}

	/* ---- 10. Fill handle table ---- */
	section(console, "10. Fill handle table (fd exhaustion)");

	{
		/* the handle table size is a kernel policy, so fill it the only way
		 * that survives a change to that policy: keep opening until it says
		 * no, and check the refusal is the exhaustion error */
		int fds[128];
		int count = 0;
		int refusal = 0;

		while (count < (int)(sizeof(fds) / sizeof(fds[0]))) {
			int fd = panutisysf_open("/dvc/console");
			if (fd < 0) {
				refusal = fd;
				break;
			}
			fds[count++] = fd;
		}

		write_str(console, "  opened ");
		write_int(console, count);
		write_str(console, " handles\n");

		check_is_error(console, "handle table refuses further opens", refusal);
		check(console, "refusal is NOFDS", refusal, PANUTIERRNO_NOFDS);
		check(console, "table filled to its limit", count > 0 ? 1 : 0, 1);

		/* close them all */
		for (int i = 0; i < count; i++) {
			panutisysf_close(fds[i]);
		}
		write_str(console, "  (cleaned up ");
		write_int(console, count);
		write_str(console, " handles)\n");

		/* and the table is reusable afterwards */
		int again = panutisysf_open("/dvc/console");
		check_is_success(console, "open works again after the table drained", again);
		panutisysf_close(again);
	}

	/* ---- 11. Invalid syscall number ---- */
	section(console, "11. Invalid syscall number");

	{
		int32_t r = panuti_syscall(99, 0, 0, 0, 0);
		check(console, "syscall(99) -> INVALIDSYSCALL", r, PANUTIERRNO_INVALIDSYSCALL);
	}
	{
		int32_t r = panuti_syscall(255, 0, 0, 0, 0);
		check(console, "syscall(255) -> INVALIDSYSCALL", r, PANUTIERRNO_INVALIDSYSCALL);
	}
	{
		/* 40 is unimplemented; use a number past the dispatch table */
		int32_t r = panuti_syscall(40, 0, 0, 0, 0);
		check(console, "syscall(40) (unimplemented) -> INVALIDSYSCALL", r, PANUTIERRNO_INVALIDSYSCALL);
	}

	/* ---- 12. Double-close recovery ---- */
	section(console, "12. Double-close recovery");

	{
		int fd1 = panutisysf_open("/dvc/console");
		int fd2 = panutisysf_open("/dvc/console");
		panutisysf_close(fd1);
		/* fd1 is closed, fd2 should still work */
		int32_t r = panutisysf_write(fd2, "fd2 works\n", 11);
		check(console, "write to fd2 after closing fd1", r, 11);
		panutisysf_close(fd2);
	}

	/* ---- 13. Mkdir nesting and path traversal ---- */
	section(console, "13. Nested mkdir and path resolution");

	{
		int32_t r = panutisysf_mkdir("/level1");
		check_is_success(console, "mkdir /level1", r);
		r = panutisysf_mkdir("/level1/level2");
		check_is_success(console, "mkdir /level1/level2", r);
		/* double slashes should be handled */
		r = panutisysf_mkdir("/level1//level2");
		check(console, "mkdir /level1//level2 (double slash, exists)", r, -1);
		/* relative-style in absolute */
		r = panutisysf_mkdir("/./level1");
		/* this should fail: the "." is root itself, and level1 already exists */
		check(console, "mkdir /./level1 (collision)", r, -1);
	}

	/* ---- 14. Write after close (use-after-free test) ---- */
	section(console, "14. Write to closed fd (use-after-close)");

	{
		int fd = panutisysf_open("/dvc/console");
		panutisysf_close(fd);
		int32_t r = panutisysf_write(fd, "should fail\n", 12);
		check(console, "write to closed fd -> BADFD", r, PANUTIERRNO_BADFD);
	}

	/* ---- 15. Syscall with pointer-like garbage args ---- */
	section(console, "15. Syscall with garbage pointer args");

	{
		/* pass a bogus pointer to open -- kernel now validates user pointers */
		int32_t r = panuti_syscall(SYSHANDLER_OPEN, 0xDEAD0000, 0, 0, 0);
		check(console, "open(badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
	}
	{
		/* pass bogus pointer to write */
		int32_t r = panuti_syscall(SYSHANDLER_WRITE, 0, 0xDEAD0000, 1, 0);
		check(console, "write(badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
	}
	{
		/* pass bogus pointer to read */
		int32_t r = panuti_syscall(SYSHANDLER_READ, 0, 0xDEAD0000, 1, 0);
		check(console, "read(badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
	}
	{
		/* pass bogus pointer to mkdir */
		int32_t r = panuti_syscall(SYSHANDLER_MKDIR, 0xDEAD0000, 0, 0, 0);
		check(console, "mkdir(badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
	}

	/* ---- 16. Rapid open/close cycling ---- */
	section(console, "16. Rapid open/close cycling");

	{
		int good = 0;
		int bad = 0;
		for (int i = 0; i < 100; i++) {
			int fd = panutisysf_open("/dvc/console");
			if (fd >= 0) {
				panutisysf_close(fd);
				good++;
			} else {
				bad++;
			}
		}
		write_str(console, "  100 cycles: ");
		write_int(console, good);
		write_str(console, " ok, ");
		write_int(console, bad);
		write_str(console, " failed\n");
		check(console, "all 100 open/close cycles succeeded", good, 100);
	}

	/* ---- 17. Large write ---- */
	section(console, "17. Large write (256 bytes)");

	{
		char big[256];
		for (int i = 0; i < 256; i++) {
			big[i] = 'A' + (i % 26);
		}
		int fd = panutisysf_open("/dvc/console");
		int32_t r = panutisysf_write(fd, big, 256);
		write_str(console, "\n");
		check(console, "write 256 bytes", r, 256);
		panutisysf_close(fd);
	}

	/* ---- 18. Write with offset pointer (simulated) ---- */
	section(console, "18. Write from different addresses");

	{
		const char* msg1 = "addr_test_1\n";
		const char* msg2 = "addr_test_2\n";
		int fd = panutisysf_open("/dvc/console");
		int32_t r1 = panutisysf_write(fd, msg1, strlen(msg1));
		int32_t r2 = panutisysf_write(fd, msg2, strlen(msg2));
		check(console, "write from addr1", r1, (int32_t)strlen(msg1));
		check(console, "write from addr2", r2, (int32_t)strlen(msg2));
		panutisysf_close(fd);
	}

	/* ---- 19. Mkdir deep nesting ---- */
	section(console, "19. Deep mkdir chain");

	{
		int32_t r;
		r = panutisysf_mkdir("/d");
		check_is_success(console, "mkdir /d", r);
		r = panutisysf_mkdir("/d/d");
		check_is_success(console, "mkdir /d/d", r);
		r = panutisysf_mkdir("/d/d/d");
		check_is_success(console, "mkdir /d/d/d", r);
		r = panutisysf_mkdir("/d/d/d/d");
		check_is_success(console, "mkdir /d/d/d/d", r);
		r = panutisysf_mkdir("/d/d/d/d/d");
		check_is_success(console, "mkdir /d/d/d/d/d", r);
	}

	/* ---- 20. chdir basic ---- */
	section(console, "20. chdir basic");

	{
		int32_t r = panutisysf_chdir("/testdir");
		check_is_success(console, "chdir /testdir", r);
	}
	{
		int32_t r = panutisysf_chdir("/no/such/dir");
		check(console, "chdir /no/such/dir -> NOTFOUND", r, PANUTIERRNO_NOTFOUND);
	}

	/* ---- 21. chdir to a file ---- */
	section(console, "21. chdir to a file");

	{
		int fd = panutisysf_open("/dvc/console");
		/* can't chdir to a file; open gives us an fd but the path is a device inode */
		panutisysf_close(fd);
		int32_t r = panutisysf_chdir("/dvc/console");
		check(console, "chdir /dvc/console -> UNSUPPORTEDOP", r, PANUTIERRNO_UNSUPPORTEDOP);
	}

	/* ---- 22. chdir relative path ---- */
	section(console, "22. chdir relative path");

	{
		/* we should be in /testdir from test 20 */
		int32_t r = panutisysf_mkdir("/testdir/sub");
		check_is_success(console, "mkdir /testdir/sub", r);
		r = panutisysf_chdir("sub");
		check_is_success(console, "chdir \"sub\" (relative)", r);
		/* create something to prove we're in /testdir/sub */
		r = panutisysf_mkdir("proof");
		check_is_success(console, "mkdir proof (inside /testdir/sub)", r);
		/* go back to root */
		r = panutisysf_chdir("/");
		check_is_success(console, "chdir / (back to root)", r);
	}

	/* ---- 23. chdir garbage pointer ---- */
	section(console, "23. chdir garbage pointer");

	{
		int32_t r = panuti_syscall(SYSHANDLER_CHDIR, 0xDEAD0000, 0, 0, 0);
		check(console, "chdir(badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
	}

	/* ---- 24. unlink basic ---- */
	section(console, "24. unlink basic");

	{
		int32_t r = panutisysf_mkdir("/unlink_test");
		check_is_success(console, "mkdir /unlink_test", r);
		r = panutisysf_unlink("/unlink_test");
		check_is_success(console, "unlink /unlink_test", r);
		/* should be gone now */
		int fd = panutisysf_open("/unlink_test");
		check(console, "open /unlink_test after unlink -> NOTFOUND", fd, PANUTIERRNO_NOTFOUND);
	}

	/* ---- 25. unlink non-existent ---- */
	section(console, "25. unlink non-existent");

	{
		int32_t r = panutisysf_unlink("/no/such/thing");
		check(console, "unlink /no/such/thing -> NOTFOUND", r, PANUTIERRNO_NOTFOUND);
	}

	/* ---- 26. unlink root ---- */
	section(console, "26. unlink root");

	{
		int32_t r = panutisysf_unlink("/");
		/* root's "." entry can't be unlinked by name lookup (path parsing
		   returns empty name) so this should be NOTFOUND */
		check(console, "unlink \"/\" -> NOTFOUND", r, PANUTIERRNO_NOTFOUND);
	}

	/* ---- 27. unlink then mkdir (reuse inode slot) ---- */
	section(console, "27. unlink then mkdir (slot reuse)");

	{
		int32_t r = panutisysf_mkdir("/recycle");
		check_is_success(console, "mkdir /recycle", r);
		r = panutisysf_unlink("/recycle");
		check_is_success(console, "unlink /recycle", r);
		r = panutisysf_mkdir("/recycle");
		check_is_success(console, "mkdir /recycle again", r);
		r = panutisysf_unlink("/recycle");
		check_is_success(console, "unlink /recycle again", r);
	}

	/* ---- 28. unlink with trailing slash ---- */
	section(console, "28. unlink with trailing slash");

	{
		int32_t r = panutisysf_mkdir("/trail_test");
		check_is_success(console, "mkdir /trail_test", r);
		r = panutisysf_unlink("/trail_test/");
		check_is_success(console, "unlink \"/trail_test/\" (trailing slash)", r);
		int fd = panutisysf_open("/trail_test");
		check(console, "open /trail_test after unlink -> NOTFOUND", fd, PANUTIERRNO_NOTFOUND);
	}

	/* ---- 29. unlink garbage pointer ---- */
	section(console, "29. unlink garbage pointer");

	{
		int32_t r = panuti_syscall(SYSHANDLER_UNLINK, 0xDEAD0000, 0, 0, 0);
		check(console, "unlink(badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
	}

	/* ---- 30. chdir + unlink interaction ---- */
	section(console, "30. chdir + unlink interaction");

	{
		int32_t r = panutisysf_mkdir("/interact");
		check_is_success(console, "mkdir /interact", r);
		r = panutisysf_mkdir("/interact/child");
		check_is_success(console, "mkdir /interact/child", r);
		r = panutisysf_chdir("/interact/child");
		check_is_success(console, "chdir /interact/child", r);
		/* we are now inside the dir we're about to unlink from parent */
		r = panutisysf_unlink("/interact/child");
		check_is_success(console, "unlink /interact/child (while cwd inside it)", r);
		/* cwd still points to the inode; it's just unlinked from parent */
		/* go back to root so we don't confuse later tests */
		panutisysf_chdir("/");
	}

	/* ---- 31. Open after full close cycle ---- */
	section(console, "31. Sanity: open still works after all tests");

	{
		int fd = panutisysf_open("/dvc/console");
		check_is_success(console, "open /dvc/console (final)", fd);
		int32_t r = panutisysf_write(fd, "kernel is alive!\n", 17);
		check(console, "final write", r, 17);
		panutisysf_close(fd);
	}

	/* ---- 32. getpid ---- */
	section(console, "32. getpid");

	{
		uint32_t p1 = panutisysf_getpid();
		uint32_t p2 = panutisysf_getpid();
		write_str(console, "  pid=");
		write_int(console, (int)p1);
		write_str(console, "\n");
		check(console, "getpid is positive", p1 > 0 ? 1 : 0, 1);
		check(console, "getpid stable across calls", p1 == p2 ? 1 : 0, 1);
	}

	/* ---- 33. getcwd ---- */
	section(console, "33. getcwd");

	{
		char buf[256];
		int32_t r = panutisysf_getcwd(buf, sizeof(buf));
		check_is_success(console, "getcwd at root", r);
		check(console, "getcwd at root -> \"/\"", strcmp(buf, "/") == 0 ? 1 : 0, 1);
	}

	{
		int32_t r = panutisysf_chdir("/level1/level2");
		check_is_success(console, "chdir /level1/level2 (getcwd setup)", r);
		char buf[256];
		r = panutisysf_getcwd(buf, sizeof(buf));
		check_is_success(console, "getcwd after chdir", r);
		check(console, "getcwd -> \"/level1/level2\"", strcmp(buf, "/level1/level2") == 0 ? 1 : 0, 1);
		/* "/level1/level2" is 14 chars, needs 15 bytes with the NUL */
		char exact[15];
		r = panutisysf_getcwd(exact, sizeof(exact));
		check(console, "getcwd exact-fit buffer", r, 0);
		/* one byte short -> INVALIDADDR */
		char tiny[14];
		r = panutisysf_getcwd(tiny, sizeof(tiny));
		check(console, "getcwd too-small buffer -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
		/* zero-length buffer -> INVALIDADDR */
		r = panutisysf_getcwd(buf, 0);
		check(console, "getcwd len 0 -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
		/* back to root */
		r = panutisysf_chdir("/");
		check_is_success(console, "chdir / (back to root)", r);
	}

	{
		int32_t r = panuti_syscall(SYSHANDLER_GETCWD, 0xDEAD0000, 256, 0, 0);
		check(console, "getcwd(badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
	}

	/* ---- 35. yield ---- */
	section(console, "34. yield");

	{
		panutisysf_yield();
		panutisysf_yield();
		/* still alive and kicking after giving up the cpu */
		int fd = panutisysf_open("/dvc/console");
		check_is_success(console, "open works after yield", fd);
		panutisysf_close(fd);
	}

	/* ---- 36. rename ---- */
	section(console, "35. rename");

	{
		int32_t r;
		r = panutisysf_mkdir("/mv_a");
		check_is_success(console, "mkdir /mv_a", r);
		r = panutisysf_mkdir("/mv_a/thing");
		check_is_success(console, "mkdir /mv_a/thing", r);
		r = panutisysf_mkdir("/mv_b");
		check_is_success(console, "mkdir /mv_b", r);
		r = panutisysf_rename("/mv_a/thing", "/mv_b/thing");
		check_is_success(console, "rename /mv_a/thing /mv_b/thing", r);
		/* old name should be gone */
		r = panutisysf_chdir("/mv_a/thing");
		check(console, "chdir old path after rename -> NOTFOUND", r, PANUTIERRNO_NOTFOUND);
		/* new name should be there */
		r = panutisysf_chdir("/mv_b/thing");
		check_is_success(console, "chdir new path after rename", r);
		r = panutisysf_chdir("/");
		check_is_success(console, "chdir / (reset)", r);
	}

	{
		int32_t r;
		/* self rename is a no-op */
		r = panutisysf_rename("/mv_b/thing", "/mv_b/thing");
		check(console, "rename onto itself (no-op)", r, 0);
		/* rename onto a fresh name */
		r = panutisysf_rename("/mv_b/thing", "/mv_b/thing2");
		check_is_success(console, "rename /mv_b/thing /mv_b/thing2", r);
		/* collision: /mv_b/thing no longer exists, so create one to clash with */
		r = panutisysf_mkdir("/mv_b/thing");
		check_is_success(console, "mkdir /mv_b/thing (collision setup)", r);
		r = panutisysf_rename("/mv_b/thing2", "/mv_b/thing");
		check(console, "rename onto existing name -> EXISTS", r, PANUTIERRNO_EXISTS);
	}

	{
		int32_t r = panutisysf_rename("/mv_a/nonexistent", "/mv_b/foo");
		check(console, "rename nonexistent -> NOTFOUND", r, PANUTIERRNO_NOTFOUND);
	}

	{
		int32_t r = panutisysf_rename("/", "/mv_b/root");
		check(console, "rename root -> NOTFOUND", r, PANUTIERRNO_NOTFOUND);
	}

	{
		int32_t r = panuti_syscall(SYSHANDLER_RENAME,
			(uint32_t)0xDEAD0000, (uint32_t)"/mv_b/thing", 0, 0);
		check(console, "rename(badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
	}

	/* ---- 37. link ---- */
	section(console, "36. link");

	{
		/* /dvc/console is a FILE inode, linkable */
		int32_t r = panutisysf_link("/dvc/console", "/ln_console");
		check_is_success(console, "link /dvc/console /ln_console", r);
		/* the new name must work like the original */
		int fd = panutisysf_open("/ln_console");
		check_is_success(console, "open linked /ln_console", fd);
		r = panutisysf_write(fd, "ln works!\n", 10);
		check(console, "write via linked name", r, 10);
		panutisysf_close(fd);
		/* collision: linking over an existing name */
		r = panutisysf_link("/dvc/console", "/ln_console");
		check(console, "link onto existing name -> EXISTS", r, PANUTIERRNO_EXISTS);
		/* block devices are linkable too */
		r = panutisysf_link("/dvc/ram0", "/ln_ram");
		check_is_success(console, "link /dvc/ram0 /ln_ram", r);
		fd = panutisysf_open("/ln_ram");
		check_is_success(console, "open linked /ln_ram", fd);
		panutisysf_close(fd);
		/* directories are not */
		r = panutisysf_link("/mv_b", "/ln_mv_b");
		check(console, "link a directory -> UNSUPPORTEDOP", r, PANUTIERRNO_UNSUPPORTEDOP);
		/* missing target */
		r = panutisysf_link("/no/such/node", "/ln_nope");
		check(console, "link nonexistent target -> NOTFOUND", r, PANUTIERRNO_NOTFOUND);
		/* cleanup our names */
		r = panutisysf_unlink("/ln_console");
		check_is_success(console, "unlink /ln_console", r);
		r = panutisysf_unlink("/ln_ram");
		check_is_success(console, "unlink /ln_ram", r);
	}

	{
		int32_t r = panuti_syscall(SYSHANDLER_LINK,
			(uint32_t)0xDEAD0000, (uint32_t)"/ln_x", 0, 0);
		check(console, "link(badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
	}

	/* ---- 38. block device i/o on /dvc/ram0 ---- */
	section(console, "37. block device i/o on /dvc/ram0");

	{
		/* 700 bytes crosses the 512-byte sector boundary, so the write
		   has to read-modify-write a partial tail */
		char pattern[700];
		for (int i = 0; i < 700; i++) {
			pattern[i] = (char)('0' + (i % 10));
		}
		int b = panutisysf_open("/dvc/ram0");
		check_is_success(console, "open /dvc/ram0 (block)", b);
		int32_t r = panutisysf_write(b, pattern, 700);
		check(console, "write 700 bytes to ram0", r, 700);

		/* an independent handle sees the data from the start */
		int c = panutisysf_open("/dvc/ram0");
		check_is_success(console, "open /dvc/ram0 (2nd handle)", c);
		char buf[700];
		r = panutisysf_read(c, buf, 700);
		check(console, "read 700 bytes back", r, 700);
		check(console, "ram0 round-trip content", memcmp(buf, pattern, 700) == 0 ? 1 : 0, 1);
		panutisysf_close(c);

		/* the write advanced the first handle's offset */
		char tail[8];
		r = panutisysf_read(b, tail, 8);
		check(console, "block read continues after write (offset advance)", r, 8);

		/* zero-length io is a no-op */
		r = panutisysf_read(b, tail, 0);
		check(console, "read 0 bytes on block", r, 0);
		r = panutisysf_write(b, "", 0);
		check(console, "write 0 bytes on block", r, 0);

		/* bad buffers bounce before touching the device */
		r = panuti_syscall(SYSHANDLER_READ, (uint32_t)b, 0xDEAD0000, 16, 0);
		check(console, "read(badptr) on block -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
		r = panuti_syscall(SYSHANDLER_WRITE, (uint32_t)b, 0xDEAD0000, 16, 0);
		check(console, "write(badptr) on block -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);

		/* block handles don't activate */
		r = panutisysf_activate(b);
		check_is_error(console, "activate block handle -> error", r);

		/* close frees the slot */
		r = panutisysf_close(b);
		check(console, "close block handle", r, 0);
		r = panutisysf_read(b, buf, 1);
		check(console, "read closed block fd -> BADFD", r, PANUTIERRNO_BADFD);

		/* trailing slash on a non-directory path doesn't resolve */
		int fd = panutisysf_open("/dvc/ram0/");
		check(console, "open \"/dvc/ram0/\" -> NOTFOUND", fd, PANUTIERRNO_NOTFOUND);
	}

	{
		/* partial-block write: start at a non-sector-aligned offset, which
		   forces the read-modify-write path */
		char head[100];
		char body[100];
		for (int i = 0; i < 100; i++) {
			head[i] = 'H';
			body[i] = 'B';
		}
		int e = panutisysf_open("/dvc/ram0");
		check_is_success(console, "open /dvc/ram0 (partial-write)", e);
		int32_t r = panutisysf_write(e, head, 100);
		check(console, "write 100 bytes (head)", r, 100);
		r = panutisysf_write(e, body, 100);
		check(console, "write 100 bytes (body, offset 100)", r, 100);

		int f = panutisysf_open("/dvc/ram0");
		check_is_success(console, "open /dvc/ram0 (verify)", f);
		char mix[200];
		r = panutisysf_read(f, mix, 200);
		check(console, "read 200 bytes back", r, 200);
		check(console, "first 100 bytes are head", memcmp(mix, head, 100) == 0 ? 1 : 0, 1);
		check(console, "next 100 bytes are body", memcmp(mix + 100, body, 100) == 0 ? 1 : 0, 1);
		panutisysf_close(e);
		panutisysf_close(f);
	}

	/* ---- 39. files: rename + link interplay ---- */
	section(console, "38. files: rename + link interplay");

	{
		/* rename a file (not just a dir), old name must die */
		int32_t r = panutisysf_link("/dvc/console", "/rn_file");
		check_is_success(console, "link /dvc/console /rn_file", r);
		r = panutisysf_rename("/rn_file", "/rn_file2");
		check_is_success(console, "rename linked file /rn_file /rn_file2", r);
		int fd = panutisysf_open("/rn_file");
		check(console, "open old name after rename -> NOTFOUND", fd, PANUTIERRNO_NOTFOUND);
		fd = panutisysf_open("/rn_file2");
		check_is_success(console, "open new name", fd);
		r = panutisysf_write(fd, "renamed!\n", 9);
		check(console, "write via renamed file", r, 9);
		panutisysf_close(fd);

		/* renaming a file onto an existing dir name is a collision */
		r = panutisysf_mkdir("/rn_dir2");
		check_is_success(console, "mkdir /rn_dir2 (collision setup)", r);
		r = panutisysf_rename("/rn_file2", "/rn_dir2");
		check(console, "rename file over dir -> EXISTS", r, PANUTIERRNO_EXISTS);
		r = panutisysf_unlink("/rn_file2");
		check_is_success(console, "unlink renamed file", r);

		/* renaming into a missing parent is NOTFOUND */
		r = panutisysf_rename("/rn_dir2", "/no/such/parent/child");
		check(console, "rename into missing parent -> NOTFOUND", r, PANUTIERRNO_NOTFOUND);
		r = panutisysf_unlink("/rn_dir2");
		check_is_success(console, "unlink /rn_dir2", r);
	}

	{
		/* rename a dir into another dir, trailing slashes tolerated */
		int32_t r = panutisysf_mkdir("/rn_a");
		check_is_success(console, "mkdir /rn_a", r);
		r = panutisysf_mkdir("/rn_b");
		check_is_success(console, "mkdir /rn_b", r);
		r = panutisysf_rename("/rn_a", "/rn_b/new");
		check_is_success(console, "rename /rn_a /rn_b/new", r);
		r = panutisysf_chdir("/rn_a");
		check(console, "chdir old dir after rename -> NOTFOUND", r, PANUTIERRNO_NOTFOUND);
		r = panutisysf_chdir("/rn_b/new");
		check_is_success(console, "chdir to renamed dir", r);
		r = panutisysf_chdir("/");
		check_is_success(console, "chdir / (reset)", r);
		/* trailing slash on the target parses to the same name */
		r = panutisysf_rename("/rn_b/new", "/rn_b/new/");
		check(console, "rename onto itself with trailing slash (no-op)", r, 0);
		/* trailing slash in the source works too */
		r = panutisysf_rename("/rn_b/new/", "/rn_b/moved");
		check_is_success(console, "rename with trailing slash in source", r);
		r = panutisysf_unlink("/rn_b/moved");
		check_is_success(console, "unlink renamed dir", r);
		r = panutisysf_unlink("/rn_b");
		check_is_success(console, "unlink /rn_b", r);
	}

	{
		/* unlink of an open file: the fd keeps the inode alive */
		int32_t r = panutisysf_link("/dvc/console", "/ln_live");
		check_is_success(console, "link /dvc/console /ln_live", r);
		int fd = panutisysf_open("/ln_live");
		check_is_success(console, "open /ln_live", fd);
		r = panutisysf_unlink("/ln_live");
		check_is_success(console, "unlink /ln_live while open", r);
		r = panutisysf_write(fd, "still here\n", 11);
		check(console, "write through open fd after unlink", r, 11);
		panutisysf_close(fd);
		int gone = panutisysf_open("/ln_live");
		check(console, "open unlinked name -> NOTFOUND", gone, PANUTIERRNO_NOTFOUND);
	}

	/* ---- 40. deep paths and .. traversal ---- */
	section(console, "39. deep paths and .. traversal");

	{
		/* deepest chain created back in section 19 */
		int32_t r = panutisysf_chdir("/d/d/d/d/d");
		check_is_success(console, "chdir 5 levels deep", r);
		char buf[256];
		r = panutisysf_getcwd(buf, sizeof(buf));
		check_is_success(console, "getcwd deep", r);
		check(console, "getcwd -> \"/d/d/d/d/d\"", strcmp(buf, "/d/d/d/d/d") == 0 ? 1 : 0, 1);
		/* climb all the way back out */
		for (int i = 0; i < 5; i++) {
			r = panutisysf_chdir("..");
			if (r != 0) {
				break;
			}
		}
		r = panutisysf_getcwd(buf, sizeof(buf));
		check(console, "getcwd back at root after 5x ..", r == 0 && strcmp(buf, "/") == 0 ? 1 : 0, 1);
	}

	{
		/* . and .. as relative path components */
		int32_t r = panutisysf_mkdir("/dot_dot");
		check_is_success(console, "mkdir /dot_dot", r);
		r = panutisysf_mkdir("/dot_dot/sub");
		check_is_success(console, "mkdir /dot_dot/sub", r);
		r = panutisysf_chdir("/dot_dot/sub");
		check_is_success(console, "chdir /dot_dot/sub", r);
		r = panutisysf_chdir(".");
		check_is_success(console, "chdir . (no-op)", r);
		char buf[256];
		r = panutisysf_getcwd(buf, sizeof(buf));
		check(console, "getcwd after chdir .", r == 0 && strcmp(buf, "/dot_dot/sub") == 0 ? 1 : 0, 1);
		r = panutisysf_chdir("..");
		check_is_success(console, "chdir ..", r);
		r = panutisysf_getcwd(buf, sizeof(buf));
		check(console, "getcwd after chdir ..", r == 0 && strcmp(buf, "/dot_dot") == 0 ? 1 : 0, 1);
		r = panutisysf_chdir("../..");
		check_is_success(console, "chdir ../.. (to root)", r);
		r = panutisysf_getcwd(buf, sizeof(buf));
		check(console, "getcwd back at root", r == 0 && strcmp(buf, "/") == 0 ? 1 : 0, 1);
		/* walking past the top stops at the root */
		r = panutisysf_chdir("/../..");
		check_is_success(console, "chdir /../.. stays valid", r);
		r = panutisysf_getcwd(buf, sizeof(buf));
		check(console, "getcwd after /../..", r == 0 && strcmp(buf, "/") == 0 ? 1 : 0, 1);
	}

	{
		/* chdir to the empty string is a miss */
		int32_t r = panutisysf_chdir("");
		check(console, "chdir \"\" -> NOTFOUND", r, PANUTIERRNO_NOTFOUND);
	}

	/* ---- 41. mount / unmount ---- */
	section(console, "40. mount / unmount");

	{
		int32_t r = panutisysf_mkdir("/kststmp");
		check_is_success(console, "mkdir /kststmp", r);
	}

	{
		/* mount the boot ISO (isofs on the atapi cdrom) */
		int32_t r = panutisysf_mount("/kststmp", "isofs", "/dvc/cdrom0");
		check_is_success(console, "mount /kststmp isofs /dvc/cdrom0", r);
	}

	{
		/* mounting over an already-mounted point fails */
		int32_t r = panutisysf_mount("/kststmp", "isofs", "/dvc/cdrom0");
		check_is_error(console, "mount over existing mountpoint -> error", r);
	}

	{
		/* unknown filesystem type */
		int32_t r = panutisysf_mount("/kststmp", "nonsensefs", "/dvc/cdrom0");
		check(console, "mount unknown fstype -> NOTSUPPORTED", r, PANUTIERRNO_NOTSUPPORTED);
	}

	{
		/* nonexistent block device */
		int32_t r = panutisysf_mount("/kststmp2", "isofs", "/dvc/ghost");
		check(console, "mount missing blkdev -> NOTFOUND", r, PANUTIERRNO_NOTFOUND);
	}

	{
		/* mounting onto a non-directory mountpoint */
		int32_t r = panutisysf_mount("/dvc/console", "isofs", "/dvc/cdrom0");
		check_is_error(console, "mount onto non-dir mountpoint -> error", r);
	}

	{
		/* unmount works */
		int32_t r = panutisysf_unmount("/kststmp");
		check(console, "unmount /kststmp (clean) -> success", r, 0);
	}

	{
		/* unmounting something not mounted fails */
		int32_t r = panutisysf_unmount("/kststmp");
		check(console, "unmount /kststmp again (not mounted) -> NOTFOUND", r, PANUTIERRNO_NOTFOUND);
		r = panutisysf_unmount("/no/such/kststmp");
		check(console, "unmount bogus path -> NOTFOUND", r, PANUTIERRNO_NOTFOUND);
	}

	{
		/* mount + unmount cycle works */
		int32_t r = panutisysf_mount("/kststmp", "isofs", "/dvc/cdrom0");
		check_is_success(console, "re-mount /kststmp", r);
		r = panutisysf_unmount("/kststmp");
		check(console, "unmount /kststmp (after re-mount) -> success", r, 0);
	}

	{
		/* garbage pointers are rejected before touching anything */
		int32_t r = panuti_syscall(SYSHANDLER_MOUNT, 0xDEAD0000, (uint32_t)"isofs", (uint32_t)"/dvc/cdrom0", 0);
		check(console, "mount(badptr mountp) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
		r = panuti_syscall(SYSHANDLER_MOUNT, (uint32_t)"/kststmp", 0xDEAD0000, (uint32_t)"/dvc/cdrom0", 0);
		check(console, "mount(badptr fstype) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
		r = panuti_syscall(SYSHANDLER_MOUNT, (uint32_t)"/kststmp", (uint32_t)"isofs", 0xDEAD0000, 0);
		check(console, "mount(badptr blkdev) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
		r = panuti_syscall(SYSHANDLER_UNMOUNT, 0xDEAD0000, 0, 0, 0);
		check(console, "unmount(badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
	}

	/* ---- 42. pipe_create ---- */
	section(console, "41. pipe_create");
	{
		int rfd, wfd;
		int32_t r = panutisysf_pipe_create(&rfd, &wfd);
		check_is_success(console, "pipe_create succeeds", r);
		check(console, "read fd >= 0", rfd >= 0, 1);
		check(console, "write fd >= 0", wfd >= 0, 1);
		check(console, "fds differ", rfd != wfd, 1);

		/* round-trip: write data, then read it back */
		const char msg[] = "hello pipe";
		r = panutisysf_write(wfd, msg, sizeof(msg));
		check(console, "write to write end", r, (int32_t)sizeof(msg));
		char buf[32] = {0};
		r = panutisysf_read(rfd, buf, sizeof(buf));
		check(console, "read from read end", r, (int32_t)sizeof(msg));
		/* byte-level check */
		int match = 1;
		for (size_t i = 0; i < sizeof(msg); i++) {
			if (buf[i] != msg[i]) { match = 0; break; }
		}
		check(console, "data matches", match, 1);

		panutisysf_close(rfd);
		panutisysf_close(wfd);
	}
	/* EOF when write end is closed */
	{
		int rfd, wfd;
		panutisysf_pipe_create(&rfd, &wfd);
		panutisysf_close(wfd);
		char buf[8];
		int32_t r = panutisysf_read(rfd, buf, sizeof(buf));
		check(console, "read after close(write) -> EOF (0)", r, 0);
		panutisysf_close(rfd);
	}
	/* broken pipe when read end is closed */
	{
		int rfd, wfd;
		panutisysf_pipe_create(&rfd, &wfd);
		panutisysf_close(rfd);
		int32_t r = panutisysf_write(wfd, "x", 1);
		check(console, "write after close(read) -> broken (-1)", r, -1);
		panutisysf_close(wfd);
	}
	/* writing to read end or reading from write end */
	{
		int rfd, wfd;
		panutisysf_pipe_create(&rfd, &wfd);
		int32_t r = panutisysf_write(rfd, "x", 1);
		check(console, "write to read end -> -1", r, -1);
		char buf[4];
		r = panutisysf_read(wfd, buf, sizeof(buf));
		check(console, "read from write end -> -1", r, -1);
		panutisysf_close(rfd);
		panutisysf_close(wfd);
	}
	/* multiple independent pipes */
	{
		int r1, w1, r2, w2;
		panutisysf_pipe_create(&r1, &w1);
		panutisysf_pipe_create(&r2, &w2);
		panutisysf_write(w1, "aaa", 3);
		panutisysf_write(w2, "bbb", 3);
		char a[8] = {0}, b[8] = {0};
		panutisysf_read(r1, a, 3);
		panutisysf_read(r2, b, 3);
		check(console, "pipe1 data independent", a[0] == 'a', 1);
		check(console, "pipe2 data independent", b[0] == 'b', 1);
		panutisysf_close(r1);
		panutisysf_close(w1);
		panutisysf_close(r2);
		panutisysf_close(w2);
	}
	/* garbage pointer args */
	{
		int32_t r = panuti_syscall(SYSHANDLER_PIPE_CREATE, 0xDEAD0000, 0xDEAD0004, 0, 0);
		check(console, "pipe_create(badptr,badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
		r = panuti_syscall(SYSHANDLER_PIPE_CREATE, 0xDEAD0000, 0, 0, 0);
		check(console, "pipe_create(badptr,0) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
		r = panuti_syscall(SYSHANDLER_PIPE_CREATE, 0, 0xDEAD0000, 0, 0);
		check(console, "pipe_create(0,badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
	}

	/* ---- 43. nstream ---- */
	section(console, "42. nstream");
	{
		int counts[2] = { -1, -1 };
		panutisysf_nstream(counts);
		check(console, "nstream wrote input count (0..16)", counts[0] >= 0 && counts[0] <= 16, 1);
		check(console, "nstream wrote output count (0..16)", counts[1] >= 0 && counts[1] <= 16, 1);
		/* the console device is bound as out-stream 0 at task creation */
		check(console, "at least one output stream (console bound)", counts[1] >= 1, 1);
		write_str(console, "  in=");
		write_int(console, counts[0]);
		write_str(console, " out=");
		write_int(console, counts[1]);
		write_str(console, "\n");
		nin = counts[0];
		nout = counts[1];
	}
	{
		int32_t r = panuti_syscall(SYSHANDLER_NSTREAM, 0xDEAD0000, 0, 0, 0);
		check(console, "nstream(badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
		r = panuti_syscall(SYSHANDLER_NSTREAM, 0, 0, 0, 0);
		check(console, "nstream(NULL) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
	}

	/* ---- 44. stream_read ---- */
	section(console, "43. stream_read");
	{
		char buf[64];

		/* negative index, valid buffer */
		int32_t r = panutisysf_stream_read(-1, buf, 1);
		check(console, "stream_read(-1, buf) -> BADFD", r, PANUTIERRNO_BADFD);

		/* index past the reported input streams */
		r = panutisysf_stream_read(nin + 5, buf, 1);
		check(console, "stream_read(nin+5, buf) -> BADFD", r, PANUTIERRNO_BADFD);

		/* the buffer is validated before the stream index */
		r = panutisysf_stream_read(0, (void*)0xDEAD0000, 1);
		check(console, "stream_read(0, badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);
		r = panutisysf_stream_read(nin + 5, (void*)0xDEAD0000, 1);
		check(console, "stream_read(bad index, badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);

		/* zero-length read with a valid buffer passes the range check,
		   then fails on the stream index if there are no in-streams */
		r = panutisysf_stream_read(0, buf, 0);
		if (nin == 0) {
			check(console, "stream_read(0, buf, 0) with no in-streams -> BADFD", r, PANUTIERRNO_BADFD);
		} else {
			check_is_success(console, "stream_read(0, buf, 0)", r);
		}
	}

	/* ---- 45. stream_write ---- */
	section(console, "44. stream_write");
	{
		const char msg[] = "stream write ok!\n";

		/* stream 0 is the console bound at task creation */
		int32_t r = panutisysf_stream_write(0, msg, sizeof(msg));
		if (nout == 0) {
			check(console, "stream_write(0, msg) with no out-streams -> BADFD", r, PANUTIERRNO_BADFD);
		} else {
			check(console, "stream_write(0, msg) wrote all bytes", r, (int32_t)sizeof(msg));
		}

		/* negative index, valid buffer */
		r = panutisysf_stream_write(-1, msg, sizeof(msg));
		check(console, "stream_write(-1, msg) -> BADFD", r, PANUTIERRNO_BADFD);

		/* index past the reported output streams */
		r = panutisysf_stream_write(nout + 5, msg, sizeof(msg));
		check(console, "stream_write(nout+5, msg) -> BADFD", r, PANUTIERRNO_BADFD);

		/* the buffer is validated before the stream index */
		r = panutisysf_stream_write(0, (void*)0xDEAD0000, 1);
		check(console, "stream_write(0, badptr) -> INVALIDADDR", r, PANUTIERRNO_INVALIDADDR);

		/* zero-length write with a valid buffer reaches the stream */
		r = panutisysf_stream_write(0, msg, 0);
		if (nout == 0) {
			check(console, "stream_write(0, msg, 0) with no out-streams -> BADFD", r, PANUTIERRNO_BADFD);
		} else {
			check(console, "stream_write(0, msg, 0) wrote 0 bytes", r, 0);
		}
	}

	/* ---- 46. procreate + wait ---- */
	section(console, "45. procreate + wait (mount ISO, spawn child, reap)");

	{
		/* mount the boot ISO so we can read ksts from it */
		int32_t r = panutisysf_mkdir("/kststmp");
		write_str(console, "  mkdir /kststmp -> ");
		write_int(console, r);
		write_str(console, "\n");

		r = panutisysf_mount("/kststmp", "isofs", "/dvc/cdrom0");
		check_is_success(console, "mount /kststmp isofs /dvc/cdrom0", r);

		/* procreate ksts with --child so it exits immediately */
		char* child_argv[] = { "ksts", "--child" };
		procreate_args_t cargs = {
			.path = "/cd/usr/bin/ksts",
			.argv = child_argv,
			.argc = 2,
			.in_streams = (int*)0,
			.no_in_streams = 0,
			.out_streams = (int*)0,
			.no_out_streams = 0,
		};
		pid_t child = panutisysf_procreate(&cargs);
		write_str(console, "  procreate pid=");
		write_int(console, (int)child);
		write_str(console, "\n");
		check(console, "procreate returns positive pid", child > 0 ? 1 : 0, 1);

		/* wait for the child to finish */
		int ec = -1;
		int32_t w = panutisysf_wait(child, &ec);
		write_str(console, "  wait returned=");
		write_int(console, w);
		write_str(console, " exit_code=");
		write_int(console, ec);
		write_str(console, "\n");
		check_is_success(console, "wait returns success", w);
		check(console, "child exit code == 42", ec, 42);

		/* the child's pid should be gone now; waiting again must fail */
		ec = -1;
		w = panutisysf_wait(child, &ec);
		check(console, "wait on reaped pid -> NOTFOUND", w, PANUTIERRNO_NOTFOUND);

		/* unmount */
		r = panutisysf_unmount("/kststmp");
		check(console, "unmount /kststmp after test", r, 0);
	}

	/* ---- 47. procreate / wait edge cases ---- */
	section(console, "46. procreate / wait edge cases");

	{
		/* procreate with a bogus path */
		procreate_args_t bad = {
			.path = "/no/such/bin.elf",
			.argv = (char**)0,
			.argc = 0,
			.in_streams = (int*)0,
			.no_in_streams = 0,
			.out_streams = (int*)0,
			.no_out_streams = 0,
		};
		pid_t p = panutisysf_procreate(&bad);
		write_str(console, "  procreate(badpath) pid=");
		write_int(console, (int)p);
		write_str(console, "\n");
		/* procreate must fail (returns error code, not a pid) */
		check_is_error(console, "procreate bad path -> error", (int32_t)p);
	}

	{
		/* procreate with garbage pointer in path */
		procreate_args_t gp;
		gp.path = (const char*)0xDEAD0000;
		gp.argv = (char**)0;
		gp.argc = 0;
		gp.in_streams = (int*)0;
		gp.no_in_streams = 0;
		gp.out_streams = (int*)0;
		gp.no_out_streams = 0;
		pid_t p = panutisysf_procreate(&gp);
		write_str(console, "  procreate(badptr path) pid=");
		write_int(console, (int)p);
		write_str(console, "\n");
		check_is_error(console, "procreate bad pointer path -> error", (int32_t)p);
	}

	{
		/* wait on a pid that doesn't exist */
		int ec = -1;
		int32_t w = panutisysf_wait(9999, &ec);
		write_str(console, "  wait(9999) -> ");
		write_int(console, w);
		write_str(console, "\n");
		check(console, "wait(nonexistent) -> NOTFOUND", w, PANUTIERRNO_NOTFOUND);
	}

	{
		/* wait on self should fail */
		uint32_t my_pid = panutisysf_getpid();
		int ec = -1;
		int32_t w = panutisysf_wait((pid_t)my_pid, &ec);
		write_str(console, "  wait(self) -> ");
		write_int(console, w);
		write_str(console, "\n");
		check(console, "wait(self) -> PLAINERR", w, PANUTIERRNO_PLAINERR);
	}

	{
		/* wait with garbage pointer for exit code */
		int32_t w = panutisysf_wait(1, (int*)0xDEAD0000);
		write_str(console, "  wait(badptr ec) -> ");
		write_int(console, w);
		write_str(console, "\n");
		check(console, "wait(badptr) -> INVALIDADDR", w, PANUTIERRNO_INVALIDADDR);
	}

	/* ---- 48. pipe stress tests ---- */
	section(console, "47. pipe stress tests");

	/* large data: fill pipe close to buffer capacity (4096) */
	{
		int rfd, wfd;
		panutisysf_pipe_create(&rfd, &wfd);

		static char big[2048];
		for (int i = 0; i < 2048; i++) {
			big[i] = (char)('a' + (i % 26));
		}
		int32_t r = panutisysf_write(wfd, big, 2048);
		check(console, "write 2048 bytes to pipe", r, 2048);

		static char buf[2048];
		r = panutisysf_read(rfd, buf, 2048);
		check(console, "read 2048 bytes from pipe", r, 2048);

		int match = 1;
		for (int i = 0; i < 2048; i++) {
			if (buf[i] != big[i]) { match = 0; break; }
		}
		check(console, "2048-byte content matches", match, 1);

		panutisysf_close(rfd);
		panutisysf_close(wfd);
	}

	/* multiple small writes, single read */
	{
		int rfd, wfd;
		panutisysf_pipe_create(&rfd, &wfd);

		panutisysf_write(wfd, "AAA", 3);
		panutisysf_write(wfd, "BBB", 3);
		panutisysf_write(wfd, "CCC", 3);

		char buf[16] = {0};
		int32_t r = panutisysf_read(rfd, buf, sizeof(buf));
		check(console, "read after 3 writes (got 9 bytes)", r, 9);

		int match = (buf[0] == 'A' && buf[3] == 'B' && buf[6] == 'C') ? 1 : 0;
		check(console, "concatenated data order preserved", match, 1);

		panutisysf_close(rfd);
		panutisysf_close(wfd);
	}

	/* single write, multiple partial reads */
	{
		int rfd, wfd;
		panutisysf_pipe_create(&rfd, &wfd);

		panutisysf_write(wfd, "0123456789", 10);

		char part1[4] = {0};
		char part2[4] = {0};
		char part3[4] = {0};
		int32_t r1 = panutisysf_read(rfd, part1, 4);
		int32_t r2 = panutisysf_read(rfd, part2, 4);
		int32_t r3 = panutisysf_read(rfd, part3, 4);
		check(console, "partial read 1 (4 bytes)", r1, 4);
		check(console, "partial read 2 (4 bytes)", r2, 4);
		check(console, "partial read 3 (2 bytes left)", r3, 2);
		check(console, "part1 == '0123'", memcmp(part1, "0123", 4) == 0 ? 1 : 0, 1);
		check(console, "part2 == '4567'", memcmp(part2, "4567", 4) == 0 ? 1 : 0, 1);
		check(console, "part3 == '89'", memcmp(part3, "89", 2) == 0 ? 1 : 0, 1);

		panutisysf_close(rfd);
		panutisysf_close(wfd);
	}

	/* read more than available */
	{
		int rfd, wfd;
		panutisysf_pipe_create(&rfd, &wfd);

		panutisysf_write(wfd, "short", 5);

		char buf[64] = {0};
		int32_t r = panutisysf_read(rfd, buf, 64);
		check(console, "read 64 but only 5 available -> got 5", r, 5);
		check(console, "data from partial fill", memcmp(buf, "short", 5) == 0 ? 1 : 0, 1);

		panutisysf_close(rfd);
		panutisysf_close(wfd);
	}

	/* zero-byte write edge case */
	{
		int rfd, wfd;
		panutisysf_pipe_create(&rfd, &wfd);

		int32_t r = panutisysf_write(wfd, "", 0);
		check(console, "zero-byte write on pipe -> 0", r, 0);

		panutisysf_close(rfd);
		panutisysf_close(wfd);
	}

	/* double close both ends */
	{
		int rfd, wfd;
		panutisysf_pipe_create(&rfd, &wfd);
		panutisysf_close(rfd);
		panutisysf_close(wfd);
		int32_t r1 = panutisysf_close(rfd);
		int32_t r2 = panutisysf_close(wfd);
		check(console, "double close read end -> BADFD", r1, PANUTIERRNO_BADFD);
		check(console, "double close write end -> BADFD", r2, PANUTIERRNO_BADFD);
	}

	/* close write end while data still in buffer: read drains remainder, then EOF */
	{
		int rfd, wfd;
		panutisysf_pipe_create(&rfd, &wfd);

		panutisysf_write(wfd, "leftover", 8);
		panutisysf_close(wfd);

		char buf[16] = {0};
		int32_t r = panutisysf_read(rfd, buf, sizeof(buf));
		check(console, "read leftover after close(write)", r, 8);
		check(console, "leftover data intact", memcmp(buf, "leftover", 8) == 0 ? 1 : 0, 1);

		/* now EOF */
		r = panutisysf_read(rfd, buf, sizeof(buf));
		check(console, "EOF after draining leftover", r, 0);

		panutisysf_close(rfd);
	}

	/* ---- 49. stream stress tests ---- */
	section(console, "48. stream stress tests");

	/* stream 0 round-trip via console */
	{
		const char msg[] = "stream0-test!\n";
		int32_t r = panutisysf_stream_write(0, msg, sizeof(msg));
		if (nout == 0) {
			check(console, "stream_write(0) with no out-streams -> BADFD", r, PANUTIERRNO_BADFD);
		} else {
			check(console, "stream_write(0) via console", r, (int32_t)sizeof(msg));
		}
	}

	/* stream index range validation: every index from nout..15 is out of range */
	{
		const char msg[] = "x";
		int pass_count = 0;
		int bad_expected = 0;
		for (int i = nout; i < 16; i++) {
			int32_t r = panutisysf_stream_write(i, msg, 1);
			if ((int32_t)r == (int32_t)PANUTIERRNO_BADFD) pass_count++;
			bad_expected++;
		}
		check(console, "stream_write(indices nout..15) all BADFD", pass_count, bad_expected);
	}

	/* read index range validation: every index from nin..15 is out of range */
	{
		char buf[4];
		int pass_count = 0;
		int bad_expected = 0;
		for (int i = nin; i < 16; i++) {
			int32_t r = panutisysf_stream_read(i, buf, 1);
			if ((int32_t)r == (int32_t)PANUTIERRNO_BADFD) pass_count++;
			bad_expected++;
		}
		check(console, "stream_read(indices nin..15) all BADFD", pass_count, bad_expected);
	}

	/* negative stream indices */
	{
		char buf[4];
		int32_t r = panutisysf_stream_read(-1, buf, 1);
		check(console, "stream_read(-1) -> BADFD", r, PANUTIERRNO_BADFD);
		r = panutisysf_stream_write(-1, "x", 1);
		check(console, "stream_write(-1) -> BADFD", r, PANUTIERRNO_BADFD);
		r = panutisysf_stream_read(-100, buf, 1);
		check(console, "stream_read(-100) -> BADFD", r, PANUTIERRNO_BADFD);
	}

	/* zero-length stream ops on valid index */
	{
		char buf[4];
		int32_t r;
		if (nin > 0) {
			r = panutisysf_stream_read(0, buf, 0);
			check_is_success(console, "stream_read(0, buf, 0) zero-length", r);
		}
		if (nout > 0) {
			r = panutisysf_stream_write(0, "x", 0);
			check(console, "stream_write(0, x, 0) zero-length -> 0", r, 0);
		}
	}

	/* nstream stability: counts should not change after ops */
	{
		int counts[2] = { -1, -1 };
		panutisysf_nstream(counts);
		check(console, "nstream count stable (in)", counts[0], nin);
		check(console, "nstream count stable (out)", counts[1], nout);
	}

	/* ---- 50. procreate: child writes to parent via pipe ---- */
	section(console, "49. procreate + pipe: child writes to parent");
	{
		int rfd, wfd;
		int32_t r = panutisysf_pipe_create(&rfd, &wfd);
		check_is_success(console, "pipe for child->parent", r);

		/* mount ISO to get ksts */
		r = panutisysf_mkdir("/kststmp");
		r = panutisysf_mount("/kststmp", "isofs", "/dvc/cdrom0");
		check_is_success(console, "mount /kststmp", r);

		/* pass wfd as child's out-stream 0 (which maps to parent's wfd)
		   and rfd stays open in parent for reading */
		char* child_argv[] = { "ksts", "--write-parent" };
		int child_out_streams[] = { wfd };
		procreate_args_t cargs = {
			.path = "/cd/usr/bin/ksts",
			.argv = child_argv,
			.argc = 2,
			.in_streams = (int*)0,
			.no_in_streams = 0,
			.out_streams = child_out_streams,
			.no_out_streams = 1,
		};
		pid_t child = panutisysf_procreate(&cargs);
		write_str(console, "  child pid=");
		write_int(console, (int)child);
		write_str(console, "\n");
		check(console, "procreate child (write-parent) ok", child > 0 ? 1 : 0, 1);

		/* close write end in parent so child's fd is the only write end */
		panutisysf_close(wfd);

		/* wait for child to finish */
		int ec = -1;
		int32_t w = panutisysf_wait(child, &ec);
		check_is_success(console, "wait for write-parent child", w);
		check(console, "child exit code == 10", ec, 10);

		/* read what the child wrote */
		char buf[32] = {0};
		r = panutisysf_read(rfd, buf, sizeof(buf));
		check(console, "parent reads from pipe", r, 12);
		check(console, "child wrote 'hello parent'", memcmp(buf, "hello parent", 12) == 0 ? 1 : 0, 1);

		panutisysf_close(rfd);
		r = panutisysf_unmount("/kststmp");
		check(console, "unmount /kststmp", r, 0);
	}

	/* ---- 51. procreate: parent writes to child via pipe ---- */
	section(console, "50. procreate + pipe: parent writes to child");
	{
		int rfd, wfd;
		int32_t r = panutisysf_pipe_create(&rfd, &wfd);
		check_is_success(console, "pipe for parent->child", r);

		r = panutisysf_mkdir("/kststmp");
		r = panutisysf_mount("/kststmp", "isofs", "/dvc/cdrom0");
		check_is_success(console, "mount /kststmp", r);

		/* pass rfd as child's in-stream 0 */
		char* child_argv[] = { "ksts", "--read-parent" };
		int child_in_streams[] = { rfd };
		procreate_args_t cargs = {
			.path = "/cd/usr/bin/ksts",
			.argv = child_argv,
			.argc = 2,
			.in_streams = child_in_streams,
			.no_in_streams = 1,
			.out_streams = (int*)0,
			.no_out_streams = 0,
		};
		pid_t child = panutisysf_procreate(&cargs);
		write_str(console, "  child pid=");
		write_int(console, (int)child);
		write_str(console, "\n");
		check(console, "procreate child (read-parent) ok", child > 0 ? 1 : 0, 1);

		/* close read end in parent so child's fd is the only read end */
		panutisysf_close(rfd);

		/* write data to the pipe for the child to read */
		const char msg[] = "hello child!";
		r = panutisysf_write(wfd, msg, sizeof(msg));
		check(console, "parent writes to pipe", r, (int32_t)sizeof(msg));

		/* close write end so child sees EOF after reading */
		panutisysf_close(wfd);

		/* wait for child */
		int ec = -1;
		int32_t w = panutisysf_wait(child, &ec);
		check_is_success(console, "wait for read-parent child", w);
		check(console, "child exit code == 11", ec, 11);

		r = panutisysf_unmount("/kststmp");
		check(console, "unmount /kststmp", r, 0);
	}

	/* ---- 52. procreate: inherited stream isolation ---- */
	section(console, "51. procreate: inherited stream isolation");
	{
		/* child with no streams gets its own default console */
		int32_t r2 = panutisysf_mkdir("/kststmp");
		r2 = panutisysf_mount("/kststmp", "isofs", "/dvc/cdrom0");
		check_is_success(console, "mount /kststmp", r2);

		char* child_argv[] = { "ksts", "--stream-write" };
		procreate_args_t cargs = {
			.path = "/cd/usr/bin/ksts",
			.argv = child_argv,
			.argc = 2,
			.in_streams = (int*)0,
			.no_in_streams = 0,
			.out_streams = (int*)0,
			.no_out_streams = 0,
		};
		pid_t child = panutisysf_procreate(&cargs);
		write_str(console, "  child pid=");
		write_int(console, (int)child);
		write_str(console, "\n");
		check(console, "procreate child (stream-write, no pipes) ok", child > 0 ? 1 : 0, 1);

		int ec = -1;
		int32_t w = panutisysf_wait(child, &ec);
		check_is_success(console, "wait for stream-write child", w);
		check(console, "child exit code == 20", ec, 20);

		r2 = panutisysf_unmount("/kststmp");
		check(console, "unmount /kststmp", r2, 0);
	}

	/* child with streams: verify parent's fds not affected after child exits */
	{
		int rfd, wfd;
		panutisysf_pipe_create(&rfd, &wfd);

		int32_t r = panutisysf_mkdir("/kststmp");
		r = panutisysf_mount("/kststmp", "isofs", "/dvc/cdrom0");
		check_is_success(console, "mount /kststmp (isolation test)", r);

		/* pass wfd to child as out-stream 0 */
		char* child_argv[] = { "ksts", "--write-parent" };
		int child_out[] = { wfd };
		procreate_args_t cargs = {
			.path = "/cd/usr/bin/ksts",
			.argv = child_argv,
			.argc = 2,
			.in_streams = (int*)0,
			.no_in_streams = 0,
			.out_streams = child_out,
			.no_out_streams = 1,
		};
		pid_t child = panutisysf_procreate(&cargs);
		check(console, "procreate for isolation test", child > 0 ? 1 : 0, 1);
		panutisysf_close(wfd);

		/* parent's console write should still work while child runs */
		const char msg[] = "parent alive\n";
		r = panutisysf_write(console, msg, sizeof(msg));
		check(console, "parent console write during child", r, (int32_t)sizeof(msg));

		int ec = -1;
		int32_t w = panutisysf_wait(child, &ec);
		check_is_success(console, "wait isolation child", w);

		/* after child exits, parent's console still works */
		r = panutisysf_write(console, "parent after child\n", 19);
		check(console, "parent console write after child exit", r, 19);

		/* read what child sent */
		char buf[32] = {0};
		r = panutisysf_read(rfd, buf, sizeof(buf));
		check(console, "parent reads child data after child exit", r, 12);
		check(console, "data intact after child exit", memcmp(buf, "hello parent", 12) == 0 ? 1 : 0, 1);

		panutisysf_close(rfd);
		r = panutisysf_unmount("/kststmp");
		check(console, "unmount /kststmp (isolation cleanup)", r, 0);
	}

	/* ---- 53. pur 0.2 statement wiring ---- */
	section(console, "52. pur 0.2 statement wiring (pur -n)");

	/* Runs `pur -n <stmt>` with its stdout on a pipe, collects the plan and
	 * the exit code. Returns 1 if pur could not be started. */
	{
		int plan_ec = 0;
		int ran = 0;
		static char plan[512];
		plan[0] = '\0';
		int plan_len = 0;

		/* smoke: the first case also proves pur -n is runnable at all */
		{
			int rfd, wfd;
			int32_t r = panutisysf_pipe_create(&rfd, &wfd);

			if (r < 0) {
				check_is_error(console, "pipe for pur -n", r);
			} else {
				char* pur_argv[] = { "pur", "-n", "ls > cat" };
				int pur_out[] = { wfd };
				procreate_args_t cargs = {
					.path = "/cd/usr/bin/pur",
					.argv = pur_argv,
					.argc = 3,
					.in_streams = (int*)0,
					.no_in_streams = 0,
					.out_streams = pur_out,
					.no_out_streams = 1,
				};

				pid_t child = panutisysf_procreate(&cargs);
				panutisysf_close(wfd);

				if (child == 0 || (int32_t)child < 0) {
					check(console, "procreate pur -n", 0, 1);
				} else {
					panutisysf_wait(child, &plan_ec);
					ran = 1;

					for (;;) {
						char chunk[64];
						int32_t n = panutisysf_read(rfd, chunk, sizeof(chunk));
						if (n <= 0) {
							break;
						}
						for (int32_t k = 0; k < n && plan_len < (int)sizeof(plan) - 1; k++) {
							plan[plan_len++] = chunk[k];
						}
					}
					plan[plan_len] = '\0';
				}

				panutisysf_close(rfd);
			}
		}

		check(console, "procreate pur -n ran", ran, 1);
		check(console, "pur -n 'ls > cat' exit code", plan_ec, 0);
		check(console, "pur -n 'ls > cat' plan",
		      strcmp(plan, "ls in=0 out=1\ncat in=1 out=0\n") == 0 ? 1 : 0, 1);

		if (strcmp(plan, "ls in=0 out=1\ncat in=1 out=0\n") != 0) {
			write_str(console, "    got: ");
			write_str(console, plan);
		}
	}

	/* malformed statements must be rejected: nonzero exit and no plan */
	{
		static const char* bad[] = {
			"()",
			"(a ; ())",
			"> ls",
			"ls >",
			"ls > cat >",
			"ls > > cat",
			"ls > (cat ; ; more)",
			"ls > ( ; a)",
			"(ls",
			"ls)",
			"\"unterminated",
		};

		for (int i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); i++) {
			int rfd, wfd;
			int32_t r = panutisysf_pipe_create(&rfd, &wfd);

			if (r < 0) {
				check_is_error(console, "pipe for pur reject", r);
				continue;
			}

			char* pur_argv[] = { "pur", "-n", (char*)bad[i] };
			int pur_out[] = { wfd };
			procreate_args_t cargs = {
				.path = "/cd/usr/bin/pur",
				.argv = pur_argv,
				.argc = 3,
				.in_streams = (int*)0,
				.no_in_streams = 0,
				.out_streams = pur_out,
				.no_out_streams = 1,
			};

			pid_t child = panutisysf_procreate(&cargs);
			panutisysf_close(wfd);

			int ec = 0;
			int ok = 0;

			if (child == 0 || (int32_t)child < 0) {
				check(console, "procreate pur reject case", 0, 1);
			} else {
				panutisysf_wait(child, &ec);

				char drain[128];
				while (panutisysf_read(rfd, drain, sizeof(drain)) > 0) {
					/* discard the error message */
				}

				/* ec 1 and only the error line, never a plan */
				ok = (ec == 1);
			}

			panutisysf_close(rfd);
			check(console, "pur -n rejects malformed statement", ok, 1);
		}
	}

	/* accepted statements and their exact stream counts */
	{
		/* statement, expected plan */
		static const char* cases[][2] = {
			/* '>' folds left, so this is (ls > cat) > more */
			{ "ls > cat > more",
			  "ls in=0 out=1\ncat in=1 out=1\nmore in=1 out=0\n" },

			/* a one member group is transparent */
			{ "ls > (cat > more)",
			  "ls in=0 out=2\ncat in=1 out=1\nmore in=2 out=0\n" },
			{ "ls > ((cat) > more)",
			  "ls in=0 out=2\ncat in=1 out=1\nmore in=2 out=0\n" },

			/* a group on the sink side is not a boundary: ls reaches cat
			 * and more, but not through the group that follows cat */
			{ "ls > (cat > (a ; b))",
			  "ls in=0 out=1\ncat in=1 out=2\na in=1 out=0\nb in=1 out=0\n" },

			/* a group on the source side is a boundary: ls reaches only
			 * c, while a and b still feed c */
			{ "ls > ((a ; b) > c)",
			  "ls in=0 out=1\na in=0 out=1\nb in=0 out=1\nc in=3 out=0\n" },

			/* an external stream takes both bare leaf sides */
			{ "a > b > (c > d)",
			  "a in=0 out=1\nb in=1 out=2\nc in=1 out=1\nd in=2 out=0\n" },

			/* five pipes */
			{ "echo hello > mul > (app1 > (cat ; more) ; cat)",
			  "echo in=0 out=1\nmul in=1 out=2\napp1 in=1 out=2\n"
			  "cat in=1 out=0\nmore in=1 out=0\ncat in=1 out=0\n" },

			/* a group fans the upstream command out to every member */
			{ "ls > (cat ; mul)",
			  "ls in=0 out=2\ncat in=1 out=0\nmul in=1 out=0\n" },

			/* a group on the left feeds only the group's own outputs */
			{ "(a ; b) > c",
			  "a in=0 out=1\nb in=0 out=1\nc in=2 out=0\n" },

			/* a bare command is unwired on both sides */
			{ "echo hello",
			  "echo in=0 out=0\n" },
		};

		for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
			int rfd, wfd;
			int32_t r = panutisysf_pipe_create(&rfd, &wfd);

			if (r < 0) {
				check_is_error(console, "pipe for pur case", r);
				continue;
			}

			char* pur_argv[] = { "pur", "-n", (char*)cases[i][0] };
			int pur_out[] = { wfd };
			procreate_args_t cargs = {
				.path = "/cd/usr/bin/pur",
				.argv = pur_argv,
				.argc = 3,
				.in_streams = (int*)0,
				.no_in_streams = 0,
				.out_streams = pur_out,
				.no_out_streams = 1,
			};

			pid_t child = panutisysf_procreate(&cargs);
			panutisysf_close(wfd);

			static char plan[512];
			plan[0] = '\0';
			int plan_len = 0;
			int ec = 0;
			int ran = 0;

			if (child == 0 || (int32_t)child < 0) {
				check(console, "procreate pur case", 0, 1);
			} else {
				panutisysf_wait(child, &ec);
				ran = 1;

				for (;;) {
					char chunk[64];
					int32_t n = panutisysf_read(rfd, chunk, sizeof(chunk));
					if (n <= 0) {
						break;
					}
					for (int32_t k = 0; k < n && plan_len < (int)sizeof(plan) - 1; k++) {
						plan[plan_len++] = chunk[k];
					}
				}
				plan[plan_len] = '\0';
			}

			panutisysf_close(rfd);

			if (ran) {
				int ok = (ec == 0) && strcmp(plan, cases[i][1]) == 0;
				check(console, cases[i][0], ok ? 1 : 0, 1);

				if (!ok) {
					write_str(console, "    got: ");
					write_str(console, plan);
					write_str(console, "    exit=");
					write_int(console, ec);
					write_str(console, "\n");
				}
			}
		}
	}


	/* ---- 54. pur 0.2 statement execution (pur -c) ---- */
	section(console, "53. pur 0.2 statement execution (pur -c)");

	/* -c must run the statement and hand the pipeline's status back to the
	 * caller, which is the whole point of it over -n and the repl */
	{
		static char out[512];
		int ec = 0;

		/* a working pipeline: status 0. pipeline data goes to the wired
		 * streams (the console by default), not to pur's own stdout, so the
		 * only thing assertable here is that pur itself stays quiet */
		{
			char* argv[] = { "pur", "-c", "ls /cd/usr/bin > cat" };

			if (run_pur(argv, 3, out, sizeof(out), &ec)) {
				check(console, "pur -c successful pipeline exit", ec, 0);
				check(console, "pur -c success is quiet on stdout", out[0] == '\0' ? 1 : 0, 1);
			} else {
				check(console, "procreate pur -c success", 0, 1);
			}
		}

		/* a syntax error is pur's own failure, so it is 1 and not a child code */
		{
			char* argv[] = { "pur", "-c", "ls >" };

			if (run_pur(argv, 3, out, sizeof(out), &ec)) {
				check(console, "pur -c malformed exit", ec, 1);
			} else {
				check(console, "procreate pur -c malformed", 0, 1);
			}
		}

		/* a command that cannot be started is pur's own failure too */
		{
			char* argv[] = { "pur", "-c", "nosuchcmd" };

			if (run_pur(argv, 3, out, sizeof(out), &ec)) {
				check(console, "pur -c command not found exit", ec, 1);
			} else {
				check(console, "procreate pur -c missing", 0, 1);
			}
		}

		/* a child that runs and fails has to reach the exit status: the repl
		 * swallows this, and -c exists to be the one that does not. only
		 * assert nonzero, the exact code is a child convention. */
		{
			char* argv[] = { "pur", "-c", "in /cd/nosuch > cat" };

			if (run_pur(argv, 3, out, sizeof(out), &ec)) {
				/* the repl swallows this, and -c exists to be the one that
				 * does not. a child that returns -1 is 0xffffffff, which
				 * pur masks down to 255, so this also proves the code came
				 * from the child rather than pur's own failure of 1 */
				check(console, "pur -c propagates child failure", ec, 255);
			} else {
				check(console, "procreate pur -c child failure", 0, 1);
			}
		}

		/* exec level errors are positive codes, so they must be caught by a
		 * compare against PUR_EXEC_OK. 15 pipes is over the limit, and this
		 * used to exit 0 silently for both -n and -c */
		{
			char* over[] = { "pur", "-c",
					 "a > b > c > d > e > f > g > h > i > j > k > l > m > n > o > p" };

			if (run_pur(over, 3, out, sizeof(out), &ec)) {
				check(console, "pur -c too many pipes exit", ec, 1);
			} else {
				check(console, "procreate pur -c too many pipes", 0, 1);
			}

			char* over_n[] = { "pur", "-n",
					   "a > b > c > d > e > f > g > h > i > j > k > l > m > n > o > p" };

			if (run_pur(over_n, 3, out, sizeof(out), &ec)) {
				check(console, "pur -n too many pipes exit", ec, 1);
			} else {
				check(console, "procreate pur -n too many pipes", 0, 1);
			}
		}
	}


	/* ---- Summary ---- */
	write_str(console, "\n==============================\n");
	write_str(console, "RESULTS: ");
	write_int(console, tests_passed);
	write_str(console, " passed, ");
	write_int(console, tests_failed);
	write_str(console, " failed, ");
	write_int(console, tests_run);
	write_str(console, " total\n");

	if (tests_failed == 0) {
		write_str(console, "ALL TESTS PASSED\n");
	} else {
		write_str(console, "SOME TESTS FAILED\n");
	}
	write_str(console, "==============================\n");

	panutisysf_close(console);
	return tests_failed == 0 ? 0 : 1;
}
