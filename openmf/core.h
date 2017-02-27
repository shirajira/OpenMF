//------------------------------------------------------------------------------------------------------//
//
//                                          License Agreement
//                                     For Open Source SMAF Library
//
//                         Copyright (c) 2015-2017, @shirajira, all rights reserved.
//
//------------------------------------------------------------------------------------------------------//

#ifndef openmf_core_h__
#define openmf_core_h__
#pragma once

#include "basic_type.h"

namespace smaf {

//------------------------------------------------------------------------------------------------------//
// Format Type (enum)
//------------------------------------------------------------------------------------------------------//
enum format_type
{
	HANDY_PHONE        = 0x00,									// Handy Phone Standard
	MOBILE_COMPRESS    = 0x01,									// Mobile Standard (Compress)
	MOBILE_NO_COMPRESS = 0x02,									// Mobile Standard (No Compress)
	FORMAT_RESERVED    = 0xFF									// Reserved (Error)
};

//------------------------------------------------------------------------------------------------------//
// Timebase Class
//------------------------------------------------------------------------------------------------------//
class timebase
{
public:
	timebase();
	timebase(const timebase& rTimebase_);
	timebase(u8_t timebase_);
	timebase(u8_t duration_, u8_t gatetime_);
	~timebase();

public:
	timebase& operator=(const timebase& rTimebase);

public:
	enum
	{
		x01_ms   = 0x00,										// Timebase Code:  1 [ms] (No Supported)
		x02_ms   = 0x01,										// Timebase Code:  2 [ms] (No Supported)
		x04_ms   = 0x02,										// Timebase Code:  4 [ms] (ATS-MAx)
		x05_ms   = 0x03,										// Timebase Code:  5 [ms] (JKS)
		x10_ms   = 0x10,										// Timebase Code: 10 [ms]
		x20_ms   = 0x11,										// Timebase Code: 20 [ms] (SH)
		x40_ms   = 0x12,										// Timebase Code: 40 [ms]
		x50_ms   = 0x13,										// Timebase Code: 50 [ms]
		RESERVED = 0xFF											// Reserved (Error)
	};

	// Check.
	bool is_valid() const;

	// Convert duration's timebase code to msec.
	u32_t D_ms() const;

	// Convert gatetime's timebase code to msec.
	u32_t G_ms() const;

public:
	u8_t D;														// Duration's Timebase Code
	u8_t G;														// Gatetime's Timebase Code
};

//------------------------------------------------------------------------------------------------------//
// Channel Status Union (format_type::MOBILE_COMPRESS/format_type::MOBILE_NO_COMPRESS)
//------------------------------------------------------------------------------------------------------//
union channel_status
{
public:
	channel_status();
	channel_status(const channel_status& rStatus_);
	channel_status(u8_t data_);
	channel_status(u8_t kcs_, u8_t vs_, u8_t led_, u8_t type_);
	~channel_status();

public:
	channel_status& operator=(const channel_status& rStatus_);

public:
	enum
	{
		KCS_NOCARE   = 0x0,										// Key Control Status: No Care
		KCS_OFF      = 0x1,										// Key Control Status: OFF
		KCS_ON       = 0x2,										// Key Control Status: ON
		KCS_RESERVED = 0x3,										// Key Control Status: Reserved

		VS_OFF = 0x0,											// Vibration Status: OFF
		VS_ON  = 0x1,											// Vibration Status: ON

		LED_OFF = 0x0,											// LED Status: OFF
		LED_ON  = 0x1,											// LED Status: ON

		TYPE_NOCARE   = 0x0,									// Channel type: No Care
		TYPE_MELODY   = 0x1,									// Channel type: Melody
		TYPE_NOMELODY = 0x2,									// Channel type: No Melody
		TYPE_RHYTHM   = 0x3										// Channel type: Rhythm
	};

	// Return channel status.
	u8_t operator()() const;

	// Return key control status.
	u8_t kcs() const;

	// Return vibration status.
	u8_t vs() const;

	// Return LED status.
	u8_t led() const;

	// Return channel type.
	u8_t ch_type() const;

private:
	u8_t m_status;												// Channel Status (8bit)
	struct
	{
		u8_t m_type : 2;										// Bit Field: Ch Type (2bit)
		u8_t        : 2;										// Bit Field: Reserved (2bit)
		u8_t m_led  : 1;										// Bit Field: LED Status (1bit)
		u8_t m_vs   : 1;										// Bit Field: Vibration Status (1bit)
		u8_t m_kcs  : 2;										// Bit Field: Key Control Status (2bit)		
	};
};

//------------------------------------------------------------------------------------------------------//
// Data Array Template Class
//------------------------------------------------------------------------------------------------------//
template<typename tp_> class data_array_
{
public:
	data_array_()
		: m_size(0)
		, m_pDataArr(nullptr)
	{}
	data_array_(const data_array_& rData_)
		: m_size(0)
		, m_pDataArr(nullptr)
	{
		this->create(rData_.size(), rData_.data_ptr());
	}
	data_array_(u32_t size_)
		: m_size(0)
		, m_pDataArr(nullptr)
	{
		this->create(size_);
	}
	data_array_(u32_t size_, const tp_* pArr_)
		: m_size(0)
		, m_pDataArr(nullptr)
	{
		this->create(size_, pArr_);
	}
	virtual ~data_array_()
	{
		this->release();
	}

public:
	data_array_& operator=(const data_array_& rData_)
	{
		this->create(rData_.size(), rData_.data_ptr());
		return *this;
	}
	bool operator==(const data_array_& rData_) const
	{
		return (m_pDataArr == rData_.data_ptr()) ? true : false;
	}
	bool operator!=(const data_array_& rData_) const
	{
		return !this->operator==(rData_);
	}

public:
	// Create memory.
	virtual bool create(u32_t size_)
	{
		if (size_ == 0) return false;
		this->release();
		m_pDataArr = new tp_[size_];
		if (m_pDataArr == nullptr) return false;
		m_size = size_;
		return true;
	}

	// Create memory and copy data.
	virtual bool create(u32_t size_, const tp_* pArr_)
	{
		if (pArr_ == nullptr || !this->create(size_)) return false;
		for (u32_t i = 0; i < this->size(); i++)
		{
			(*this)[i] = *pArr_++;
		}
		return true;
	}

	// Set data to all elements.
	virtual void set(const tp_& rVal_)
	{
		if (!this->empty())
		{
			for (u32_t i = 0; i < this->size(); i++)
			{
				(*this)[i] = rVal_;
			}
		}
	}

	// Check the data is empty.
	virtual bool empty() const
	{
		return (m_pDataArr == nullptr) ? true : false;
	}

	// Release.
	virtual void release()
	{
		if (!this->empty())
		{
			delete[] m_pDataArr;
			m_pDataArr = nullptr;
			m_size = 0;
		}
	}

	// Access data.
	virtual tp_& operator[](u32_t n_)
	{
		return m_pDataArr[n_];
	}

	// Access data. (const function)
	virtual tp_ at(u32_t n_) const
	{
		return m_pDataArr[n_];
	}

	// Return data size.
	virtual u32_t size() const
	{
		return m_size;
	}

	// Return data ptr.
	virtual tp_* data_ptr() const
	{
		return m_pDataArr;
	}

	// Push data.
	virtual void push(const tp_& rData_)
	{
		if (this->empty())
		{
			this->create(1);
			(*this)[0] = rData_;
		}
		else
		{
			const u32_t new_size = (this->size() + 1);
			data_array_ temp(new_size);
			for (u32_t i = 0; i < new_size; i++)
			{
				temp[i] = (*this)[i];
				if (i == this->size()) temp[i] = rData_;
			}
			*this = temp;
		}
	}

	// Pop data.
	virtual void pop()
	{
		if (this->size() <= 1)
		{
			this->release();
		}
		else
		{
			const u32_t new_size = (this->size() - 1);
			data_array_ temp(new_size, this->data_ptr());
			*this = temp;
		}
	}

protected:
	u32_t m_size;												// Data Size (Protected Member)
	tp_*  m_pDataArr;											// Array Ptr (Protected Member)
};

typedef data_array_<u8_t> binary_array;							// Data Array for Binary

//------------------------------------------------------------------------------------------------------//
// SMAF Data Class (MA-3)
//------------------------------------------------------------------------------------------------------//
class MA_3 : public binary_array
{
public:
	MA_3();
	MA_3(const MA_3& rData_);
	MA_3(const binary_array& rData_);
	MA_3(u32_t size_);
	MA_3(u32_t size_, const u8_t* pArr_);
	~MA_3();

public:
	MA_3& operator=(const MA_3& rData_);

public:
	static const u32_t CHUNK_HEAD_SIZE;							// Chunk Header Size [byte]
	static const u32_t CHUNK_DATA_SIZE;							// Chunk Data Size [byte]
	static const u32_t NOP_SIZE;								// NOP Size [byte]
	static const u32_t EOS_SIZE;								// EOS Size [byte]
	static const u32_t CRC_SIZE;								// CRC Size [byte]
	static const u32_t CHANNELS;								// Number of Channels

	// Shrink to fit with actual data size.
	bool shrink_to_fit();

	// Return format type.
	format_type get_format() const;

	// Return timebase.
	timebase get_timebase() const;

	// Return channel status.
	channel_status get_channel_status(u32_t ch_) const;
};

//------------------------------------------------------------------------------------------------------//
// CRC16 Class
//------------------------------------------------------------------------------------------------------//
class CRC16
{
public:
	CRC16();
	~CRC16();

private:
	CRC16(const CRC16&);
	CRC16& operator=(const CRC16&);

public:
	static const u16_t POLY;									// Default Polynomial for SMAF
	static const u32_t TABLE_SIZE;								// CRC Table Size

	// Initialize.
	bool initialize(u16_t polynomial_ = POLY);

	// Check the table is initialized.
	bool is_initialized() const;

	// Release.
	void release();

	// Make CRC code.
	u16_t make(const u8_t* pArr_, u32_t len_) const;

private:
	data_array_<u16_t> m_table;									// CRC Table Array
};

//------------------------------------------------------------------------------------------------------//
}																// namespace smaf

#endif															// openmf_core_h__
//------------------------------------------------------------------------------------------------------//
// End of File
//------------------------------------------------------------------------------------------------------//