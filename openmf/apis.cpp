//------------------------------------------------------------------------------------------------------//
//
//                                          License Agreement
//                                     For Open Source SMAF Library
//
//                         Copyright (c) 2015-2017, @shirajira, all rights reserved.
//
//------------------------------------------------------------------------------------------------------//

#include "apis.h"
#include "array_operations.h"
#include <fstream>

using namespace smaf;

//------------------------------------------------------------------------------------------------------//
// Load Binary Data from File
//------------------------------------------------------------------------------------------------------//
bool smaf::load(const char* szFile, binary_array& rDst_)
{
	std::ifstream fin(szFile, std::ios::binary);
	if (!fin.is_open()) return false;

	fin.seekg(0, std::ios::end);
	const u32_t size = static_cast<u32_t>(fin.tellg());
	fin.seekg(0, std::ios::beg);

	if (!rDst_.create(size))
	{
		fin.close();
		return false;
	}

	u8_t bin;
	u8_t* pAddr = rDst_.data_ptr();
	for (u32_t i = 0; i < rDst_.size(); i++)
	{
		fin.read(reinterpret_cast<char*>(&bin), sizeof(u8_t));
		*pAddr++ = bin;
	}
	fin.close();

	return true;
}

//------------------------------------------------------------------------------------------------------//
// Save Binary Data to File
//------------------------------------------------------------------------------------------------------//
bool smaf::save(const char* szFile, const binary_array& rSrc_)
{
	if (rSrc_.empty()) return false;

	std::ofstream fout(szFile, std::ios::binary);
	if (!fout.is_open()) return false;

	u8_t* pAddr = rSrc_.data_ptr();
	for (u32_t i = 0; i < rSrc_.size(); i++)
	{
		fout.write(reinterpret_cast<char*>(pAddr++), sizeof(u8_t));
	}
	fout.close();

	return true;
}

//------------------------------------------------------------------------------------------------------//
// Fix CRC16 Code
//------------------------------------------------------------------------------------------------------//
bool smaf::fix_crc16(MA_3& rSrcDst_)
{
	if (rSrcDst_.empty()) return false;

	CRC16 crc_gen;
	if (!crc_gen.initialize(CRC16::POLY)) return false;

	const u32_t act_size = (rSrcDst_.size() - MA_3::CRC_SIZE);
	const u16_t crc_code = crc_gen.make(rSrcDst_.data_ptr(), act_size);

	rSrcDst_[act_size + 0] = ((crc_code >> 8) & 0xFF);
	rSrcDst_[act_size + 1] = ((crc_code >> 0) & 0xFF);

	crc_gen.release();

	return true;
}

//------------------------------------------------------------------------------------------------------//
// Remove EOS for Smooth Loop
//------------------------------------------------------------------------------------------------------//
bool smaf::remove_nop(MA_3& rSrcDst_)
{
	const format_type fmt = rSrcDst_.get_format();
	if (fmt != format_type::MOBILE_NO_COMPRESS) return false;

	u8_t* pAddr = rSrcDst_.data_ptr();
	u32_t size;
	u32_t cnt = 0;

	u32_t file_size, score_size, sequence_size;
	u32_t file_size_pos, score_size_pos, sequence_size_pos;

	if (!check_chunk("MMMD", &pAddr[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	file_size_pos = cnt;
	file_size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += MA_3::CHUNK_DATA_SIZE;

	if (!check_chunk("CNTI", &pAddr[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("OPDA", &pAddr[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("MTR*", &pAddr[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	score_size_pos = cnt;
	score_size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += MA_3::CHUNK_DATA_SIZE;
	cnt++;														// Format Type
	cnt++;														// Sequence Type
	cnt++;														// Timebase of Duration
	cnt++;														// Timebase of Gatetime
	cnt += MA_3::CHANNELS;										// Channel Status

	if (check_chunk("MspI", &pAddr[cnt]))
	{
		cnt += MA_3::CHUNK_HEAD_SIZE;
		size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
		cnt += (MA_3::CHUNK_DATA_SIZE + size);
	}

	if (check_chunk("Mtsu", &pAddr[cnt]))
	{
		cnt += MA_3::CHUNK_HEAD_SIZE;
		size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
		cnt += (MA_3::CHUNK_DATA_SIZE + size);
	}

	if (!check_chunk("Mtsq", &pAddr[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	sequence_size_pos = cnt;
	sequence_size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += MA_3::CHUNK_DATA_SIZE;

	u32_t nNOP = 0;												// Number of NOP
	bool bEOS = false;											// EOS Flag

	sequence_state next_state = SS_DURATION;

	const u32_t sequence_end = (cnt + sequence_size);
	while (cnt < sequence_end)
	{
		switch (next_state)
		{
		case SS_DURATION:
			next_state = SS_STATUS;
			{
				u32_t len;										// For Variable Size
				u32_t duration = calc_variable_size(&pAddr[cnt], len);
				cnt += len;
			}
			break;
		case SS_STATUS:
			switch (pAddr[cnt] & 0xF0)
			{
			case SE_NOTE_NOVELOCITY:
				next_state = SS_GATETIME;
				{
					cnt += 2;
				}
				break;
			case SE_NOTE_VELOCITY:
				next_state = SS_GATETIME;
				{
					cnt += 3;
				}
				break;
			case SE_RESERVED_3BYTE:
				next_state = SS_DURATION;
				{
					cnt += 3;
				}
				break;
			case SE_CONTROL_CHANGE:
				next_state = SS_DURATION;
				{
					cnt += 3;
				}
				break;
			case SE_PROGRAM_CHANGE:
				next_state = SS_DURATION;
				{
					cnt += 2;
				}
				break;
			case SE_RESERVED_2BYTE:
				next_state = SS_DURATION;
				{
					cnt += 2;
				}
				break;
			case SE_PITCH_BEND:
				next_state = SS_DURATION;
				{
					cnt += 3;
				}
				break;
			case SE_SYSTEM_EXCLUSIVE:
				next_state = SS_DURATION;
				{
					if (pAddr[cnt] == SE_SYSTEM_EXCLUSIVE)
					{
						cnt++;
						u32_t len;
						u32_t info_size = calc_variable_size(&pAddr[cnt], len);
						cnt += (len + info_size);
					}
					else if (pAddr[cnt] == SE_EOS_NOP)
					{
						if (pAddr[cnt + 1] == 0x2F)
						{
							cnt += MA_3::EOS_SIZE;				// EOS
							bEOS = true;
						}
						else
						{
							cnt += MA_3::NOP_SIZE;				// NOP
							nNOP++;
						}
					}
					else
					{
						cnt++;
					}
				}
				break;
			default:
				return false;									// Error
			}
			break;
		case SS_GATETIME:
			next_state = SS_DURATION;
			{
				u32_t len;										// For Variable Size
				u32_t gatetime = calc_variable_size(&pAddr[cnt], len);
				cnt += len;

				nNOP = 0;
			}
			break;
		default:
			return false;										// Error
		}
	}

	const u32_t nop_duration = 1;								// NOP's Duration [byte]
	const u32_t eos_duration = 1;								// EOS's Duration [byte]

	u32_t reduce_size = 0;
	if (bEOS)
	{
		reduce_size = ((nNOP * (nop_duration + MA_3::NOP_SIZE)) + (eos_duration + MA_3::EOS_SIZE));
	}
	else
	{
		reduce_size = (nNOP * (nop_duration + MA_3::NOP_SIZE));
	}

	if (reduce_size != 0)
	{
		const u32_t new_size = (rSrcDst_.size() - reduce_size);
		MA_3 temp(new_size, rSrcDst_.data_ptr());

		file_size -= reduce_size;
		make_size_array(file_size, MA_3::CHUNK_DATA_SIZE, &temp[file_size_pos]);

		score_size -= reduce_size;
		make_size_array(score_size, MA_3::CHUNK_DATA_SIZE, &temp[score_size_pos]);

		sequence_size -= reduce_size;
		make_size_array(sequence_size, MA_3::CHUNK_DATA_SIZE, &temp[sequence_size_pos]);

		if (!fix_crc16(temp)) return false;
		rSrcDst_ = temp;
	}

	return true;
}

//------------------------------------------------------------------------------------------------------//
// Clear Channel Status
//------------------------------------------------------------------------------------------------------//
bool smaf::clear_channel_status(MA_3& rSrcDst_)
{
	if (rSrcDst_.empty()) return false;

	const format_type fmt = rSrcDst_.get_format();
	if (fmt != format_type::MOBILE_COMPRESS && fmt != format_type::MOBILE_NO_COMPRESS) return false;

	u8_t* pAddr = rSrcDst_.data_ptr();
	u32_t size;

	if (!check_chunk("MMMD", pAddr)) return false;
	pAddr += MA_3::CHUNK_HEAD_SIZE;
	pAddr += MA_3::CHUNK_DATA_SIZE;

	if (!check_chunk("CNTI", pAddr)) return false;
	pAddr += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(pAddr, MA_3::CHUNK_DATA_SIZE);
	pAddr += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("OPDA", pAddr)) return false;
	pAddr += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(pAddr, MA_3::CHUNK_DATA_SIZE);
	pAddr += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("MTR*", pAddr)) return false;
	pAddr += MA_3::CHUNK_HEAD_SIZE;
	pAddr += MA_3::CHUNK_DATA_SIZE;

	pAddr++;													// Format Type
	pAddr++;													// Sequence Type
	pAddr++;													// Timebase of Duration
	pAddr++;													// Timebase of Gatetime

	const channel_status reset = (
		channel_status::KCS_NOCARE,
		channel_status::VS_OFF,
		channel_status::LED_OFF,
		channel_status::TYPE_NOCARE);
	for (u32_t ch = 0; ch < MA_3::CHANNELS; ch++)
	{
		*pAddr++ = reset();
	}

	return fix_crc16(rSrcDst_);
}

//------------------------------------------------------------------------------------------------------//
// Change Channel Status
//------------------------------------------------------------------------------------------------------//
bool smaf::change_channel_status(MA_3& rSrcDst_, u32_t ch_, const channel_status& rStatus_)
{
	if (rSrcDst_.empty() || ch_ >= MA_3::CHANNELS) return false;

	const format_type fmt = rSrcDst_.get_format();
	if (fmt != format_type::MOBILE_COMPRESS && fmt != format_type::MOBILE_NO_COMPRESS) return false;

	u8_t* pAddr = rSrcDst_.data_ptr();
	u32_t size;

	if (!check_chunk("MMMD", pAddr)) return false;
	pAddr += MA_3::CHUNK_HEAD_SIZE;
	pAddr += MA_3::CHUNK_DATA_SIZE;

	if (!check_chunk("CNTI", pAddr)) return false;
	pAddr += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(pAddr, MA_3::CHUNK_DATA_SIZE);
	pAddr += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("OPDA", pAddr)) return false;
	pAddr += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(pAddr, MA_3::CHUNK_DATA_SIZE);
	pAddr += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("MTR*", pAddr)) return false;
	pAddr += MA_3::CHUNK_HEAD_SIZE;
	pAddr += MA_3::CHUNK_DATA_SIZE;

	pAddr++;													// Format Type
	pAddr++;													// Sequence Type
	pAddr++;													// Timebase of Duration
	pAddr++;													// Timebase of Gatetime

	pAddr += ch_;												// Move to Target Address
	*pAddr = rStatus_();

	return fix_crc16(rSrcDst_);
}

//------------------------------------------------------------------------------------------------------//
// Change Timebase
//------------------------------------------------------------------------------------------------------//
bool smaf::change_timebase(MA_3& rSrcDst_, const timebase& rNewTimebase_)
{
	if (rSrcDst_.empty() || !rNewTimebase_.is_valid()) return false;

	const format_type fmt = rSrcDst_.get_format();
	if (fmt != format_type::MOBILE_COMPRESS && fmt != format_type::MOBILE_NO_COMPRESS) return false;

	u8_t* pAddr = rSrcDst_.data_ptr();
	u32_t size;

	if (!check_chunk("MMMD", pAddr)) return false;
	pAddr += MA_3::CHUNK_HEAD_SIZE;
	pAddr += MA_3::CHUNK_DATA_SIZE;

	if (!check_chunk("CNTI", pAddr)) return false;
	pAddr += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(pAddr, MA_3::CHUNK_DATA_SIZE);
	pAddr += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("OPDA", pAddr)) return false;
	pAddr += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(pAddr, MA_3::CHUNK_DATA_SIZE);
	pAddr += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("MTR*", pAddr)) return false;
	pAddr += MA_3::CHUNK_HEAD_SIZE;
	pAddr += MA_3::CHUNK_DATA_SIZE;

	pAddr++;													// Format Type
	pAddr++;													// Sequence Type

	*pAddr++ = rNewTimebase_.D;									// Timebase of Duration
	*pAddr = rNewTimebase_.G;									// Timebase of Gatetime

	return fix_crc16(rSrcDst_);
}

//------------------------------------------------------------------------------------------------------//
// Change Tempo
//------------------------------------------------------------------------------------------------------//
bool smaf::change_tempo(const MA_3& rSrc_, const timebase& rNewTimebase_, f64_t ratio_, MA_3& rDst_)
{
	const format_type fmt = rSrc_.get_format();
	if (fmt != format_type::MOBILE_NO_COMPRESS) return false;

	if (!rNewTimebase_.is_valid() || ratio_ == 0.0) return false;

	if (rSrc_ == rDst_) return false;

	const timebase curr_timebase = rSrc_.get_timebase();
	const f64_t ratio = curr_timebase.D_ms() / (rNewTimebase_.D_ms() * ratio_);

	u8_t* pAddr = rSrc_.data_ptr();
	u32_t size;
	u32_t cnt = 0;

	u32_t file_size, score_size, sequence_size;
	u32_t file_size_pos, score_size_pos, sequence_size_pos;

	if (!check_chunk("MMMD", &pAddr[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	file_size_pos = cnt;
	file_size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += MA_3::CHUNK_DATA_SIZE;

	if (!check_chunk("CNTI", &pAddr[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("OPDA", &pAddr[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("MTR*", &pAddr[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	score_size_pos = cnt;
	score_size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += MA_3::CHUNK_DATA_SIZE;
	cnt++;														// Format Type
	cnt++;														// Sequence Type
	cnt++;														// Timebase of Duration
	cnt++;														// Timebase of Gatetime
	cnt += MA_3::CHANNELS;										// Channel Status

	if (check_chunk("MspI", &pAddr[cnt]))
	{
		cnt += MA_3::CHUNK_HEAD_SIZE;
		size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
		cnt += (MA_3::CHUNK_DATA_SIZE + size);
	}

	if (check_chunk("Mtsu", &pAddr[cnt]))
	{
		cnt += MA_3::CHUNK_HEAD_SIZE;
		size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
		cnt += (MA_3::CHUNK_DATA_SIZE + size);
	}

	if (!check_chunk("Mtsq", &pAddr[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	sequence_size_pos = cnt;
	sequence_size = calc_size(&pAddr[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += MA_3::CHUNK_DATA_SIZE;

	if (!rDst_.create(cnt, pAddr)) return false;

	sequence_state next_state = SS_DURATION;

	const u32_t sequence_end = (cnt + sequence_size);
	while (cnt < sequence_end)
	{
		switch (next_state)
		{
		case SS_DURATION:
			next_state = SS_STATUS;
			{
				u8_t buf[4];									// For Variable Size
				u32_t len;										// For Variable Size
				u32_t duration = calc_variable_size(&pAddr[cnt], len);
				cnt += len;
				if (duration != 0)
				{
					duration = static_cast<u32_t>((duration * ratio) + 0.5);
				}
				make_variable_size_array(duration, buf, len);
				for (u32_t i = 0; i < len; i++)
				{
					rDst_.push(buf[i]);
				}
			}
			break;
		case SS_STATUS:
			switch (pAddr[cnt] & 0xF0)
			{
			case SE_NOTE_NOVELOCITY:
				next_state = SS_GATETIME;
				{
					rDst_.push(pAddr[cnt++]);
					rDst_.push(pAddr[cnt++]);
				}
				break;
			case SE_NOTE_VELOCITY:
				next_state = SS_GATETIME;
				{
					rDst_.push(pAddr[cnt++]);
					rDst_.push(pAddr[cnt++]);
					rDst_.push(pAddr[cnt++]);
				}
				break;
			case SE_RESERVED_3BYTE:
				next_state = SS_DURATION;
				{
					rDst_.push(pAddr[cnt++]);
					rDst_.push(pAddr[cnt++]);
					rDst_.push(pAddr[cnt++]);
				}
				break;
			case SE_CONTROL_CHANGE:
				next_state = SS_DURATION;
				{
					rDst_.push(pAddr[cnt++]);
					rDst_.push(pAddr[cnt++]);
					rDst_.push(pAddr[cnt++]);
				}
				break;
			case SE_PROGRAM_CHANGE:
				next_state = SS_DURATION;
				{
					rDst_.push(pAddr[cnt++]);
					rDst_.push(pAddr[cnt++]);
				}
				break;
			case SE_RESERVED_2BYTE:
				next_state = SS_DURATION;
				{
					rDst_.push(pAddr[cnt++]);
					rDst_.push(pAddr[cnt++]);
				}
				break;
			case SE_PITCH_BEND:
				next_state = SS_DURATION;
				{
					rDst_.push(pAddr[cnt++]);
					rDst_.push(pAddr[cnt++]);
					rDst_.push(pAddr[cnt++]);
				}
				break;
			case SE_SYSTEM_EXCLUSIVE:
				next_state = SS_DURATION;
				{
					if (pAddr[cnt] == SE_SYSTEM_EXCLUSIVE)
					{
						u32_t len;
						u32_t info_size;
						rDst_.push(pAddr[cnt++]);
						info_size = calc_variable_size(&pAddr[cnt], len);

						for (u32_t i = 0; i < (len + info_size); i++)
						{
							rDst_.push(pAddr[cnt++]);
						}
					}
					else if (pAddr[cnt] == SE_EOS_NOP)
					{
						rDst_.push(pAddr[cnt++]);

						if (pAddr[cnt] == 0x2F)
						{
							// EOS
							rDst_.push(pAddr[cnt++]);
							rDst_.push(pAddr[cnt++]);
						}
						else
						{
							// NOP
							rDst_.push(pAddr[cnt++]);
						}
					}
					else
					{
						rDst_.push(pAddr[cnt++]);
					}
				}
				break;
			default:
				return false;									// Error
			}
			break;
		case SS_GATETIME:
			next_state = SS_DURATION;
			{
				u8_t buf[4];									// For Variable Size
				u32_t len;										// For Variable Size
				u32_t gatetime = calc_variable_size(&pAddr[cnt], len);
				cnt += len;
				if (gatetime != 0)
				{
					gatetime = static_cast<u32_t>((gatetime * ratio) + 0.5);
				}
				make_variable_size_array(gatetime, buf, len);
				for (u32_t i = 0; i < len; i++)
				{
					rDst_.push(buf[i]);
				}
			}
			break;
		default:
			return false;										// Error
		}
	}

	rDst_.push(0x00);											// Push Dummy CRC Code (Upper 8bit)
	rDst_.push(0x00);											// Push Dummy CRC Code (Lower 8bit)

	s32_t diff_size = (rDst_.size() - rSrc_.size());
	if (diff_size != 0)
	{
		file_size += diff_size;
		make_size_array(file_size, MA_3::CHUNK_DATA_SIZE, &rDst_[file_size_pos]);

		score_size += diff_size;
		make_size_array(score_size, MA_3::CHUNK_DATA_SIZE, &rDst_[score_size_pos]);

		sequence_size += diff_size;
		make_size_array(sequence_size, MA_3::CHUNK_DATA_SIZE, &rDst_[sequence_size_pos]);
	}

	return change_timebase(rDst_, rNewTimebase_);
}

//------------------------------------------------------------------------------------------------------//
// Combine SMAF Data
//------------------------------------------------------------------------------------------------------//
bool smaf::combine(const MA_3& rSrc1_, const MA_3& rSrc2_, MA_3& rDst_, u32_t gap_)
{
	const format_type fmt1 = rSrc1_.get_format();
	const format_type fmt2 = rSrc1_.get_format();
	if (fmt1 != format_type::MOBILE_NO_COMPRESS || fmt2 != format_type::MOBILE_NO_COMPRESS) return false;

	if (rSrc1_ == rDst_ || rSrc2_ == rDst_) return false;

	// (1) rSrc1_ Analysis
	//
	u8_t* pAddr1 = rSrc1_.data_ptr();
	u32_t size;
	u32_t cnt = 0;

	u32_t file_size, score_size, sequence_size;
	u32_t file_size_pos, score_size_pos, sequence_size_pos;

	if (!check_chunk("MMMD", &pAddr1[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	file_size_pos = cnt;
	file_size = calc_size(&pAddr1[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += MA_3::CHUNK_DATA_SIZE;

	if (!check_chunk("CNTI", &pAddr1[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(&pAddr1[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("OPDA", &pAddr1[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(&pAddr1[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("MTR*", &pAddr1[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	score_size_pos = cnt;
	score_size = calc_size(&pAddr1[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += MA_3::CHUNK_DATA_SIZE;
	cnt++;														// Format Type
	cnt++;														// Sequence Type
	cnt++;														// Timebase of Duration
	cnt++;														// Timebase of Gatetime
	cnt += MA_3::CHANNELS;										// Channel Status

	if (check_chunk("MspI", &pAddr1[cnt]))
	{
		cnt += MA_3::CHUNK_HEAD_SIZE;
		size = calc_size(&pAddr1[cnt], MA_3::CHUNK_DATA_SIZE);
		cnt += (MA_3::CHUNK_DATA_SIZE + size);
	}

	if (check_chunk("Mtsu", &pAddr1[cnt]))
	{
		cnt += MA_3::CHUNK_HEAD_SIZE;
		size = calc_size(&pAddr1[cnt], MA_3::CHUNK_DATA_SIZE);
		cnt += (MA_3::CHUNK_DATA_SIZE + size);
	}

	if (!check_chunk("Mtsq", &pAddr1[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	sequence_size_pos = cnt;
	sequence_size = calc_size(&pAddr1[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += MA_3::CHUNK_DATA_SIZE;

	u32_t nNOP = 0;												// Number of NOP
	bool bEOS = false;											// EOS Flag
	u32_t last_gatetime;										// Last Gatetime

	sequence_state next_state = SS_DURATION;

	const u32_t sequence_end = (cnt + sequence_size);
	while (cnt < sequence_end)
	{
		switch (next_state)
		{
		case SS_DURATION:
			next_state = SS_STATUS;
			{
				u32_t len;										// For Variable Size
				u32_t duration = calc_variable_size(&pAddr1[cnt], len);
				cnt += len;
			}
			break;
		case SS_STATUS:
			switch (pAddr1[cnt] & 0xF0)
			{
			case SE_NOTE_NOVELOCITY:
				next_state = SS_GATETIME;
				{
					cnt += 2;
				}
				break;
			case SE_NOTE_VELOCITY:
				next_state = SS_GATETIME;
				{
					cnt += 3;
				}
				break;
			case SE_RESERVED_3BYTE:
				next_state = SS_DURATION;
				{
					cnt += 3;
				}
				break;
			case SE_CONTROL_CHANGE:
				next_state = SS_DURATION;
				{
					cnt += 3;
				}
				break;
			case SE_PROGRAM_CHANGE:
				next_state = SS_DURATION;
				{
					cnt += 2;
				}
				break;
			case SE_RESERVED_2BYTE:
				next_state = SS_DURATION;
				{
					cnt += 2;
				}
				break;
			case SE_PITCH_BEND:
				next_state = SS_DURATION;
				{
					cnt += 3;
				}
				break;
			case SE_SYSTEM_EXCLUSIVE:
				next_state = SS_DURATION;
				{
					if (pAddr1[cnt] == SE_SYSTEM_EXCLUSIVE)
					{
						cnt++;
						u32_t len;
						u32_t info_size = calc_variable_size(&pAddr1[cnt], len);
						cnt += (len + info_size);
					}
					else if (pAddr1[cnt] == SE_EOS_NOP)
					{
						if (pAddr1[cnt + 1] == 0x2F)
						{
							cnt += MA_3::EOS_SIZE;				// EOS
							bEOS = true;
						}
						else
						{
							cnt += MA_3::NOP_SIZE;				// NOP
							nNOP++;
						}
					}
					else
					{
						cnt++;
					}
				}
				break;
			default:
				return false;									// Error
			}
			break;
		case SS_GATETIME:
			next_state = SS_DURATION;
			{
				u32_t len;										// For Variable Size
				last_gatetime = calc_variable_size(&pAddr1[cnt], len);
				cnt += len;

				nNOP = 0;
			}
			break;
		default:
			return false;										// Error
		}
	}

	const u32_t nop_duration = 1;								// NOP's Duration [byte]
	const u32_t eos_duration = 1;								// EOS's Duration [byte]

	u32_t reduce_size = 0;
	if (bEOS)
	{
		reduce_size = ((nNOP * (nop_duration + MA_3::NOP_SIZE)) + (eos_duration + MA_3::EOS_SIZE) + MA_3::CRC_SIZE);
	}
	else
	{
		reduce_size = (nNOP * (nop_duration + MA_3::NOP_SIZE) + MA_3::CRC_SIZE);
	}

	// (2) rSrc2_ Analysis
	//
	const u8_t* pAddr2 = rSrc2_.data_ptr();
	cnt = 0;

	if (!check_chunk("MMMD", &pAddr2[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	cnt += MA_3::CHUNK_DATA_SIZE;

	if (!check_chunk("CNTI", &pAddr2[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(&pAddr2[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("OPDA", &pAddr2[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	size = calc_size(&pAddr2[cnt], MA_3::CHUNK_DATA_SIZE);
	cnt += (MA_3::CHUNK_DATA_SIZE + size);

	if (!check_chunk("MTR*", &pAddr2[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	cnt += MA_3::CHUNK_DATA_SIZE;
	cnt++;														// Format Type
	cnt++;														// Sequence Type
	cnt++;														// Timebase of Duration
	cnt++;														// Timebase of Gatetime
	cnt += MA_3::CHANNELS;										// Channel Status

	if (check_chunk("MspI", &pAddr2[cnt]))
	{
		cnt += MA_3::CHUNK_HEAD_SIZE;
		size = calc_size(&pAddr2[cnt], MA_3::CHUNK_DATA_SIZE);
		cnt += (MA_3::CHUNK_DATA_SIZE + size);
	}

	if (check_chunk("Mtsu", &pAddr2[cnt]))
	{
		cnt += MA_3::CHUNK_HEAD_SIZE;
		size = calc_size(&pAddr2[cnt], MA_3::CHUNK_DATA_SIZE);
		cnt += (MA_3::CHUNK_DATA_SIZE + size);
	}

	if (!check_chunk("Mtsq", &pAddr2[cnt])) return false;
	cnt += MA_3::CHUNK_HEAD_SIZE;
	cnt += MA_3::CHUNK_DATA_SIZE;

	while (true)
	{
		if ((pAddr2[cnt] & 0xF0) != SE_NOTE_VELOCITY)			// Search First Note with Velocity.
		{
			cnt++;
			continue;
		}
		break;
	}

	u8_t gap_duration[4];										// Duration of Gap.
	u32_t gap_duration_len;
	make_variable_size_array((last_gatetime + gap_), gap_duration, gap_duration_len);

	// (3) rSrc1_ Header + rSrc1_ Sequence Data + Duration of Gap + rSrc2_ Sequence Data
	//
	const u32_t src1_size = (rSrc1_.size() - reduce_size);
	const u32_t src2_size = (rSrc2_.size() - cnt);
	const u32_t dst_size = (src1_size + gap_duration_len + src2_size);
	rDst_.create(dst_size);

	u8_t j = 0;
	for (u32_t i = 0; i < rDst_.size(); i++)
	{
		if (i < src1_size)
		{
			rDst_[i] = rSrc1_.at(i);							// rSrc1_ All Data
		}
		else if (i < (src1_size + gap_duration_len))
		{
			rDst_[i] = gap_duration[j++];						// Duration of Gap
		}
		else
		{
			rDst_[i] = rSrc2_.at(cnt++);						// rSrc2_ Sequence Data
		}
	}

	// (4) Data Fix
	//
	file_size += (src2_size + gap_duration_len - reduce_size);
	make_size_array(file_size, MA_3::CHUNK_DATA_SIZE, &rDst_[file_size_pos]);

	score_size += (src2_size + gap_duration_len - reduce_size);
	make_size_array(score_size, MA_3::CHUNK_DATA_SIZE, &rDst_[score_size_pos]);

	sequence_size += (src2_size + gap_duration_len - reduce_size);
	make_size_array(sequence_size, MA_3::CHUNK_DATA_SIZE, &rDst_[sequence_size_pos]);

	return fix_crc16(rDst_);
}

//------------------------------------------------------------------------------------------------------//
// End of File
//------------------------------------------------------------------------------------------------------//
