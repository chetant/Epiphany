//===-- EpiphanyInstPrinter.h - Convert Epiphany MCInst to assembly syntax --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class prints an Epiphany MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EPIPHANYINSTPRINTER_H
#define LLVM_EPIPHANYINSTPRINTER_H

#include "EpiphanyConfig.h"

#include "llvm/MC/MCInstPrinter.h"

namespace llvm {
// These enumeration declarations were orignally in EpiphanyInstrInfo.h but
// had to be moved here to avoid circular dependencies between
// LLVMEpiphanyCodeGen and LLVMEpiphanyAsmPrinter.

  class TargetMachine;

  class EpiphanyInstPrinter : public MCInstPrinter {
  public:
    EpiphanyInstPrinter(const MCAsmInfo &MAI, const MCInstrInfo &MII,
                        const MCRegisterInfo &MRI);

    // Autogenerated by tblgen
    void printInstruction(const MCInst *MI, raw_ostream &O);
    static const char *getRegisterName(unsigned RegNo);
    
    void printRegName(raw_ostream &OS, unsigned RegNo) const override;
    void printInst(const MCInst *MI, raw_ostream &O, StringRef Annot,
                   const MCSubtargetInfo &STI) override;
    
    bool printAliasInstr(const MCInst *MI, raw_ostream &O);
    void printCustomAliasOperand(const MCInst *MI, unsigned OpIdx,
                                 unsigned PrintMethodIdx, raw_ostream &O);


  private:
    void printOperand(const MCInst *MI, unsigned OpNo, raw_ostream &O);
    void printUnsignedImm(const MCInst *MI, unsigned OpNo, raw_ostream &O);
    void printMemOperand(const MCInst *MI, unsigned OpNo, raw_ostream &O);
};
}

#endif