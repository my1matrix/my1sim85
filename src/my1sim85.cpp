//------------------------------------------------------------------------------
#include "my1sim85.hpp"
//------------------------------------------------------------------------------
static const char I8255_NAME[]="8255";
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
my1Memory::my1Memory(int aStart, int aSize)
{
	mStart = aStart;
	mSize = aSize;
	mSpace = new abyte[mSize];
}
//------------------------------------------------------------------------------
my1Memory::~my1Memory()
{
	delete mSpace;
}
//------------------------------------------------------------------------------
int my1Memory::GetStart(void)
{
	return mStart;
}
//------------------------------------------------------------------------------
int my1Memory::GetSize(void)
{
	return mSize;
}
//------------------------------------------------------------------------------
abyte my1Memory::ReadData(aword anAddress)
{
	if(anAddress<mStart||anAddress>=mStart+mSize)
		return 0x0;
	return mSpace[anAddress-mStart];
}
//------------------------------------------------------------------------------
bool my1Memory::WriteData(aword anAddress, abyte aData)
{
	if(anAddress<mStart||anAddress>=mStart+mSize)
		return false;
	mSpace[anAddress-mStart] = aData;
	return true;
}
//------------------------------------------------------------------------------
bool my1Memory::IsSelected(aword anAddress)
{
	if(anAddress<mStart||anAddress>=mStart+mSize)
		return false;
	return true;
}
//------------------------------------------------------------------------------
bool my1Memory::IsWithin(int aStart, int aSize)
{
	bool cFlag = false;
	if((aStart>=mStart&&aStart<mStart+mSize)||
		(mStart>=aStart&&mStart<aStart+aSize))
		cFlag = true;
	return cFlag;
}
//------------------------------------------------------------------------------
bool my1Memory::IsWithin(my1Memory* aMemory)
{
	return this->IsWithin(aMemory->mStart,aMemory->mSize);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
my1Device::my1Device(char *aName, int aStart, int aSize)
	: my1Memory(aStart, aSize)
{
	int cCount = 0;
	if(aName)
	{
		while(aName[cCount]&&cCount<MAX_DEVNAME_CHAR-1)
		{
			mName[cCount] = aName[cCount];
			cCount++;
		}
	}
	mName[cCount] = 0x0;
}
//------------------------------------------------------------------------------
abyte my1Device::ReadDevice(aword anAddress)
{
	return ReadData(anAddress);
}
//------------------------------------------------------------------------------
bool my1Device::WriteDevice(aword anAddress, abyte aData)
{
	return WriteData(anAddress, aData);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
my1Sim8255::my1Sim8255(int aStart)
	: my1Device((char*)I8255_NAME, aStart, 4)
{
	// by default config is random?
}
//------------------------------------------------------------------------------
abyte my1Sim8255::GetData(int anIndex)
{
	if(mIsInput[anIndex])
		return 0x00;
	return mSpace[anIndex];
}
//------------------------------------------------------------------------------
void my1Sim8255::PutData(int anIndex, abyte aData, abyte aMask=0xff)
{
	if(!mIsInput[anIndex])
		return;
	mSpace[anIndex] = (aData & aMask) | (mSpace[anIndex] & ~aMask);
}
//------------------------------------------------------------------------------
abyte my1Sim8255::ReadDevice(aword anAddress)
{
	return ReadData(anAddress);
}
//------------------------------------------------------------------------------
bool my1Sim8255::WriteDevice(aword anAddress, abyte aData)
{
	bool cFlag = true;
	int cIndex = anAddress-mStart;
	switch(cIndex)
	{
		case I8255_PORTA:
		case I8255_PORTB:
			if(!mIsInput[cIndex])
				mSpace[cIndex] = aData;
			break;
		case I8255_PORTC:
			if(!mIsInput[I8255_PORTC_U])
				mSpace[cIndex] = (aData & 0xF0) | (mSpace[cIndex] & 0x0F);
			if(!mIsInput[I8255_PORTC_L])
				mSpace[cIndex] = (aData & 0x0F) | (mSpace[cIndex] & 0xF0);
			break;
		case I8255_CONTROL:
			if(aData&0x80)
			{
				// config data - always assume mode 0!
				if(aData&0x10) mIsInput[I8255_PORTA] = true;
				else mIsInput[I8255_PORTA] = false;
				if(aData&0x02) mIsInput[I8255_PORTB] = true;
				else mIsInput[I8255_PORTB] = false;
				if(aData&0x08) mIsInput[I8255_PORTC_U] = true;
				else mIsInput[I8255_PORTC_U] = false;
				if(aData&0x01) mIsInput[I8255_PORTC_L] = true;
				else mIsInput[I8255_PORTC_L] = false;
			}
			else
			{
				// bsr mode!
				abyte cCount = aData&0x0e;
				abyte cData = aData&0x01;
				cData <<= cCount;
				if(cCount<4&&!mIsInput[I8255_PORTC_L]||cCount>3&&!mIsInput[I8255_PORTC_U])
				{
					if(cData)
						mSpace[I8255_PORTC] |= cData;
					else
						mSpace[I8255_PORTC] &= ~cData;
				}
			}
			break;
		default:
			cFlag = false;
	}
	return cFlag;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
my1Sim85::my1Sim85()
{
	mHalted = false;
	mIEnabled = false;
	mIntrReg = 0x00; // not sure!
	mPCounter = 0x0000;
	mMemCount = 0;
	for(int cLoop=0;cLoop<MAX_MEMCOUNT;cLoop++)
		mMems[cLoop] = 0x0;
	mDevCount = 0;
	for(int cLoop=0;cLoop<MAX_DEVCOUNT;cLoop++)
		mDevs[cLoop] = 0x0;
	mCodCount = 0;
	mCodexList = 0x0;
	mStateExec = 0;
	mCodexExec = 0x0;
	DoDelay = 0x0;
}
//------------------------------------------------------------------------------
my1Sim85::~my1Sim85()
{
	this->ResetCodex();
}
//------------------------------------------------------------------------------
int my1Sim85::GetStateExec(void)
{
	return mStateExec;
}
//------------------------------------------------------------------------------
int my1Sim85::GetCodexLine(void)
{
	int cLine = 0;
	if(mCodexExec)
		cLine = mCodexExec->line;
	return cLine;
}
//------------------------------------------------------------------------------
bool my1Sim85::AddMemory(my1Memory* aMemory)
{
	// check existing memory space
	for(int cLoop=0;cLoop<mMemCount;cLoop++)
	{
		if(aMemory->IsWithin(mMems[cLoop]))
			return false;
	}
	// okay... insert!
	mMems[mMemCount++] = aMemory;
	return true;
}
//------------------------------------------------------------------------------
bool my1Sim85::AddDevice(my1Device* aDevice)
{
	// check existing i/o space
	for(int cLoop=0;cLoop<mDevCount;cLoop++)
	{
		if(aDevice->IsWithin(mDevs[cLoop]))
			return false;
	}
	// okay... insert!
	mDevs[mDevCount++] = aDevice;
	return true;
}
//------------------------------------------------------------------------------
my1Memory* my1Sim85::GetMemory(int aStart)
{
	my1Memory* cTemp = 0x0;
	for(int cLoop=0;cLoop<mMemCount;cLoop++)
	{
		if(aStart==mMems[cLoop]->GetStart())
		{
			cTemp = mMems[cLoop];
			break;
		}
	}
	return cTemp;
}
//------------------------------------------------------------------------------
my1Device* my1Sim85::GetDevice(int aStart)
{
	my1Device* cTemp = 0x0;
	for(int cLoop=0;cLoop<mDevCount;cLoop++)
	{
		if(aStart==mDevs[cLoop]->GetStart())
		{
			cTemp = mDevs[cLoop];
			break;
		}
	}
	return cTemp;
}
//------------------------------------------------------------------------------
bool my1Sim85::ReadMemory(aword anAddress, abyte& rData)
{
	bool cFlag = false;
	for(int cLoop=0;cLoop<mMemCount&&!cFlag;cLoop++)
	{
		cFlag = mMems[cLoop]->IsSelected(anAddress);
		rData = mMems[cLoop]->ReadData(anAddress);
	}
	return cFlag;
}
//------------------------------------------------------------------------------
bool my1Sim85::WriteMemory(aword anAddress, abyte aData)
{
	bool cFlag = false;
	for(int cLoop=0;cLoop<mMemCount&&!cFlag;cLoop++)
	{
		cFlag = mMems[cLoop]->WriteData(anAddress, aData);
	}
	return cFlag;
}
//------------------------------------------------------------------------------
bool my1Sim85::ReadDevice(abyte anAddress, abyte& rData)
{
	bool cFlag = false;
	for(int cLoop=0;cLoop<mDevCount&&!cFlag;cLoop++)
	{
		cFlag = mDevs[cLoop]->IsSelected(anAddress);
		rData = mDevs[cLoop]->ReadDevice(anAddress);
	}
	return cFlag;
}
//------------------------------------------------------------------------------
bool my1Sim85::WriteDevice(abyte anAddress, abyte aData)
{
	bool cFlag = false;
	for(int cLoop=0;cLoop<mDevCount&&!cFlag;cLoop++)
	{
		cFlag = mDevs[cLoop]->WriteDevice(anAddress, aData);
	}
	return cFlag;
}
//------------------------------------------------------------------------------
bool my1Sim85::ResetCodex(void)
{
	bool cFlag = true;
	while(mCodexList)
	{
		CODEX* pCodex = mCodexList;
		mCodexList = mCodexList->next;
		free_codex(pCodex);
		mCodCount--;
	}
	if(mCodCount)
	{
		mCodCount = 0;
		cFlag = false;
	}
	mStateExec = 0;
	mCodexExec = 0x0;
	return cFlag;
}
//------------------------------------------------------------------------------
bool my1Sim85::LoadCodex(char *aFilename)
{
	// initialize main data structure
	STUFFS things;
	initialize(&things);
	things.afile = aFilename;
	// false do-while loop - for error exits
	do
	{
		// first pass - build label db
		things.pass = EXEC_PASS_1;
		if(process_asmfile(&things)>0)
			break;
		// second pass - arrange code
		things.pass = EXEC_PASS_2;
		if(process_asmfile(&things)>0)
			break;
		// copy codex to local
		this->ResetCodex();
		this->LoadStuff(&things);
	}
	while(0);
	// clean-up main data structure
	cleanup(&things);
	return things.errc; // still maintained in structure
}
//------------------------------------------------------------------------------
void my1Sim85::LoadStuff(STUFFS* pstuffs)
{
	CODEX *pcodex, *tcodex, *ccodex;
	pcodex = pstuffs->pcodex;
	ccodex = mCodexList; // should be 0x0
	if(pcodex)
		pstuffs->addr = pcodex->addr;
	while(pcodex)
	{
		// create new codex to save in class storage
		tcodex = clone_codex(pcodex);
		if(ccodex)
			ccodex->next = tcodex;
		else
			mCodexList = tcodex;
		ccodex = tcodex;
		mCodCount++;
		/* check for error/discontinuity */
		if(pcodex->addr<pstuffs->addr)
		{
			pstuffs->errc++;
		}
		else if(pcodex->addr>pstuffs->addr)
		{
			pstuffs->addr = pcodex->addr;
		}
		/* fill memory with codex data */
		for(int cLoop=0;cLoop<pcodex->size;cLoop++)
		{
			if(!this->WriteMemory(pstuffs->addr++,pcodex->data[cLoop]))
				pstuffs->errc++;
		}
		pcodex = pcodex->next;
	}
}
//------------------------------------------------------------------------------
bool my1Sim85::GetCodex(aword anAddress)
{
	CODEX *pcodex = mCodexList;
	mStateExec = 0;
	mCodexExec = 0x0;
	while(pcodex)
	{
		if(pcodex->addr==anAddress)
		{
			if(pcodex->line) // abort if NOT an instruction!
				mCodexExec = pcodex;
			break;
		}
		pcodex = pcodex->next;
	}
	return mCodexExec ? true : false;
}
//------------------------------------------------------------------------------
bool my1Sim85::ExeDelay(void)
{
	bool cFlag = false;
	if(DoDelay&&mStateExec)
	{
		(*DoDelay)((void*)this);
		cFlag = true;
	}
	return cFlag;
}
//------------------------------------------------------------------------------
bool my1Sim85::ExeCodex(void)
{
	CODEX *pcodex = mCodexExec;
	if(!pcodex)
		return false;
	// update program counter?
	mPCounter += pcodex->size; // should check if code memory is valid?
	// check opcode!
	if((pcodex->data[0]&0xC0)==0x40) // MOV group
	{
		if((pcodex->data[0]&0x3F)==0x36) // check for HALT!
		{
			mStateExec = 5;
			this->ExeDelay();
			this->mHalted = true;
		}
		else
		{
			mStateExec = 4;
			if((pcodex->data[0]&0x38)==0x30||(pcodex->data[0]&0x07)==0x06)
				mStateExec += 3;
			this->ExeDelay();
			this->ExecMOV((pcodex->data[0]&0x38)>>3,(pcodex->data[0]&0x07));
		}
	}
	else if((pcodex->data[0]&0xC0)==0x80) // ALU group
	{
		mStateExec = 4;
		if((pcodex->data[0]&0x07)==0x06)
			mStateExec+=3;
		this->ExeDelay();
		this->ExecALU((pcodex->data[0]&0x38)>>3,(pcodex->data[0]&0x07));
	}
	else if((pcodex->data[0]&0xC0)==0x00) // data proc group
	{
		if((pcodex->data[0]&0x07)==0x06) // mvi
		{
			mStateExec = 7;
			this->ExeDelay();
			this->ExecMOVi((pcodex->data[0]&0x38)>>3,pcodex->data[1]);
		}
		else if((pcodex->data[0]&0x0F)==0x01) // lxi
		{
			aword *pdata = (aword*) &pcodex->data[1];
			mStateExec = 10;
			this->ExeDelay();
			this->ExecLXI((pcodex->data[0]&0x30)>>4,*pdata);
		}
		else if((pcodex->data[0]&0x0F)==0x09) // dad
		{
			mStateExec = 10;
			this->ExeDelay();
			this->ExecDAD((pcodex->data[0]&0x30)>>4);
		}
		else if((pcodex->data[0]&0x27)==0x02) // stax/ldax
		{
			mStateExec = 7;
			this->ExeDelay();
			this->ExecSTAXLDAX((pcodex->data[0]&0x18)>>3);
		}
		else if((pcodex->data[0]&0x37)==0x22) // shld/lhld
		{
			aword *pdata = (aword*) &pcodex->data[1];
			mStateExec = 16;
			this->ExeDelay();
			this->ExecSLHLD((pcodex->data[0]&0x08)>>3,*pdata);
		}
		else if((pcodex->data[0]&0x37)==0x32) // sta/lda
		{
			aword *pdata = (aword*) &pcodex->data[1];
			mStateExec = 13;
			this->ExeDelay();
			this->ExecSTALDA((pcodex->data[0]&0x08)>>3,*pdata);
		}
		else if((pcodex->data[0]&0x07)==0x03) // inx/dcx
		{
			mStateExec = 6;
			this->ExeDelay();
			this->ExecINXDCX((pcodex->data[0]&0x38)>>3);
		}
		else if((pcodex->data[0]&0x06)==0x04) // inr/dcr
		{
			mStateExec = 4;
			if((pcodex->data[0]&0x38)==0x30)
				mStateExec+=6;
			this->ExeDelay();
			this->ExecINRDCR((pcodex->data[0]&0x38)>>3,(pcodex->data[0]&0x01));
		}
		else if((pcodex->data[0]&0x27)==0x07) // rotates
		{
			mStateExec = 4;
			this->ExeDelay();
			this->ExecROTATE((pcodex->data[0]&0x18)>>3);
		}
		else if((pcodex->data[0]&0x27)==0x17) // misc - daa, cma, stc, cmc
		{
			mStateExec = 4;
			this->ExeDelay();
			this->ExecDCSC((pcodex->data[0]&0x18)>>3);
		}
		else if((pcodex->data[0]&0x2F)==0x20) // rim, sim
		{
			mStateExec = 4;
			this->ExeDelay();
			this->ExecRSIM((pcodex->data[0]&0x10)>>4);
		}
		else if(pcodex->data[0]==0x00) // nop
		{
			mStateExec = 4;
			this->ExeDelay();
			// NOP!
		}
		else // unspecified instructions (0x08, 0x10, 0x18, 0x28, 0x38)
		{
			return false;
		}
	}
	else if((pcodex->data[0]&0xC0)==0xC0) // control group
	{
		if((pcodex->data[0]&0x07)==0x06) // alu_i group
		{
			mStateExec = 7;
			this->ExeDelay();
			this->ExecALUi((pcodex->data[0]&0x38)>>3,pcodex->data[1]);
		}
		else if((pcodex->data[0]&0x0b)==0x01) // push/pop
		{
			mStateExec = 10;
			if((pcodex->data[0]&0x04))
				mStateExec += 2;
			this->ExeDelay();
			if((pcodex->data[0]&0x04))
				this->ExecPUSH((pcodex->data[0]&0x30)>>4);
			else
				this->ExecPOP((pcodex->data[0]&0x30)>>4);
		}
		else if((pcodex->data[0]&0x3b)==0x09) // call/ret
		{
			aword *pdata = (aword*) &pcodex->data[1];
			mStateExec = 10;
			if((pcodex->data[0]&0x04))
				mStateExec += 8;
			this->ExeDelay();
			if((pcodex->data[0]&0x04))
				this->ExecCALL(*pdata);
			else
				this->ExecRET();
		}
		else if((pcodex->data[0]&0x07)==0x04) // call conditional
		{
			aword *pdata = (aword*) &pcodex->data[1];
			bool cDoThis = this->ChkFlag((pcodex->data[0]&0x38)>>3);
			mStateExec = 9;
			if(cDoThis)
				mStateExec += 9;
			this->ExeDelay();
			if(cDoThis)
				this->ExecCALL(*pdata);
		}
		else if((pcodex->data[0]&0x07)==0x00) // return conditional
		{
			bool cDoThis = this->ChkFlag((pcodex->data[0]&0x38)>>3);
			mStateExec = 6;
			if(cDoThis)
				mStateExec += 6;
			this->ExeDelay();
			if(cDoThis)
				this->ExecRET();
		}
		else if((pcodex->data[0]&0x07)==0x07) // rst n
		{
			mStateExec = 12;
			this->ExeDelay();
			this->ExecRSTn((pcodex->data[0]&0x38)>>3);
		}
		else if((pcodex->data[0]&0x3F)==0x03) // jmp
		{
			aword *pdata = (aword*) &pcodex->data[1];
			mStateExec = 10;
			this->ExeDelay();
			this->ExecJMP(*pdata);
		}
		else if((pcodex->data[0]&0x07)==0x02) // jump conditional
		{
			aword *pdata = (aword*) &pcodex->data[1];
			bool cDoThis = this->ChkFlag((pcodex->data[0]&0x38)>>3);
			mStateExec = 7;
			if(cDoThis)
				mStateExec += 3;
			this->ExeDelay();
			if(cDoThis)
				this->ExecJMP(*pdata);
		}
		else if((pcodex->data[0]&0x37)==0x13) // out/in
		{
			mStateExec = 10;
			this->ExeDelay();
			this->ExecOUTIN((pcodex->data[0]&0x08)>>3, pcodex->data[1]);
		}
		else if((pcodex->data[0]&0x37)==0x23) // xthl/xchg
		{
			mStateExec = 4;
			if(!(pcodex->data[0]&0x08))
				mStateExec += 16;
			this->ExeDelay();
			this->ExecCHG((pcodex->data[0]&0x08)>>3);
		}
		else // unspecified instructions (0xcb, 0xd9, 0xdd, 0xed, 0xfd)
		{
			return false;
		}
	}
	return true;
}
//------------------------------------------------------------------------------
abyte* my1Sim85::GetReg8(abyte anIndex)
{
	abyte* pReg8 = (abyte*) mRegPairs;
	return (abyte*) pReg8[(anIndex^0x01)];
}
//------------------------------------------------------------------------------
abyte my1Sim85::GetParity(abyte aData)
{
	abyte cOddParity = 0x01;
	for(int cLoop=0;cLoop<8;cLoop++)
	{
		cOddParity ^= (aData&0x01);
		aData >>= 1;
	}
	return (cOddParity << 6);
}
//------------------------------------------------------------------------------
abyte my1Sim85::GetSrcData(abyte src)
{
	abyte cData;
	abyte *pReg8 = &cData;
	switch(src)
	{
		case I8085_REG_M:
			this->ReadMemory(mRegPairs[I8085_RP_HL],cData);
 			break;
		default:
			pReg8 = GetReg8(src);
	}
	return *pReg8;
}
//------------------------------------------------------------------------------
void my1Sim85::PutDstData(abyte dst, abyte aData)
{
	abyte *pReg8;
	switch(dst)
	{
		case I8085_REG_M:
			this->WriteMemory(mRegPairs[I8085_RP_HL],aData);
 			break;
		default:
			pReg8 = GetReg8(src);
			*pReg8 = aData;
	}
}
//------------------------------------------------------------------------------
void my1Sim85::DoStackPush(aword* pData)
{
	abyte *pDataL = (abyte*) pData;
	abyte *pDataH = (abyte*) (pData+1);
	this->WriteMemory(--mSPointer,*pDataH);
	this->WriteMemory(--mSPointer,*pDataL);
}
//------------------------------------------------------------------------------
void my1Sim85::DoStackPop(aword* pData)
{
	abyte *pDataL = (abyte*) pData;
	abyte *pDataH = (abyte*) (pData+1);
	this->ReadMemory(mSPointer++,*pDataL);
	this->ReadMemory(mSPointer++,*pDataH);
}
//------------------------------------------------------------------------------
bool my1Sim85::ChkFlag(abyte sel)
{
	bool cStatus = false;
	abyte cFlag = *GetReg8(I8085_REG_F);
	switch(sel&0x06)
	{
		case 0: // carry
			cStatus = (cFlag&I8085_FLAG_C) ? true : false;
			if(sel&0x01)
				cStatus = !cStatus;
		case 2: // sign
			cStatus = (cFlag&I8085_FLAG_S) ? false : true;
			if(sel&0x01)
				cStatus = !cStatus;
		case 4: // zero
			cStatus = (cFlag&I8085_FLAG_Z) ? true : false;
			if(sel&0x01)
				cStatus = !cStatus;
		case 8: // parity
			cStatus = (cFlag&I8085_FLAG_P) ? true : false;
			if(sel&0x01)
				cStatus = !cStatus;
	}
	return cStatus;
}
//------------------------------------------------------------------------------
void my1Sim85::ExecMOV(abyte dst, abyte src)
{
	this->PutDstData(dst,this->GetSrcData(src));
}
//------------------------------------------------------------------------------
void my1Sim85::ExecMOVi(abyte dst, abyte aData)
{
	this->PutDstData(dst,aData);
}
//------------------------------------------------------------------------------
void my1Sim85::ExecALU(abyte sel, abyte src)
{
	this->ExecALUi(sel,GetSrcData(src));
}
//------------------------------------------------------------------------------
void my1Sim85::ExecALUi(abyte sel, abyte aData)
{
	abyte cFlag, *pReg8 = GetReg8(I8085_REG_A);
	// do the operation!
	if((sel&0x04)&&(sel!=0x07)) // logical excluding cmp!
	{
		cFlag = 0x00; // by default ac reset, cy reset!
		switch(sel&0x03)
		{
			case 0x00:
				(*pReg8) &= aData;
				cFlag = 0x10; // ac set, cy reset!
				break;
			case 0x01:
				(*pReg8) ^= aData;
				break;
			case 0x10:
				(*pReg8) |= aData;
				break;
		}
		*pAccu = cAccu;
	}
	else // arithmetic
	{
		aword cTestX, cTestY;
		if(sel==0x07) // actually a compare, do subtract
			sel = 0x06;
		cTestX = *pReg8;
		cTestY = (*pReg8)&0x0F;
		if(sel&0x02)
		{
			cTestX -= (aword)aData+(sel&0x01);
			cTestY -= (aword)(aData&0x0F)+(sel&0x01);
		}
		else
		{
			cTestX += (aword)aData+(sel&0x01);
			cTestY += (aword)(aData&0x0F)+(sel&0x01);
		}
		aData = (cTestX&0xFF);
		if(!(sel&0x04)) // cmp doesn't update accumulator!
			(*pReg8) = aData;
		// update flag!
		cFlag = 0x00;
		if(cTestX&0x100) // carry
			cFlag |= 0x01;
		if(cTestY&0x10) // aux carry
			cFlag |= 0x10;
	}
	// update flag!
	if(aData==0x00) flag |= 0x40; // zero
	cFlag |= (aData&0x80); // sign flag
	cFlag |= GetParity(aData); // parity flag
	pReg8 = GetReg8(I8085_REG_F);
	*pReg8 = cFlag;
}
//------------------------------------------------------------------------------
void my1Sim85::ExecDAD(abyte sel)
{
	aword *pReg16;
	switch(sel&0x03) // 2-bits selection
	{
		case I8085_RP_BC:
		case I8085_RP_DE:
		case I8085_RP_HL:
			pReg16 = &mRegPairs[sel&0x03];
			break;
		case I8085_RP_SP:
			pReg16 = &mSPointer;
			break;
	}
	mRegPairs[I8085_RP_HL] += *pReg16;
}
//------------------------------------------------------------------------------
void my1Sim85::ExecLXI(abyte sel, aword aData)
{
	aword *pReg16;
	switch(sel&0x03) // 2-bits selection
	{
		case I8085_RP_BC:
		case I8085_RP_DE:
		case I8085_RP_HL:
			pReg16 = &mRegPairs[sel&0x03];
			break;
		case I8085_RP_SP:
			pReg16 = &mSPointer;
			break;
	}
	*pReg16 = aData;
}
//------------------------------------------------------------------------------
void my1Sim85::ExecSTAXLDAX(abyte sel)
{
	this->ExecSTALDA(sel, mRegPairs[(sel&0x02)>>1]);
}
//------------------------------------------------------------------------------
void my1Sim85::ExecSTALDA(abyte sel, aword anAddr)
{
	abyte cData;
	abyte *pReg8 = GetReg8(I8085_REG_A);
	// do the transfer!
	if(sel&0x01)
	{
		this->ReadMemory(anAddr,cData);
		*pReg8 = cData;
	}
	else
	{
		cData = *pReg8;
		this->WriteMemory(anAddr,cData);
	}
}
//------------------------------------------------------------------------------
void my1Sim85::ExecSLHLD(abyte sel, aword anAddr)
{
	abyte cData, *pReg8;
	// do the transfer!
	if(sel&0x01)
	{
		this->ReadMemory(anAddr++,cData);
		pReg8 = GetReg8(I8085_REG_L);
		*pReg8 = cData;
		this->ReadMemory(anAddr,cData);
		pReg8 = GetReg8(I8085_REG_H);
		*pReg8 = cData;
	}
	else
	{
		pReg8 = GetReg8(I8085_REG_L);
		cData = *pReg8;
		this->WriteMemory(anAddr++,cData);
		pReg8 = GetReg8(I8085_REG_H);
		cData = *pReg8;
		this->WriteMemory(anAddr,cData);
	}
}
//------------------------------------------------------------------------------
void my1Sim85::ExecINXDCX(abyte sel)
{
	aword *pReg16;
	switch((sel&0X06)>>1) // 2-bits selection
	{
		case I8085_RP_BC:
		case I8085_RP_DE:
		case I8085_RP_HL:
			pReg16 = &mRegPairs[sel&0x03];
			break;
		case I8085_RP_SP:
			pReg16 = &mSPointer;
			break;
	}
	if(sel&0x01)
		(*pReg16)--;
	else
		(*pReg16)++;
}
//------------------------------------------------------------------------------
void my1Sim85::ExecINRDCR(abyte reg, abyte sel) // FIXME!
{
	abyte data, flag;
	aword testx, testy;
	testx = this->GetSrcData(reg);
	testy = testx&0x000F;
	if(sel&0x01)
	{
		testx--;
		testy--;
	}
	else
	{
		testx++;
		testy++;
	}
	// check carries!
	flag = 0x00;
	if(testx&0x100) // carry
		flag |= 0x01;
	if(testy&0x10) // aux carry
		flag |= 0x10;
	// update flag!
	if(data==0x00) flag |= 0x40; // zero
	flag |= (data&0x80); // sign flag
	flag |= GetParity(data); // parity flag
	mFullReg[I8085_REG_F] = flag;
	this->PutDstData(reg,data);
}
//------------------------------------------------------------------------------
void my1Sim85::ExecROTATE(abyte sel)
{
	abyte carry;
	if(sel&0x01) // rotate right
	{
		carry = mFullReg[I8085_REG_A]&0x01;
		if(sel&0x02)
			carry = mFullReg[I8085_REG_F]&0x01;
		mFullReg[I8085_REG_A] = (mFullReg[I8085_REG_A] >> 1) | carry << 7;
	}
	else // rotate left
	{
		carry = (mFullReg[I8085_REG_A]&0x80)>>7;
		if(sel&0x02)
			carry = mFullReg[I8085_REG_F]&0x01;
		mFullReg[I8085_REG_A] = (mFullReg[I8085_REG_A] << 1) | carry;
	}
	// update carry flag
	if(carry)
		mFullReg[I8085_REG_F] |= 0x01;
	else
		mFullReg[I8085_REG_F] &= ~0x01;
}
//------------------------------------------------------------------------------
void my1Sim85::ExecDCSC(abyte sel)
{
	if(sel&0x02) // carry flag ops
	{
		if(sel&0x01)
			mFullReg[I8085_REG_F] ^= 0x01;
		else
			mFullReg[I8085_REG_F] |= 0x01;
	}
	else // accumulator ops
	{
		if(sel&0x01)
			mFullReg[I8085_REG_A] = ~mFullReg[I8085_REG_A];
		else // DAA
		{
			abyte data, flag = 0x00;
			aword testx, testy;
			testx = mFullReg[I8085_REG_A]&0x00F0;
			testy = mFullReg[I8085_REG_A]&0x000F;
			if(testy>0x09||(mFullReg[I8085_REG_F]&0x10))
			{
				testy += 6;
				if(testy&0x0010)
					flag |= 0x10;
			}
			testx += flag;
			if(testx>0x90||(mFullReg[I8085_REG_F]&0x01))
			{
				testx += 0x60;
				if(testx>0xFF)
					flag |= 0x01;
			}
			data = (testx+testy)&0xFF;
			// update flag!
			if(data==0x00) flag |= 0x40; // zero
			flag |= (data&0x80); // sign flag
			flag |= GetParity(data); // parity flag
			mFullReg[I8085_REG_F] = flag;
			mFullReg[I8085_REG_A] = data;
		}
	}
}
//------------------------------------------------------------------------------
void my1Sim85::ExecRSIM(abyte sel)
{
	// ONLY A 'PLACEHOLDER' NOT PROPERLY IMPLEMENTED!
	if(sel)
	{
		// sim
		mIntrReg = mFullReg[I8085_REG_A];
	}
	else
	{
		// rim
		mFullReg[I8085_REG_A] = mIntrReg;
	}
}
//------------------------------------------------------------------------------
void my1Sim85::ExecPUSH(abyte sel)
{
	aword *pData;
	switch(asel&0x02)
	{
		case I8085_RP_BC:
			pData = (aword*) &mFullReg[I8085_REG_B];
		case I8085_RP_DE:
			pData = (aword*) &mFullReg[I8085_REG_D];
		case I8085_RP_HL:
			pData = (aword*) &mFullReg[I8085_REG_H];
		case I8085_RPPSW:
			pData = (aword*) &mFullReg[I8085_REG_F];
	}
	this->DoStackPush(pData);
}
//------------------------------------------------------------------------------
void my1Sim85::ExecPOP(abyte sel)
{
	aword *pData;
	switch(asel&0x02)
	{
		case I8085_RP_BC:
			pData = (aword*) &mFullReg[I8085_REG_B];
		case I8085_RP_DE:
			pData = (aword*) &mFullReg[I8085_REG_D];
		case I8085_RP_HL:
			pData = (aword*) &mFullReg[I8085_REG_H];
		case I8085_RPPSW:
			pData = (aword*) &mFullReg[I8085_REG_F];
	}
	this->DoStackPop(pData);
}
//------------------------------------------------------------------------------
void my1Sim85::ExecCALL(aword p_addr)
{
	this->DoStackPush(mPCounter);
	mPCounter = p_addr;
}
//------------------------------------------------------------------------------
void my1Sim85::ExecRET(void)
{
	this->DoStackPop(mPCounter);
}
//------------------------------------------------------------------------------
void my1Sim85::ExecRSTn(abyte sel)
{
	aword cTarget = sel * 8;
	this->ExecCALL(cTarget);
}
//------------------------------------------------------------------------------
void my1Sim85::ExecJUMP(aword p_addr)
{
	mPCounter = p_addr;
}
//------------------------------------------------------------------------------
void my1Sim85::ExecOUTIN(abyte sel, abyte p_addr)
{
	if(sel&0x01)
		this->ReadDevice(p_addr,mFullReg[I8085_REG_A]);
	else
		this->WriteDevice(p_addr,mFullReg[I8085_REG_A]);
}
//------------------------------------------------------------------------------
void my1Sim85::ExecCHG(abyte sel)
{
	aword t_data, *pdata;
	if(sel&0x01) // xchg
	{
		//pdata = &mFullReg[I8085_REG_H];
		//t_data = ;
	}
	else
		this->WriteDevice(p_addr,mFullReg[I8085_REG_A]);
}
//------------------------------------------------------------------------------
bool my1Sim85::ResetSim(void)
{
	mPCounter = 0x0000;
	return this->GetCodex(mPCounter);
}
//------------------------------------------------------------------------------
bool my1Sim85::StepSim(void)
{
	if(mHalted)
		return true;
	if(!this->ExeCodex())
		return false;
	return this->GetCodex(mPCounter);
}
//------------------------------------------------------------------------------
bool my1Sim85::RunSim(int aStep=1)
{
	bool cFlag = true;
	while(cFlag&&aStep>0)
	{
		cFlag = this->StepSim();
		aStep--;
	}
	return cFlag;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
