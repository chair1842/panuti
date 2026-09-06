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

/* ------------------------------------------------------------------ */

int main(void) {
	int console = panutisysf_open("/dvc/console");
	if (console < 0) {
		/* nowhere to output; just die */
		panutisysf_exit(1);
	}

	write_str(console, "=== PANUTI SYSCALL STRESS TEST ===\n");

	/* ---- 1. Basic open/write/close on console ---- */
	section(console, "1. Basic open/write/close");

	{
		int fd = panutisysf_open("/dvc/console");
		check_is_success(console, "open /dvc/console", fd);
		int32_t r = panutisysf_write(fd, "hello from pint\n", 16);
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
		check(console, "open /testdir -> UNSUPPORTEDOP", fd, PANUTIERRNO_UNSUPPORTEDOP);
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
		/* root is a directory, should get UNSUPPORTEDOP */
		check(console, "open \"/\" (root dir) -> UNSUPPORTEDOP", fd, PANUTIERRNO_UNSUPPORTEDOP);
	}
	{
		int fd = panutisysf_open("/dvc/console/");
		/* trailing slash on a non-directory: walk tries to descend, fails */
		check_is_error(console, "open \"/dvc/console/\" (trailing slash)", fd);
	}

	/* ---- 10. Fill handle table ---- */
	section(console, "10. Fill handle table (fd exhaustion)");

	{
		int fds[32];
		int count = 0;
		for (int i = 0; i < 32; i++) {
			fds[i] = panutisysf_open("/dvc/console");
			if (fds[i] < 0) {
				break;
			}
			count++;
		}
		write_str(console, "  opened ");
		write_int(console, count);
		write_str(console, " handles\n");
		/* the next open should fail with NOFDS */
		{
			int extra = panutisysf_open("/dvc/console");
			check(console, "open after exhaustion -> NOFDS", extra, PANUTIERRNO_NOFDS);
		}
		/* close them all */
		for (int i = 0; i < count; i++) {
			panutisysf_close(fds[i]);
		}
		write_str(console, "  (cleaned up ");
		write_int(console, count);
		write_str(console, " handles)\n");
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
		/* 8 is CHDIR (implemented); use a number past the dispatch table */
		int32_t r = panuti_syscall(10, 0, 0, 0, 0);
		check(console, "syscall(10) (unimplemented) -> INVALIDSYSCALL", r, PANUTIERRNO_INVALIDSYSCALL);
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
