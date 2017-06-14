//===---------------------EpiphanyFpuConfigPass.h--------------------------===//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef _LLVM_LIB_TARGET_EPIPHANY_EPIPHANYLSOPASS_H
#define _LLVM_LIB_TARGET_EPIPHANY_EPIPHANYLSOPASS_H

#include "Epiphany.h"
#include "EpiphanyConfig.h"
#include "EpiphanyMachineFunction.h"
#include "EpiphanySubtarget.h"
#include "EpiphanyTargetMachine.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetRegisterInfo.h"

namespace llvm {
  void initializeEpiphanyLoadStoreOptimizerPass(PassRegistry&);

  typedef struct LoadStoreFlags {
    // If a matching instruction is found, MergeForward is set to true if the
    // merge is to remove the first instruction and replace the second with
    // a pair-wise insn, and false if the reverse is true.
    bool MergeForward;
    LoadStoreFlags() : MergeForward(false) {}

    void setMergeForward(bool V = true) { MergeForward = V; }
    bool getMergeForward() const { return MergeForward; }
  } LoadStoreFlags;


  class EpiphanyLoadStoreOptimizer : public MachineFunctionPass {

    private:
      const EpiphanyInstrInfo  *TII;
      const TargetRegisterInfo *TRI;
      const EpiphanySubtarget  *Subtarget;
      MachineRegisterInfo *MRI;
      MachineFrameInfo   *MFI;
      // Track which registers have been modified and used.
      BitVector ModifiedRegs, UsedRegs;
      bool optimizeBlock(MachineBasicBlock &MBB);
      bool tryToMergeZeroStInst(MachineBasicBlock::iterator &MBBI);
      bool tryToPairLoadStoreInst(MachineBasicBlock::iterator &MBBI);
      MachineBasicBlock::iterator findMatchingInst(MachineBasicBlock::iterator I, 
          LoadStoreFlags &Flags, unsigned Limit);
      MachineBasicBlock::iterator mergePairedInsns(MachineBasicBlock::iterator I,
          MachineBasicBlock::iterator Paired, const LoadStoreFlags &Flags);
    public:
      static char ID;
      EpiphanyLoadStoreOptimizer() : MachineFunctionPass(ID) {
        initializeEpiphanyLoadStoreOptimizerPass(*PassRegistry::getPassRegistry());
      }

      StringRef getPassName() const {
        return "Epiphany Load/Store Optimization Pass";
      }
      bool runOnMachineFunction(MachineFunction &MF);
  };

} // namespace llvm


#endif
