#include <dmac.hpp>
#include <Bus.hpp>
#include <EE.hpp>
#include <sif.hpp>
#include <gs/gif.hpp>
#include <cassert>

inline uint32_t get_channel(uint32_t value)
{
	switch (value)
	{
	case 0x80: return 0;
	case 0x90: return 1;
	case 0xa0: return 2;
	case 0xb0: return 3;
	case 0xb4: return 4;
	case 0xc0: return 5;
	case 0xc4: return 6;
	case 0xc8: return 7;
	case 0xd0: return 8;
	case 0xd4: return 9;
	default:
		printf("[DMAC] Invalid channel id provided 0x%X, aborting...\n", value);
        exit(1);
	}
}

constexpr const char* REGS[] =
{
	"Dn_CHCR",
	"Dn_MADR",
	"Dn_QWC",
	"Dn_TADR",
	"Dn_ASR0",
	"Dn_ASR1",
	"", "",
	"Dn_SADR"
};

constexpr const char* GLOBALS[] =
{
	"D_CTRL",
	"D_STAT",
	"D_PCR",
	"D_SQWC",
	"D_RBSR",
	"D_RBOR",
	"D_STADT",
};

	DMAController::DMAController(Bus* parent, EmotionEngine* cpu) :
		bus(parent), cpu(cpu)
	{}

	uint32_t DMAController::read_channel(uint32_t addr)
	{
		assert((addr & 0xff) <= 0x80);

		uint32_t id = (addr >> 8) & 0xff;
		uint32_t channel = get_channel(id);
		uint32_t offset = (addr >> 4) & 0xf;
		auto ptr = (uint32_t*)&channels[channel] + offset;

		printf("[DMAC] Reading 0x%X from %s of channel %d\n", *ptr, REGS[offset], channel);
		return *ptr;
	}

	void DMAController::write_channel(uint32_t addr, uint32_t data)
	{
		assert((addr & 0xff) <= 0x80);

		uint32_t id = (addr >> 8) & 0xff;
		uint32_t channel = get_channel(id);
		uint32_t offset = (addr >> 4) & 0xf;
		auto ptr = (uint32_t*)&channels[channel] + offset;

		printf("[DMAC] Writing 0x%X to %s of channel %d\n", data, REGS[offset], channel);
		/* The lower bits of MADR must be zero: */
		/* NOTE: This is actually required since the BIOS writes
		   unaligned addresses to the GIF channel for some reason
		   and expects it to be read correctly... */
        if (offset == 1)
            data &= 0x01fffff0;

        *ptr = data;

		if (channels[channel].control.running)
		{
			printf("\n[DMAC] Transfer for channel %d started (%d)!\n\n", channel, channels[channel].qword_count);
		}
	}

	uint32_t DMAController::read_global(uint32_t addr)
	{
		assert(addr <= 0x1000E060);

		uint32_t offset = (addr >> 4) & 0xf;
		auto ptr = (uint32_t*)&globals + offset;

		printf("[DMAC] Reading 0x%X from %s\n", *ptr, GLOBALS[offset]);
		return *ptr;
	}

	uint32_t DMAController::read_enabler(uint32_t addr)
	{
		assert(addr == 0x1000F520);

		printf("[DMAC] Reading D_ENABLER = 0x%X\n", globals.d_enable);
		return globals.d_enable;
	}

	void DMAController::write_global(uint32_t addr, uint32_t data)
	{
		assert(addr <= 0x1000E060);

		uint32_t offset = (addr >> 4) & 0xf;
		auto ptr = (uint32_t*)&globals + offset;

		printf("[DMAC] Writing 0x%X to %s\n", data, GLOBALS[offset]);

		if (offset == 1) /* D_STAT */
		{
			auto& cop0 = cpu->cop0;

			/* The lower bits are cleared while the upper ones are reversed */
			globals.d_stat.clear &= ~(data & 0xffff);
			globals.d_stat.reverse ^= (data >> 16);

			/* IMPORTANT: Update COP0 INT1 status when D_STAT is written */
			cop0.cause.ip1_pending = globals.d_stat.channel_irq & globals.d_stat.channel_irq_mask;
		}
		else
		{
			*ptr = data;
		}
	}

	void DMAController::write_enabler(uint32_t addr, uint32_t data)
	{
		assert(addr == 0x1000F590);

		printf("[DMAC] Writing D_ENABLEW = 0x%X\n", data);
		globals.d_enable = data;
	}

	void DMAController::tick(uint32_t cycles)
	{
		if (globals.d_enable & 0x10000)
			return;

		for (int cycle = cycles; cycle > 0; cycle--)
		{
			/* Check each channel */
			for (uint32_t id = 0; id < 10; id++)
			{
				auto& channel = channels[id];
				if (channel.control.running)
				{
					/* Transfer any pending qwords */
					if (channel.qword_count > 0)
					{
						/* This is channel specific */
						switch (id)
						{
						case DMAChannels::VIF0:
						case DMAChannels::VIF1:
						{
							auto& vif = bus->vif[id];
							uint128_t qword = *(uint128_t*)&bus->eeRam[channel.address];
							if (vif->write_fifo(0, qword))
							{
								uint64_t upper = qword >> 64, lower = qword;
								printf("[DMAC] Writing 0x%lX%016lX to VIF%d\n", upper, lower, id);

								channel.address += 16;
								channel.qword_count--;

								if (!channel.qword_count && !channel.control.mode)
									channel.end_transfer = true;
							}
							break;
						}
						case DMAChannels::GIFC:
						{
							auto& gif = bus->gif;
							uint128_t qword = *(uint128_t*)&bus->eeRam[channel.address];

							/* Send data to the PATH3 port of the GIF */
							if (gif->write_path3(0, qword))
							{
								uint64_t upper = qword >> 64, lower = qword;
								printf("[DMAC][GIF] Writing 0x%lX%016lX to PATH3\n", upper, lower);

								channel.address += 16;
								channel.qword_count--;

								if (!channel.qword_count && !channel.control.mode)
									channel.end_transfer = true;
							}

							break;
						}
						case DMAChannels::SIF0:
						{
							/* SIF0 receives data from the SIF0 fifo */
							auto& sif = bus->sif;
							if (sif->sif0_fifo.size() >= 4)
							{
								uint32_t data[4];
								for (int i = 0; i < 4; i++)
								{
									data[i] = sif->sif0_fifo.front();
									sif->sif0_fifo.pop();
								}

								uint128_t qword = *(uint128_t*)data;
								uint64_t upper = qword >> 64, lower = qword;
								printf("[DMAC][SIF0] Receiving packet from SIF0 FIFO: 0x%lX%016lX\n", upper, lower);

								/* Write the packet to the specified address */
								bus->Write128(channel.address, *(Register*)&qword);

								/* MADR/TADR update while a transfer is ongoing */
								channel.qword_count--;
								channel.address += 16;
							}

							break;
						}
						case DMAChannels::SIF1:
						{
							/* SIF1 pushes data to the SIF1 fifo */
							auto& sif = bus->sif;
							
							uint128_t qword = *(uint128_t*)&bus->eeRam[channel.address];
							uint32_t* data = (uint32_t*)&qword;
							for (int i = 0; i < 4; i++)
							{
								sif->sif1_fifo.push(data[i]);
							}

							uint64_t upper = qword >> 64, lower = qword;
							printf("[DMAC][SIF1] Transfering to SIF1 FIFO: 0x%lX%016lX\n", upper, lower);

							/* MADR/TADR update while a transfer is ongoing */
							channel.qword_count--;
							channel.address += 16;
							break;
						}
						default:
                            printf("[DMAC] Unknown channel transfer with id %d\n", id);
						}
					} /* If the transfer ended, disable channel */
					else if (channel.end_transfer)
					{
						printf("[DMAC] End transfer of channel %d\n", id);

						/* End the transfer */
						channel.end_transfer = false;
						channel.control.running = 0;

						/* Set the channel bit in the interrupt field of D_STAT */
						globals.d_stat.channel_irq |= (1 << id);

						/* Check for interrupts */
						if (globals.d_stat.channel_irq & globals.d_stat.channel_irq_mask)
						{
							printf("\n[DMAC] INT1!\n\n");
							cpu->cop0.cause.ip1_pending = 1;
						}
					}
					else /* Read the next DMAtag */
					{
						fetch_tag(id);
					}
				}
			}
		}
	}
	
	void DMAController::fetch_tag(uint32_t id)
	{
		DMATag tag;
		auto& channel = channels[id];
		switch (id)
		{
		case DMAChannels::GIFC:
		{
			assert(!channel.tag_address.mem_select);

			auto& gif = bus->gif;
			auto address = channel.tag_address.address;

			tag.value = *(uint128_t*)&bus->eeRam[address];
			printf("[DMAC] Read GIF DMA tag 0x%lX\n", (uint64_t)tag.value);

			/* Update channel from tag */
			channel.qword_count = tag.qwords;
			channel.control.tag = (tag.value >> 16) & 0xffff;

			uint16_t tag_id = tag.id;
			switch (tag_id)
			{
			case DMASourceID::REFE:
				channel.address = tag.address;
				channel.tag_address.value += 16;
				channel.end_transfer = true;
				break;
			case DMASourceID::CNT:
				channel.address = channel.tag_address.address + 16;
				channel.tag_address.value = channel.address + channel.qword_count * 16;
				break;
            case DMASourceID::END:
                channel.address = channel.tag_address.address + 16;
                channel.end_transfer = true;
                break;
			default:
                printf("\n[DMAC] Unrecognized GIF DMAtag id %d\n", tag_id);
			}

			/* Just end transfer, since an interrupt will be raised there anyways  */
			if (channel.control.enable_irq_bit && tag.irq)
				channel.end_transfer = true;

			break;
		}
		case DMAChannels::VIF0:
		case DMAChannels::VIF1:
		{
			assert(!channel.tag_address.mem_select);

			auto& vif = bus->vif[id];
			auto address = channel.tag_address.address;

			tag.value = *(uint128_t*)&bus->eeRam[address];
			printf("[DMAC] Read VIF%d DMA tag 0x%lX\n", id, (uint64_t)tag.value);

			/* Transfer the tag before any data */
			if (channel.control.transfer_tag)
			{
                if (!vif->write_fifo<uint64_t>(0, tag.data))
					return;
			}

			/* Update channel from tag */
			channel.qword_count = tag.qwords;
			channel.control.tag = (tag.value >> 16) & 0xffff;

			uint16_t tag_id = tag.id;
			switch (tag_id)
			{
			case DMASourceID::REFE:
				channel.address = tag.address;
				channel.tag_address.value += 16;
				channel.end_transfer = true;
				break;
			case DMASourceID::CNT:
				channel.address = channel.tag_address.address + 16;
				channel.tag_address.value = channel.address + channel.qword_count * 16;
				break;
			case DMASourceID::NEXT:
				channel.address = channel.tag_address.address + 16;
				channel.tag_address.address = tag.address;
				break;
			default:
                printf("\n[DMAC] Unrecognized VIF%d DMAtag id %d\n", id, tag_id);
			}

			/* Just end transfer, since an interrupt will be raised there anyways  */
			if (channel.control.enable_irq_bit && tag.irq)
				channel.end_transfer = true;

			break;
		}
		case DMAChannels::SIF0:
		{
			auto& sif = bus->sif;
            if (sif->sif0_fifo.size() >= 2)
			{
				uint32_t data[2] = {};
				for (int i = 0; i < 2; i++)
				{
					data[i] = sif->sif0_fifo.front();
					sif->sif0_fifo.pop();
				}

				tag.value = *(uint64_t*)data;
				printf("[DMAC] Read SIF0 DMA tag 0x%lX\n", (uint64_t)tag.value);

				/* Update channel from tag */
				channel.qword_count = tag.qwords;
				channel.control.tag = (tag.value >> 16) & 0xffff;
				channel.address = tag.address;
				channel.tag_address.address += 16;

				printf("[DMAC] QWC: %d\nADDR: 0x%X\n", channel.qword_count, channel.address);

				/* Just end transfer, since an interrupt will be raised there anyways */
				if (channel.control.enable_irq_bit && tag.irq)
					channel.end_transfer = true;
			}
			break;
		}
		case DMAChannels::SIF1:
		{
			assert(channel.control.mode == 1);
			assert(!channel.tag_address.mem_select);

			auto address = channel.tag_address.address;

			/* Get tag from memory */
			/* TODO: Access to other memory types */
			tag.value = *(uint128_t*)&bus->eeRam[address];
			printf("[DMAC] Read SIF1 DMA tag 0x%lX\n", (uint64_t)tag.value);

			/* Update channel from tag */
			channel.qword_count = tag.qwords;
			channel.control.tag = (tag.value >> 16) & 0xffff;

			uint16_t tag_id = tag.id;
			switch (tag_id)
			{
			case DMASourceID::REFE:
			{
				channel.address = tag.address;
				channel.tag_address.value += 16;
				channel.end_transfer = true;
				break;
			}
			case DMASourceID::NEXT:
			{
				channel.address = channel.tag_address.address + 16;
				channel.tag_address.value = tag.address;
				break;
			}
			case DMASourceID::REF:
			{
				channel.address = tag.address;
				channel.tag_address.value += 16;
				break;
			}
			default:
                printf("\n[DMAC] Unrecognized SIF1 DMAtag id %d\n", tag_id);
			}

			/* Just end transfer, since an interrupt will be raised there anyways */
			if (channel.control.enable_irq_bit && tag.irq)
				channel.end_transfer = true;

			break;
		}
		default:
            printf("[DMAC] Unknown channel %d\n", id);
		}
	}
