#pragma once

#include <floral.h>

#include "events.h"

namespace lotus {

	void										init_capture_for_this_thread(const u32 i_threadId, const_cstr i_captureName);
	void										begin_capture(const u64 i_captureIdx);
	void										end_capture(const u64 i_captureIdx);

	const event*								begin_event(const_cstr i_name);
	void										end_event(const event* i_event);

}
