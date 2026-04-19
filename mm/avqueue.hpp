/** favia: AV frame queue
2025, Simon Zolin */

#pragma once

struct qframe : ffmpeg_frame {
	uint64_t ts;
	uint dur;
};

struct avqueue {
	uint r, w, cap, mask;
	qframe data[0];

	void free() {
		for (uint i = 0;  i < this->cap;  i++) {
			this->data[i].destroy();
		}
		ffmem_alignfree(this);
	}

	uint length() const { return w - r; }

	void reset() {
		qframe *f;
		while ((f = read())) {
			f->unref();
		}
		this->w = this->r = 0;
	}

	qframe* push() {
		uint used = this->w - this->r;
		if (used == this->cap)
			return NULL;
		uint i = this->w++ & this->mask;
		return this->data + i;
	}

	void pop() {
		this->w--;
		assert(this->w >= this->r);
	}

	qframe* read() {
		uint used = this->w - this->r;
		if (used == 0)
			return NULL;
		uint i = this->r++ & this->mask;
		return this->data + i;
	}

	qframe* peek() {
		uint used = this->w - this->r;
		if (used == 0)
			return NULL;
		uint i = this->r & this->mask;
		return this->data + i;
	}
};

static struct avqueue* queue_alloc(uint n)
{
	uint nn = sizeof(struct avqueue) + n * sizeof(struct qframe);
	struct avqueue *q = (struct avqueue*)ffmem_align(nn, 64);
	ffmem_zero(q, nn);
	q->r = q->w = 0;
	q->cap = n;
	q->mask = n - 1;
	for (uint i = 0;  i < n;  i++) {
		q->data[i].alloc();
	}
	return q;
}

static struct avqueue* queue_realloc(struct avqueue *q, uint n)
{
	uint len = q->w - q->r, n_right, i;
	if (n < len)
		return NULL;

	uint nn = sizeof(struct avqueue) + n * sizeof(struct qframe);
	struct avqueue *nq = (struct avqueue*)ffmem_align(nn, 64);
	ffmem_zero(nq, nn);
	nq->r = nq->w = 0;
	nq->cap = n;
	nq->mask = n - 1;
	if (!nq)
		return NULL;

	i = q->r & q->mask;
	n_right = ffmin(len, q->cap - i);
	ffmem_copy(nq->data, q->data + i, n_right * sizeof(qframe));
	ffmem_copy(nq->data + n_right, q->data, (len - n_right) * sizeof(qframe));
	nq->w = len;

	for (uint i = len;  i < n;  i++) {
		nq->data[i].alloc();
	}

	ffmem_alignfree(q);
	return nq;
}
