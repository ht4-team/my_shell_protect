#include "Capstone.h"
#include "windows.h"
#include "iostream"
#include "CombatShell/CombatShell.h"
#include <time.h>

using std::cout; using std::endl;

extern FILE* fpVmFile;
extern _VmNode* g_Vm;
extern char* g_dataHlpers;
extern DWORD64 g_dataoffset;
bool g_DebugMode = false;

Capstone::Capstone()
{
	srand(time(0));
}

Capstone::~Capstone()
{

}

// 初始化反汇编引擎
void Capstone::InitCapstone()
{
	// 配置堆空间的回调函数
	OptMem.free = free;
	OptMem.calloc = calloc;
	OptMem.malloc = malloc;
	OptMem.realloc = realloc;
	OptMem.vsnprintf = (cs_vsnprintf_t)vsprintf_s;
	// 注册堆空间管理组函数
	cs_option(NULL, CS_OPT_MEM, (size_t)&OptMem);
	// 打开一个句柄
#ifdef _WIN64
	cs_open(CS_ARCH_X86, CS_MODE_64, &Handle);
#else
	cs_open(CS_ARCH_X86, CS_MODE_32, &Handle);
#endif
}

// 反汇编信息输出
void Capstone::ShowAssembly(const void* pAddr, int nLen)
{
	this->InitCapstone();
	// 接收OpCode大小 最大16保存机器指令			
	BYTE* pOpCode = (BYTE *)malloc(nLen * 16);
	memset(pOpCode, 0, (sizeof(BYTE) * 16 * nLen));
	SIZE_T read = 0;
	// 反汇编指定条数的语句
	// 用来读取指令位置内存的缓冲区信息
	cs_insn* ins = nullptr;

	// 读取指定长度的内存空间
	SIZE_T dwCount = 0;
	ReadProcessMemory(NULL, pAddr, pOpCode, nLen * 16, &dwCount);

	int count = cs_disasm(Handle, (uint8_t*)pOpCode, nLen * 16, (uint64_t)pAddr, 0, &ins);

	for (int i = 0; i < nLen; ++i)
	{
		printf("%08X\t", ins[i].address);
		for (uint16_t j = 0; j < 16; ++j)
		{
			if (j < ins[i].size)
				printf("%02X", ins[i].bytes[j]);
			else
				printf(" ");
		}
		printf("\t");
		printf("%s  ", ins[i].mnemonic);
		cout << ins[i].op_str << endl;
	}
	printf("\n");
	// 释放动态分配的空间
	delete[] pOpCode;
	cs_free(ins, count);
}

int Capstone::AnalyencodeVmHlper(cs_insn* ins, unsigned int rankey)
{
	/*
		1. 如果支持全指令，可以不用记录偏移
		2. 如果支持部分指令，需要记录偏移，保证没有进行VMcode加密代码也可以执行
		目前测试代码是全指令，因为都可以识别
		但是用到其它代码段上需要记录偏移
	*/
	/*
	思考了两种方案：
		1. 线性-递归
		2. 精准-模糊
		方案用线性-模糊
		对call-jmp等跳转函数不进行VM加密，模糊含义只对指令判断
		比如mov edi,edi  无论mov后面根什么如果是mov就同一套handler挂钩处理
	*/
	// 目前支持加密的指令
	if (
		0 == _stricmp(ins->mnemonic, "mov")		||
		0 == _stricmp(ins->mnemonic, "push")	||
		0 == _stricmp(ins->mnemonic, "jmp")		||
		0 == _stricmp(ins->mnemonic, "call")	||
		0 == _stricmp(ins->mnemonic, "lea")		||
		0 == _stricmp(ins->mnemonic, "sub")		||
		0 == _stricmp(ins->mnemonic, "add")		||
		0 == _stricmp(ins->mnemonic, "ret")		||
		0 == _stricmp(ins->mnemonic, "xor")		||
		0 == _stricmp(ins->mnemonic, "nop")		||
		0 == _stricmp(ins->mnemonic, "test")	||
		0 == _stricmp(ins->mnemonic, "je")		||
		0 == _stricmp(ins->mnemonic, "jne")		||
		0 == _stricmp(ins->mnemonic, "pop")		||
		0 == _stricmp(ins->mnemonic, "cmp")
		)
	{
		for (size_t i = 0; i < ins->size; i++)
		{
			//if (*((char *)ins->address) == (unsigned char)('\x00'))
			//	continue;
			*((char *)ins->address) ^= rankey;
			ins->address += 1;
		}
		return 1;
	}
	return 0;
}

// 指令分析-数据保存
void Capstone::AnalyOpcodeHlper(const void* pAddr, int nLen)
{
	if (!g_dataHlpers || (nullptr == g_dataHlpers))
		return;
	g_Vm->data = (ArrayHlerp *)g_dataHlpers;
	memset(g_Vm->data, 0, nLen * sizeof(ArrayHlerp));

	this->InitCapstone();
	if (!pAddr && !nLen)
		return;
	BYTE* pOpCode = (BYTE *)malloc(nLen * 16);
	if (!pOpCode)
		return;
	memset(pOpCode, 0, (sizeof(BYTE) * 16 * nLen));
	SIZE_T read = 0;
	cs_insn* ins = nullptr;
	SIZE_T dwCount = 0;
	memcpy(pOpCode, pAddr, nLen * 16);
	int count = cs_disasm(Handle, (uint8_t*)pOpCode, nLen * 16, (uint64_t)pAddr, 0, &ins);
	int vmflag = 1;
	unsigned short randnumber = 0;

	for (int i = 0; i < nLen; ++i)
	{
		g_Vm->data->startoffset = ins[i].address - (uint64_t)pAddr;
		randnumber = rand() % 0xff;
		g_Vm->data->xorKey = randnumber;
		g_Vm->data->bytesize = ins[i].size;

		// Save original bytes before encryption for debug
		unsigned char origBytes[16] = { 0 };
		if (g_DebugMode && ins[i].size <= 16) {
			memcpy(origBytes, (void*)ins[i].address, ins[i].size);
		}

		if (AnalyencodeVmHlper(&ins[i], randnumber))
		{
			g_Vm->data->encodeflag = 1;
		}
		else
		{
			g_Vm->data->encodeflag = 0;
		}
		strcpy(g_Vm->data->mnemonic, ins[i].mnemonic);

		// Print per-instruction debug: original asm → encrypted bytes
		if (g_DebugMode) {
			// Original hex bytes
			char hexOrig[64] = { 0 };
			char hexEnc[64] = { 0 };
			int pos = 0;
			for (uint16_t j = 0; j < ins[i].size && j < 16; ++j)
				pos += sprintf(hexOrig + pos, "%02X ", origBytes[j]);

			// Encrypted bytes (address was advanced by AnalyencodeVmHlper, recompute)
			unsigned char* encAddr = (unsigned char*)pAddr + g_Vm->data->startoffset;
			pos = 0;
			for (uint16_t j = 0; j < ins[i].size && j < 16; ++j)
				pos += sprintf(hexEnc + pos, "%02X ", encAddr[j]);

			fprintf(stderr, "[debug]   [%2d] 0x%04X  %-24s  %-8s %-28s  enc=%u  xor=0x%02X\n",
				i, g_Vm->data->startoffset,
				hexOrig,
				ins[i].mnemonic, ins[i].op_str,
				g_Vm->data->encodeflag, g_Vm->data->xorKey);
			if (g_Vm->data->encodeflag) {
				fprintf(stderr, "                        -> %-24s\n", hexEnc);
			}
		}

		g_Vm->data++;
	}
	g_Vm->Hlperdataoffset = g_dataoffset;

	if (g_DebugMode) {
		fprintf(stderr, "[debug] Capstone: %d instructions analyzed for VM encryption\n", nLen);
		fflush(stderr);
	}

	printf("\n");
	// 释放动态分配的空间
	delete[] pOpCode;
	cs_free(ins, count);
}