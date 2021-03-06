//===-- EpiphanyAsmPrinter.cpp - Print machine code to an Epiphany .s file --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to GAS-format Epiphany assembly language.
//
//===----------------------------------------------------------------------===//

#include "EpiphanyAsmPrinter.h"

#include "InstPrinter/EpiphanyInstPrinter.h"
#include "MCTargetDesc/EpiphanyAddressingModes.h"
#include "MCTargetDesc/EpiphanyBaseInfo.h"
#include "Epiphany.h"
#include "EpiphanyInstrInfo.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Mangler.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

bool EpiphanyAsmPrinter::runOnMachineFunction(MachineFunction &MF) {
  EpiphanyFI = MF.getInfo<EpiphanyMachineFunctionInfo>();
  AsmPrinter::runOnMachineFunction(MF);
  return true;
}

//@EmitInstruction {
//- EmitInstruction() must exists or will have run time error.
void EpiphanyAsmPrinter::EmitInstruction(const MachineInstr *MI) {
  if (MI->isDebugValue()) {
    SmallString<128> Str;
    raw_svector_ostream OS(Str);

    PrintDebugValueComment(MI, OS);
    return;
  }
  
  //@print out instruction:
  //  Print out both ordinary instruction and boudle instruction
  MachineBasicBlock::const_instr_iterator I = MI->getIterator();
  MachineBasicBlock::const_instr_iterator E = MI->getParent()->instr_end();
  do {
  
    if (I->isPseudo())
      llvm_unreachable("Pseudo opcode found in EmitInstruction()");
  
    MCInst TmpInst;
    MCInstLowering.Lower(&*I, TmpInst);
    OutStreamer->EmitInstruction(TmpInst, getSubtargetInfo());
  } while ((++I != E) && I->isInsideBundle());
}
//@EmitInstruction }

//===----------------------------------------------------------------------===//
//
//  Epiphany Asm Directives
//
//  -- Frame directive "frame Stackpointer, Stacksize, RARegister"
//  Describe the stack frame.
//
//  -- Mask directives "(f)mask  bitmask, offset"
//  Tells the assembler which registers are saved and where.
//  bitmask - contain a little endian bitset indicating which registers are
//            saved on function prologue (e.g. with a 0x80000000 mask, the
//            assembler knows the register 31 (RA) is saved at prologue.
//  offset  - the position before stack pointer subtraction indicating where
//            the first saved register on prologue is located. (e.g. with a
//
//  Consider the following function prologue:
//
//    .frame  FP,48,R1
//    .mask   0xc0000000,-8
//       addiu SP, SP, -48
//       str A1, [SP,#40]
//       str FP, [SP,#36]
//
//    With a 0xc0000000 mask, the assembler knows the register 0 (A1) and
//    11 (FP) are saved at prologue. As the save order on prologue is from
//    left to right, A1 is saved first. A -8 offset means that after the
//    stack pointer subtration, the first register in the mask (A1) will be
//    saved at address 48-8=40.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Mask directives
//===----------------------------------------------------------------------===//
//	.frame	SP,8,LR
//->	.mask 	0x00000000,0
//	.set	noreorder
//	.set	nomacro

// Create a bitmask with all callee saved registers for CPU or Floating Point
// registers. For CPU registers consider LR, GP and FP for saving if necessary.
void EpiphanyAsmPrinter::printSavedRegsBitmask(raw_ostream &O) {
  // CPU and FPU Saved Registers Bitmasks
  unsigned CPUBitmask = 0;
  int CPUTopSavedRegOff;

  // Set the CPU and FPU Bitmasks
  const MachineFrameInfo &MFI = MF->getFrameInfo();
  const TargetRegisterInfo *TRI = MF->getSubtarget().getRegisterInfo();
  const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
  // size of stack area to which FP callee-saved regs are saved.
  unsigned CPURegSize = Epiphany::GPR32RegClass.getSize();
  unsigned i = 0, e = CSI.size();

  // Set CPU Bitmask.
  for (; i != e; ++i) {
    unsigned Reg = CSI[i].getReg();
    unsigned RegNum = TRI->getEncodingValue(Reg);
    CPUBitmask |= (1 << RegNum);
  }

  CPUTopSavedRegOff = CPUBitmask ? -CPURegSize : 0;

  // Print CPUBitmask
  O << "\t.mask \t"; printHex32(CPUBitmask, O);
  O << ',' << CPUTopSavedRegOff << '\n';
}

// Print a 32 bit hex number with all numbers.
void EpiphanyAsmPrinter::printHex32(unsigned Value, raw_ostream &O) {
  O << "0x";
  for (int i = 7; i >= 0; i--)
    O.write_hex((Value & (0xF << (i*4))) >> (i*4));
}

//===----------------------------------------------------------------------===//
// Frame and Set directives
//===----------------------------------------------------------------------===//
//->	.frame	$sp,8,$lr
//	.mask 	0x00000000,0
//	.set	noreorder
//	.set	nomacro
/// Frame Directive
void EpiphanyAsmPrinter::emitFrameDirective() {
  const TargetRegisterInfo &RI = *MF->getSubtarget().getRegisterInfo();

  unsigned stackReg  = RI.getFrameRegister(*MF);
  unsigned returnReg = RI.getRARegister();
  unsigned stackSize = MF->getFrameInfo().getStackSize();

  if (OutStreamer->hasRawTextSupport())
    OutStreamer->EmitRawText("\t.frame\t$" +
           StringRef(EpiphanyInstPrinter::getRegisterName(stackReg)).lower() +
           "," + Twine(stackSize) + ",$" +
           StringRef(EpiphanyInstPrinter::getRegisterName(returnReg)).lower());
}

/// Emit Set directives.
const char *EpiphanyAsmPrinter::getCurrentABIString() const {
  switch (static_cast<EpiphanyTargetMachine &>(TM).getABI().GetEnumValue()) {
  case EpiphanyABIInfo::ABI::E16:  return "abiE16";
  default: llvm_unreachable("Unknown Epiphany ABI");
  }
}


//		.type	main,@function
//->		.ent	main                    # @main
//	main:
void EpiphanyAsmPrinter::EmitFunctionEntryLabel() {
  if (OutStreamer->hasRawTextSupport())
    OutStreamer->EmitRawText("\t.ent\t" + Twine(CurrentFnSym->getName()));
  OutStreamer->EmitLabel(CurrentFnSym);
}

//  .frame  $sp,8,$pc
//  .mask   0x00000000,0
//->  .set  noreorder
//@-> .set  nomacro
/// EmitFunctionBodyStart - Targets can override this to emit stuff before
/// the first basic block in the function.
void EpiphanyAsmPrinter::EmitFunctionBodyStart() {
  MCInstLowering.Initialize(&MF->getContext());

  emitFrameDirective();

  if (OutStreamer->hasRawTextSupport()) {
    SmallString<128> Str;
    raw_svector_ostream OS(Str);
    printSavedRegsBitmask(OS);
    OutStreamer->EmitRawText(OS.str());
    OutStreamer->EmitRawText(StringRef("\t.set\tnoreorder"));
    OutStreamer->EmitRawText(StringRef("\t.set\tnomacro"));
  }
}

//->	.set	macro
//->	.set	reorder
//->	.end	main
/// EmitFunctionBodyEnd - Targets can override this to emit stuff after
/// the last basic block in the function.
void EpiphanyAsmPrinter::EmitFunctionBodyEnd() {
  // There are instruction for this macros, but they must
  // always be at the function end, and we can't emit and
  // break with BB logic.
  if (OutStreamer->hasRawTextSupport()) {
    OutStreamer->EmitRawText(StringRef("\t.set\tmacro"));
    OutStreamer->EmitRawText(StringRef("\t.set\treorder"));
    OutStreamer->EmitRawText("\t.end\t" + Twine(CurrentFnSym->getName()));
  }
}

//	.section .mdebug.abi32
//	.previous
void EpiphanyAsmPrinter::EmitStartOfAsmFile(Module &M) {
  // FIXME: Use SwitchSection.

  // Tell the assembler which ABI we are using
  if (OutStreamer->hasRawTextSupport())
    OutStreamer->EmitRawText("\t.section .mdebug." +
                            Twine(getCurrentABIString()));

  // return to previous section
  if (OutStreamer->hasRawTextSupport())
    OutStreamer->EmitRawText(StringRef("\t.previous"));
}

void EpiphanyAsmPrinter::PrintDebugValueComment(const MachineInstr *MI,
                                           raw_ostream &OS) {
  // TODO: implement
  OS << "PrintDebugValueComment()";
}

void EpiphanyAsmPrinter::printOperand(const MachineInstr *MI, int opNum,
                                  raw_ostream &O) {
  const MachineOperand &MO = MI->getOperand(opNum);
  bool closeP = false;

  if (MO.getTargetFlags())
    closeP = true;

  switch(MO.getTargetFlags()) {
    case EpiphanyII::MO_LOW: 
      O << "%low("; 
      break;
    case EpiphanyII::MO_HIGH: 
      O << "%high("; 
      break;
    case EpiphanyII::MO_PCREL8:
    case EpiphanyII::MO_PCREL16:
    case EpiphanyII::MO_PCREL32: 
      O << "%pcrel("; 
      break;
    default:break;
  }

  switch (MO.getType()) {
    case MachineOperand::MO_Register:
      O << StringRef(EpiphanyInstPrinter::getRegisterName(MO.getReg())).lower();
      break;

    case MachineOperand::MO_Immediate:
      O << "#" << MO.getImm();
      break;

    case MachineOperand::MO_MachineBasicBlock:
      MO.getMBB()->getSymbol()->print(O, MAI);
      return;

    case MachineOperand::MO_GlobalAddress:
      getSymbol(MO.getGlobal())->print(O, MAI);
      break;

    case MachineOperand::MO_BlockAddress: {
      MCSymbol *BA = GetBlockAddressSymbol(MO.getBlockAddress());
      O << BA->getName();
      break;
    }

    case MachineOperand::MO_ConstantPoolIndex:
      O << getDataLayout().getPrivateGlobalPrefix() << "CPI"
        << getFunctionNumber() << "_" << MO.getIndex();
      if (MO.getOffset())
        O << "+" << MO.getOffset();
      break;

    default:
      llvm_unreachable("<unknown operand type>");
  }

  if (closeP) O << ")";
}


// Print out an operand for an inline asm expression.
// Based on Mips backend code
bool EpiphanyAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNum,
    unsigned AsmVariant, const char *ExtraCode, raw_ostream &O) {
  // Does this asm operand have a single letter operand modifier?
  if (ExtraCode && ExtraCode[0]) {
    if (ExtraCode[1] != 0) return true; // Unknown modifier.

    const MachineOperand &MO = MI->getOperand(OpNum);
    switch (ExtraCode[0]) {
      default:
        // See if this is a generic print operand
        return AsmPrinter::PrintAsmOperand(MI,OpNum,AsmVariant,ExtraCode,O);
      case 'X': // hex const int
        if ((MO.getType()) != MachineOperand::MO_Immediate)
          return true;
        O << "0x" << Twine::utohexstr(MO.getImm());
        return false;
      case 'x': // hex const int (low 16 bits)
        if ((MO.getType()) != MachineOperand::MO_Immediate)
          return true;
        O << "0x" << Twine::utohexstr(MO.getImm() & 0xffff);
        return false;
      case 'd': // decimal const int
        if ((MO.getType()) != MachineOperand::MO_Immediate)
          return true;
        O << MO.getImm();
        return false;
      case 'm': // decimal const int minus 1
        if ((MO.getType()) != MachineOperand::MO_Immediate)
          return true;
        O << MO.getImm() - 1;
        return false;
      case 'z': 
        // #0 if zero, regular printing otherwise
        if (MO.getType() == MachineOperand::MO_Immediate && MO.getImm() == 0) {
          O << "#0";
          return false;
        }
        // If not, call printOperand as normal.
        break;
    }
  }

  // Use generic printOperand if there's no modifier
  printOperand(MI, OpNum, O);
  return false;
}

bool EpiphanyAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
    unsigned OpNum, unsigned AsmVariant, const char *ExtraCode, raw_ostream &O) {
  assert(OpNum + 1 < MI->getNumOperands() && "Insufficient operands");
  const MachineOperand &BaseMO = MI->getOperand(OpNum);
  const MachineOperand &OffsetMO = MI->getOperand(OpNum + 1);
  assert(BaseMO.isReg() && "Unexpected base pointer for inline asm memory operand.");
  assert(OffsetMO.isImm() && "Unexpected offset for inline asm memory operand.");
  int Offset = OffsetMO.getImm();

  // Currently we are expecting either no ExtraCode or 'D'
  assert(!ExtraCode && "Do not expect extracode. FIXME");

  O << EpiphanyInstPrinter::getRegisterName(BaseMO.getReg()) << ", #" << Offset;

  return false;
}


// Force static initialization.
extern "C" void LLVMInitializeEpiphanyAsmPrinter() {
    RegisterAsmPrinter<EpiphanyAsmPrinter> X(TheEpiphanyTarget);
}
