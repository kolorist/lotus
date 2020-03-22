namespace lotus {

template <typename t_allocator>
void unpack_capture(floral::fixed_array<unpacked_event, t_allocator>& o_unpackedEvents, const sidx i_captureIdx)
{
	detail::unpacked_event_buffer_t& eb = detail::s_unpacked_event_buffers[i_captureIdx];
	floral::lock_guard captureGuard(eb.mtx);

	sidx rslot = eb.ridx;
	sidx wslot = eb.widx;

	while (rslot != wslot) {
		if (!eb.data[rslot].ready) break;
		o_unpackedEvents.push_back(eb.data[rslot]);
		rslot = (rslot + 1) % EVENTS_CAP;
	}

	eb.ridx = rslot;
}

template <typename t_allocator>
void unpack_capture(floral::fast_fixed_array<unpacked_event, t_allocator>& o_unpackedEvents, const sidx i_captureIdx)
{
	detail::unpacked_event_buffer_t& eb = detail::s_unpacked_event_buffers[i_captureIdx];
	floral::lock_guard captureGuard(eb.mtx);

	sidx rslot = eb.ridx;
	sidx wslot = eb.widx;

	while (rslot != wslot) {
		if (!eb.data[rslot].ready) break;
		o_unpackedEvents.push_back(eb.data[rslot]);
		rslot = (rslot + 1) % EVENTS_CAP;
	}

	eb.ridx = rslot;
}

template <typename t_allocator, u32 t_capacity>
void unpack_capture(floral::ring_buffer_st<unpacked_event, t_allocator, t_capacity>& o_unpackedEvents, const sidx i_captureIdx)
{
	detail::unpacked_event_buffer_t& eb = detail::s_unpacked_event_buffers[i_captureIdx];
	floral::lock_guard captureGuard(eb.mtx);

	sidx rslot = eb.ridx;
	sidx wslot = eb.widx;

	while (rslot != wslot) {
		if (!eb.data[rslot].ready) break;
		o_unpackedEvents.push_back(eb.data[rslot]);
		rslot = (rslot + 1) % EVENTS_CAP;
	}

	eb.ridx = rslot;
}

template <typename t_allocator, u32 t_capacity>
void unpack_capture(floral::fast_ring_buffer_st<unpacked_event, t_allocator, t_capacity>& o_unpackedEvents, const sidx i_captureIdx)
{
	detail::unpacked_event_buffer_t& eb = detail::s_unpacked_event_buffers[i_captureIdx];
	floral::lock_guard captureGuard(eb.mtx);

	sidx rslot = eb.ridx;
	sidx wslot = eb.widx;

	while (rslot != wslot) {
		if (!eb.data[rslot].ready) break;
		o_unpackedEvents.push_back(eb.data[rslot]);
		rslot = (rslot + 1) % EVENTS_CAP;
	}

	eb.ridx = rslot;
}

}
