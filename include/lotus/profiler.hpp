namespace lotus {

template <typename allocator_t>
void unpack_capture(floral::fixed_array<unpacked_event, allocator_t>& o_unpackedEvents, const sidx i_captureIdx)
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
