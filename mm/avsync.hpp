/** favia: AV synchronization
2025, Simon Zolin */

#include <ffsys/perf.h>

struct avsync {
	uint64_t rt_last[2] // real time of the last frame
		, ts_cur[2] // AV time of the last frame
		, ts_next[2] // AV time of the next frame
		, a_sig_next // real time of the next audio update signal
		;
	uint a_active;

	uint master, a_buf_usec;

	static uint64_t now() {
		return xxtime(fftime_monotonic()).to_usec();
	}

	void reset() {
		ffmem_zero(this, FF_OFF(struct avsync, master));
		a_sig_next = ~0ULL;
	}

	// usec
	uint64_t pos() const {
		if (master == 1)
			return ffmax((int64_t)(ts_next[1] - a_buf_usec), 0);
		return ts_cur[0];
	}

	void audio(uint buf_len_msec) {
		a_buf_usec = buf_len_msec * 1000;
		master = 1;
	}

	void a_start() {
		if (!a_active)
			dbglog("audio started");
		uint64_t rt_now = now();
		rt_last[0] = rt_last[1] = rt_now;
		a_sig_next = rt_now + a_buf_usec/4; // update audio 4 times per buffer
		a_active = 1;
	}

	// ts, dur: usec
	void frame(uint64_t ts, uint dur, uint flags) {
		int i = !!(flags & 1);

		if (ts < ts_next[i]) {
			dbglog("fix non-monotonic PTS: %U -> %U", ts, ts_next[i]);
			ts = ts_next[i];
		}

		ts_cur[i] = ts;
		ts_next[i] = ts + dur;

		rt_last[i] = now();
	}

	/**
	timeout_usec: time to wait when there are no events
	Return bitmask of the streams that need action */
	int read(int *timeout_usec) {
		if (master == 1 && !a_active)
			return 2; // keep filling audio buffer until it's full

		uint64_t rt_now = now();

		uint64_t rtj = rt_now - rt_last[master] + pos();
		dbglog("rtj:%D[%D,%D]  vts:%U-%U  ats:%U-%U"
			, rtj, rtj - ts_cur[0], rtj - ts_cur[1]
			, ts_cur[0], ts_next[0]
			, ts_cur[1], ts_next[1]);

		uint suspend[2] = { ~0U, ~0U };
		int r = 0;

		if (rtj >= ts_next[0]) {
			r |= 1;
		} else {
			suspend[0] = ts_next[0] - rtj;
		}

		if (rt_now >= a_sig_next) {
			r |= 2;
		} else {
			suspend[1] = a_sig_next - rt_now;
		}

		if (!r) {
			*timeout_usec = ffmin(suspend[0], suspend[1]);
		}

		return r;
	}
};
