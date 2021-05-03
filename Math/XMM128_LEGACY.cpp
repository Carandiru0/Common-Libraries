#include "pch.h"
#pragma message("WARNING: XMM128.cpp is for legacy usage only, not for new development - deprecated")

#include "XMM128.h"
#include <bitset>

class InstructionSet
{
	// forward declarations
	class InstructionSet_Internal;

public:
	// getters
	static bool const SSE3(void) { return( CPU_Rep.f_1_ECX_[0]); }
	static bool const PCLMULQDQ(void) { return( CPU_Rep.f_1_ECX_[1]); }
	static bool const MONITOR(void) { return( CPU_Rep.f_1_ECX_[3]); }
	static bool const SSSE3(void) { return( CPU_Rep.f_1_ECX_[9]); }
	static bool const FMA(void) { return( CPU_Rep.f_1_ECX_[12]); }
	static bool const CMPXCHG16B(void) { return( CPU_Rep.f_1_ECX_[13]); }
	static bool const SSE41(void) { return( CPU_Rep.f_1_ECX_[19]); }
	static bool const SSE42(void) { return( CPU_Rep.f_1_ECX_[20]); }
	static bool const MOVBE(void) { return( CPU_Rep.f_1_ECX_[22]); }
	static bool const POPCNT(void) { return( CPU_Rep.f_1_ECX_[23]); }
	static bool const AES(void) { return( CPU_Rep.f_1_ECX_[25]); }
	static bool const XSAVE(void) { return( CPU_Rep.f_1_ECX_[26]); }
	static bool const OSXSAVE(void) { return( CPU_Rep.f_1_ECX_[27]); }
	static bool const AVX(void) { return( CPU_Rep.f_1_ECX_[28]); }
	static bool const F16C(void) { return( CPU_Rep.f_1_ECX_[29]); }
	static bool const RDRAND(void) { return( CPU_Rep.f_1_ECX_[30]); }

	static bool const MSR(void) { return( CPU_Rep.f_1_EDX_[5]); }
	static bool const CX8(void) { return( CPU_Rep.f_1_EDX_[8]); }
	static bool const SEP(void) { return( CPU_Rep.f_1_EDX_[11]); }
	static bool const CMOV(void) { return( CPU_Rep.f_1_EDX_[15]); }
	static bool const CLFSH(void) { return( CPU_Rep.f_1_EDX_[19]); }
	static bool const MMX(void) { return( CPU_Rep.f_1_EDX_[23]); }
	static bool const FXSR(void) { return( CPU_Rep.f_1_EDX_[24]); }
	static bool const SSE(void) { return( CPU_Rep.f_1_EDX_[25]); }
	static bool const SSE2(void) { return( CPU_Rep.f_1_EDX_[26]); }

	static bool const FSGSBASE(void) { return( CPU_Rep.f_7_EBX_[0]); }
	static bool const BMI1(void) { return( CPU_Rep.f_7_EBX_[3]); }
	static bool const AVX2(void) { return( CPU_Rep.f_7_EBX_[5]); }
	static bool const BMI2(void) { return( CPU_Rep.f_7_EBX_[8]); }
	static bool const ERMS(void) { return( CPU_Rep.f_7_EBX_[9]); }
	static bool const INVPCID(void) { return( CPU_Rep.f_7_EBX_[10]); }
	static bool const AVX512F(void) { return( CPU_Rep.f_7_EBX_[16]); }
	static bool const RDSEED(void) { return( CPU_Rep.f_7_EBX_[18]); }
	static bool const ADX(void) { return( CPU_Rep.f_7_EBX_[19]); }
	static bool const AVX512PF(void) { return( CPU_Rep.f_7_EBX_[26]); }
	static bool const AVX512ER(void) { return( CPU_Rep.f_7_EBX_[27]); }
	static bool const AVX512CD(void) { return( CPU_Rep.f_7_EBX_[28]); }
	static bool const SHA(void) { return( CPU_Rep.f_7_EBX_[29]); }

	static bool const PREFETCHWT1(void) { return( CPU_Rep.f_7_ECX_[0]); }

	static bool const LAHF(void) { return( CPU_Rep.f_81_ECX_[0]); }

private:
	static const InstructionSet_Internal CPU_Rep;

	class InstructionSet_Internal
	{
	public:
		InstructionSet_Internal()
			: nIds_{ 0 },
			nExIds_{ 0 },
			f_1_ECX_{ 0 },
			f_1_EDX_{ 0 },
			f_7_EBX_{ 0 },
			f_7_ECX_{ 0 },
			f_81_ECX_{ 0 },
			f_81_EDX_{ 0 },
			data_{},
			extdata_{}
		{
			//int cpuInfo[4] = {-1};
			std::array<int, 4> cpui;

			// Calling __cpuid with 0x0 as the function_id argument
			// gets the number of the highest valid function ID.
			__cpuid(cpui.data(), 0);
			nIds_ = cpui[0];

			for (int i = 0; i <= nIds_; ++i)
			{
				__cpuidex(cpui.data(), i, 0);
				data_.push_back(cpui);
			}

			// load bitset with flags for function 0x00000001
			if (nIds_ >= 1)
			{
				f_1_ECX_ = data_[1][2];
				f_1_EDX_ = data_[1][3];
			}

			// load bitset with flags for function 0x00000007
			if (nIds_ >= 7)
			{
				f_7_EBX_ = data_[7][1];
				f_7_ECX_ = data_[7][2];
			}

			// Calling __cpuid with 0x80000000 as the function_id argument
			// gets the number of the highest valid extended ID.
			__cpuid(cpui.data(), 0x80000000);
			nExIds_ = cpui[0];

			for (int i = 0x80000000; i <= nExIds_; ++i)
			{
				__cpuidex(cpui.data(), i, 0);
				extdata_.push_back(cpui);
			}

			// load bitset with flags for function 0x80000001
			if (nExIds_ >= 0x80000001)
			{
				f_81_ECX_ = extdata_[1][2];
				f_81_EDX_ = extdata_[1][3];
			}
		};

		int nIds_;
		int nExIds_;

		std::bitset<32> f_1_ECX_;
		std::bitset<32> f_1_EDX_;
		std::bitset<32> f_7_EBX_;
		std::bitset<32> f_7_ECX_;
		std::bitset<32> f_81_ECX_;
		std::bitset<32> f_81_EDX_;
		std::vector<std::array<int, 4>> data_;
		std::vector<std::array<int, 4>> extdata_;
	};
};

// Initialize static member data
const InstructionSet::InstructionSet_Internal InstructionSet::CPU_Rep;

bool const XMM128::m_bSSE41 = InstructionSet::SSE41();
bool const XMM256::m_bAVX2 = InstructionSet::AVX2();