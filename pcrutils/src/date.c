/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <pcrutils/pcrutils.h>

static const char HELP[] =
	"date - a pcrutils utility\n\n"
	"date prints the current date and time in UTC, read from /dvc/time/clock\n\n"
	"usage:\n"
	"  date\n"
	"  date -u\n"
	"  date +FORMAT\n\n"
	"args:\n"
	"  -h - prints this help message\n"
	"  -u - print UTC (the clock has no time zone, so this is the default)\n"
	"  +FORMAT - print the time in the given format\n\n"
	"FORMAT follows the usual date specifiers:\n"
	"  %Y %y %C - year, short year, century\n"
	"  %m %d %e - month, day, space-padded day\n"
	"  %H %M %S - hour, minute, second (24 hour)\n"
	"  %a %A - short and long weekday names\n"
	"  %b %B - short and long month names\n"
	"  %j - day of the year\n"
	"  %F %T %R %D - YYYY-MM-DD, HH:MM:SS, HH:MM, MM/DD/YY\n"
	"  %s - seconds since the Unix epoch\n"
	"  %Z %z - time zone name and offset (UTC, +0000)\n"
	"  %% %n %t - a percent sign, a newline, a tab\n\n"
	"examples:\n"
	"  date\n"
	"  date +%Y-%m-%d\n";

static const char* const WEEKDAYS_SHORT[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};

static const char* const WEEKDAYS_LONG[] = {
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
};

static const char* const MONTHS_SHORT[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static const char* const MONTHS_LONG[] = {
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December",
};

typedef struct {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int weekday;
	int yday;
} date_tm_t;

static int is_leap_year(int year) {
	return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int days_in_month(int month, int year) {
	static const int days[] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
	};

	if (month == 2 && is_leap_year(year)) {
		return 29;
	}

	return days[month - 1];
}

static int64_t floor_div(int64_t a, int64_t b) {
	int64_t q = a / b;

	if ((a % b) != 0 && ((a < 0) != (b < 0))) {
		q--;
	}

	return q;
}

// civil date from days since 1970-01-01, the same conversion the kernel's
// rtc uses, but without walking every year from 1970
static void civil_from_days(int64_t days, int* year, int* month, int* day) {
	int64_t z = days + 719468;
	int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	unsigned doe = (unsigned)(z - era * 146097);
	unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	int y = (int)(yoe + era * 400);
	unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	unsigned mp = (5 * doy + 2) / 153;
	unsigned d = doy - (153 * mp + 2) / 5 + 1;
	unsigned m = mp < 10 ? mp + 3 : mp - 9;

	*year = y + (m <= 2);
	*month = (int)m;
	*day = (int)d;
}

static void split_centis(int64_t centis, date_tm_t* t) {
	int64_t seconds = floor_div(centis, 100);
	int64_t days = floor_div(seconds, 86400);
	int64_t rem = seconds - days * 86400;

	civil_from_days(days, &t->year, &t->month, &t->day);

	t->hour = (int)(rem / 3600);
	rem %= 3600;
	t->minute = (int)(rem / 60);
	t->second = (int)(rem % 60);

	t->weekday = (int)((days + 4) % 7);
	if (t->weekday < 0) {
		t->weekday += 7;
	}

	t->yday = t->day;
	for (int m = 1; m < t->month; m++) {
		t->yday += days_in_month(m, t->year);
	}
}

static void print_default(const date_tm_t* t) {
	printf("%s %s %2d %02d:%02d:%02d UTC %d\n",
		WEEKDAYS_SHORT[t->weekday],
		MONTHS_SHORT[t->month - 1],
		t->day,
		t->hour,
		t->minute,
		t->second,
		t->year);
}

static void print_format(const char* fmt, const date_tm_t* t, int64_t centis) {
	for (const char* p = fmt; *p; p++) {
		if (*p != '%') {
			putchar(*p);
			continue;
		}

		p++;
		if (*p == '\0') {
			putchar('%');
			break;
		}

		switch (*p) {
			case 'Y': printf("%04d", t->year); break;
			case 'y': printf("%02d", t->year % 100); break;
			case 'C': printf("%02d", t->year / 100); break;
			case 'm': printf("%02d", t->month); break;
			case 'd': printf("%02d", t->day); break;
			case 'e': printf("%2d", t->day); break;
			case 'H': printf("%02d", t->hour); break;
			case 'M': printf("%02d", t->minute); break;
			case 'S': printf("%02d", t->second); break;
			case 'a': printf("%s", WEEKDAYS_SHORT[t->weekday]); break;
			case 'A': printf("%s", WEEKDAYS_LONG[t->weekday]); break;
			case 'b':
			case 'h': printf("%s", MONTHS_SHORT[t->month - 1]); break;
			case 'B': printf("%s", MONTHS_LONG[t->month - 1]); break;
			case 'j': printf("%03d", t->yday); break;
			case 'F':
				printf("%04d-%02d-%02d", t->year, t->month, t->day);
				break;
			case 'T':
				printf("%02d:%02d:%02d", t->hour, t->minute, t->second);
				break;
			case 'R': printf("%02d:%02d", t->hour, t->minute); break;
			case 'D': printf("%02d/%02d/%02d", t->month, t->day, t->year % 100); break;
			case 's': printf("%lld", (long long)floor_div(centis, 100)); break;
			case 'Z': printf("UTC"); break;
			case 'z': printf("+0000"); break;
			case 'c':
				printf("%s %s %2d %02d:%02d:%02d %d",
					WEEKDAYS_SHORT[t->weekday],
					MONTHS_SHORT[t->month - 1],
					t->day,
					t->hour,
					t->minute,
					t->second,
					t->year);
				break;
			case 'n': putchar('\n'); break;
			case 't': putchar('\t'); break;
			case '%': putchar('%'); break;
			default:
				putchar('%');
				putchar(*p);
				break;
		}
	}

	putchar('\n');
}

int main(int argc, char** argv) {
	if (pcr_help_wanted(argc, argv, HELP)) {
		return 0;
	}

	const char* format = NULL;

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];

		if (strcmp(a, "-u") == 0) {
			continue;
		}

		if (a[0] == '+' && format == NULL) {
			format = a + 1;
			continue;
		}

		if (a[0] == '-' && a[1] != '\0') {
			printf("date: invalid option '%s'\n", a);
			return -1;
		}

		printf("date: unexpected argument '%s'\n", a);
		return -1;
	}

	int64_t centis;
	if (pcr_clock_centis(&centis) != 0) {
		printf("date: cannot read /dvc/time/clock\n");
		return -1;
	}

	date_tm_t t;
	split_centis(centis, &t);

	if (format) {
		print_format(format, &t, centis);
	} else {
		print_default(&t);
	}

	return 0;
}
