#include <kernel/kbd/core.h>
#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>

#define KBD_QUEUE_SIZE 64
#define KBD_MAX_WAITERS 8

typedef struct {
	keypacket_t buffer[KBD_QUEUE_SIZE];
	size_t head;
	size_t tail;
	size_t count;
	task_t* waiting[KBD_MAX_WAITERS];
	int n_waiting;
} kbd_queue_t;

static kbd_queue_t line_queue;
static kbd_queue_t raw_queue;

static void queue_push(kbd_queue_t* q, keypacket_t pkt) {
	if (q->count == KBD_QUEUE_SIZE) {
		// drop oldest packet
		q->head = (q->head + 1) % KBD_QUEUE_SIZE;
		q->count--;
	}

	q->buffer[q->tail] = pkt;
	q->tail = (q->tail + 1) % KBD_QUEUE_SIZE;
	q->count++;

	for (int i = 0; i < q->n_waiting; i++) {
		task_wake(q->waiting[i]);
	}
	q->n_waiting = 0;
}

static int queue_read(kbd_queue_t* q, keypacket_t* out) {
	while (q->count == 0) {
		if (q->n_waiting < KBD_MAX_WAITERS) {
			q->waiting[q->n_waiting++] = sched_current();
		}
		task_block(sched_current());
	}

	*out = q->buffer[q->head];
	q->head = (q->head + 1) % KBD_QUEUE_SIZE;
	q->count--;

	return 0;
}

void kbd_core_init(void) {
	line_queue = (kbd_queue_t){0};
	raw_queue = (kbd_queue_t){0};
}

void kbd_core_push(keypacket_t packet) {
	queue_push(&line_queue, packet);
	queue_push(&raw_queue, packet);
}

int kbd_core_read_line_queue(keypacket_t* out) {
	return queue_read(&line_queue, out);
}

int kbd_core_read_raw_queue(keypacket_t* out) {
	return queue_read(&raw_queue, out);
}