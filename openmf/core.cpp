//------------------------------------------------------------------------------------------------------//
//
//                                          License Agreement
//                                     For Open Source SMAF Library
//
//                         Copyright (c) 2015-2017, @shirajira, all rights reserved.
//
//------------------------------------------------------------------------------------------------------//

#include "core.h"
#include "array_operations.h"

using namespace smaf;

//------------------------------------------------------------------------------------------------------//
// Timebase Class
//------------------------------------------------------------------------------------------------------//

timebase::timebase()
	: D(RESERVED)
	, G(RESERVED)
{}

timebase::timebase(const timebase& rTimebase_)
	: D(rTimebase_.D)
	, G(rTimebase_.G)
{}

timebase::timebase(u8_t timebase_)
	: D(timebase_)
	, G(timebase_)
{}

timebase::timebase(u8_t duration_, u8_t gatetime_)
	: D(duration_)
	, G(gatetime_)
{}

timebase::~timebase()
{}

timebase& timebase::operator=(const timebase& rTimebase)
{
	D = rTimebase.D;
	G = rTimebase.G;
	return *this;
}

bool timebase::is_valid() const
{
	return (D != RESERVED && G != RESERVED && D == G) ? true : false;
}

u32_t timebase::D_ms() const
{
	u32_t ret_val = 0;
	switch (D)
	{
	case x01_ms: ret_val =  1; break;
	case x02_ms: ret_val =  2; break;
	case x04_ms: ret_val =  4; break;
	case x05_ms: ret_val =  5; break;
	case x10_ms: ret_val = 10; break;
	case x20_ms: ret_val = 20; break;
	case x40_ms: ret_val = 40; break;
	case x50_ms: ret_val = 50; break;
	default: ret_val = 0; break;
	}
	return ret_val;
}

u32_t timebase::G_ms() const
{
	u32_t ret_val = 0;
	switch (G)
	{
	case x01_ms: ret_val =  1; break;
	case x02_ms: ret_val =  2; break;
	case x04_ms: ret_val =  4; break;
	case x05_ms: ret_val =  5; break;
	case x10_ms: ret_val = 10; break;
	case x20_ms: ret_val = 20; break;
	case x40_ms: ret_val = 40; break;
	case x50_ms: ret_val = 50; break;
	default: ret_val = 0; break;
	}
	return ret_val;
}

//------------------------------------------------------------------------------------------------------//
// Channel Status Class (format_type::MOBILE_COMPRESS/format_type::MOBILE_NO_COMPRESS)
//------------------------------------------------------------------------------------------------------//

channel_status::channel_status()
	: m_kcs(KCS_NOCARE)
	, m_vs(VS_OFF)
	, m_led(LED_OFF)
	, m_type(TYPE_NOCARE)
{}

channel_status::channel_status(const channel_status& rStatus_)
	: m_status(rStatus_())
{}

channel_status::channel_status(u8_t data_)
	: m_status(data_)
{}

channel_status::channel_status(u8_t kcs_, u8_t vs_, u8_t led_, u8_t type_)
	: m_kcs(kcs_)
	, m_vs(vs_)
	, m_led(led_)
	, m_type(type_)
{}

channel_status::~channel_status()
{}

channel_status& channel_status::operator=(const channel_status& rStatus_)
{
	m_status = rStatus_();
	return *this;
}

u8_t channel_status::operator()() const
{
	return m_status;
}

u8_t channel_status::kcs() const
{
	return m_kcs;
}

u8_t channel_status::vs() const
{
	return m_vs;
}

u8_t channel_status::led() const
{
	return m_led;
}

u8_t channel_status::ch_type() const
{
	return m_type;
}

//------------------------------------------------------------------------------------------------------//
// SMAF Data Class (MA-3)
//------------------------------------------------------------------------------------------------------//

const u32_t MA_3::CHUNK_HEAD_SIZE = 4;							// Chunk Header Size [byte]
const u32_t MA_3::CHUNK_DATA_SIZE = 4;							// Chunk Data Size [byte]
const u32_t MA_3::NOP_SIZE = 2;									// NOP Size [byte]
const u32_t MA_3::EOS_SIZE = 3;									// EOS Size [byte]
const u32_t MA_3::CRC_SIZE = 2;									// CRC Size [byte]
const u32_t MA_3::CHANNELS = 16;								// Number of Channels

MA_3::MA_3()
	: binary_array()
{}

MA_3::MA_3(const MA_3& rData_)
	: binary_array(rData_)
{}

MA_3::MA_3(const binary_array& rData_)
	: binary_array(rData_)
{}

MA_3::MA_3(u32_t size_)
	: binary_array(size_)
{}

MA_3::MA_3(u32_t size_, const u8_t* pArr_)
	: binary_array(size_, pArr_)
{}

MA_3::~MA_3()
{}

MA_3& MA_3::operator=(const MA_3& rData_)
{
	this->create(rData_.size(), rData_.data_ptr());
	return *this;
}

bool MA_3::shrink_to_fit()
{
	if (this->empty()) return false;

	const u8_t* pAddr = this->data_ptr();
	if (!check_chunk("MMMD", pAddr)) return false;
	pAddr += CHUNK_HEAD_SIZE;

	const u32_t act_size
		= calc_size(pAddr, CHUNK_DATA_SIZE)
		+ CHUNK_HEAD_SIZE
		+ CHUNK_DATA_SIZE;

	if (act_size > this->size()) return false;					// Data Missing Error

	if (act_size < this->size())
	{
		MA_3 shrink_array(act_size, this->data_ptr());
		*this = shrink_array;
	}
	return true;
}

format_type MA_3::get_format() const
{
	if (this->empty()) return format_type::FORMAT_RESERVED;

	const u8_t* pAddr = this->data_ptr();
	u32_t size;

	if (!check_chunk("MMMD", pAddr)) return format_type::FORMAT_RESERVED;
	pAddr += CHUNK_HEAD_SIZE;
	pAddr += CHUNK_DATA_SIZE;

	if (!check_chunk("CNTI", pAddr)) return format_type::FORMAT_RESERVED;
	pAddr += CHUNK_HEAD_SIZE;
	size = calc_size(pAddr, CHUNK_DATA_SIZE);
	pAddr += (CHUNK_DATA_SIZE + size);

	if (!check_chunk("OPDA", pAddr)) return format_type::FORMAT_RESERVED;
	pAddr += CHUNK_HEAD_SIZE;
	size = calc_size(pAddr, CHUNK_DATA_SIZE);
	pAddr += (CHUNK_DATA_SIZE + size);

	if (!check_chunk("MTR*", pAddr)) return format_type::FORMAT_RESERVED;
	pAddr += CHUNK_HEAD_SIZE;
	pAddr += CHUNK_DATA_SIZE;

	format_type fmt;
	switch (*pAddr)
	{
	case 0x00:
		fmt = format_type::HANDY_PHONE;
		break;
	case 0x01:
		fmt = format_type::MOBILE_COMPRESS;
		break;
	case 0x02:
		fmt = format_type::MOBILE_NO_COMPRESS;
		break;
	default:
		fmt = format_type::FORMAT_RESERVED;
		break;
	}
	return fmt;
}

timebase MA_3::get_timebase() const
{
	if (this->empty()) return timebase();

	const u8_t* pAddr = this->data_ptr();
	u32_t size;

	if (!check_chunk("MMMD", pAddr)) return timebase();
	pAddr += CHUNK_HEAD_SIZE;
	pAddr += CHUNK_DATA_SIZE;

	if (!check_chunk("CNTI", pAddr)) return timebase();
	pAddr += CHUNK_HEAD_SIZE;
	size = calc_size(pAddr, CHUNK_DATA_SIZE);
	pAddr += (CHUNK_DATA_SIZE + size);

	if (!check_chunk("OPDA", pAddr)) return timebase();
	pAddr += CHUNK_HEAD_SIZE;
	size = calc_size(pAddr, CHUNK_DATA_SIZE);
	pAddr += (CHUNK_DATA_SIZE + size);

	if (!check_chunk("MTR*", pAddr)) return timebase();
	pAddr += CHUNK_HEAD_SIZE;
	pAddr += CHUNK_DATA_SIZE;

	pAddr++;													// Format Type
	pAddr++;													// Sequence Type
	const u8_t duration = *pAddr++;								// Timebase of Duration
	const u8_t gatetime = *pAddr;								// Timebase of Timebase

	return timebase(duration, gatetime);
}

channel_status MA_3::get_channel_status(u32_t ch_) const
{
	if (this->empty() || ch_ >= CHANNELS) return channel_status();

	const u8_t* pAddr = this->data_ptr();
	u32_t size;

	if (!check_chunk("MMMD", pAddr)) return channel_status();
	pAddr += CHUNK_HEAD_SIZE;
	pAddr += CHUNK_DATA_SIZE;

	if (!check_chunk("CNTI", pAddr)) return channel_status();
	pAddr += CHUNK_HEAD_SIZE;
	size = calc_size(pAddr, CHUNK_DATA_SIZE);
	pAddr += (CHUNK_DATA_SIZE + size);

	if (!check_chunk("OPDA", pAddr)) return channel_status();
	pAddr += CHUNK_HEAD_SIZE;
	size = calc_size(pAddr, CHUNK_DATA_SIZE);
	pAddr += (CHUNK_DATA_SIZE + size);

	if (!check_chunk("MTR*", pAddr)) return channel_status();
	pAddr += CHUNK_HEAD_SIZE;
	pAddr += CHUNK_DATA_SIZE;

	pAddr++;													// Format Type
	pAddr++;													// Sequence Type
	pAddr++;													// Timebase of Duration
	pAddr++;													// Timebase of Timebase

	channel_status ch_status[CHANNELS];
	for (u32_t i = 0; i < CHANNELS; i++)
	{
		ch_status[i] = *pAddr++;
	}
	return ch_status[ch_];
}

//------------------------------------------------------------------------------------------------------//
// CRC16 Class
//------------------------------------------------------------------------------------------------------//

const u16_t CRC16::POLY = 0x1021;								// Default Polynomial for SMAF
const u32_t CRC16::TABLE_SIZE = 256;							// CRC Table Size

CRC16::CRC16()
	: m_table()
{}

CRC16::~CRC16()
{
	this->release();
}

bool CRC16::initialize(u16_t polynomial_)
{
	if (!m_table.create(TABLE_SIZE)) return false;

	for (u16_t i = 0; i < TABLE_SIZE; i++)
	{
		u16_t r = (i << 8);
		for (u8_t j = 0; j < 8; j++)
		{
			if (r & 0x8000)
			{
				r = (r << 1) ^ polynomial_;
			}
			else
			{
				r <<= 1;
			}
		}
		m_table[i] = (r & 0xFFFF);
	}
	return true;
}

bool CRC16::is_initialized() const
{
	return !m_table.empty();
}

void CRC16::release()
{
	m_table.release();
}

u16_t CRC16::make(const u8_t* pArr_, u32_t len_) const
{
	if (!this->is_initialized()) return u16_t(0x0000);

	u16_t r = 0xFFFF;
	for (u32_t i = 0; i < len_; i++)
	{
		r = (r << 8) ^ m_table.at(static_cast<u8_t>(r >> 8) ^ pArr_[i]);
	}
	return (~r & 0xFFFF);
}

//------------------------------------------------------------------------------------------------------//
// End of File
//------------------------------------------------------------------------------------------------------//
