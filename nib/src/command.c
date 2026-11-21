/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "editor.h"

static inline void switch_normal() {
	line_free(&EGG.cmstate.command);
	EGG.mode = EM_NORMAL;
}

static int process_command(void) {
	line_t* command = &EGG.cmstate.command;

	if (command->len == 0) {
		return 0;
	}

	if (strcmp(command->string, "q") == 0) {
		if (EGG.dirty) {
			set_errmsg("unsaved changes, use \":q!\" to override");
			return 2;
		}

		return 1;
	} else if (strcmp(command->string, "w") == 0) {
		if (!save(EGG.path)) {
			set_errmsg("could not save the file");
			return 2;
		}

		return 0;
	} else if (strcmp(command->string, "wq") == 0) {
		if (!save(EGG.path)) {
			set_errmsg("could not save the file");
			return 2;
		}

		return 1;
	} else if (strcmp(command->string, "q!") == 0) {
		return 1;
	}

	if (EGG.nlines > 0) {
		int target = 0;
		bool numeric = true;
		bool too_big = false;

		for (int i = 0; i < command->len; i++) {
			char c = command->string[i];

			if (c < '0' || c > '9') {
				numeric = false;
				break;
			}

			if (!too_big) {
				target = target * 10 + (c - '0');
				if (target > EGG.nlines) {
					too_big = true;
				}
			}
		}

		if (numeric) {
			if (too_big) {
				target = EGG.nlines;
			} else if (target == 0) {
				target = 1;
			}

			goto_line(target - 1);
			return 0;
		}
	}

	line_set(&EGG.errormsg, "command \"", 9);

	for (int i = 0; i < command->len; i++) {
		if (!line_append(&EGG.errormsg, command->string[i])) {
			break;
		}
	}

	const char* tail = "\" is not recognized";
	for (const char* p = tail; *p != '\0'; p++) {
		if (!line_append(&EGG.errormsg, *p)) {
			break;
		}
	}

	EGG.show_errmsg = true;
	return 2;
}

int handin_command(keypacket_t kp) {
	EGG.show_errmsg = false;
	
	if (kp.keycode == KEYCODE_ESCAPE) {
		switch_normal();
		
		return 0;
	} else if (kp.keycode == KEYCODE_ENTER) {
		int prc = process_command();
		switch_normal();
		
		return prc;
	} else if (kp.keycode == KEYCODE_BACKSPACE) {
		if (EGG.cmstate.command.len == 0) {
			switch_normal();
			
			return 0;
		}

		line_backspace(&EGG.cmstate.command);
		
		return 0;
	}

	char c = keypacket_to_ascii(kp);
	if (c != 0) {
		if (!line_append(&EGG.cmstate.command, c)) {
			set_errmsg("out of memory");
			return 2;
		}
	}
	
	return 0;
}
