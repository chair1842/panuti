/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdint.h>
#include <kernel/rtc.h>
#include "../../io.h"

// this code here is basically from the CMOS article of the osdev wiki

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

#define RTC_SECR 0x00
#define RTC_MINR 0x02
#define RTC_HOURR 0x04
#define RTC_DAYR 0x07
#define RTC_MONR 0x08
#define RTC_YEARR 0x09

#define RTC_REGB 0x0B

#define RTC_24H_S 0x02
#define RTC_BCD_S 0x04

#define RTC_PMH 0x80

typedef struct rtc_datetime {
	uint16_t year;
	uint8_t second;
	uint8_t minute;
	uint8_t hour;
	uint8_t day;
	uint8_t month;
} rtc_datetime_t;

static int getf_update_in_prog(void) {
	outb(CMOS_ADDRESS, 0x0A);
	return inb(CMOS_DATA) & 0x80;
}

static int get_cmos_reg(int reg) {
	outb(CMOS_ADDRESS, reg);
	return inb(CMOS_DATA);
}

static int is_leap_year(int year) {
	if (year % 400 == 0) {
		return 1;
	}
	
	if (year % 100 == 0) {
		return 0;
	}
	
	if (year % 4 == 0) {
		return 1;
	}
	return 0;
}

static int days_in_month(int month, int year) {
	switch (month) {
		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
			return 31;
		case 4:
		case 6:
		case 9:
		case 11:
			return 30;
		case 2:
			return is_leap_year(year) ? 29 : 28;
		default:
			return 30;
	}
}

static rtc_datetime_t read_rtc(void) {
	rtc_datetime_t dt = {0};

	uint8_t lsec = 0;
	uint8_t lmin = 0;
	uint8_t lhr = 0;
	uint8_t ld = 0;
	uint8_t lmo = 0;
	uint8_t lyr = 0;

	uint8_t regb = 0;

	while (getf_update_in_prog());

	dt.second = get_cmos_reg(RTC_SECR);
	dt.minute = get_cmos_reg(RTC_MINR);
	dt.hour = get_cmos_reg(RTC_HOURR);
	dt.day = get_cmos_reg(RTC_DAYR);
	dt.month = get_cmos_reg(RTC_MONR);
	dt.year = get_cmos_reg(RTC_YEARR);

	do {
		lsec = dt.second;
		lmin = dt.minute;
		lhr = dt.hour;
		ld = dt.day;
		lmo = dt.month;
		lyr = dt.year;

		while (getf_update_in_prog());
		
		dt.second = get_cmos_reg(RTC_SECR);
		dt.minute = get_cmos_reg(RTC_MINR);
		dt.hour = get_cmos_reg(RTC_HOURR);
		dt.day = get_cmos_reg(RTC_DAYR);
		dt.month = get_cmos_reg(RTC_MONR);
		dt.year = get_cmos_reg(RTC_YEARR);

		
	} while (
		(lsec != dt.second) ||
		(lmin != dt.minute) ||
		(lhr != dt.hour) ||
		(ld != dt.day) ||
		(lmo != dt.month) ||
		(lyr != dt.year)
	);

	regb = get_cmos_reg(RTC_REGB);

	if (!(regb & RTC_BCD_S)) {
		dt.second = (dt.second & 0x0F) + ((dt.second / 16) * 10);
		dt.minute = (dt.minute & 0x0F) + ((dt.minute / 16) * 10);
		dt.hour = ((dt.hour & 0x0F) + (((dt.hour & 0x70) / 16) * 10)) | (dt.hour & 0x80);
		dt.day = (dt.day & 0x0F) + ((dt.day / 16) * 10);
		dt.month = (dt.month & 0x0F) + ((dt.month / 16) * 10);
		dt.year = (dt.year & 0x0F) + ((dt.year / 16) * 10);
	}

	if (!(regb & RTC_24H_S) && (dt.hour & RTC_PMH)) {
		dt.hour = ((dt.hour & 0x7F) + 12) % 24;
	} else if (!(regb & RTC_24H_S)) {
		if (dt.hour == 12) {
			dt.hour = 0;
		}
	}

	if (dt.year < 70) {
		dt.year += 2000;
	} else {
		dt.year += 1900;
	}

	return dt;
}

int64_t rtc_cur_timestamp(void) {
	rtc_datetime_t dt = read_rtc();
	int64_t days = 0;
	
	for (int y = 1970; y < dt.year; y++) {
		if (is_leap_year(y)) {
			days += 366;
		} else {
			days += 365;
		}
	}
	
	for (int m = 1; m < dt.month; m++) {
		days += days_in_month(m, dt.year);
	}
	
	days += (dt.day - 1);
	
	int64_t seconds = days * 86400LL +
		(int64_t)dt.hour * 3600LL +
		(int64_t)dt.minute * 60LL +
		(int64_t)dt.second;
	
	return seconds * 100LL;
}
